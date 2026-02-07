/*
  ==============================================================================

    AudioTranscriber.h

    Polyphonic audio-to-MIDI transcription using NeuralNote's Basic Pitch model.
    Collects sidechain audio, resamples to 22050 Hz, runs Basic Pitch in a
    background thread, and provides detected notes as MIDI messages.

    NOT real-time: CQT + CNN + note extraction require batch processing.
    Audio is accumulated during recording, then transcribed when triggered.

  ==============================================================================
*/

#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include <juce_dsp/juce_dsp.h>

#include "BasicPitch.h"
#include "BasicPitchConstants.h"
#include "Resampler.h"
#include "Notes.h"

#include <atomic>
#include <mutex>
#include <vector>

/**
 * Polyphonic audio-to-MIDI transcriber using Basic Pitch (NeuralNote).
 *
 * Usage:
 *   1. Call prepare() with the host sample rate
 *   2. Call pushAudioBlock() from processBlock to feed sidechain audio
 *   3. Call startTranscription() when recording stops or manually
 *   4. Poll isTranscribing() / hasResults() to check status
 *   5. Call getNoteEvents() to get detected notes
 *   6. Use convertToMidiMessages() to create MIDI from events
 */
class AudioTranscriber : public juce::Thread
{
public:
    AudioTranscriber();
    ~AudioTranscriber() override;

    //==========================================================================
    // Setup
    //==========================================================================

    /** Prepare with host sample rate and block size. */
    void prepare(double sampleRate, int maxBlockSize);

    /** Reset all state, clear accumulated audio. */
    void reset();

    //==========================================================================
    // Audio Thread Interface
    //==========================================================================

    /**
     * Push audio samples from the sidechain input.
     * Called from the audio thread (processBlock).
     * Only the first channel is used (mono).
     *
     * @param audioBuffer The sidechain audio buffer
     */
    void pushAudioBlock(const juce::AudioBuffer<float>& audioBuffer);

    /**
     * Get any pending MIDI messages from the latest transcription.
     * Called from the audio thread. Returns MIDI note-on/off messages
     * timed relative to the start of the accumulated recording.
     *
     * @param midiMessages Output MIDI buffer to add messages to
     * @param currentSampleInRecording Current sample position in the recording
     * @param numSamples Number of samples in this block
     */
    void pullMidiMessages(juce::MidiBuffer& midiMessages,
                          int64_t currentSampleInRecording,
                          int numSamples);

    //==========================================================================
    // Message Thread Interface
    //==========================================================================

    /** Start transcription of accumulated audio in background thread. */
    void startTranscription();

    /** Clear all accumulated audio and results. */
    void clearRecording();

    //==========================================================================
    // Parameters
    //==========================================================================

    /** Note sensitivity (0.05 to 0.95). Higher = more notes detected. */
    void setNoteSensitivity(float val);

    /** Split sensitivity (0.05 to 0.95). Higher = more note splits. */
    void setSplitSensitivity(float val);

    /** Minimum note duration in milliseconds. */
    void setMinNoteDurationMs(float ms);

    /** MIDI channel for output (1-16). */
    void setMidiChannel(int channel);

    //==========================================================================
    // State Queries
    //==========================================================================

    /** True while transcription is running in background. */
    bool isTranscribing() const { return transcriptionInProgress.load(); }

    /** True when transcription results are available. */
    bool hasResults() const { return resultsAvailable.load(); }

    /** Acknowledge results have been consumed. Resets hasResults() to false. */
    void clearResults() { resultsAvailable.store(false); }

    /** Returns duration of accumulated audio in seconds. */
    double getRecordedDurationSeconds() const;

    /** Returns the latest transcription note events. Thread-safe. */
    std::vector<Notes::Event> getNoteEvents() const;

    //==========================================================================
    // Static Helpers
    //==========================================================================

    /**
     * Convert Basic Pitch note events to JUCE MIDI messages.
     *
     * @param events The note events from Basic Pitch
     * @param midiBuffer Output MIDI buffer
     * @param sampleRate The target sample rate for message timestamps
     * @param midiChannel MIDI channel (1-16)
     */
    static void convertToMidiMessages(const std::vector<Notes::Event>& events,
                                      juce::MidiBuffer& midiBuffer,
                                      double sampleRate,
                                      int midiChannel = 1);

private:
    //==========================================================================
    // Thread
    //==========================================================================
    void run() override;

    //==========================================================================
    // Members
    //==========================================================================

    // Audio accumulation (written from audio thread, read from bg thread)
    std::vector<float> mAccumulationBuffer;  // Resampled to 22050 Hz
    std::atomic<int> mAccumulatedSamples{0};
    static constexpr int kMaxRecordingSeconds = 300; // 5 minutes max
    static constexpr int kMaxAccumulationSamples = BASIC_PITCH_SAMPLE_RATE * kMaxRecordingSeconds;

    // Resampler (host sample rate -> 22050 Hz)
    Resampler mResampler;
    std::vector<float> mResampleOutputBuffer; // temp buffer per processBlock

    // Basic Pitch pipeline
    BasicPitch mBasicPitch;

    // Transcription input (copy made when transcription starts)
    std::vector<float> mTranscriptionInput;
    int mTranscriptionInputSize = 0;

    // Results
    std::vector<Notes::Event> mNoteEvents;
    mutable std::mutex mResultsMutex;

    // Parameters (atomic for thread safety)
    std::atomic<float> mNoteSensitivity{0.7f};
    std::atomic<float> mSplitSensitivity{0.5f};
    std::atomic<float> mMinNoteDurationMs{100.0f};
    std::atomic<int> mMidiChannel{1};

    // State
    std::atomic<bool> mTranscriptionRequested{false};
    std::atomic<bool> transcriptionInProgress{false};
    std::atomic<bool> resultsAvailable{false};

    double mHostSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioTranscriber)
};
