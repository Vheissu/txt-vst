#pragma once

#include "EffectBase.h"
#include "../ParameterSchema.h"

namespace incant {

class Overdrive : public EffectBase {
public:
    Overdrive();
    ~Overdrive() override = default;

    void prepare(double sampleRate, int samplesPerBlock) override;
    void process(juce::AudioBuffer<float>& buffer) override;
    void reset() override;

    void setParameter(int index, float value) override;
    float getParameter(int index) const override;
    int getNumParameters() const override { return 5; }
    const char* getParameterName(int index) const override;

    void setParams(const OverdriveParams& params);

private:
    void updateFilters();
    float softClip(float sample);

    OverdriveParams params_;

    // Input high-pass for tightness (TS-style bass cut)
    juce::dsp::ProcessorDuplicator<
        juce::dsp::IIR::Filter<float>,
        juce::dsp::IIR::Coefficients<float>
    > inputHighPass_;

    // Mid-boost EQ (the signature TS "hump")
    juce::dsp::ProcessorDuplicator<
        juce::dsp::IIR::Filter<float>,
        juce::dsp::IIR::Coefficients<float>
    > midBoost_;

    // Tone control (lowpass)
    juce::dsp::ProcessorDuplicator<
        juce::dsp::IIR::Filter<float>,
        juce::dsp::IIR::Coefficients<float>
    > toneFilter_;
};

} // namespace incant
