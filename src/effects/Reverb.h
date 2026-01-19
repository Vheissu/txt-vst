#pragma once

#include "EffectBase.h"
#include "../ParameterSchema.h"

namespace incant {

class Reverb : public EffectBase {
public:
    Reverb();
    ~Reverb() override = default;

    void prepare(double sampleRate, int samplesPerBlock) override;
    void process(juce::AudioBuffer<float>& buffer) override;
    void reset() override;

    void setParameter(int index, float value) override;
    float getParameter(int index) const override;
    int getNumParameters() const override { return 5; }
    const char* getParameterName(int index) const override;

    void setParams(const ReverbParams& params);

private:
    void updateReverb();

    ReverbParams params_;
    juce::dsp::Reverb reverb_;
    juce::dsp::Reverb::Parameters reverbParams_;

    // Pre-delay buffer
    juce::AudioBuffer<float> predelayBuffer_;
    int predelayWritePos_ = 0;
    int predelaySamples_ = 0;
};

} // namespace incant
