#pragma once

#include "EffectBase.h"
#include "../ParameterSchema.h"

namespace incant {

class Chorus : public EffectBase {
public:
    Chorus();
    ~Chorus() override = default;

    void prepare(double sampleRate, int samplesPerBlock) override;
    void process(juce::AudioBuffer<float>& buffer) override;
    void reset() override;

    void setParameter(int index, float value) override;
    float getParameter(int index) const override;
    int getNumParameters() const override { return 5; }
    const char* getParameterName(int index) const override;

    void setParams(const ChorusParams& params);

private:
    ChorusParams params_;

    // Delay buffer for chorus effect
    juce::AudioBuffer<float> delayBuffer_;
    int writePosition_ = 0;

    // LFO phase (separate for stereo spread)
    float lfoPhaseL_ = 0.0f;
    float lfoPhaseR_ = 0.25f;  // 90 degree offset for stereo
};

} // namespace incant
