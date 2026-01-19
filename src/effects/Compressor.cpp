#include "Compressor.h"

namespace incant {

Compressor::Compressor() = default;

void Compressor::prepare(double sampleRate, int samplesPerBlock) {
    sampleRate_ = sampleRate;
    blockSize_ = samplesPerBlock;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels = 2;

    compressor_.prepare(spec);
    makeupGain_.prepare(spec);

    updateCompressor();
}

void Compressor::process(juce::AudioBuffer<float>& buffer) {
    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::ProcessContextReplacing<float> context(block);

    compressor_.process(context);
    makeupGain_.process(context);
}

void Compressor::reset() {
    compressor_.reset();
    makeupGain_.reset();
}

void Compressor::setParameter(int index, float value) {
    value = juce::jlimit(0.0f, 1.0f, value);

    switch (index) {
        case 0: params_.threshold = value; break;
        case 1: params_.ratio = value; break;
        case 2: params_.attack = value; break;
        case 3: params_.release = value; break;
        case 4: params_.makeup = value; break;
    }

    updateCompressor();
}

float Compressor::getParameter(int index) const {
    switch (index) {
        case 0: return params_.threshold;
        case 1: return params_.ratio;
        case 2: return params_.attack;
        case 3: return params_.release;
        case 4: return params_.makeup;
    }
    return 0.0f;
}

const char* Compressor::getParameterName(int index) const {
    static const char* names[] = {"Threshold", "Ratio", "Attack", "Release", "Makeup"};
    if (index >= 0 && index < 5) return names[index];
    return "";
}

void Compressor::setParams(const CompressorParams& params) {
    params_ = params;
    updateCompressor();
}

void Compressor::updateCompressor() {
    // Convert normalized values to actual values
    // Threshold: 0-1 maps to -60dB to 0dB
    float thresholdDB = params_.threshold * 60.0f - 60.0f;

    // Ratio: 0-1 maps to 1:1 to 20:1
    float ratio = 1.0f + params_.ratio * 19.0f;

    // Attack: 0-1 maps to 0.1ms to 100ms
    float attackMs = 0.1f + params_.attack * 99.9f;

    // Release: 0-1 maps to 10ms to 1000ms
    float releaseMs = 10.0f + params_.release * 990.0f;

    // Makeup: 0-1 maps to 0dB to 24dB
    float makeupDB = params_.makeup * 24.0f;

    compressor_.setThreshold(thresholdDB);
    compressor_.setRatio(ratio);
    compressor_.setAttack(attackMs);
    compressor_.setRelease(releaseMs);
    makeupGain_.setGainDecibels(makeupDB);
}

} // namespace incant
