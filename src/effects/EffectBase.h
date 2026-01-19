#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

namespace incant {

class EffectBase {
public:
    virtual ~EffectBase() = default;

    virtual void prepare(double sampleRate, int samplesPerBlock) = 0;
    virtual void process(juce::AudioBuffer<float>& buffer) = 0;
    virtual void reset() = 0;

    // Parameter update (0.0 - 1.0 normalized)
    virtual void setParameter(int index, float value) = 0;
    virtual float getParameter(int index) const = 0;
    virtual int getNumParameters() const = 0;
    virtual const char* getParameterName(int index) const = 0;

protected:
    double sampleRate_ = 44100.0;
    int blockSize_ = 512;
};

} // namespace incant
