#include "Phaser.h"
#include <cmath>

namespace incant {

Phaser::Phaser() {
}

void Phaser::prepare(double sampleRate, int samplesPerBlock) {
    sampleRate_ = sampleRate;
    blockSize_ = samplesPerBlock;
    reset();
}

void Phaser::process(juce::AudioBuffer<float>& buffer) {
    const int numSamples = buffer.getNumSamples();
    const int numChannels = std::min(buffer.getNumChannels(), 2);

    // LFO rate: 0.05 to 5 Hz
    float lfoFreq = 0.05f + params_.rate * 4.95f;
    float lfoIncrement = lfoFreq / static_cast<float>(sampleRate_);

    // Determine number of stages: 4, 6, 8, or 12
    int numStages;
    if (params_.stages < 0.25f) numStages = 4;
    else if (params_.stages < 0.5f) numStages = 6;
    else if (params_.stages < 0.75f) numStages = 8;
    else numStages = 12;

    // Feedback amount (limit to prevent instability)
    float feedback = params_.feedback * 0.85f;

    float dryWet = params_.dryWet;

    // Frequency range for phaser sweep
    float minFreq = 100.0f;
    float maxFreq = 4000.0f;

    for (int sample = 0; sample < numSamples; ++sample) {
        // Calculate LFO (sine wave)
        float lfo = std::sin(lfoPhase_ * 2.0f * juce::MathConstants<float>::pi);

        // Apply depth to LFO
        lfo *= params_.depth;

        // Calculate sweep frequency
        float sweepNorm = 0.5f + 0.5f * lfo;  // 0 to 1
        float sweepFreq = minFreq * std::pow(maxFreq / minFreq, sweepNorm);

        // Calculate all-pass coefficient from frequency
        float w0 = 2.0f * juce::MathConstants<float>::pi * sweepFreq / static_cast<float>(sampleRate_);
        float coefficient = (1.0f - std::tan(w0 * 0.5f)) / (1.0f + std::tan(w0 * 0.5f));

        // Get input samples
        float inputL = buffer.getSample(0, sample);
        float inputR = numChannels > 1 ? buffer.getSample(1, sample) : inputL;

        // Add feedback
        float wetL = inputL + feedbackL_ * feedback;
        float wetR = inputR + feedbackR_ * feedback;

        // Process through all-pass stages
        for (int stage = 0; stage < numStages; ++stage) {
            wetL = stages_[stage].process(wetL, 0, coefficient);
            wetR = stages_[stage].process(wetR, 1, coefficient);
        }

        // Store feedback (from output of all-pass chain)
        feedbackL_ = std::tanh(wetL);  // Soft limit feedback
        feedbackR_ = std::tanh(wetR);

        // Mix dry/wet
        float outL = inputL * (1.0f - dryWet) + wetL * dryWet;
        float outR = inputR * (1.0f - dryWet) + wetR * dryWet;

        buffer.setSample(0, sample, outL);
        if (numChannels > 1)
            buffer.setSample(1, sample, outR);

        // Advance LFO phase
        lfoPhase_ += lfoIncrement;
        if (lfoPhase_ >= 1.0f) lfoPhase_ -= 1.0f;
    }
}

void Phaser::reset() {
    for (auto& stage : stages_) {
        stage.reset();
    }
    lfoPhase_ = 0.0f;
    feedbackL_ = 0.0f;
    feedbackR_ = 0.0f;
}

void Phaser::setParameter(int index, float value) {
    value = juce::jlimit(0.0f, 1.0f, value);

    switch (index) {
        case 0: params_.rate = value; break;
        case 1: params_.depth = value; break;
        case 2: params_.feedback = value; break;
        case 3: params_.stages = value; break;
        case 4: params_.dryWet = value; break;
    }
}

float Phaser::getParameter(int index) const {
    switch (index) {
        case 0: return params_.rate;
        case 1: return params_.depth;
        case 2: return params_.feedback;
        case 3: return params_.stages;
        case 4: return params_.dryWet;
    }
    return 0.0f;
}

const char* Phaser::getParameterName(int index) const {
    static const char* names[] = {"Rate", "Depth", "Feedback", "Stages", "Dry/Wet"};
    if (index >= 0 && index < 5) return names[index];
    return "";
}

void Phaser::setParams(const PhaserParams& params) {
    params_ = params;
}

} // namespace incant
