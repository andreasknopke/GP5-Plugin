/*
  ==============================================================================

    AudioTranscriber.cpp

    Polyphonic audio-to-MIDI transcription using NeuralNote's Basic Pitch model.

  ==============================================================================
*/

#include "AudioTranscriber.h"

AudioTranscriber::AudioTranscriber()
    : juce::Thread("BasicPitchTranscriber")
{
    mAccumulationBuffer.resize(kMaxAccumulationSamples, 0.0f);
}

AudioTranscriber::~AudioTranscriber()
{
    stopThread(5000);
}

//==============================================================================
// Setup
//==============================================================================

void AudioTranscriber::prepare(double sampleRate, int maxBlockSize)
{
    mHostSampleRate = sampleRate;

    // Prepare resampler: host rate -> 22050 Hz
    mResampler.prepareToPlay(sampleRate, maxBlockSize, BASIC_PITCH_SAMPLE_RATE);

    // Allocate temp buffer for resampled output per block
    // Worst case: maxBlockSize at host rate -> how many at 22050?
    int maxResampledSamples = static_cast<int>(
        std::ceil(static_cast<double>(maxBlockSize) * BASIC_PITCH_SAMPLE_RATE / sampleRate)) + 16;
    mResampleOutputBuffer.resize(static_cast<size_t>(maxResampledSamples), 0.0f);

    reset();
}

void AudioTranscriber::reset()
{
    // Stop any running transcription
    if (isThreadRunning())
    {
        signalThreadShouldExit();
        stopThread(5000);
    }

    mResampler.reset();
    mAccumulatedSamples.store(0);
    transcriptionInProgress.store(false);
    resultsAvailable.store(false);
    mTranscriptionRequested.store(false);

    {
        std::lock_guard<std::mutex> lock(mResultsMutex);
        mNoteEvents.clear();
    }

    mBasicPitch.reset();
}

//==============================================================================
// Audio Thread Interface
//==============================================================================

void AudioTranscriber::pushAudioBlock(const juce::AudioBuffer<float>& audioBuffer)
{
    if (audioBuffer.getNumChannels() == 0 || audioBuffer.getNumSamples() == 0)
        return;

    // Don't accumulate while transcribing
    if (transcriptionInProgress.load())
        return;

    const int numSamples = audioBuffer.getNumSamples();
    const float* inputData = audioBuffer.getReadPointer(0);

    // Resample from host rate to 22050 Hz
    int numResampled = mResampler.processBlock(inputData, mResampleOutputBuffer.data(), numSamples);

    // Append resampled data to accumulation buffer
    int currentPos = mAccumulatedSamples.load();
    int newPos = currentPos + numResampled;

    if (newPos > kMaxAccumulationSamples)
    {
        // Buffer full - stop accumulating
        numResampled = kMaxAccumulationSamples - currentPos;
        if (numResampled <= 0)
            return;
        newPos = kMaxAccumulationSamples;
    }

    std::memcpy(mAccumulationBuffer.data() + currentPos,
                mResampleOutputBuffer.data(),
                static_cast<size_t>(numResampled) * sizeof(float));

    mAccumulatedSamples.store(newPos);
}

void AudioTranscriber::pullMidiMessages(juce::MidiBuffer& /*midiMessages*/,
                                         int64_t /*currentSampleInRecording*/,
                                         int /*numSamples*/)
{
    // For now, MIDI output from transcription is handled separately
    // (e.g., the PluginProcessor reads getNoteEvents() and inserts into tab).
    // This method is a placeholder for future real-time MIDI streaming.
}

//==============================================================================
// Message Thread Interface
//==============================================================================

void AudioTranscriber::startTranscription()
{
    int numSamples = mAccumulatedSamples.load();
    if (numSamples < static_cast<int>(BASIC_PITCH_SAMPLE_RATE * 0.1)) // Need at least 100ms
    {
        DBG("AudioTranscriber: Not enough audio to transcribe (" + juce::String(numSamples) + " samples)");
        return;
    }

    if (transcriptionInProgress.load())
    {
        DBG("AudioTranscriber: Transcription already in progress");
        return;
    }

    // Copy accumulated audio for the background thread
    mTranscriptionInput.resize(static_cast<size_t>(numSamples));
    std::memcpy(mTranscriptionInput.data(), mAccumulationBuffer.data(),
                static_cast<size_t>(numSamples) * sizeof(float));
    mTranscriptionInputSize = numSamples;

    transcriptionInProgress.store(true);
    resultsAvailable.store(false);
    mTranscriptionRequested.store(true);

    // Start background thread
    startThread(juce::Thread::Priority::normal);
}

void AudioTranscriber::clearRecording()
{
    reset();
}

//==============================================================================
// Parameters
//==============================================================================

void AudioTranscriber::setNoteSensitivity(float val)
{
    mNoteSensitivity.store(juce::jlimit(0.05f, 0.95f, val));
}

void AudioTranscriber::setSplitSensitivity(float val)
{
    mSplitSensitivity.store(juce::jlimit(0.05f, 0.95f, val));
}

void AudioTranscriber::setMinNoteDurationMs(float ms)
{
    mMinNoteDurationMs.store(juce::jlimit(10.0f, 2000.0f, ms));
}

void AudioTranscriber::setMidiChannel(int channel)
{
    mMidiChannel.store(juce::jlimit(1, 16, channel));
}

//==============================================================================
// State
//==============================================================================

double AudioTranscriber::getRecordedDurationSeconds() const
{
    return static_cast<double>(mAccumulatedSamples.load()) / BASIC_PITCH_SAMPLE_RATE;
}

std::vector<Notes::Event> AudioTranscriber::getNoteEvents() const
{
    std::lock_guard<std::mutex> lock(mResultsMutex);
    return mNoteEvents;
}

//==============================================================================
// Static Helpers
//==============================================================================

void AudioTranscriber::convertToMidiMessages(const std::vector<Notes::Event>& events,
                                              juce::MidiBuffer& midiBuffer,
                                              double sampleRate,
                                              int midiChannel)
{
    midiChannel = juce::jlimit(1, 16, midiChannel);

    for (const auto& event : events)
    {
        int noteOnSample = static_cast<int>(event.startTime * sampleRate);
        int noteOffSample = static_cast<int>(event.endTime * sampleRate);

        // Clamp note to valid MIDI range
        int midiNote = juce::jlimit(0, 127, event.pitch);

        // Velocity from amplitude (0.0 - 1.0 -> 1 - 127)
        int velocity = juce::jlimit(1, 127, static_cast<int>(event.amplitude * 127.0));

        midiBuffer.addEvent(
            juce::MidiMessage::noteOn(midiChannel, midiNote, static_cast<juce::uint8>(velocity)),
            noteOnSample);

        midiBuffer.addEvent(
            juce::MidiMessage::noteOff(midiChannel, midiNote),
            noteOffSample);
    }
}

//==============================================================================
// Background Thread
//==============================================================================

void AudioTranscriber::run()
{
    if (!mTranscriptionRequested.load())
        return;

    mTranscriptionRequested.store(false);

    DBG("AudioTranscriber: Starting transcription of "
        + juce::String(mTranscriptionInputSize) + " samples ("
        + juce::String(static_cast<double>(mTranscriptionInputSize) / BASIC_PITCH_SAMPLE_RATE, 1) + "s)");

    auto startTime = juce::Time::getMillisecondCounterHiRes();

    // Configure Basic Pitch parameters
    mBasicPitch.setParameters(
        mNoteSensitivity.load(),
        mSplitSensitivity.load(),
        mMinNoteDurationMs.load()
    );

    // Reset and run transcription
    mBasicPitch.reset();

    if (threadShouldExit())
    {
        transcriptionInProgress.store(false);
        return;
    }

    mBasicPitch.transcribeToMIDI(mTranscriptionInput.data(), mTranscriptionInputSize);

    if (threadShouldExit())
    {
        transcriptionInProgress.store(false);
        return;
    }

    // Copy results
    {
        std::lock_guard<std::mutex> lock(mResultsMutex);
        mNoteEvents = mBasicPitch.getNoteEvents();
    }

    auto elapsed = juce::Time::getMillisecondCounterHiRes() - startTime;

    DBG("AudioTranscriber: Transcription complete - "
        + juce::String(static_cast<int>(mNoteEvents.size())) + " notes detected in "
        + juce::String(elapsed / 1000.0, 2) + "s");

    resultsAvailable.store(true);
    transcriptionInProgress.store(false);
}
