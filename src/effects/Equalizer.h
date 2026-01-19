#pragma once

#include "EffectBase.h"
#include "../ParameterSchema.h"

namespace incant {

class Equalizer : public EffectBase {
public:
    Equalizer();
    ~Equalizer() override = default;

    void prepare(double sampleRate, int samplesPerBlock) override;
    void process(juce::AudioBuffer<float>& buffer) override;
    void reset() override;

    void setParameter(int index, float value) override;
    float getParameter(int index) const override;
    int getNumParameters() const override { return 5; }
    const char* getParameterName(int index) const override;

    void setParams(const EQParams& params);

private:
    void updateFilters();

    EQParams params_;

    // Biquad filters for each band (stereo)
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                   juce::dsp::IIR::Coefficients<float>> lowShelf_;
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                   juce::dsp::IIR::Coefficients<float>> midPeak_;
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                   juce::dsp::IIR::Coefficients<float>> highPeak_;
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                   juce::dsp::IIR::Coefficients<float>> airShelf_;

    static constexpr float LOW_FREQ = 100.0f;
    static constexpr float MID_FREQ = 1000.0f;
    static constexpr float HIGH_FREQ = 4000.0f;
    static constexpr float AIR_FREQ = 10000.0f;
    static constexpr float Q = 0.707f;
};

} // namespace incant
