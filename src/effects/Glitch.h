#pragma once

#include "EffectBase.h"
#include "../ParameterSchema.h"
#include <random>

namespace incant {

class Glitch : public EffectBase {
public:
    Glitch();
    ~Glitch() override = default;

    void prepare(double sampleRate, int samplesPerBlock) override;
    void process(juce::AudioBuffer<float>& buffer) override;
    void reset() override;

    void setParameter(int index, float value) override;
    float getParameter(int index) const override;
    int getNumParameters() const override { return 5; }
    const char* getParameterName(int index) const override;

    void setParams(const GlitchParams& params);

private:
    void triggerGlitch();
    float bitCrush(float sample, float amount);

    GlitchParams params_;

    // Capture buffer for stutter/reverse
    juce::AudioBuffer<float> captureBuffer_;
    int captureLength_ = 0;
    int capturePosition_ = 0;

    // Playback state
    bool isGlitching_ = false;
    bool isReversed_ = false;
    int glitchPlaybackPos_ = 0;
    int glitchRepeatCount_ = 0;
    int currentRepeat_ = 0;

    // Timing
    int samplesUntilNextGlitch_ = 0;
    int glitchDuration_ = 0;

    // Random generator
    std::mt19937 rng_;
    std::uniform_real_distribution<float> dist_{0.0f, 1.0f};

    // Dry buffer for mixing
    juce::AudioBuffer<float> dryBuffer_;
};

} // namespace incant
