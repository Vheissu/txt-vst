#include "Equalizer.h"

namespace incant {

Equalizer::Equalizer() = default;

void Equalizer::prepare(double sampleRate, int samplesPerBlock) {
    sampleRate_ = sampleRate;
    blockSize_ = samplesPerBlock;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels = 2;

    lowShelf_.prepare(spec);
    midPeak_.prepare(spec);
    highPeak_.prepare(spec);
    airShelf_.prepare(spec);

    updateFilters();
}

void Equalizer::process(juce::AudioBuffer<float>& buffer) {
    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::ProcessContextReplacing<float> context(block);

    lowShelf_.process(context);
    midPeak_.process(context);
    highPeak_.process(context);
    airShelf_.process(context);

    // Apply dry/wet mix
    if (params_.dryWet < 1.0f) {
        // This is simplified - a proper implementation would keep a dry copy
        // For now, dryWet just scales the processed signal
        buffer.applyGain(params_.dryWet);
    }
}

void Equalizer::reset() {
    lowShelf_.reset();
    midPeak_.reset();
    highPeak_.reset();
    airShelf_.reset();
}

void Equalizer::setParameter(int index, float value) {
    value = juce::jlimit(0.0f, 1.0f, value);

    switch (index) {
        case 0: params_.lowGain = value; break;
        case 1: params_.midGain = value; break;
        case 2: params_.highGain = value; break;
        case 3: params_.airGain = value; break;
        case 4: params_.dryWet = value; break;
    }

    updateFilters();
}

float Equalizer::getParameter(int index) const {
    switch (index) {
        case 0: return params_.lowGain;
        case 1: return params_.midGain;
        case 2: return params_.highGain;
        case 3: return params_.airGain;
        case 4: return params_.dryWet;
    }
    return 0.0f;
}

const char* Equalizer::getParameterName(int index) const {
    static const char* names[] = {"Low", "Mid", "High", "Air", "Dry/Wet"};
    if (index >= 0 && index < 5) return names[index];
    return "";
}

void Equalizer::setParams(const EQParams& params) {
    params_ = params;
    updateFilters();
}

void Equalizer::updateFilters() {
    // Convert 0-1 to dB (-12 to +12)
    auto gainToDB = [](float normalized) {
        return (normalized - 0.5f) * 24.0f;  // -12 to +12 dB
    };

    float lowDB = gainToDB(params_.lowGain);
    float midDB = gainToDB(params_.midGain);
    float highDB = gainToDB(params_.highGain);
    float airDB = gainToDB(params_.airGain);

    *lowShelf_.state = *juce::dsp::IIR::Coefficients<float>::makeLowShelf(
        sampleRate_, LOW_FREQ, Q, juce::Decibels::decibelsToGain(lowDB));

    *midPeak_.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter(
        sampleRate_, MID_FREQ, Q, juce::Decibels::decibelsToGain(midDB));

    *highPeak_.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter(
        sampleRate_, HIGH_FREQ, Q, juce::Decibels::decibelsToGain(highDB));

    *airShelf_.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf(
        sampleRate_, AIR_FREQ, Q, juce::Decibels::decibelsToGain(airDB));
}

} // namespace incant
