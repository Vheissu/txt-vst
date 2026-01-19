#pragma once

#include "EffectBase.h"
#include "../ParameterSchema.h"

namespace incant {

class Delay : public EffectBase {
public:
    Delay();
    ~Delay() override = default;

    void prepare(double sampleRate, int samplesPerBlock) override;
    void process(juce::AudioBuffer<float>& buffer) override;
    void reset() override;

    void setParameter(int index, float value) override;
    float getParameter(int index) const override;
    int getNumParameters() const override { return 5; }
    const char* getParameterName(int index) const override;

    void setParams(const DelayParams& params);

private:
    void updateDelay();

    DelayParams params_;

    // Delay buffers for left and right channels
    juce::AudioBuffer<float> delayBuffer_;
    int writePosition_ = 0;
    int delaySamples_ = 0;

    // Feedback filter (lowpass for dark delays)
    juce::dsp::ProcessorDuplicator<
        juce::dsp::IIR::Filter<float>,
        juce::dsp::IIR::Coefficients<float>
    > feedbackFilter_;

    // Smoothed feedback to avoid clicks
    juce::SmoothedValue<float> smoothedFeedback_;
    juce::SmoothedValue<float> smoothedDryWet_;
};

} // namespace incant
