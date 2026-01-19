#pragma once

#include "EffectBase.h"
#include "../ParameterSchema.h"

namespace incant {

class Tremolo : public EffectBase {
public:
    Tremolo();
    ~Tremolo() override = default;

    void prepare(double sampleRate, int samplesPerBlock) override;
    void process(juce::AudioBuffer<float>& buffer) override;
    void reset() override;

    void setParameter(int index, float value) override;
    float getParameter(int index) const override;
    int getNumParameters() const override { return 5; }
    const char* getParameterName(int index) const override;

    void setParams(const TremoloParams& params);

private:
    float getLfoValue(float phase, float shape);

    TremoloParams params_;

    // LFO phase (stereo phases for pan tremolo)
    float lfoPhaseL_ = 0.0f;
    float lfoPhaseR_ = 0.0f;
};

} // namespace incant
