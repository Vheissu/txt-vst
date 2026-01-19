#pragma once

#include "EffectBase.h"
#include "../ParameterSchema.h"

namespace incant {

class Compressor : public EffectBase {
public:
    Compressor();
    ~Compressor() override = default;

    void prepare(double sampleRate, int samplesPerBlock) override;
    void process(juce::AudioBuffer<float>& buffer) override;
    void reset() override;

    void setParameter(int index, float value) override;
    float getParameter(int index) const override;
    int getNumParameters() const override { return 5; }
    const char* getParameterName(int index) const override;

    void setParams(const CompressorParams& params);

private:
    void updateCompressor();

    CompressorParams params_;
    juce::dsp::Compressor<float> compressor_;
    juce::dsp::Gain<float> makeupGain_;
};

} // namespace incant
