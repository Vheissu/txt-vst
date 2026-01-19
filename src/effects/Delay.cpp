#include "Delay.h"

namespace incant {

Delay::Delay() {
    smoothedFeedback_.setCurrentAndTargetValue(params_.feedback);
    smoothedDryWet_.setCurrentAndTargetValue(params_.dryWet);
}

void Delay::prepare(double sampleRate, int samplesPerBlock) {
    sampleRate_ = sampleRate;
    blockSize_ = samplesPerBlock;

    // Max delay: 1 second
    int maxDelaySamples = static_cast<int>(sampleRate);
    delayBuffer_.setSize(2, maxDelaySamples);
    delayBuffer_.clear();
    writePosition_ = 0;

    // Prepare feedback filter
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels = 2;

    feedbackFilter_.prepare(spec);

    // Smoothing for parameter changes (50ms ramp)
    smoothedFeedback_.reset(sampleRate, 0.05);
    smoothedDryWet_.reset(sampleRate, 0.05);

    updateDelay();
}

void Delay::process(juce::AudioBuffer<float>& buffer) {
    const int numSamples = buffer.getNumSamples();
    const int numChannels = std::min(buffer.getNumChannels(), 2);
    const int delayBufferSize = delayBuffer_.getNumSamples();

    if (delayBufferSize == 0 || delaySamples_ == 0)
        return;

    // Ping-pong factor (0 = mono, 1 = full ping-pong)
    const float pingPong = params_.pingPong;

    for (int sample = 0; sample < numSamples; ++sample) {
        const float feedback = smoothedFeedback_.getNextValue();
        const float dryWet = smoothedDryWet_.getNextValue();

        // Read position for delay
        int readPos = (writePosition_ - delaySamples_ + delayBufferSize) % delayBufferSize;

        // Get delayed samples
        float delayedL = delayBuffer_.getSample(0, readPos);
        float delayedR = delayBuffer_.getSample(1, readPos);

        // Apply ping-pong: cross-feed channels
        float feedbackL = delayedL * (1.0f - pingPong) + delayedR * pingPong;
        float feedbackR = delayedR * (1.0f - pingPong) + delayedL * pingPong;

        // Get input samples
        float inputL = buffer.getSample(0, sample);
        float inputR = numChannels > 1 ? buffer.getSample(1, sample) : inputL;

        // Write to delay buffer (input + feedback)
        float writeL = inputL + feedbackL * feedback;
        float writeR = inputR + feedbackR * feedback;

        // Soft clip to prevent runaway feedback
        writeL = std::tanh(writeL);
        writeR = std::tanh(writeR);

        delayBuffer_.setSample(0, writePosition_, writeL);
        delayBuffer_.setSample(1, writePosition_, writeR);

        // Mix dry/wet
        float outL = inputL * (1.0f - dryWet) + delayedL * dryWet;
        float outR = inputR * (1.0f - dryWet) + delayedR * dryWet;

        buffer.setSample(0, sample, outL);
        if (numChannels > 1)
            buffer.setSample(1, sample, outR);

        // Advance write position
        writePosition_ = (writePosition_ + 1) % delayBufferSize;
    }

    // Apply filter to delay buffer feedback (for next iteration)
    // This creates the darkening effect on repeated echoes
    juce::dsp::AudioBlock<float> delayBlock(delayBuffer_);
    juce::dsp::ProcessContextReplacing<float> context(delayBlock);
    feedbackFilter_.process(context);
}

void Delay::reset() {
    delayBuffer_.clear();
    writePosition_ = 0;
    feedbackFilter_.reset();
    smoothedFeedback_.setCurrentAndTargetValue(params_.feedback);
    smoothedDryWet_.setCurrentAndTargetValue(params_.dryWet);
}

void Delay::setParameter(int index, float value) {
    value = juce::jlimit(0.0f, 1.0f, value);

    switch (index) {
        case 0: params_.time = value; break;
        case 1: params_.feedback = value; break;
        case 2: params_.filter = value; break;
        case 3: params_.pingPong = value; break;
        case 4: params_.dryWet = value; break;
    }

    updateDelay();
}

float Delay::getParameter(int index) const {
    switch (index) {
        case 0: return params_.time;
        case 1: return params_.feedback;
        case 2: return params_.filter;
        case 3: return params_.pingPong;
        case 4: return params_.dryWet;
    }
    return 0.0f;
}

const char* Delay::getParameterName(int index) const {
    static const char* names[] = {"Time", "Feedback", "Filter", "PingPong", "Dry/Wet"};
    if (index >= 0 && index < 5) return names[index];
    return "";
}

void Delay::setParams(const DelayParams& params) {
    params_ = params;
    updateDelay();
}

void Delay::updateDelay() {
    // Map time: 0-1 to 10ms-1000ms
    float delayMs = 10.0f + params_.time * 990.0f;
    delaySamples_ = static_cast<int>(delayMs * sampleRate_ / 1000.0);
    delaySamples_ = std::min(delaySamples_, delayBuffer_.getNumSamples() - 1);

    // Update feedback smoother target
    // Limit feedback to 0.95 to prevent infinite buildup
    smoothedFeedback_.setTargetValue(params_.feedback * 0.95f);
    smoothedDryWet_.setTargetValue(params_.dryWet);

    // Filter frequency: 0=500Hz (dark), 1=15kHz (bright)
    float filterFreq = 500.0f + params_.filter * 14500.0f;
    filterFreq = std::min(filterFreq, static_cast<float>(sampleRate_ * 0.45));

    *feedbackFilter_.state = *juce::dsp::IIR::Coefficients<float>::makeLowPass(
        sampleRate_, filterFreq);
}

} // namespace incant
