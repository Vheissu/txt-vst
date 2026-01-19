#include "Chorus.h"
#include <cmath>

namespace incant {

Chorus::Chorus() {
}

void Chorus::prepare(double sampleRate, int samplesPerBlock) {
    sampleRate_ = sampleRate;
    blockSize_ = samplesPerBlock;

    // Max delay: 50ms (plenty for chorus)
    int maxDelaySamples = static_cast<int>(sampleRate * 0.05);
    delayBuffer_.setSize(2, maxDelaySamples);
    delayBuffer_.clear();
    writePosition_ = 0;

    lfoPhaseL_ = 0.0f;
    lfoPhaseR_ = 0.25f;
}

void Chorus::process(juce::AudioBuffer<float>& buffer) {
    const int numSamples = buffer.getNumSamples();
    const int numChannels = std::min(buffer.getNumChannels(), 2);
    const int delayBufferSize = delayBuffer_.getNumSamples();

    if (delayBufferSize == 0)
        return;

    // LFO rate: 0.1 to 5 Hz
    float lfoFreq = 0.1f + params_.rate * 4.9f;
    float lfoIncrement = lfoFreq / static_cast<float>(sampleRate_);

    // Base delay: 5ms to 30ms
    float baseDelayMs = 5.0f + params_.delay * 25.0f;
    float baseDelaySamples = baseDelayMs * static_cast<float>(sampleRate_) / 1000.0f;

    // Modulation depth in samples (0.5ms to 5ms)
    float modDepthMs = 0.5f + params_.depth * 4.5f;
    float modDepthSamples = modDepthMs * static_cast<float>(sampleRate_) / 1000.0f;

    float feedback = params_.feedback * 0.7f;  // Limit feedback
    float dryWet = params_.dryWet;

    for (int sample = 0; sample < numSamples; ++sample) {
        // Calculate LFO values (sine wave)
        float lfoL = std::sin(lfoPhaseL_ * 2.0f * juce::MathConstants<float>::pi);
        float lfoR = std::sin(lfoPhaseR_ * 2.0f * juce::MathConstants<float>::pi);

        // Calculate delay times with modulation
        float delayL = baseDelaySamples + lfoL * modDepthSamples;
        float delayR = baseDelaySamples + lfoR * modDepthSamples;

        // Clamp delay times
        delayL = juce::jlimit(1.0f, static_cast<float>(delayBufferSize - 1), delayL);
        delayR = juce::jlimit(1.0f, static_cast<float>(delayBufferSize - 1), delayR);

        // Linear interpolation for smooth modulation
        auto readInterpolated = [&](int channel, float delaySamples) {
            float readPosFloat = static_cast<float>(writePosition_) - delaySamples;
            if (readPosFloat < 0) readPosFloat += delayBufferSize;

            int readPos0 = static_cast<int>(readPosFloat);
            int readPos1 = (readPos0 + 1) % delayBufferSize;
            float frac = readPosFloat - std::floor(readPosFloat);

            float s0 = delayBuffer_.getSample(channel, readPos0);
            float s1 = delayBuffer_.getSample(channel, readPos1);

            return s0 + frac * (s1 - s0);
        };

        // Get input samples
        float inputL = buffer.getSample(0, sample);
        float inputR = numChannels > 1 ? buffer.getSample(1, sample) : inputL;

        // Read delayed samples
        float delayedL = readInterpolated(0, delayL);
        float delayedR = readInterpolated(1, delayR);

        // Write to delay buffer with feedback
        delayBuffer_.setSample(0, writePosition_, inputL + delayedL * feedback);
        delayBuffer_.setSample(1, writePosition_, inputR + delayedR * feedback);

        // Mix dry/wet
        float outL = inputL * (1.0f - dryWet) + delayedL * dryWet;
        float outR = inputR * (1.0f - dryWet) + delayedR * dryWet;

        buffer.setSample(0, sample, outL);
        if (numChannels > 1)
            buffer.setSample(1, sample, outR);

        // Advance write position
        writePosition_ = (writePosition_ + 1) % delayBufferSize;

        // Advance LFO phases
        lfoPhaseL_ += lfoIncrement;
        lfoPhaseR_ += lfoIncrement;
        if (lfoPhaseL_ >= 1.0f) lfoPhaseL_ -= 1.0f;
        if (lfoPhaseR_ >= 1.0f) lfoPhaseR_ -= 1.0f;
    }
}

void Chorus::reset() {
    delayBuffer_.clear();
    writePosition_ = 0;
    lfoPhaseL_ = 0.0f;
    lfoPhaseR_ = 0.25f;
}

void Chorus::setParameter(int index, float value) {
    value = juce::jlimit(0.0f, 1.0f, value);

    switch (index) {
        case 0: params_.rate = value; break;
        case 1: params_.depth = value; break;
        case 2: params_.delay = value; break;
        case 3: params_.feedback = value; break;
        case 4: params_.dryWet = value; break;
    }
}

float Chorus::getParameter(int index) const {
    switch (index) {
        case 0: return params_.rate;
        case 1: return params_.depth;
        case 2: return params_.delay;
        case 3: return params_.feedback;
        case 4: return params_.dryWet;
    }
    return 0.0f;
}

const char* Chorus::getParameterName(int index) const {
    static const char* names[] = {"Rate", "Depth", "Delay", "Feedback", "Dry/Wet"};
    if (index >= 0 && index < 5) return names[index];
    return "";
}

void Chorus::setParams(const ChorusParams& params) {
    params_ = params;
}

} // namespace incant
