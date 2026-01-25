/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "GP5Parser.h"
#include <atomic>

//==============================================================================
/**
*/
class NewProjectAudioProcessor  : public juce::AudioProcessor
{
public:
    //==============================================================================
    NewProjectAudioProcessor();
    ~NewProjectAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    // GP5 Parser - persistent data
    GP5Parser& getGP5Parser() { return gp5Parser; }
    const GP5Parser& getGP5Parser() const { return gp5Parser; }
    
    juce::String getLoadedFilePath() const { return loadedFilePath; }
    void setLoadedFilePath(const juce::String& path) { loadedFilePath = path; }
    
    bool loadGP5File(const juce::File& file);
    bool isFileLoaded() const { return fileLoaded; }
    
    //==============================================================================
    // DAW Synchronisation - Thread-safe access from Editor
    bool isHostPlaying() const { return hostIsPlaying.load(); }
    double getHostTempo() const { return hostTempo.load(); }
    double getHostPositionInBeats() const { return hostPositionBeats.load(); }
    double getHostPositionInSeconds() const { return hostPositionSeconds.load(); }
    int getHostTimeSignatureNumerator() const { return hostTimeSigNumerator.load(); }
    int getHostTimeSignatureDenominator() const { return hostTimeSigDenominator.load(); }
    
    // Berechnet den aktuellen Takt (0-basiert) basierend auf GP5-Struktur
    int getCurrentMeasureIndex() const;
    
    // Position innerhalb des aktuellen Taktes (0.0 = Anfang, 1.0 = Ende)
    double getPositionInCurrentMeasure() const;

private:
    //==============================================================================
    GP5Parser gp5Parser;
    juce::String loadedFilePath;
    bool fileLoaded = false;
    
    // DAW sync state (atomic for thread-safe access from UI)
    std::atomic<bool> hostIsPlaying { false };
    std::atomic<double> hostTempo { 120.0 };
    std::atomic<double> hostPositionBeats { 0.0 };
    std::atomic<double> hostPositionSeconds { 0.0 };
    std::atomic<int> hostTimeSigNumerator { 4 };
    std::atomic<int> hostTimeSigDenominator { 4 };
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NewProjectAudioProcessor)
};
