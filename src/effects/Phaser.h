#pragma once

#include "EffectBase.h"
#include "../ParameterSchema.h"
#include <array>

namespace incant {

class Phaser : public EffectBase {
public:
    Phaser();
    ~Phaser() override = default;

    void prepare(double sampleRate, int samplesPerBlock) override;
    void process(juce::AudioBuffer<float>& buffer) override;
    void reset() override;

    void setParameter(int index, float value) override;
    float getParameter(int index) const override;
    int getNumParameters() const override { return 5; }
    const char* getParameterName(int index) const override;

    void setParams(const PhaserParams& params);

private:
    // Simple first-order all-pass filter
    struct AllPassStage {
        float z1[2] = {0.0f, 0.0f};  // State for stereo

        float process(float input, int channel, float coefficient) {
            float output = coefficient * input + z1[channel];
            z1[channel] = input - coefficient * output;
            return output;
        }

        void reset() {
            z1[0] = z1[1] = 0.0f;
        }
    };

    PhaserParams params_;

    // Up to 12 all-pass stages
    static constexpr int kMaxStages = 12;
    std::array<AllPassStage, kMaxStages> stages_;

    // LFO phase
    float lfoPhase_ = 0.0f;

    // Feedback state
    float feedbackL_ = 0.0f;
    float feedbackR_ = 0.0f;
};

} // namespace incant
