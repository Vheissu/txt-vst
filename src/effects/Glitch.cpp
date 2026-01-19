#include "Glitch.h"

namespace incant {

Glitch::Glitch() : rng_(std::random_device{}()) {
}

void Glitch::prepare(double sampleRate, int samplesPerBlock) {
    sampleRate_ = sampleRate;
    blockSize_ = samplesPerBlock;

    // Capture buffer: up to 500ms for glitch chunks
    int maxCaptureSamples = static_cast<int>(sampleRate * 0.5);
    captureBuffer_.setSize(2, maxCaptureSamples);
    captureBuffer_.clear();

    dryBuffer_.setSize(2, samplesPerBlock);

    reset();
}

void Glitch::process(juce::AudioBuffer<float>& buffer) {
    const int numSamples = buffer.getNumSamples();
    const int numChannels = std::min(buffer.getNumChannels(), 2);
    const float dryWet = params_.dryWet;

    // Store dry signal
    for (int ch = 0; ch < numChannels; ++ch) {
        dryBuffer_.copyFrom(ch, 0, buffer, ch, 0, numSamples);
    }

    for (int sample = 0; sample < numSamples; ++sample) {
        // Check if we should trigger a new glitch
        if (!isGlitching_) {
            samplesUntilNextGlitch_--;
            if (samplesUntilNextGlitch_ <= 0) {
                triggerGlitch();
            }

            // Continuously capture audio for potential glitch
            for (int ch = 0; ch < numChannels; ++ch) {
                captureBuffer_.setSample(ch, capturePosition_, buffer.getSample(ch, sample));
            }
            capturePosition_ = (capturePosition_ + 1) % captureBuffer_.getNumSamples();
        }

        // Process glitch if active
        if (isGlitching_) {
            // Get sample from captured buffer
            int readPos;
            if (isReversed_) {
                readPos = (capturePosition_ - 1 - glitchPlaybackPos_ + captureBuffer_.getNumSamples())
                          % captureBuffer_.getNumSamples();
            } else {
                readPos = (capturePosition_ - captureLength_ + glitchPlaybackPos_ + captureBuffer_.getNumSamples())
                          % captureBuffer_.getNumSamples();
            }

            for (int ch = 0; ch < numChannels; ++ch) {
                float glitchSample = captureBuffer_.getSample(ch, readPos);

                // Apply bit crushing
                if (params_.crush > 0.01f) {
                    glitchSample = bitCrush(glitchSample, params_.crush);
                }

                buffer.setSample(ch, sample, glitchSample);
            }

            // Advance playback
            glitchPlaybackPos_++;
            if (glitchPlaybackPos_ >= captureLength_) {
                glitchPlaybackPos_ = 0;
                currentRepeat_++;

                // Check if we should continue repeating
                if (currentRepeat_ >= glitchRepeatCount_) {
                    isGlitching_ = false;
                    // Schedule next glitch
                    float rateMs = 50.0f + (1.0f - params_.rate) * 2000.0f;
                    samplesUntilNextGlitch_ = static_cast<int>(rateMs * sampleRate_ / 1000.0f);
                    // Add some randomness
                    samplesUntilNextGlitch_ = static_cast<int>(
                        samplesUntilNextGlitch_ * (0.5f + dist_(rng_)));
                }
            }
        }
    }

    // Mix dry/wet
    for (int ch = 0; ch < numChannels; ++ch) {
        float* out = buffer.getWritePointer(ch);
        const float* dry = dryBuffer_.getReadPointer(ch);
        for (int i = 0; i < numSamples; ++i) {
            out[i] = dry[i] * (1.0f - dryWet) + out[i] * dryWet;
        }
    }
}

void Glitch::triggerGlitch() {
    // Don't glitch if rate is very low
    if (params_.rate < 0.01f) {
        samplesUntilNextGlitch_ = static_cast<int>(sampleRate_);
        return;
    }

    isGlitching_ = true;
    glitchPlaybackPos_ = 0;
    currentRepeat_ = 0;

    // Determine capture length based on stutter param
    // Lower stutter = longer chunks (50-200ms), higher = shorter (5-50ms)
    float minMs = 5.0f + (1.0f - params_.stutter) * 45.0f;
    float maxMs = 50.0f + (1.0f - params_.stutter) * 150.0f;
    float lengthMs = minMs + dist_(rng_) * (maxMs - minMs);
    captureLength_ = static_cast<int>(lengthMs * sampleRate_ / 1000.0f);
    captureLength_ = std::min(captureLength_, captureBuffer_.getNumSamples() - 1);
    captureLength_ = std::max(captureLength_, 64); // Minimum chunk size

    // Determine repeat count based on stutter
    int minRepeats = 1;
    int maxRepeats = 2 + static_cast<int>(params_.stutter * 14); // 2-16 repeats
    glitchRepeatCount_ = minRepeats + static_cast<int>(dist_(rng_) * (maxRepeats - minRepeats));

    // Decide if this chunk should be reversed
    isReversed_ = (dist_(rng_) < params_.reverse);
}

float Glitch::bitCrush(float sample, float amount) {
    // amount 0-1 maps to 16 bits down to ~3 bits
    float bits = 16.0f - amount * 13.0f;
    bits = std::max(bits, 2.0f);

    float levels = std::pow(2.0f, bits);

    // Quantize
    sample = std::round(sample * levels) / levels;

    // Optional: add sample rate reduction effect at high crush
    // (already implicit in the quantization noise)

    return sample;
}

void Glitch::reset() {
    captureBuffer_.clear();
    capturePosition_ = 0;
    isGlitching_ = false;
    isReversed_ = false;
    glitchPlaybackPos_ = 0;
    glitchRepeatCount_ = 0;
    currentRepeat_ = 0;

    // Initial delay before first glitch
    samplesUntilNextGlitch_ = static_cast<int>(sampleRate_ * 0.1);
}

void Glitch::setParameter(int index, float value) {
    value = juce::jlimit(0.0f, 1.0f, value);

    switch (index) {
        case 0: params_.rate = value; break;
        case 1: params_.stutter = value; break;
        case 2: params_.crush = value; break;
        case 3: params_.reverse = value; break;
        case 4: params_.dryWet = value; break;
    }
}

float Glitch::getParameter(int index) const {
    switch (index) {
        case 0: return params_.rate;
        case 1: return params_.stutter;
        case 2: return params_.crush;
        case 3: return params_.reverse;
        case 4: return params_.dryWet;
    }
    return 0.0f;
}

const char* Glitch::getParameterName(int index) const {
    static const char* names[] = {"Rate", "Stutter", "Crush", "Reverse", "Dry/Wet"};
    if (index >= 0 && index < 5) return names[index];
    return "";
}

void Glitch::setParams(const GlitchParams& params) {
    params_ = params;
}

} // namespace incant
