/*
  ==============================================================================

    MidiExpressionEngine.h
    
    AUDIO-THREAD-SAFE MIDI Expression Engine
    
    Converts GP5 guitar techniques into realistic MIDI expressions:
    - Legato/Hammer-On with note overlapping
    - Velocity layering for dynamics (Ghost, Accent, Heavy Accent)
    - Vibrato with CC1 modulation
    - Basic articulation CCs (Legato CC68, Portamento CC65)
    
    Design-Prinzipien für Audio-Thread-Sicherheit:
    - KEINE dynamische Speicherallokation im Audio-Thread
    - KEINE std::deque, std::vector::push_back, std::map im Audio-Thread
    - Feste Array-Größen, vorallokiert
    - Deterministische Velocity-Variation (kein Random im Audio-Thread)

  ==============================================================================
*/

#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include "GP5Parser.h"
#include <array>

//==============================================================================
// Konstanten für feste Array-Größen
//==============================================================================
static constexpr int MAX_ACTIVE_NOTES_PER_CHANNEL = 12;  // Max Noten pro Kanal (6 Saiten * 2)
static constexpr int MAX_CHANNELS = 16;

//==============================================================================
// Audio-Thread-sichere aktive Noten-Verwaltung
// Verwendet feste Arrays statt std::set
//==============================================================================
struct ActiveNoteBuffer
{
    std::array<int, MAX_ACTIVE_NOTES_PER_CHANNEL> notes{};
    int count = 0;
    
    void clear()
    {
        count = 0;
    }
    
    bool contains(int note) const
    {
        for (int i = 0; i < count; ++i)
            if (notes[i] == note) return true;
        return false;
    }
    
    bool add(int note)
    {
        if (count >= MAX_ACTIVE_NOTES_PER_CHANNEL) return false;
        if (contains(note)) return false;
        notes[count++] = note;
        return true;
    }
    
    bool remove(int note)
    {
        for (int i = 0; i < count; ++i)
        {
            if (notes[i] == note)
            {
                // Swap mit letztem Element
                notes[i] = notes[count - 1];
                --count;
                return true;
            }
        }
        return false;
    }
};

//==============================================================================
// MIDI Expression Engine - Audio-Thread-Safe
//==============================================================================
class MidiExpressionEngine
{
public:
    MidiExpressionEngine()
    {
        reset();
    }
    
    //==========================================================================
    // Configuration (call from UI thread before playback)
    //==========================================================================
    void setLegatoOverlapMs(double ms) { legatoOverlapMs = ms; }
    void setVibratoDepth(int depth) { vibratoDepth = juce::jlimit(0, 127, depth); }
    void setSampleRate(double rate) { sampleRate = rate; }
    
    //==========================================================================
    // Reset (call when playback stops)
    //==========================================================================
    void reset()
    {
        for (auto& buf : activeNotesPerChannel)
            buf.clear();
        vibratoActiveChannel = -1;
        vibratoStartBeat = 0.0;
    }
    
    //==========================================================================
    // Process a GP5 beat and generate MIDI events
    // 
    // AUDIO-THREAD-SAFE: No allocations, no locks
    //==========================================================================
    void processBeat(
        juce::MidiBuffer& midiBuffer,
        int midiChannel,
        const GP5Beat& beat,
        const GP5Beat* nextBeat,
        const GP5Track& track,
        int volumeScale,        // 0-127, 100 = neutral
        double currentBeat,
        double beatsPerSecond,
        int sampleOffset = 0)
    {
        // Validate channel
        if (midiChannel < 1 || midiChannel > 16)
            return;
        
        int channelIdx = midiChannel - 1;
        
        // Rest: Stop all notes on this channel
        if (beat.isRest)
        {
            stopAllNotesOnChannel(midiBuffer, midiChannel, sampleOffset);
            stopVibrato(midiBuffer, midiChannel, sampleOffset);
            return;
        }
        
        // Safety check
        if (beat.notes.size() > 12)
            return;
        
        // Check if next beat has legato (Hammer-On)
        bool nextIsLegato = false;
        if (nextBeat != nullptr)
        {
            for (const auto& [si, n] : nextBeat->notes)
            {
                if (n.hasHammerOn)
                {
                    nextIsLegato = true;
                    break;
                }
            }
        }
        
        // Stop previous notes UNLESS next beat is legato
        if (!nextIsLegato)
        {
            stopAllNotesOnChannel(midiBuffer, midiChannel, sampleOffset);
        }
        
        // Reset pitch bend at beat start
        midiBuffer.addEvent(juce::MidiMessage::pitchWheel(midiChannel, 8192), sampleOffset);
        
        // Process each note
        bool anyVibrato = false;
        
        for (const auto& [stringIndex, gpNote] : beat.notes)
        {
            // Validate string index
            if (stringIndex < 0 || stringIndex >= 12)
                continue;
            
            // Skip tied notes (they continue from previous beat)
            if (gpNote.isTied)
                continue;
            
            // Calculate MIDI note
            int midiNote = calculateMidiNote(stringIndex, gpNote.fret, track);
            if (midiNote <= 0 || midiNote >= 128)
                continue;
            
            // Calculate velocity with dynamics
            int velocity = calculateVelocity(gpNote, volumeScale);
            
            // Dead note: very short muted note
            if (gpNote.isDead)
            {
                // Stop if already playing
                if (activeNotesPerChannel[channelIdx].contains(midiNote))
                {
                    midiBuffer.addEvent(juce::MidiMessage::noteOff(midiChannel, midiNote), sampleOffset);
                    activeNotesPerChannel[channelIdx].remove(midiNote);
                }
                // Play very quiet, short note
                midiBuffer.addEvent(juce::MidiMessage::noteOn(midiChannel, midiNote, (juce::uint8)40), sampleOffset);
                activeNotesPerChannel[channelIdx].add(midiNote);
                continue;
            }
            
            // Hammer-On: Send legato controller, softer velocity
            if (gpNote.hasHammerOn)
            {
                midiBuffer.addEvent(juce::MidiMessage::controllerEvent(midiChannel, 68, 127), sampleOffset);
                velocity = juce::jmax(40, velocity - 20);
            }
            else
            {
                midiBuffer.addEvent(juce::MidiMessage::controllerEvent(midiChannel, 68, 0), sampleOffset);
            }
            
            // Slide: Send portamento controller
            if (gpNote.hasSlide)
            {
                midiBuffer.addEvent(juce::MidiMessage::controllerEvent(midiChannel, 65, 127), sampleOffset);
                midiBuffer.addEvent(juce::MidiMessage::controllerEvent(midiChannel, 5, 64), sampleOffset);
            }
            else
            {
                midiBuffer.addEvent(juce::MidiMessage::controllerEvent(midiChannel, 65, 0), sampleOffset);
            }
            
            // Bend
            if (gpNote.hasBend && gpNote.bendValue != 0)
            {
                int pitchBend = 8192 + (gpNote.bendValue * 41);  // 4096/100 ≈ 41
                pitchBend = juce::jlimit(0, 16383, pitchBend);
                midiBuffer.addEvent(juce::MidiMessage::pitchWheel(midiChannel, pitchBend), sampleOffset);
            }
            
            // Vibrato tracking
            if (gpNote.hasVibrato)
                anyVibrato = true;
            
            // Stop note if already playing (avoid double-trigger)
            if (activeNotesPerChannel[channelIdx].contains(midiNote))
            {
                midiBuffer.addEvent(juce::MidiMessage::noteOff(midiChannel, midiNote), sampleOffset);
                activeNotesPerChannel[channelIdx].remove(midiNote);
            }
            
            // Play the note
            midiBuffer.addEvent(juce::MidiMessage::noteOn(midiChannel, midiNote, (juce::uint8)velocity), sampleOffset);
            activeNotesPerChannel[channelIdx].add(midiNote);
        }
        
        // Vibrato control
        if (anyVibrato)
        {
            startVibrato(midiChannel, currentBeat);
        }
        else
        {
            stopVibrato(midiBuffer, midiChannel, sampleOffset);
        }
    }
    
    //==========================================================================
    // Update ongoing effects (call every process block)
    // Handles vibrato modulation
    //==========================================================================
    void updateEffects(juce::MidiBuffer& midiBuffer, double currentBeat, double beatsPerSecond)
    {
        // Safety
        if (beatsPerSecond <= 0.0)
            beatsPerSecond = 2.0;
        
        // Update vibrato
        if (vibratoActiveChannel > 0)
        {
            double elapsedBeats = currentBeat - vibratoStartBeat;
            double elapsedSeconds = elapsedBeats / beatsPerSecond;
            
            // Delay before vibrato starts (0.2 seconds)
            if (elapsedSeconds < 0.2)
            {
                midiBuffer.addEvent(juce::MidiMessage::controllerEvent(vibratoActiveChannel, 1, 0), 0);
            }
            else
            {
                // Fade in over 0.3 seconds
                double fadeTime = elapsedSeconds - 0.2;
                float fadeFactor = juce::jlimit(0.0f, 1.0f, static_cast<float>(fadeTime / 0.3));
                
                // Sine wave vibrato at 5 Hz
                float phase = static_cast<float>(fadeTime * 5.0 * 2.0 * juce::MathConstants<double>::pi);
                int effectiveDepth = static_cast<int>(vibratoDepth * fadeFactor);
                int modValue = 64 + static_cast<int>(effectiveDepth * std::sin(phase));
                modValue = juce::jlimit(0, 127, modValue);
                
                midiBuffer.addEvent(juce::MidiMessage::controllerEvent(vibratoActiveChannel, 1, modValue), 0);
            }
        }
    }
    
    //==========================================================================
    // Stop all notes (call when playback stops)
    //==========================================================================
    void allNotesOff(juce::MidiBuffer& midiBuffer, int sampleOffset = 0)
    {
        for (int ch = 1; ch <= 16; ++ch)
        {
            stopAllNotesOnChannel(midiBuffer, ch, sampleOffset);
        }
        
        // Reset all controllers
        for (int ch = 1; ch <= 16; ++ch)
        {
            midiBuffer.addEvent(juce::MidiMessage::pitchWheel(ch, 8192), sampleOffset);
            midiBuffer.addEvent(juce::MidiMessage::controllerEvent(ch, 1, 0), sampleOffset);
            midiBuffer.addEvent(juce::MidiMessage::controllerEvent(ch, 68, 0), sampleOffset);
            midiBuffer.addEvent(juce::MidiMessage::controllerEvent(ch, 65, 0), sampleOffset);
        }
        
        vibratoActiveChannel = -1;
    }
    
private:
    //==========================================================================
    // Configuration
    //==========================================================================
    double legatoOverlapMs = 40.0;
    int vibratoDepth = 40;
    double sampleRate = 44100.0;
    
    //==========================================================================
    // State - alle mit fester Größe, keine dynamische Allokation
    //==========================================================================
    std::array<ActiveNoteBuffer, MAX_CHANNELS> activeNotesPerChannel;
    int vibratoActiveChannel = -1;
    double vibratoStartBeat = 0.0;
    
    //==========================================================================
    // Helper: Calculate MIDI note from string/fret
    //==========================================================================
    int calculateMidiNote(int stringIndex, int fret, const GP5Track& track) const
    {
        if (stringIndex < 0 || stringIndex >= 12)
            return -1;
        if (fret < 0 || fret > 30)
            return -1;
        
        int midiNote = 0;
        int tuningSize = track.tuning.size();
        
        if (tuningSize > 0 && tuningSize <= 12 && stringIndex < tuningSize)
        {
            int tuningValue = track.tuning.getUnchecked(stringIndex);
            if (tuningValue >= 0 && tuningValue < 128)
                midiNote = tuningValue + fret;
            else
                return -1;
        }
        else
        {
            // Standard tuning fallback (E4, B3, G3, D3, A2, E2)
            static constexpr int standardTuning[] = { 64, 59, 55, 50, 45, 40 };
            if (stringIndex < 6)
                midiNote = standardTuning[stringIndex] + fret;
            else
                return -1;
        }
        
        return (midiNote >= 0 && midiNote < 128) ? midiNote : -1;
    }
    
    //==========================================================================
    // Helper: Calculate velocity with dynamics
    // DETERMINISTIC - no random (audio-thread-safe)
    //==========================================================================
    int calculateVelocity(const GP5Note& note, int volumeScale) const
    {
        int velocity = note.velocity > 0 ? note.velocity : 95;
        
        // Apply dynamics
        if (note.isGhost)
            velocity = 45;  // Ghost notes very quiet
        else if (note.hasHeavyAccent)
            velocity = 127;
        else if (note.hasAccent)
            velocity = 115;
        else if (note.hasHammerOn)
            velocity = juce::jmax(50, velocity - 15);
        
        // Scale with track volume
        velocity = (velocity * volumeScale) / 100;
        
        return juce::jlimit(1, 127, velocity);
    }
    
    //==========================================================================
    // Helper: Stop all notes on a channel
    //==========================================================================
    void stopAllNotesOnChannel(juce::MidiBuffer& midiBuffer, int midiChannel, int sampleOffset)
    {
        if (midiChannel < 1 || midiChannel > 16)
            return;
        
        int channelIdx = midiChannel - 1;
        auto& noteBuffer = activeNotesPerChannel[channelIdx];
        
        for (int i = 0; i < noteBuffer.count; ++i)
        {
            midiBuffer.addEvent(juce::MidiMessage::noteOff(midiChannel, noteBuffer.notes[i]), sampleOffset);
        }
        noteBuffer.clear();
    }
    
    //==========================================================================
    // Helper: Vibrato control
    //==========================================================================
    void startVibrato(int midiChannel, double currentBeat)
    {
        vibratoActiveChannel = midiChannel;
        vibratoStartBeat = currentBeat;
    }
    
    void stopVibrato(juce::MidiBuffer& midiBuffer, int midiChannel, int sampleOffset)
    {
        if (vibratoActiveChannel == midiChannel)
        {
            midiBuffer.addEvent(juce::MidiMessage::controllerEvent(midiChannel, 1, 0), sampleOffset);
            vibratoActiveChannel = -1;
        }
    }
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiExpressionEngine)
};
