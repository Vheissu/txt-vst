#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "ParameterSchema.h"
#include "LLMEngine.h"
#include "PresetManager.h"
#include "effects/Equalizer.h"
#include "effects/Compressor.h"
#include "effects/Reverb.h"
#include "effects/Distortion.h"
#include "effects/Delay.h"
#include "effects/Glitch.h"
#include "effects/Overdrive.h"
#include "effects/Chorus.h"
#include "effects/Phaser.h"
#include "effects/Tremolo.h"
#include "effects/Filter.h"
#include <memory>
#include <atomic>

namespace incant {

class IncantProcessor : public juce::AudioProcessor {
public:
    IncantProcessor();
    ~IncantProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Incant"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // Effect control
    void setEffectType(EffectType type);
    EffectType getEffectType() const { return currentEffect_; }

    // Generation
    void generateFromText(const std::string& description);
    LLMEngine::Status getLLMStatus() const { return llmEngine_.getStatus(); }

    // Current effect's parameters
    EffectBase* getCurrentEffect();
    const EffectBase* getCurrentEffect() const;

    // Apply generated parameters
    void applyParameters(const ParameterResult& params);
    void setEffectParameter(int index, float targetValue);

    // Metering
    float getInputLevel() const { return inputLevel_.load(); }
    float getOutputLevel() const { return outputLevel_.load(); }
    float getGainReduction() const { return gainReduction_.load(); }

    // Preset management
    PresetManager& getPresetManager() { return presetManager_; }

private:
    EffectType currentEffect_ = EffectType::Reverb;

    std::unique_ptr<Equalizer> eq_;
    std::unique_ptr<Compressor> compressor_;
    std::unique_ptr<Reverb> reverb_;
    std::unique_ptr<Distortion> distortion_;
    std::unique_ptr<Delay> delay_;
    std::unique_ptr<Glitch> glitch_;
    std::unique_ptr<Overdrive> overdrive_;
    std::unique_ptr<Chorus> chorus_;
    std::unique_ptr<Phaser> phaser_;
    std::unique_ptr<Tremolo> tremolo_;
    std::unique_ptr<Filter> filter_;

    LLMEngine llmEngine_;
    PresetManager presetManager_;

    // Metering
    std::atomic<float> inputLevel_{0.0f};
    std::atomic<float> outputLevel_{0.0f};
    std::atomic<float> gainReduction_{0.0f};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(IncantProcessor)
};

} // namespace incant
