#include "Overdrive.h"
#include <cmath>

namespace incant {

Overdrive::Overdrive() {
}

void Overdrive::prepare(double sampleRate, int samplesPerBlock) {
    sampleRate_ = sampleRate;
    blockSize_ = samplesPerBlock;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels = 2;

    inputHighPass_.prepare(spec);
    midBoost_.prepare(spec);
    toneFilter_.prepare(spec);

    updateFilters();
}

void Overdrive::process(juce::AudioBuffer<float>& buffer) {
    const int numSamples = buffer.getNumSamples();
    const int numChannels = std::min(buffer.getNumChannels(), 2);

    // Calculate drive gain (1x to 100x)
    float driveGain = 1.0f + params_.drive * 99.0f;

    // Output level (0.1 to 2.0)
    float outputLevel = 0.1f + params_.level * 1.9f;

    // Apply input high-pass (tightness)
    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::ProcessContextReplacing<float> context(block);
    inputHighPass_.process(context);

    // Apply drive and soft clipping
    for (int ch = 0; ch < numChannels; ++ch) {
        float* data = buffer.getWritePointer(ch);
        for (int i = 0; i < numSamples; ++i) {
            // Apply gain
            float sample = data[i] * driveGain;

            // Soft clip (asymmetric TS-style)
            sample = softClip(sample);

            data[i] = sample;
        }
    }

    // Apply mid-boost EQ
    midBoost_.process(context);

    // Apply tone filter
    toneFilter_.process(context);

    // Apply output level
    for (int ch = 0; ch < numChannels; ++ch) {
        float* data = buffer.getWritePointer(ch);
        for (int i = 0; i < numSamples; ++i) {
            data[i] *= outputLevel;
        }
    }
}

float Overdrive::softClip(float sample) {
    // Asymmetric soft clipping inspired by TS diode clipping
    // Positive side clips slightly harder than negative
    if (sample > 0.0f) {
        // Positive: tanh-like with slight asymmetry
        return std::tanh(sample * 1.2f) * 0.9f;
    } else {
        // Negative: softer clipping
        return std::tanh(sample * 0.9f);
    }
}

void Overdrive::reset() {
    inputHighPass_.reset();
    midBoost_.reset();
    toneFilter_.reset();
}

void Overdrive::setParameter(int index, float value) {
    value = juce::jlimit(0.0f, 1.0f, value);

    switch (index) {
        case 0: params_.drive = value; break;
        case 1: params_.tone = value; break;
        case 2: params_.level = value; break;
        case 3: params_.midBoost = value; break;
        case 4: params_.tightness = value; break;
    }

    updateFilters();
}

float Overdrive::getParameter(int index) const {
    switch (index) {
        case 0: return params_.drive;
        case 1: return params_.tone;
        case 2: return params_.level;
        case 3: return params_.midBoost;
        case 4: return params_.tightness;
    }
    return 0.0f;
}

const char* Overdrive::getParameterName(int index) const {
    static const char* names[] = {"Drive", "Tone", "Level", "MidBoost", "Tightness"};
    if (index >= 0 && index < 5) return names[index];
    return "";
}

void Overdrive::setParams(const OverdriveParams& params) {
    params_ = params;
    updateFilters();
}

void Overdrive::updateFilters() {
    // Input high-pass: 60Hz (loose) to 720Hz (tight)
    // This is the "tightness" control - classic TS cuts bass
    float hpFreq = 60.0f + params_.tightness * 660.0f;
    hpFreq = std::min(hpFreq, static_cast<float>(sampleRate_ * 0.45));

    *inputHighPass_.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass(
        sampleRate_, hpFreq);

    // Mid-boost: Peak EQ around 720Hz (the TS "hump")
    // Q varies with boost amount
    float midFreq = 720.0f;
    float midGainDb = params_.midBoost * 12.0f;  // 0 to 12dB boost
    float midQ = 0.7f + params_.midBoost * 0.8f;  // Q increases with boost

    *midBoost_.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter(
        sampleRate_, midFreq, midQ, juce::Decibels::decibelsToGain(midGainDb));

    // Tone control: Lowpass from 1kHz (dark) to 8kHz (bright)
    float toneFreq = 1000.0f + params_.tone * 7000.0f;
    toneFreq = std::min(toneFreq, static_cast<float>(sampleRate_ * 0.45));

    *toneFilter_.state = *juce::dsp::IIR::Coefficients<float>::makeLowPass(
        sampleRate_, toneFreq, 0.707f);
}

} // namespace incant
