#include "Tremolo.h"
#include <cmath>

namespace incant {

Tremolo::Tremolo() {
}

void Tremolo::prepare(double sampleRate, int samplesPerBlock) {
    sampleRate_ = sampleRate;
    blockSize_ = samplesPerBlock;
    reset();
}

void Tremolo::process(juce::AudioBuffer<float>& buffer) {
    const int numSamples = buffer.getNumSamples();
    const int numChannels = std::min(buffer.getNumChannels(), 2);

    // LFO rate: 1 to 20 Hz
    float lfoFreq = 1.0f + params_.rate * 19.0f;
    float lfoIncrement = lfoFreq / static_cast<float>(sampleRate_);

    float depth = params_.depth;
    float dryWet = params_.dryWet;

    // Stereo phase offset (0 = mono, 0.5 = opposite phase)
    float stereoOffset = params_.stereo * 0.5f;

    for (int sample = 0; sample < numSamples; ++sample) {
        // Get LFO values for each channel
        float lfoL = getLfoValue(lfoPhaseL_, params_.shape);
        float lfoR = getLfoValue(lfoPhaseR_, params_.shape);

        // Convert LFO (-1 to 1) to gain modulation
        // At depth=1, goes from 0 to 1. At depth=0, stays at 1.
        float gainL = 1.0f - depth * (0.5f - 0.5f * lfoL);
        float gainR = 1.0f - depth * (0.5f - 0.5f * lfoR);

        // Get input samples
        float inputL = buffer.getSample(0, sample);
        float inputR = numChannels > 1 ? buffer.getSample(1, sample) : inputL;

        // Apply tremolo
        float wetL = inputL * gainL;
        float wetR = inputR * gainR;

        // Mix dry/wet
        float outL = inputL * (1.0f - dryWet) + wetL * dryWet;
        float outR = inputR * (1.0f - dryWet) + wetR * dryWet;

        buffer.setSample(0, sample, outL);
        if (numChannels > 1)
            buffer.setSample(1, sample, outR);

        // Advance LFO phases
        lfoPhaseL_ += lfoIncrement;
        lfoPhaseR_ += lfoIncrement;
        if (lfoPhaseL_ >= 1.0f) lfoPhaseL_ -= 1.0f;
        if (lfoPhaseR_ >= 1.0f) lfoPhaseR_ -= 1.0f;
    }
}

float Tremolo::getLfoValue(float phase, float shape) {
    // shape: 0 = sine, 0.5 = triangle, 1 = square

    if (shape < 0.33f) {
        // Sine wave (crossfade from pure sine)
        float sine = std::sin(phase * 2.0f * juce::MathConstants<float>::pi);
        float triangle = 4.0f * std::abs(phase - 0.5f) - 1.0f;
        float blend = shape / 0.33f;
        return sine * (1.0f - blend) + triangle * blend;
    }
    else if (shape < 0.66f) {
        // Triangle wave (crossfade to square)
        float triangle = 4.0f * std::abs(phase - 0.5f) - 1.0f;
        float square = phase < 0.5f ? 1.0f : -1.0f;
        float blend = (shape - 0.33f) / 0.33f;
        return triangle * (1.0f - blend) + square * blend;
    }
    else {
        // Square wave
        return phase < 0.5f ? 1.0f : -1.0f;
    }
}

void Tremolo::reset() {
    lfoPhaseL_ = 0.0f;
    // Apply stereo offset
    lfoPhaseR_ = params_.stereo * 0.5f;
    if (lfoPhaseR_ >= 1.0f) lfoPhaseR_ -= 1.0f;
}

void Tremolo::setParameter(int index, float value) {
    value = juce::jlimit(0.0f, 1.0f, value);

    switch (index) {
        case 0: params_.rate = value; break;
        case 1: params_.depth = value; break;
        case 2: params_.shape = value; break;
        case 3:
            params_.stereo = value;
            // Update stereo phase offset
            lfoPhaseR_ = lfoPhaseL_ + value * 0.5f;
            if (lfoPhaseR_ >= 1.0f) lfoPhaseR_ -= 1.0f;
            break;
        case 4: params_.dryWet = value; break;
    }
}

float Tremolo::getParameter(int index) const {
    switch (index) {
        case 0: return params_.rate;
        case 1: return params_.depth;
        case 2: return params_.shape;
        case 3: return params_.stereo;
        case 4: return params_.dryWet;
    }
    return 0.0f;
}

const char* Tremolo::getParameterName(int index) const {
    static const char* names[] = {"Rate", "Depth", "Shape", "Stereo", "Dry/Wet"};
    if (index >= 0 && index < 5) return names[index];
    return "";
}

void Tremolo::setParams(const TremoloParams& params) {
    params_ = params;
    // Update stereo phase
    lfoPhaseR_ = lfoPhaseL_ + params_.stereo * 0.5f;
    if (lfoPhaseR_ >= 1.0f) lfoPhaseR_ -= 1.0f;
}

} // namespace incant
