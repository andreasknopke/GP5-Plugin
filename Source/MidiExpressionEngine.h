/*
  ==============================================================================

    MidiExpressionEngine.h
    
    Converts GP5 guitar techniques into realistic MIDI expressions
    - Legato/Hammer-On with note overlapping
    - Quantized pitch bend slides (simulates fret steps)
    - Vibrato with sine wave modulation
    - Keyswitches for articulations
    - Velocity layering for dynamics

  ==============================================================================
*/

#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include "GP5Parser.h"
#include <map>
#include <set>
#include <deque>

//==============================================================================
// Keyswitch Configuration (can be customized per library)
//==============================================================================
struct KeyswitchConfig
{
    int palmMute = 24;      // C0 - Palm Mute
    int harmonics = 26;     // D0 - Harmonics
    int sustain = 28;       // E0 - Sustain (normal)
    int staccato = 29;      // F0 - Staccato
    int legato = 31;        // G0 - Legato mode
    bool enabled = false;   // Keyswitches disabled by default
};

//==============================================================================
// Pending slide event for quantized pitch bend
//==============================================================================
struct PendingSlide
{
    int midiChannel;
    int startNote;
    int endNote;
    double startBeat;
    double endBeat;
    int currentStep;
    int totalSteps;
};

//==============================================================================
// Pending note-off for legato timing
//==============================================================================
struct PendingNoteOff
{
    int midiChannel;
    int midiNote;
    double scheduledBeat;
};

//==============================================================================
// MIDI Expression Engine
//==============================================================================
class MidiExpressionEngine
{
public:
    MidiExpressionEngine() = default;
    
    //==========================================================================
    // Configuration
    //==========================================================================
    void setKeyswitchConfig(const KeyswitchConfig& config) { keyswitchConfig = config; }
    KeyswitchConfig& getKeyswitchConfig() { return keyswitchConfig; }
    
    void setLegatoOverlapMs(double ms) { legatoOverlapMs = ms; }
    void setSlideStepDurationMs(double ms) { slideStepDurationMs = ms; }
    void setVibratoRate(float hz) { vibratoRateHz = hz; }
    void setVibratoDepth(int depth) { vibratoDepth = depth; }
    void setVibratoDelay(double seconds) { vibratoDelaySeconds = seconds; }
    void setVibratoAttack(double seconds) { vibratoAttackSeconds = seconds; }
    void setStrumDelay(double ms) { strumDelayPerStringMs = ms; }
    void setHumanizeAmount(int amount) { humanizeAmount = juce::jlimit(0, 20, amount); }
    void setSampleRate(double rate) { sampleRate = rate; }
    
    //==========================================================================
    // Process a beat and generate expressive MIDI
    // volumeScale: 0-127 where 100 = 100% velocity (default)
    // beatDurationInBeats: actual duration of this beat in quarter notes
    //==========================================================================
    void processBeat(juce::MidiBuffer& midiBuffer,
                     int midiChannel,
                     const GP5Beat& beat,
                     const GP5Beat* nextBeat,  // For lookahead (legato detection)
                     const GP5Track& track,
                     int transposeOffset,
                     int volumeScale,  // 0-127, scales velocity output
                     double currentBeat,
                     double beatsPerSecond,
                     double beatDurationInBeats,  // Actual note duration for scheduling note-offs
                     int sampleOffset = 0)
    {
        // Store volume scale for use in velocity calculation
        currentVolumeScale = volumeScale;
        currentBeatDuration = beatDurationInBeats;  // Store for use in helper functions
        
        // Safety: validate MIDI channel
        if (midiChannel < 1 || midiChannel > 16)
            return;
        
        if (beat.isRest)
        {
            // Stop all active notes on this channel (except pending legato)
            stopAllNotes(midiBuffer, midiChannel, sampleOffset);
            return;
        }
        
        // Check if next beat has legato notes
        bool nextIsLegato = false;
        if (nextBeat != nullptr && nextBeat->notes.size() <= 12)
        {
            for (const auto& [si, n] : nextBeat->notes)
            {
                if (si >= 0 && si < 12 && n.hasHammerOn) 
                    nextIsLegato = true;
            }
        }
        
        // Calculate strum delay per string (for realistic chord strumming)
        int strumDelaySamples = 0;
        int delayPerString = static_cast<int>((strumDelayPerStringMs / 1000.0) * sampleRate);
        bool isChord = beat.notes.size() > 1;
        
        // Safety: limit notes per beat to prevent runaway loops
        if (beat.notes.size() > 12)
            return;  // Corrupted data, skip this beat
        
        // Process each note in the beat
        for (const auto& [stringIndex, gpNote] : beat.notes)
        {
            // Safety: validate stringIndex from map key
            if (stringIndex < 0 || stringIndex >= 12)
                continue;  // Skip invalid string
            
            if (gpNote.isDead)
            {
                // Dead note - very short, muted sound
                processDeadNote(midiBuffer, midiChannel, stringIndex, track, transposeOffset, sampleOffset);
                continue;
            }
            
            if (gpNote.isTied)
            {
                // Tied note - don't retrigger, just continue
                continue;
            }
            
            // Calculate MIDI note
            int midiNote = calculateMidiNote(stringIndex, gpNote.fret, track, transposeOffset);
            if (midiNote <= 0 || midiNote >= 128)
                continue;
            
            // Calculate velocity with dynamics
            int velocity = calculateVelocity(gpNote);
            
            // Send keyswitch if needed
            if (keyswitchConfig.enabled)
            {
                sendKeyswitch(midiBuffer, midiChannel, gpNote, beat, sampleOffset);
            }
            
            // Handle different articulations
            if (gpNote.hasHammerOn)
            {
                processHammerOn(midiBuffer, midiChannel, midiNote, velocity, nextIsLegato, sampleOffset, currentBeat, beatsPerSecond);
            }
            else if (gpNote.hasSlide)
            {
                processSlide(midiBuffer, midiChannel, midiNote, gpNote, currentBeat, beatsPerSecond, sampleOffset);
            }
            else
            {
                // Calculate final sample offset with strum delay for chords
                int finalOffset = sampleOffset + (isChord ? strumDelaySamples : 0);
                
                // Normal note - don't stop notes here anymore, let scheduled note-offs handle it
                // Only stop if we're about to play the same note again
                if (activeNotes.count(midiChannel) && activeNotes[midiChannel].count(midiNote))
                {
                    midiBuffer.addEvent(juce::MidiMessage::noteOff(midiChannel, midiNote), finalOffset);
                    activeNotes[midiChannel].erase(midiNote);
                }
                
                // Vibrato
                if (gpNote.hasVibrato)
                {
                    startVibrato(midiChannel, currentBeat);
                }
                else
                {
                    stopVibrato(midiBuffer, midiChannel, finalOffset);
                }
                
                // Bend
                if (gpNote.hasBend && gpNote.bendValue != 0)
                {
                    int pitchBend = calculatePitchBend(gpNote.bendValue);
                    midiBuffer.addEvent(juce::MidiMessage::pitchWheel(midiChannel, pitchBend), finalOffset);
                }
                else
                {
                    midiBuffer.addEvent(juce::MidiMessage::pitchWheel(midiChannel, 8192), finalOffset);
                }
                
                // Note On with strum delay
                midiBuffer.addEvent(juce::MidiMessage::noteOn(midiChannel, midiNote, (juce::uint8)velocity), finalOffset);
                activeNotes[midiChannel].insert(midiNote);
                
                // Note-off will be handled when next beat starts (old behavior, stable)
                
                // Increment strum delay for next string in chord
                if (isChord) strumDelaySamples += delayPerString;
            }
        }
    }
    
    //==========================================================================
    // Update ongoing effects (call every process block)
    //==========================================================================
    void updateEffects(juce::MidiBuffer& midiBuffer, double currentBeat, double beatsPerSecond, int /*bufferSize*/)
    {
        // Safety check - avoid invalid values
        if (beatsPerSecond <= 0.0)
            beatsPerSecond = 2.0;  // Default to 120 BPM
        
        if (currentBeat < 0.0 || std::isnan(currentBeat) || std::isinf(currentBeat))
            return;  // Invalid position, skip processing
        
        // Update vibrato (sinusoidal modulation with fade-in envelope)
        for (auto& [channel, startBeat] : vibratoChannels)
        {
            double elapsedBeats = currentBeat - startBeat;
            double elapsedSeconds = elapsedBeats / beatsPerSecond;
            
            // VIBRATO ENVELOPE: Delay + Attack fade-in
            // Don't start vibrato until delay time has passed
            if (elapsedSeconds < vibratoDelaySeconds)
                continue;
            
            // Calculate fade factor (0.0 to 1.0) over attack time
            double timeSinceDelay = elapsedSeconds - vibratoDelaySeconds;
            float fadeFactor = 1.0f;
            if (vibratoAttackSeconds > 0.0 && timeSinceDelay < vibratoAttackSeconds)
            {
                fadeFactor = static_cast<float>(timeSinceDelay / vibratoAttackSeconds);
            }
            
            // Calculate vibrato phase (use time since delay for consistent phase)
            float phase = static_cast<float>(timeSinceDelay * vibratoRateHz * 2.0 * juce::MathConstants<double>::pi);
            
            // Apply fade factor to depth
            int effectiveDepth = static_cast<int>(vibratoDepth * fadeFactor);
            int modValue = 64 + static_cast<int>(effectiveDepth * std::sin(phase));
            modValue = juce::jlimit(0, 127, modValue);
            
            midiBuffer.addEvent(juce::MidiMessage::controllerEvent(channel, 1, modValue), 0);
        }
        
        // Update quantized slides
        updateSlides(midiBuffer, currentBeat, beatsPerSecond);
        
        // Note-offs are now handled when next beat starts (old behavior, stable)
    }
    
    //==========================================================================
    // Stop all notes (e.g., when playback stops)
    //==========================================================================
    void allNotesOff(juce::MidiBuffer& midiBuffer, int sampleOffset = 0)
    {
        for (auto& [channel, notes] : activeNotes)
        {
            for (int note : notes)
            {
                midiBuffer.addEvent(juce::MidiMessage::noteOff(channel, note), sampleOffset);
            }
            notes.clear();
        }
        
        // Reset pitch bend on all channels
        for (int ch = 1; ch <= 16; ++ch)
        {
            midiBuffer.addEvent(juce::MidiMessage::pitchWheel(ch, 8192), sampleOffset);
            midiBuffer.addEvent(juce::MidiMessage::controllerEvent(ch, 1, 0), sampleOffset);  // Mod wheel off
        }
        
        vibratoChannels.clear();
        pendingSlides.clear();
        pendingNoteOffs.clear();
    }
    
    //==========================================================================
    // Get active notes for a channel
    //==========================================================================
    const std::set<int>& getActiveNotes(int channel) const
    {
        static std::set<int> empty;
        auto it = activeNotes.find(channel);
        return it != activeNotes.end() ? it->second : empty;
    }
    
private:
    KeyswitchConfig keyswitchConfig;
    
    // Timing parameters
    double legatoOverlapMs = 40.0;       // How long notes overlap for legato (increased!)
    double slideStepDurationMs = 50.0;   // Duration of each fret step in slide
    float vibratoRateHz = 5.0f;          // Vibrato oscillation rate
    int vibratoDepth = 40;               // Vibrato depth (0-64)
    
    // Vibrato envelope parameters
    double vibratoDelaySeconds = 0.25;   // Delay before vibrato starts
    double vibratoAttackSeconds = 0.4;   // Time to reach full vibrato depth
    
    // Strumming parameters
    double strumDelayPerStringMs = 12.0; // Delay between strings in a chord (ms)
    double sampleRate = 44100.0;         // Sample rate for timing calculations
    
    // Humanization
    juce::Random humanizeRng;            // Random generator for velocity humanization
    int humanizeAmount = 8;              // +/- velocity variation
    
    // State tracking
    std::map<int, std::set<int>> activeNotes;  // channel -> active MIDI notes
    std::map<int, double> vibratoChannels;     // channel -> start beat
    std::deque<PendingSlide> pendingSlides;
    
    // Current volume scale for velocity calculation (set per processBeat call)
    int currentVolumeScale = 100;
    double currentBeatDuration = 1.0;  // Current beat duration in quarter notes
    std::deque<PendingNoteOff> pendingNoteOffs;
    
    //==========================================================================
    // Helper: Calculate MIDI note from string/fret
    //==========================================================================
    int calculateMidiNote(int stringIndex, int fret, const GP5Track& track, int transposeOffset)
    {
        // Safety: check for invalid stringIndex
        if (stringIndex < 0 || stringIndex >= 12)
            return -1;  // Invalid note
        
        // Safety: check for invalid fret
        if (fret < 0 || fret > 30)
            return -1;  // Invalid fret
        
        int midiNote = 0;
        
        // Safety: check if tuning vector is valid and not empty
        int tuningSize = track.tuning.size();
        if (tuningSize > 0 && tuningSize <= 12 && stringIndex < tuningSize)
        {
            // Use safe access method
            int tuningValue = track.tuning.getUnchecked(stringIndex);
            // Sanity check: tuning should be in MIDI range
            if (tuningValue >= 0 && tuningValue < 128)
            {
                midiNote = tuningValue + fret;
            }
            else
            {
                return -1;  // Invalid tuning value
            }
        }
        else
        {
            // Standard tuning fallback (E4, B3, G3, D3, A2, E2)
            const int standardTuning[] = { 64, 59, 55, 50, 45, 40 };
            if (stringIndex < 6)
                midiNote = standardTuning[stringIndex] + fret;
            else
                return -1;  // Invalid string
        }
        
        int result = midiNote + transposeOffset;
        
        // Final sanity check
        if (result < 0 || result >= 128)
            return -1;
        
        return result;
    }
    
    //==========================================================================
    // Helper: Calculate velocity with dynamics and volume scaling
    //==========================================================================
    int calculateVelocity(const GP5Note& note)
    {
        int velocity = note.velocity > 0 ? note.velocity : 95;
        
        if (note.isGhost)
            velocity = juce::jlimit(30, 50, velocity / 2);  // Ghost notes very quiet
        else if (note.hasHeavyAccent)
            velocity = juce::jlimit(115, 127, velocity + 30);
        else if (note.hasAccent)
            velocity = juce::jlimit(100, 120, velocity + 15);
        else if (note.hasHammerOn)
            velocity = juce::jlimit(50, 80, velocity - 15);  // Hammer-ons slightly softer
        
        // HUMANIZATION: Add random variation (+/- humanizeAmount)
        if (humanizeAmount > 0)
        {
            int humanize = humanizeRng.nextInt(humanizeAmount * 2 + 1) - humanizeAmount;
            velocity += humanize;
        }
        
        // Apply volume scale (0-127, where 100 = 100%)
        // Scale velocity proportionally: velocity * (volumeScale / 100)
        velocity = static_cast<int>(velocity * currentVolumeScale / 100.0);
        
        return juce::jlimit(1, 127, velocity);
    }
    
    //==========================================================================
    // Helper: Calculate pitch bend value
    //==========================================================================
    int calculatePitchBend(int bendValue)
    {
        // GP5 bend values:
        // 50 = quarter tone (1/4 semitone)
        // 100 = half tone (1 semitone)  
        // 200 = whole tone (2 semitones)
        // 300 = 1.5 whole tones (3 semitones)
        // 400 = 2 whole tones (4 semitones) - max typical guitar bend
        //
        // MIDI pitch bend: 0-16383, center at 8192
        // Standard pitch bend range is ±2 semitones (can be configured in synth)
        // 8192 units per 2 semitones = 4096 per semitone = 40.96 per GP5 unit
        //
        // For bends > 2 semitones, we use the full range and hope synth is set to ±12
        
        // Calculate: bendValue/100 = semitones, * 4096 = pitch bend units
        int pitchBend = 8192 + (bendValue * 41);  // 4096/100 ≈ 41
        
        DBG("Pitch Bend: GP5 value=" << bendValue << " -> MIDI PB=" << pitchBend);
        
        return juce::jlimit(0, 16383, pitchBend);
    }
    
    //==========================================================================
    // Process dead note (muted, percussive)
    //==========================================================================
    void processDeadNote(juce::MidiBuffer& midiBuffer, int channel, int stringIndex,
                         const GP5Track& track, int transposeOffset, int sampleOffset)
    {
        // Safety: validate stringIndex
        if (stringIndex < 0 || stringIndex >= 12)
            return;
        
        int midiNote = calculateMidiNote(stringIndex, 0, track, transposeOffset);
        if (midiNote > 0 && midiNote < 128)
        {
            // Very short note with low velocity
            midiBuffer.addEvent(juce::MidiMessage::noteOn(channel, midiNote, (juce::uint8)40), sampleOffset);
            // Note-off almost immediately (will be handled by next beat)
            activeNotes[channel].insert(midiNote);
        }
    }
    
    //==========================================================================
    // Process hammer-on/pull-off with TRUE LEGATO (note overlap)
    //==========================================================================
    void processHammerOn(juce::MidiBuffer& midiBuffer, int channel, int midiNote,
                         int velocity, bool /*nextIsLegato*/, int sampleOffset,
                         double currentBeat, double beatsPerSecond)
    {
        // TRUE LEGATO: DO NOT stop previous notes immediately!
        // Instead, schedule them for note-off after a short overlap period
        
        // Schedule all currently active notes on this channel for delayed note-off
        if (activeNotes.count(channel))
        {
            double overlapBeats = (legatoOverlapMs / 1000.0) * beatsPerSecond;
            double noteOffBeat = currentBeat + overlapBeats;
            
            for (int oldNote : activeNotes[channel])
            {
                // Don't schedule note-off for the same note we're about to play
                if (oldNote != midiNote)
                {
                    PendingNoteOff pending;
                    pending.midiChannel = channel;
                    pending.midiNote = oldNote;
                    pending.scheduledBeat = noteOffBeat;
                    pendingNoteOffs.push_back(pending);
                }
            }
            // Don't clear activeNotes here - the pending system handles it
        }
        
        // Send legato controller (for sample libraries that support it)
        midiBuffer.addEvent(juce::MidiMessage::controllerEvent(channel, 68, 127), sampleOffset);
        
        // Play new note immediately (overlapping with the old note)
        midiBuffer.addEvent(juce::MidiMessage::noteOn(channel, midiNote, (juce::uint8)velocity), sampleOffset);
        activeNotes[channel].insert(midiNote);
        
        // Note-off will be handled when next beat starts (old behavior, stable)
    }
    
    //==========================================================================
    // Process slide with quantized pitch bend (fret steps)
    //==========================================================================
    void processSlide(juce::MidiBuffer& midiBuffer, int channel, int startNote,
                      const GP5Note& gpNote, double currentBeat, double beatsPerSecond, int sampleOffset)
    {
        // Start the note with pitch bend at center
        int velocity = calculateVelocity(gpNote);
        midiBuffer.addEvent(juce::MidiMessage::pitchWheel(channel, 8192), sampleOffset);  // Reset bend first
        midiBuffer.addEvent(juce::MidiMessage::noteOn(channel, startNote, (juce::uint8)velocity), sampleOffset);
        activeNotes[channel].insert(startNote);
        
        // Determine slide direction and amount
        // slideType: 1=shift slide up, 2=shift slide down, 3=slide into from below, 4=slide into from above
        //            5=slide out downwards, 6=slide out upwards
        int semitonesSlide = 0;
        switch (gpNote.slideType)
        {
            case 1: case 6: semitonesSlide = 3; break;   // Slide up: 3 semitones
            case 2: case 5: semitonesSlide = -3; break;  // Slide down: 3 semitones
            case 3: semitonesSlide = 2; break;           // Into from below (bend up)
            case 4: semitonesSlide = -2; break;          // Into from above (bend down)
            default: return;  // No slide
        }
        
        if (semitonesSlide != 0)
        {
            // Schedule the slide
            PendingSlide slide;
            slide.midiChannel = channel;
            slide.startNote = startNote;
            slide.endNote = startNote + semitonesSlide;
            slide.startBeat = currentBeat;
            
            // Slide duration: roughly 1/8 note at current tempo
            double safeBps = (beatsPerSecond > 0.0) ? beatsPerSecond : 2.0;
            slide.endBeat = currentBeat + (0.5 / safeBps);  // Half a beat
            slide.currentStep = 0;
            slide.totalSteps = juce::jmax(1, std::abs(semitonesSlide) * 4);
            
            pendingSlides.push_back(slide);
            DBG("Slide scheduled: " << semitonesSlide << " semitones over " << (slide.endBeat - slide.startBeat) << " beats");
        }
        
        // Note-off will be handled when next beat starts (old behavior, stable)
    }
    
    //==========================================================================
    // Update pending slides (quantized pitch bend)
    //==========================================================================
    void updateSlides(juce::MidiBuffer& midiBuffer, double currentBeat, double /*beatsPerSecond*/)
    {
        for (auto it = pendingSlides.begin(); it != pendingSlides.end(); )
        {
            PendingSlide& slide = *it;
            
            // Safety: avoid division by zero
            double slideDuration = slide.endBeat - slide.startBeat;
            if (slideDuration <= 0.0)
            {
                it = pendingSlides.erase(it);
                continue;
            }
            
            double progress = (currentBeat - slide.startBeat) / slideDuration;
            progress = juce::jlimit(0.0, 1.0, progress);
            
            // Calculate current step (quantized)
            int newStep = static_cast<int>(progress * slide.totalSteps);
            
            if (newStep != slide.currentStep || newStep == 0)
            {
                slide.currentStep = newStep;
                
                // Calculate pitch bend for this step
                // semitoneDiff tells us how many semitones to bend
                // Each semitone = 4096 pitch bend units (assuming ±2 semitone range)
                int semitoneDiff = slide.endNote - slide.startNote;
                int maxBend = semitoneDiff * 4096;  // Full bend amount in PB units
                int currentBend = static_cast<int>(progress * maxBend);
                int pitchBend = 8192 + currentBend;
                pitchBend = juce::jlimit(0, 16383, pitchBend);
                
                midiBuffer.addEvent(juce::MidiMessage::pitchWheel(slide.midiChannel, pitchBend), 0);
            }
            
            // Remove completed slides
            if (progress >= 1.0)
            {
                it = pendingSlides.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }
    
    //==========================================================================
    // Vibrato control
    //==========================================================================
    void startVibrato(int channel, double currentBeat)
    {
        vibratoChannels[channel] = currentBeat;
    }
    
    void stopVibrato(juce::MidiBuffer& midiBuffer, int channel, int sampleOffset)
    {
        vibratoChannels.erase(channel);
        midiBuffer.addEvent(juce::MidiMessage::controllerEvent(channel, 1, 0), sampleOffset);
    }
    
    //==========================================================================
    // Keyswitch handling
    //==========================================================================
    void sendKeyswitch(juce::MidiBuffer& midiBuffer, int channel, const GP5Note& note,
                       const GP5Beat& beat, int sampleOffset)
    {
        int keyswitch = -1;
        
        if (beat.isPalmMute)
            keyswitch = keyswitchConfig.palmMute;
        else if (note.hasHarmonic)
            keyswitch = keyswitchConfig.harmonics;
        else if (note.hasHammerOn)
            keyswitch = keyswitchConfig.legato;
        else
            keyswitch = keyswitchConfig.sustain;
        
        if (keyswitch >= 0)
        {
            // Send keyswitch (very short note)
            midiBuffer.addEvent(juce::MidiMessage::noteOn(channel, keyswitch, (juce::uint8)100), sampleOffset);
            midiBuffer.addEvent(juce::MidiMessage::noteOff(channel, keyswitch), sampleOffset + 1);
        }
    }
    
    //==========================================================================
    // Note management
    //==========================================================================
    void stopNotesOnChannel(juce::MidiBuffer& midiBuffer, int channel, int sampleOffset)
    {
        if (activeNotes.count(channel))
        {
            for (int note : activeNotes[channel])
            {
                midiBuffer.addEvent(juce::MidiMessage::noteOff(channel, note), sampleOffset);
            }
            activeNotes[channel].clear();
        }
        
        // Reset legato controller
        midiBuffer.addEvent(juce::MidiMessage::controllerEvent(channel, 68, 0), sampleOffset);
    }
    
    void stopAllNotes(juce::MidiBuffer& midiBuffer, int channel, int sampleOffset)
    {
        stopNotesOnChannel(midiBuffer, channel, sampleOffset);
        stopVibrato(midiBuffer, channel, sampleOffset);
        midiBuffer.addEvent(juce::MidiMessage::pitchWheel(channel, 8192), sampleOffset);
    }
    
    //==========================================================================
    // Process scheduled note-offs (only used for legato overlap timing)
    //==========================================================================
    void processPendingNoteOffs(juce::MidiBuffer& midiBuffer, double currentBeat)
    {
        // Only process legato overlap note-offs
        // Regular note-offs are handled when next beat starts
        while (!pendingNoteOffs.empty() && pendingNoteOffs.front().scheduledBeat <= currentBeat)
        {
            const auto& pending = pendingNoteOffs.front();
            midiBuffer.addEvent(juce::MidiMessage::noteOff(pending.midiChannel, pending.midiNote), 0);
            if (activeNotes.count(pending.midiChannel))
            {
                activeNotes[pending.midiChannel].erase(pending.midiNote);
            }
            pendingNoteOffs.pop_front();
        }
    }
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiExpressionEngine)
};
