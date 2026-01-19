#pragma once

#include "EffectBase.h"
#include "../ParameterSchema.h"

namespace incant {

class Filter : public EffectBase {
public:
    Filter();
    ~Filter() override = default;

    void prepare(double sampleRate, int samplesPerBlock) override;
    void process(juce::AudioBuffer<float>& buffer) override;
    void reset() override;

    void setParameter(int index, float value) override;
    float getParameter(int index) const override;
    int getNumParameters() const override { return 5; }
    const char* getParameterName(int index) const override;

    void setParams(const FilterParams& params);

private:
    enum class FilterType {
        LowPass,
        HighPass,
        BandPass,
        Notch
    };

    void updateFilter(float cutoffHz);
    FilterType getFilterType() const;

    FilterParams params_;

    // State-variable filter implementation for smooth modulation
    // This allows us to change cutoff per-sample without artifacts
    struct SVFState {
        float ic1eq = 0.0f;
        float ic2eq = 0.0f;
    };

    SVFState stateL_, stateR_;

    // LFO phase
    float lfoPhase_ = 0.0f;

    // Cached filter coefficients
    float g_ = 0.0f;
    float k_ = 0.0f;
};

} // namespace incant
