#include "Distortion.h"
#include <cmath>

namespace incant {

Distortion::Distortion() = default;

void Distortion::prepare(double sampleRate, int samplesPerBlock) {
    sampleRate_ = sampleRate;
    blockSize_ = samplesPerBlock;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels = 2;

    toneFilter_.prepare(spec);
    updateFilter();
}

void Distortion::process(juce::AudioBuffer<float>& buffer) {
    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    // Keep dry signal for mixing
    juce::AudioBuffer<float> dryBuffer;
    if (params_.dryWet < 1.0f) {
        dryBuffer.makeCopyOf(buffer);
    }

    // Apply drive and distortion
    for (int ch = 0; ch < numChannels; ++ch) {
        float* data = buffer.getWritePointer(ch);
        for (int i = 0; i < numSamples; ++i) {
            data[i] = processSample(data[i] * driveGain_);
        }
    }

    // Apply tone filter
    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::ProcessContextReplacing<float> context(block);
    toneFilter_.process(context);

    // Mix dry/wet
    if (params_.dryWet < 1.0f) {
        for (int ch = 0; ch < numChannels; ++ch) {
            float* wet = buffer.getWritePointer(ch);
            const float* dry = dryBuffer.getReadPointer(ch);
            for (int i = 0; i < numSamples; ++i) {
                wet[i] = dry[i] * (1.0f - params_.dryWet) + wet[i] * params_.dryWet;
            }
        }
    }
}

void Distortion::reset() {
    toneFilter_.reset();
}

float Distortion::processSample(float sample) const {
    switch (curveType_) {
        case CurveType::SoftClip:
            // Soft saturation using tanh
            return std::tanh(sample);

        case CurveType::HardClip:
            // Hard clipping at -1 to 1
            return juce::jlimit(-1.0f, 1.0f, sample);

        case CurveType::Tube: {
            // Asymmetric tube-style distortion
            if (sample >= 0.0f) {
                return 1.0f - std::exp(-sample);
            } else {
                return -1.0f + std::exp(sample);
            }
        }

        case CurveType::Fuzz: {
            // Fuzz with rectification
            float rectified = std::abs(sample);
            return std::tanh(rectified * 2.0f) * (sample >= 0 ? 1.0f : -0.5f);
        }
    }
    return sample;
}

void Distortion::setParameter(int index, float value) {
    value = juce::jlimit(0.0f, 1.0f, value);

    switch (index) {
        case 0: params_.drive = value; break;
        case 1: params_.tone = value; break;
        case 2: params_.dryWet = value; break;
        case 3: params_.curveType = value; break;
    }

    // Update curve type
    if (params_.curveType < 0.25f) {
        curveType_ = CurveType::SoftClip;
    } else if (params_.curveType < 0.5f) {
        curveType_ = CurveType::HardClip;
    } else if (params_.curveType < 0.75f) {
        curveType_ = CurveType::Tube;
    } else {
        curveType_ = CurveType::Fuzz;
    }

    // Drive: 0-1 maps to 1x-50x gain
    driveGain_ = 1.0f + params_.drive * 49.0f;

    updateFilter();
}

float Distortion::getParameter(int index) const {
    switch (index) {
        case 0: return params_.drive;
        case 1: return params_.tone;
        case 2: return params_.dryWet;
        case 3: return params_.curveType;
    }
    return 0.0f;
}

const char* Distortion::getParameterName(int index) const {
    static const char* names[] = {"Drive", "Tone", "Dry/Wet", "Type"};
    if (index >= 0 && index < 4) return names[index];
    return "";
}

void Distortion::setParams(const DistortionParams& params) {
    params_ = params;
    setParameter(3, params.curveType); // Triggers curve type update
    updateFilter();
}

void Distortion::updateFilter() {
    // Tone: 0=dark (1kHz lowpass), 1=bright (12kHz lowpass)
    float cutoff = 1000.0f + params_.tone * 11000.0f;

    *toneFilter_.state = *juce::dsp::IIR::Coefficients<float>::makeLowPass(
        sampleRate_, cutoff);
}

} // namespace incant
