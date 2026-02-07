//
// Created by Damien Ronssin on 05.03.23.
//

#ifndef Resampler_h
#define Resampler_h

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

using namespace juce;

class Resampler
{
public:
    Resampler() = default;

    ~Resampler() = default;

    void prepareToPlay(double inSourceSampleRate, int inMaxBlockSize, double inTargetSampleRate);

    void reset();

    int processBlock(const float* inBuffer, float* outBuffer, int inNumSamples);

    int getNumOutSamplesOnNextProcessBlock(int inNumSamples) const;

private:
    LagrangeInterpolator mInterpolator;

    AudioBuffer<float> mInternalBuffer;

    const int mInitPadding = static_cast<int>(LagrangeInterpolator::getBaseLatency());

    int mNumInputSamplesAvailable = mInitPadding;
    double mSpeedRatio;
    double mSourceSampleRate;
    double mTargetSampleRate;

    // Low pass filter
    std::vector<juce::dsp::IIR::Filter<float>> mLowpassFilters;
};

#endif // Resampler_h
