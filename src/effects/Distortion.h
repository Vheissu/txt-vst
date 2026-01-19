#pragma once

#include "EffectBase.h"
#include "../ParameterSchema.h"

namespace incant {

class Distortion : public EffectBase {
public:
    enum class CurveType {
        SoftClip,   // tanh
        HardClip,   // hard clipping
        Tube,       // asymmetric tube-style
        Fuzz        // rectification/fuzz
    };

    Distortion();
    ~Distortion() override = default;

    void prepare(double sampleRate, int samplesPerBlock) override;
    void process(juce::AudioBuffer<float>& buffer) override;
    void reset() override;

    void setParameter(int index, float value) override;
    float getParameter(int index) const override;
    int getNumParameters() const override { return 4; }
    const char* getParameterName(int index) const override;

    void setParams(const DistortionParams& params);

private:
    float processSample(float sample) const;
    void updateFilter();

    DistortionParams params_;
    CurveType curveType_ = CurveType::SoftClip;

    // Tone filter (lowpass for dark, highpass bypass for bright)
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                   juce::dsp::IIR::Coefficients<float>> toneFilter_;

    // Oversampling to reduce aliasing (optional, simple version)
    float driveGain_ = 1.0f;
};

} // namespace incant
