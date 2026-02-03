/*
  ==============================================================================
    
    AudioToMidiProcessor.cpp
    
    Real-time monophonic audio to MIDI conversion using YIN pitch detection.
    
  ==============================================================================
*/

#include "AudioToMidiProcessor.h"

AudioToMidiProcessor::AudioToMidiProcessor()
{
}

void AudioToMidiProcessor::prepare(double newSampleRate, int newBlockSize)
{
    sampleRate = newSampleRate;
    blockSize = newBlockSize;
    
    // YIN buffer size: needs to be at least 2x the maximum period
    // For 80 Hz at 44100 Hz: period = 44100/80 = 551 samples
    // So we need at least 1102 samples. Use 2048 for safety.
    int maxPeriod = static_cast<int>(sampleRate / minFrequency);
    yinBufferSize = maxPeriod * 2;
    
    // Round up to power of 2 for efficiency
    int power = 1;
    while (power < yinBufferSize) power *= 2;
    yinBufferSize = power;
    
    yinBuffer.resize(yinBufferSize / 2);
    inputBuffer.resize(yinBufferSize);
    
    // Minimum note duration: ~20ms to avoid glitches
    minNoteDurationSamples = static_cast<int>(sampleRate * 0.02);
    
    // Note-off delay: ~50ms
    noteOffDelaySamples = static_cast<int>(sampleRate * 0.05);
    
    reset();
}

void AudioToMidiProcessor::reset()
{
    std::fill(inputBuffer.begin(), inputBuffer.end(), 0.0f);
    inputBufferWritePos = 0;
    currentMidiNote = -1;
    lastFrequency = 0.0f;
    lastRmsLevel = 0.0f;
    previousRmsLevel = 0.0f;
    samplesSinceNoteOn = 0;
    noteOffCounter = 0;
}

void AudioToMidiProcessor::processBlock(const juce::AudioBuffer<float>& audioBuffer,
                                         juce::MidiBuffer& midiMessages,
                                         int startSample, int numSamples)
{
    if (numSamples < 0)
        numSamples = audioBuffer.getNumSamples();
    
    if (numSamples == 0 || audioBuffer.getNumChannels() == 0)
        return;
    
    const float* inputSamples = audioBuffer.getReadPointer(0);
    
    // Add new samples to input buffer
    for (int i = startSample; i < startSample + numSamples; ++i)
    {
        inputBuffer[inputBufferWritePos] = inputSamples[i];
        inputBufferWritePos = (inputBufferWritePos + 1) % yinBufferSize;
    }
    
    // Calculate RMS level
    float rmsLevel = calculateRms(inputSamples + startSample, numSamples);
    lastRmsLevel = rmsLevel;
    
    // Convert to dB
    float levelDb = (rmsLevel > 0.0f) ? 20.0f * std::log10(rmsLevel) : -100.0f;
    
    // Check if we're above silence threshold
    bool isAboveSilence = levelDb > silenceThresholdDb;
    
    // Simple onset detection: significant increase in level
    bool isOnset = isAboveSilence && 
                   (rmsLevel > previousRmsLevel * (1.0f + onsetThreshold));
    
    // Detect pitch using YIN
    float frequency = 0.0f;
    int detectedMidiNote = -1;
    
    if (isAboveSilence)
    {
        // Build contiguous buffer for YIN (unwrap circular buffer)
        std::vector<float> analysisBuffer(yinBufferSize);
        for (int i = 0; i < yinBufferSize; ++i)
        {
            int idx = (inputBufferWritePos + i) % yinBufferSize;
            analysisBuffer[i] = inputBuffer[idx];
        }
        
        frequency = detectPitchYin(analysisBuffer.data(), yinBufferSize);
        lastFrequency = frequency;
        
        if (frequency > 0.0f)
        {
            detectedMidiNote = frequencyToMidi(frequency);
        }
    }
    
    // =========================================================================
    // State machine for MIDI note generation
    // =========================================================================
    
    // Track note duration
    if (currentMidiNote >= 0)
        samplesSinceNoteOn += numSamples;
    
    // Case 1: New note detected and different from current
    if (detectedMidiNote >= 0)
    {
        noteOffCounter = 0;  // Reset note-off delay
        
        if (currentMidiNote < 0)
        {
            // No current note - send note on
            midiMessages.addEvent(juce::MidiMessage::noteOn(midiChannel, detectedMidiNote, 
                                  static_cast<juce::uint8>(juce::jlimit(1, 127, 
                                  static_cast<int>(rmsLevel * 127.0f * 2.0f)))), 
                                  startSample);
            currentMidiNote = detectedMidiNote;
            samplesSinceNoteOn = 0;
            DBG("Audio->MIDI: Note ON " << currentMidiNote << " (freq=" << frequency << " Hz)");
        }
        else if (detectedMidiNote != currentMidiNote && samplesSinceNoteOn > minNoteDurationSamples)
        {
            // Different note - send note off for old, note on for new
            midiMessages.addEvent(juce::MidiMessage::noteOff(midiChannel, currentMidiNote), 
                                  startSample);
            midiMessages.addEvent(juce::MidiMessage::noteOn(midiChannel, detectedMidiNote,
                                  static_cast<juce::uint8>(juce::jlimit(1, 127,
                                  static_cast<int>(rmsLevel * 127.0f * 2.0f)))),
                                  startSample);
            DBG("Audio->MIDI: Note change " << currentMidiNote << " -> " << detectedMidiNote);
            currentMidiNote = detectedMidiNote;
            samplesSinceNoteOn = 0;
        }
        // else: same note, keep it playing
    }
    // Case 2: No pitch detected (silence or noise)
    else if (currentMidiNote >= 0)
    {
        noteOffCounter += numSamples;
        
        // Wait for note-off delay before sending note off
        if (noteOffCounter > noteOffDelaySamples)
        {
            midiMessages.addEvent(juce::MidiMessage::noteOff(midiChannel, currentMidiNote),
                                  startSample);
            DBG("Audio->MIDI: Note OFF " << currentMidiNote);
            currentMidiNote = -1;
            samplesSinceNoteOn = 0;
        }
    }
    
    previousRmsLevel = rmsLevel;
}

float AudioToMidiProcessor::detectPitchYin(const float* samples, int numSamples)
{
    // YIN algorithm implementation
    // Based on: de Cheveigné & Kawahara (2002)
    
    int tauMax = numSamples / 2;
    int tauMin = static_cast<int>(sampleRate / maxFrequency);
    tauMax = juce::jmin(tauMax, static_cast<int>(sampleRate / minFrequency));
    
    if (tauMax <= tauMin || tauMax > static_cast<int>(yinBuffer.size()))
        return 0.0f;
    
    // Step 1 & 2: Difference function and cumulative mean normalized difference
    yinBuffer[0] = 1.0f;
    
    float runningSum = 0.0f;
    
    for (int tau = 1; tau < tauMax; ++tau)
    {
        float delta = 0.0f;
        
        // Difference function
        for (int i = 0; i < tauMax; ++i)
        {
            float diff = samples[i] - samples[i + tau];
            delta += diff * diff;
        }
        
        runningSum += delta;
        
        // Cumulative mean normalized difference function
        yinBuffer[tau] = (runningSum > 0.0f) ? delta * tau / runningSum : 1.0f;
    }
    
    // Step 3: Absolute threshold
    int tauEstimate = -1;
    
    for (int tau = tauMin; tau < tauMax; ++tau)
    {
        if (yinBuffer[tau] < yinThreshold)
        {
            // Find the minimum in this valley
            while (tau + 1 < tauMax && yinBuffer[tau + 1] < yinBuffer[tau])
            {
                ++tau;
            }
            tauEstimate = tau;
            break;
        }
    }
    
    // No pitch found
    if (tauEstimate < 0)
        return 0.0f;
    
    // Step 4: Parabolic interpolation for better accuracy
    float betterTau = static_cast<float>(tauEstimate);
    
    if (tauEstimate > 0 && tauEstimate < tauMax - 1)
    {
        float s0 = yinBuffer[tauEstimate - 1];
        float s1 = yinBuffer[tauEstimate];
        float s2 = yinBuffer[tauEstimate + 1];
        
        float adjustment = (s2 - s0) / (2.0f * (2.0f * s1 - s0 - s2));
        
        if (std::abs(adjustment) < 1.0f)
            betterTau += adjustment;
    }
    
    // Convert period to frequency
    float frequency = static_cast<float>(sampleRate) / betterTau;
    
    // Sanity check
    if (frequency < minFrequency || frequency > maxFrequency)
        return 0.0f;
    
    return frequency;
}

float AudioToMidiProcessor::calculateRms(const float* samples, int numSamples)
{
    if (numSamples <= 0)
        return 0.0f;
    
    float sum = 0.0f;
    for (int i = 0; i < numSamples; ++i)
    {
        sum += samples[i] * samples[i];
    }
    
    return std::sqrt(sum / static_cast<float>(numSamples));
}

int AudioToMidiProcessor::frequencyToMidi(float frequency)
{
    if (frequency <= 0.0f)
        return -1;
    
    // MIDI note = 69 + 12 * log2(freq / 440)
    float midiNote = 69.0f + 12.0f * std::log2(frequency / 440.0f);
    
    // Round to nearest integer
    int note = static_cast<int>(std::round(midiNote));
    
    // Clamp to valid MIDI range
    return juce::jlimit(0, 127, note);
}

float AudioToMidiProcessor::midiToFrequency(int midiNote)
{
    return 440.0f * std::pow(2.0f, (midiNote - 69.0f) / 12.0f);
}
