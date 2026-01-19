#pragma once

#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>
#include "ParameterSchema.h"
#include <vector>
#include <string>

namespace incant {

struct Preset {
    juce::String name;
    juce::String description;  // The incantation text
    EffectType effectType;
    std::vector<float> parameters;
    juce::Time created;
};

class PresetManager {
public:
    PresetManager();
    ~PresetManager() = default;

    // Save current state as preset
    void savePreset(const juce::String& name,
                   const juce::String& description,
                   EffectType type,
                   const std::vector<float>& params);

    // Load preset by index
    bool loadPreset(int index, EffectType& type, std::vector<float>& params);

    // Get preset info
    int getNumPresets() const { return static_cast<int>(presets_.size()); }
    juce::String getPresetName(int index) const;
    juce::String getPresetDescription(int index) const;

    // Built-in presets
    void loadFactoryPresets();
    const std::vector<Preset>& getFactoryPresets() const { return factoryPresets_; }

    // Load preset directly
    void loadPreset(const Preset& preset, class IncantProcessor& processor);

    // File operations
    void saveToFile();
    void loadFromFile();

    // Get presets for a specific effect type
    std::vector<int> getPresetsForEffect(EffectType type) const;

private:
    std::vector<Preset> presets_;
    std::vector<Preset> factoryPresets_;
    juce::File presetsFile_;
};

} // namespace incant
