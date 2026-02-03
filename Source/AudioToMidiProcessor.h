/*
  ==============================================================================
    
    AudioToMidiProcessor.h
    
    Real-time monophonic audio to MIDI conversion using YIN pitch detection
    and simple onset detection.
    
    Based on the YIN algorithm: de Cheveigné, A., & Kawahara, H. (2002). 
    "YIN, a fundamental frequency estimator for speech and music."
    
  ==============================================================================
*/

#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>
#include <cmath>

/**
 * Simple monophonic Audio-to-MIDI processor using YIN pitch detection.
 * Designed for guitar/bass input - single notes, not chords.
 */
class AudioToMidiProcessor
{
public:
    AudioToMidiProcessor();
    ~AudioToMidiProcessor() = default;
    
    //==========================================================================
    // Setup
    //==========================================================================
    
    void prepare(double sampleRate, int blockSize);
    void reset();
    
    //==========================================================================
    // Processing
    //==========================================================================
    
    /**
     * Process an audio block and detect notes.
     * Call this from processBlock() with the input audio.
     * 
     * @param audioBuffer The input audio buffer (uses first channel)
     * @param midiMessages Output MIDI buffer to add detected notes to
     * @param startSample Sample offset within buffer
     * @param numSamples Number of samples to process
     */
    void processBlock(const juce::AudioBuffer<float>& audioBuffer,
                      juce::MidiBuffer& midiMessages,
                      int startSample = 0, int numSamples = -1);
    
    //==========================================================================
    // Parameters
    //==========================================================================
    
    /** Set minimum frequency to detect (default: 80 Hz for bass E) */
    void setMinFrequency(float freq) { minFrequency = freq; }
    
    /** Set maximum frequency to detect (default: 1000 Hz) */
    void setMaxFrequency(float freq) { maxFrequency = freq; }
    
    /** Set onset sensitivity threshold (0-1, default: 0.1) */
    void setOnsetThreshold(float threshold) { onsetThreshold = juce::jlimit(0.0f, 1.0f, threshold); }
    
    /** Set silence threshold in dB (default: -50 dB) */
    void setSilenceThreshold(float thresholdDb) { silenceThresholdDb = thresholdDb; }
    
    /** Set YIN threshold (0-1, lower = more accurate but might miss notes, default: 0.15) */
    void setYinThreshold(float threshold) { yinThreshold = juce::jlimit(0.01f, 0.5f, threshold); }
    
    /** Set MIDI output channel (1-16, default: 1) */
    void setMidiChannel(int channel) { midiChannel = juce::jlimit(1, 16, channel); }
    
    //==========================================================================
    // State
    //==========================================================================
    
    /** Returns true if a note is currently active */
    bool isNoteActive() const { return currentMidiNote >= 0; }
    
    /** Returns the current MIDI note number, or -1 if no note */
    int getCurrentNote() const { return currentMidiNote; }
    
    /** Returns the last detected frequency in Hz */
    float getLastFrequency() const { return lastFrequency; }
    
    /** Returns the last detected RMS level (0-1) */
    float getLastLevel() const { return lastRmsLevel; }

private:
    //==========================================================================
    // YIN Algorithm
    //==========================================================================
    
    /**
     * YIN pitch detection algorithm
     * @param samples Pointer to audio samples
     * @param numSamples Number of samples (should be >= 2 * maxPeriod)
     * @return Detected frequency in Hz, or 0 if no pitch detected
     */
    float detectPitchYin(const float* samples, int numSamples);
    
    /** Calculate RMS level of a buffer */
    float calculateRms(const float* samples, int numSamples);
    
    /** Convert frequency to MIDI note number */
    int frequencyToMidi(float frequency);
    
    /** Convert MIDI note to frequency */
    float midiToFrequency(int midiNote);
    
    //==========================================================================
    // Members
    //==========================================================================
    
    double sampleRate = 44100.0;
    int blockSize = 512;
    
    // Parameters
    float minFrequency = 80.0f;   // Low E on bass
    float maxFrequency = 1000.0f; // High enough for guitar
    float onsetThreshold = 0.1f;
    float silenceThresholdDb = -50.0f;
    float yinThreshold = 0.15f;
    int midiChannel = 1;
    
    // YIN working buffers
    std::vector<float> yinBuffer;
    std::vector<float> inputBuffer;
    int inputBufferWritePos = 0;
    int yinBufferSize = 2048;
    
    // State
    int currentMidiNote = -1;
    float lastFrequency = 0.0f;
    float lastRmsLevel = 0.0f;
    float previousRmsLevel = 0.0f;
    int samplesSinceNoteOn = 0;
    int minNoteDurationSamples = 0;  // Minimum note duration to avoid glitches
    
    // Note-off delay (to avoid retriggering on sustain)
    int noteOffDelaySamples = 0;
    int noteOffCounter = 0;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioToMidiProcessor)
};
