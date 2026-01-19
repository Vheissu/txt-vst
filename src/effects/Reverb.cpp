#include "Reverb.h"

namespace incant {

Reverb::Reverb() {
    reverbParams_.roomSize = 0.5f;
    reverbParams_.damping = 0.5f;
    reverbParams_.wetLevel = 0.33f;
    reverbParams_.dryLevel = 0.67f;
    reverbParams_.width = 1.0f;
    reverbParams_.freezeMode = 0.0f;
}

void Reverb::prepare(double sampleRate, int samplesPerBlock) {
    sampleRate_ = sampleRate;
    blockSize_ = samplesPerBlock;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels = 2;

    reverb_.prepare(spec);

    // Pre-delay buffer: up to 200ms
    int maxPredelaySamples = static_cast<int>(sampleRate * 0.2);
    predelayBuffer_.setSize(2, maxPredelaySamples);
    predelayBuffer_.clear();
    predelayWritePos_ = 0;

    updateReverb();
}

void Reverb::process(juce::AudioBuffer<float>& buffer) {
    const int numSamples = buffer.getNumSamples();
    const int numChannels = std::min(buffer.getNumChannels(), 2);

    // Apply pre-delay if needed
    if (predelaySamples_ > 0) {
        juce::AudioBuffer<float> tempBuffer(numChannels, numSamples);

        for (int ch = 0; ch < numChannels; ++ch) {
            const float* input = buffer.getReadPointer(ch);
            float* output = tempBuffer.getWritePointer(ch);
            float* delayData = predelayBuffer_.getWritePointer(ch);
            const int delaySize = predelayBuffer_.getNumSamples();

            for (int i = 0; i < numSamples; ++i) {
                // Read from delay
                int readPos = (predelayWritePos_ - predelaySamples_ + delaySize) % delaySize;
                output[i] = delayData[(readPos + i) % delaySize];

                // Write to delay
                delayData[(predelayWritePos_ + i) % delaySize] = input[i];
            }
        }

        predelayWritePos_ = (predelayWritePos_ + numSamples) % predelayBuffer_.getNumSamples();

        // Process reverb on delayed signal
        juce::dsp::AudioBlock<float> block(tempBuffer);
        juce::dsp::ProcessContextReplacing<float> context(block);
        reverb_.process(context);

        // Copy back
        for (int ch = 0; ch < numChannels; ++ch) {
            buffer.copyFrom(ch, 0, tempBuffer, ch, 0, numSamples);
        }
    } else {
        // No pre-delay, process directly
        juce::dsp::AudioBlock<float> block(buffer);
        juce::dsp::ProcessContextReplacing<float> context(block);
        reverb_.process(context);
    }
}

void Reverb::reset() {
    reverb_.reset();
    predelayBuffer_.clear();
    predelayWritePos_ = 0;
}

void Reverb::setParameter(int index, float value) {
    value = juce::jlimit(0.0f, 1.0f, value);

    switch (index) {
        case 0: params_.size = value; break;
        case 1: params_.decay = value; break;
        case 2: params_.damping = value; break;
        case 3: params_.predelay = value; break;
        case 4: params_.dryWet = value; break;
    }

    updateReverb();
}

float Reverb::getParameter(int index) const {
    switch (index) {
        case 0: return params_.size;
        case 1: return params_.decay;
        case 2: return params_.damping;
        case 3: return params_.predelay;
        case 4: return params_.dryWet;
    }
    return 0.0f;
}

const char* Reverb::getParameterName(int index) const {
    static const char* names[] = {"Size", "Decay", "Damping", "PreDelay", "Dry/Wet"};
    if (index >= 0 && index < 5) return names[index];
    return "";
}

void Reverb::setParams(const ReverbParams& params) {
    params_ = params;
    updateReverb();
}

void Reverb::updateReverb() {
    // Map normalized parameters to JUCE reverb parameters
    // Room size combines size and decay
    reverbParams_.roomSize = params_.size * 0.5f + params_.decay * 0.5f;
    reverbParams_.damping = params_.damping;
    reverbParams_.wetLevel = params_.dryWet;
    reverbParams_.dryLevel = 1.0f - params_.dryWet;
    reverbParams_.width = 1.0f;

    reverb_.setParameters(reverbParams_);

    // Pre-delay: 0-1 maps to 0-200ms
    predelaySamples_ = static_cast<int>(params_.predelay * sampleRate_ * 0.2);
}

} // namespace incant
