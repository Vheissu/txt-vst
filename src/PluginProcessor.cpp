#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace incant {

IncantProcessor::IncantProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput("Input", juce::AudioChannelSet::stereo(), true)
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    eq_ = std::make_unique<Equalizer>();
    compressor_ = std::make_unique<Compressor>();
    reverb_ = std::make_unique<Reverb>();
    distortion_ = std::make_unique<Distortion>();
    delay_ = std::make_unique<Delay>();
    glitch_ = std::make_unique<Glitch>();
    overdrive_ = std::make_unique<Overdrive>();
    chorus_ = std::make_unique<Chorus>();
    phaser_ = std::make_unique<Phaser>();
    tremolo_ = std::make_unique<Tremolo>();
    filter_ = std::make_unique<Filter>();
}

IncantProcessor::~IncantProcessor() = default;

void IncantProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    eq_->prepare(sampleRate, samplesPerBlock);
    compressor_->prepare(sampleRate, samplesPerBlock);
    reverb_->prepare(sampleRate, samplesPerBlock);
    distortion_->prepare(sampleRate, samplesPerBlock);
    delay_->prepare(sampleRate, samplesPerBlock);
    glitch_->prepare(sampleRate, samplesPerBlock);
    overdrive_->prepare(sampleRate, samplesPerBlock);
    chorus_->prepare(sampleRate, samplesPerBlock);
    phaser_->prepare(sampleRate, samplesPerBlock);
    tremolo_->prepare(sampleRate, samplesPerBlock);
    filter_->prepare(sampleRate, samplesPerBlock);
}

void IncantProcessor::releaseResources() {
    eq_->reset();
    compressor_->reset();
    reverb_->reset();
    distortion_->reset();
    delay_->reset();
    glitch_->reset();
    overdrive_->reset();
    chorus_->reset();
    phaser_->reset();
    tremolo_->reset();
    filter_->reset();
}

void IncantProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                   juce::MidiBuffer& /*midiMessages*/) {
    juce::ScopedNoDenormals noDenormals;

    // Calculate input level
    float inLevel = 0.0f;
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch) {
        inLevel = std::max(inLevel, buffer.getRMSLevel(ch, 0, buffer.getNumSamples()));
    }
    inputLevel_ = inLevel;

    // Process effect
    auto* effect = getCurrentEffect();
    if (effect) {
        effect->process(buffer);
    }

    // Calculate output level
    float outLevel = 0.0f;
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch) {
        outLevel = std::max(outLevel, buffer.getRMSLevel(ch, 0, buffer.getNumSamples()));
    }
    outputLevel_ = outLevel;

    // Calculate gain reduction (for compressor visualization)
    if (inLevel > 0.0001f) {
        gainReduction_ = juce::Decibels::gainToDecibels(outLevel / inLevel);
    }
}

juce::AudioProcessorEditor* IncantProcessor::createEditor() {
    return new IncantEditor(*this);
}

void IncantProcessor::getStateInformation(juce::MemoryBlock& destData) {
    juce::XmlElement xml("IncantState");

    xml.setAttribute("effectType", static_cast<int>(currentEffect_));

    auto* effect = getCurrentEffect();
    if (effect) {
        for (int i = 0; i < effect->getNumParameters(); ++i) {
            xml.setAttribute(juce::String("param") + juce::String(i),
                           static_cast<double>(effect->getParameter(i)));
        }
    }

    copyXmlToBinary(xml, destData);
}

void IncantProcessor::setStateInformation(const void* data, int sizeInBytes) {
    auto xml = getXmlFromBinary(data, sizeInBytes);

    if (xml && xml->hasTagName("IncantState")) {
        setEffectType(static_cast<EffectType>(xml->getIntAttribute("effectType", 0)));

        auto* effect = getCurrentEffect();
        if (effect) {
            for (int i = 0; i < effect->getNumParameters(); ++i) {
                float value = static_cast<float>(
                    xml->getDoubleAttribute(juce::String("param") + juce::String(i), 0.5));
                effect->setParameter(i, value);
            }
        }
    }
}

void IncantProcessor::setEffectType(EffectType type) {
    currentEffect_ = type;
}

void IncantProcessor::generateFromText(const std::string& description) {
    llmEngine_.generateParameters(currentEffect_, description,
        [this](bool /*success*/, const ParameterResult& result) {
            applyParameters(result);
        });
}

EffectBase* IncantProcessor::getCurrentEffect() {
    switch (currentEffect_) {
        case EffectType::EQ: return eq_.get();
        case EffectType::Compressor: return compressor_.get();
        case EffectType::Reverb: return reverb_.get();
        case EffectType::Distortion: return distortion_.get();
        case EffectType::Delay: return delay_.get();
        case EffectType::Glitch: return glitch_.get();
        case EffectType::Overdrive: return overdrive_.get();
        case EffectType::Chorus: return chorus_.get();
        case EffectType::Phaser: return phaser_.get();
        case EffectType::Tremolo: return tremolo_.get();
        case EffectType::Filter: return filter_.get();
    }
    return nullptr;
}

const EffectBase* IncantProcessor::getCurrentEffect() const {
    switch (currentEffect_) {
        case EffectType::EQ: return eq_.get();
        case EffectType::Compressor: return compressor_.get();
        case EffectType::Reverb: return reverb_.get();
        case EffectType::Distortion: return distortion_.get();
        case EffectType::Delay: return delay_.get();
        case EffectType::Glitch: return glitch_.get();
        case EffectType::Overdrive: return overdrive_.get();
        case EffectType::Chorus: return chorus_.get();
        case EffectType::Phaser: return phaser_.get();
        case EffectType::Tremolo: return tremolo_.get();
        case EffectType::Filter: return filter_.get();
    }
    return nullptr;
}

void IncantProcessor::applyParameters(const ParameterResult& params) {
    std::visit([this](auto&& p) {
        using T = std::decay_t<decltype(p)>;

        if constexpr (std::is_same_v<T, EQParams>) {
            setEffectParameter(0, p.lowGain);
            setEffectParameter(1, p.midGain);
            setEffectParameter(2, p.highGain);
            setEffectParameter(3, p.airGain);
            setEffectParameter(4, p.dryWet);
        }
        else if constexpr (std::is_same_v<T, CompressorParams>) {
            setEffectParameter(0, p.threshold);
            setEffectParameter(1, p.ratio);
            setEffectParameter(2, p.attack);
            setEffectParameter(3, p.release);
            setEffectParameter(4, p.makeup);
        }
        else if constexpr (std::is_same_v<T, ReverbParams>) {
            setEffectParameter(0, p.size);
            setEffectParameter(1, p.decay);
            setEffectParameter(2, p.damping);
            setEffectParameter(3, p.predelay);
            setEffectParameter(4, p.dryWet);
        }
        else if constexpr (std::is_same_v<T, DistortionParams>) {
            setEffectParameter(0, p.drive);
            setEffectParameter(1, p.tone);
            setEffectParameter(2, p.dryWet);
            setEffectParameter(3, p.curveType);
        }
        else if constexpr (std::is_same_v<T, DelayParams>) {
            setEffectParameter(0, p.time);
            setEffectParameter(1, p.feedback);
            setEffectParameter(2, p.filter);
            setEffectParameter(3, p.pingPong);
            setEffectParameter(4, p.dryWet);
        }
        else if constexpr (std::is_same_v<T, GlitchParams>) {
            setEffectParameter(0, p.rate);
            setEffectParameter(1, p.stutter);
            setEffectParameter(2, p.crush);
            setEffectParameter(3, p.reverse);
            setEffectParameter(4, p.dryWet);
        }
        else if constexpr (std::is_same_v<T, OverdriveParams>) {
            setEffectParameter(0, p.drive);
            setEffectParameter(1, p.tone);
            setEffectParameter(2, p.level);
            setEffectParameter(3, p.midBoost);
            setEffectParameter(4, p.tightness);
        }
        else if constexpr (std::is_same_v<T, ChorusParams>) {
            setEffectParameter(0, p.rate);
            setEffectParameter(1, p.depth);
            setEffectParameter(2, p.delay);
            setEffectParameter(3, p.feedback);
            setEffectParameter(4, p.dryWet);
        }
        else if constexpr (std::is_same_v<T, PhaserParams>) {
            setEffectParameter(0, p.rate);
            setEffectParameter(1, p.depth);
            setEffectParameter(2, p.feedback);
            setEffectParameter(3, p.stages);
            setEffectParameter(4, p.dryWet);
        }
        else if constexpr (std::is_same_v<T, TremoloParams>) {
            setEffectParameter(0, p.rate);
            setEffectParameter(1, p.depth);
            setEffectParameter(2, p.shape);
            setEffectParameter(3, p.stereo);
            setEffectParameter(4, p.dryWet);
        }
        else if constexpr (std::is_same_v<T, FilterParams>) {
            setEffectParameter(0, p.cutoff);
            setEffectParameter(1, p.resonance);
            setEffectParameter(2, p.lfoRate);
            setEffectParameter(3, p.lfoDepth);
            setEffectParameter(4, p.filterType);
        }
    }, params);
}

void IncantProcessor::setEffectParameter(int index, float targetValue) {
    auto* effect = getCurrentEffect();
    if (effect && index >= 0 && index < effect->getNumParameters()) {
        effect->setParameter(index, targetValue);
    }
}

} // namespace incant

// Create plugin instances
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new incant::IncantProcessor();
}
