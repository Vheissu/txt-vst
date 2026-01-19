#include "Filter.h"
#include <cmath>

namespace incant {

Filter::Filter() {
}

void Filter::prepare(double sampleRate, int samplesPerBlock) {
    sampleRate_ = sampleRate;
    blockSize_ = samplesPerBlock;
    reset();
}

void Filter::process(juce::AudioBuffer<float>& buffer) {
    const int numSamples = buffer.getNumSamples();
    const int numChannels = std::min(buffer.getNumChannels(), 2);

    // LFO rate: 0.1 to 10 Hz
    float lfoFreq = 0.1f + params_.lfoRate * 9.9f;
    float lfoIncrement = lfoFreq / static_cast<float>(sampleRate_);

    // Base cutoff frequency: 20Hz to 20kHz (exponential)
    float minFreq = 20.0f;
    float maxFreq = std::min(20000.0f, static_cast<float>(sampleRate_) * 0.45f);

    float baseCutoff = minFreq * std::pow(maxFreq / minFreq, params_.cutoff);

    // Resonance (Q): 0.5 to 20
    float resonance = 0.5f + params_.resonance * 19.5f;

    FilterType filterType = getFilterType();

    for (int sample = 0; sample < numSamples; ++sample) {
        // Calculate LFO modulation
        float lfo = std::sin(lfoPhase_ * 2.0f * juce::MathConstants<float>::pi);

        // Apply LFO depth to cutoff (in octaves, -2 to +2)
        float lfoOctaves = lfo * params_.lfoDepth * 2.0f;
        float modulatedCutoff = baseCutoff * std::pow(2.0f, lfoOctaves);

        // Clamp cutoff
        modulatedCutoff = juce::jlimit(minFreq, maxFreq, modulatedCutoff);

        // Calculate SVF coefficients for this cutoff
        // Using Andy Simper's SVF implementation
        float w = std::tan(juce::MathConstants<float>::pi * modulatedCutoff / static_cast<float>(sampleRate_));
        float g = w;
        float k = 1.0f / resonance;

        float a1 = 1.0f / (1.0f + g * (g + k));
        float a2 = g * a1;
        float a3 = g * a2;

        // Process each channel
        for (int ch = 0; ch < numChannels; ++ch) {
            SVFState& state = (ch == 0) ? stateL_ : stateR_;

            float input = buffer.getSample(ch, sample);

            // SVF tick
            float v3 = input - state.ic2eq;
            float v1 = a1 * state.ic1eq + a2 * v3;
            float v2 = state.ic2eq + a2 * state.ic1eq + a3 * v3;

            state.ic1eq = 2.0f * v1 - state.ic1eq;
            state.ic2eq = 2.0f * v2 - state.ic2eq;

            // Select output based on filter type
            float output;
            switch (filterType) {
                case FilterType::LowPass:
                    output = v2;
                    break;
                case FilterType::HighPass:
                    output = input - k * v1 - v2;
                    break;
                case FilterType::BandPass:
                    output = v1;
                    break;
                case FilterType::Notch:
                    output = input - k * v1;
                    break;
                default:
                    output = v2;
            }

            buffer.setSample(ch, sample, output);
        }

        // Advance LFO phase
        lfoPhase_ += lfoIncrement;
        if (lfoPhase_ >= 1.0f) lfoPhase_ -= 1.0f;
    }
}

Filter::FilterType Filter::getFilterType() const {
    if (params_.filterType < 0.25f) return FilterType::LowPass;
    if (params_.filterType < 0.5f) return FilterType::HighPass;
    if (params_.filterType < 0.75f) return FilterType::BandPass;
    return FilterType::Notch;
}

void Filter::reset() {
    stateL_ = SVFState{};
    stateR_ = SVFState{};
    lfoPhase_ = 0.0f;
}

void Filter::setParameter(int index, float value) {
    value = juce::jlimit(0.0f, 1.0f, value);

    switch (index) {
        case 0: params_.cutoff = value; break;
        case 1: params_.resonance = value; break;
        case 2: params_.lfoRate = value; break;
        case 3: params_.lfoDepth = value; break;
        case 4: params_.filterType = value; break;
    }
}

float Filter::getParameter(int index) const {
    switch (index) {
        case 0: return params_.cutoff;
        case 1: return params_.resonance;
        case 2: return params_.lfoRate;
        case 3: return params_.lfoDepth;
        case 4: return params_.filterType;
    }
    return 0.0f;
}

const char* Filter::getParameterName(int index) const {
    static const char* names[] = {"Cutoff", "Resonance", "LFO Rate", "LFO Depth", "Type"};
    if (index >= 0 && index < 5) return names[index];
    return "";
}

void Filter::setParams(const FilterParams& params) {
    params_ = params;
}

} // namespace incant
