#include "PresetManager.h"
#include "PluginProcessor.h"

namespace incant {

PresetManager::PresetManager() {
    auto appData = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
    presetsFile_ = appData.getChildFile("Incant").getChildFile("presets.xml");
    presetsFile_.getParentDirectory().createDirectory();

    loadFactoryPresets();  // Always load factory presets
    loadFromFile();        // Load user presets on top
}

void PresetManager::savePreset(const juce::String& name,
                               const juce::String& description,
                               EffectType type,
                               const std::vector<float>& params) {
    Preset preset;
    preset.name = name;
    preset.description = description;
    preset.effectType = type;
    preset.parameters = params;
    preset.created = juce::Time::getCurrentTime();

    presets_.push_back(preset);
    saveToFile();
}

bool PresetManager::loadPreset(int index, EffectType& type, std::vector<float>& params) {
    if (index < 0 || index >= static_cast<int>(presets_.size())) {
        return false;
    }

    const auto& preset = presets_[static_cast<size_t>(index)];
    type = preset.effectType;
    params = preset.parameters;
    return true;
}

juce::String PresetManager::getPresetName(int index) const {
    if (index < 0 || index >= static_cast<int>(presets_.size())) {
        return {};
    }
    return presets_[static_cast<size_t>(index)].name;
}

juce::String PresetManager::getPresetDescription(int index) const {
    if (index < 0 || index >= static_cast<int>(presets_.size())) {
        return {};
    }
    return presets_[static_cast<size_t>(index)].description;
}

void PresetManager::loadFactoryPresets() {
    factoryPresets_.clear();

    // EQ Presets
    factoryPresets_.push_back({"Warm Embrace", "warm full analog vintage", EffectType::EQ,
                       {0.65f, 0.55f, 0.4f, 0.35f, 1.0f}, juce::Time()});
    factoryPresets_.push_back({"Crystal Air", "bright airy shimmer sparkle", EffectType::EQ,
                       {0.5f, 0.5f, 0.7f, 0.75f, 1.0f}, juce::Time()});
    factoryPresets_.push_back({"Telephone", "thin tinny lo-fi", EffectType::EQ,
                       {0.3f, 0.7f, 0.35f, 0.2f, 1.0f}, juce::Time()});
    factoryPresets_.push_back({"Bass Thunder", "bass sub boom weight", EffectType::EQ,
                       {0.8f, 0.45f, 0.4f, 0.4f, 1.0f}, juce::Time()});

    // Compressor Presets
    factoryPresets_.push_back({"Glue Master", "glue cohesive smooth bus", EffectType::Compressor,
                       {0.55f, 0.2f, 0.5f, 0.6f, 0.4f}, juce::Time()});
    factoryPresets_.push_back({"Drum Punch", "punchy snappy drum transient", EffectType::Compressor,
                       {0.4f, 0.4f, 0.15f, 0.25f, 0.5f}, juce::Time()});
    factoryPresets_.push_back({"Squash", "heavy pumping aggressive", EffectType::Compressor,
                       {0.2f, 0.8f, 0.1f, 0.3f, 0.6f}, juce::Time()});
    factoryPresets_.push_back({"Gentle Touch", "subtle transparent natural", EffectType::Compressor,
                       {0.65f, 0.15f, 0.4f, 0.5f, 0.3f}, juce::Time()});

    // Reverb Presets
    factoryPresets_.push_back({"Cathedral", "huge massive cathedral epic", EffectType::Reverb,
                       {0.9f, 0.85f, 0.3f, 0.15f, 0.4f}, juce::Time()});
    factoryPresets_.push_back({"Intimate Room", "small room tight close", EffectType::Reverb,
                       {0.25f, 0.2f, 0.5f, 0.05f, 0.25f}, juce::Time()});
    factoryPresets_.push_back({"Dark Hall", "hall large dark warm", EffectType::Reverb,
                       {0.7f, 0.6f, 0.7f, 0.1f, 0.35f}, juce::Time()});
    factoryPresets_.push_back({"Shimmer Wash", "bright shimmer infinite pad", EffectType::Reverb,
                       {0.8f, 0.95f, 0.2f, 0.2f, 0.5f}, juce::Time()});

    // Distortion Presets
    factoryPresets_.push_back({"Tape Warmth", "tape saturation warm analog", EffectType::Distortion,
                       {0.35f, 0.45f, 0.6f, 0.0f}, juce::Time()});
    factoryPresets_.push_back({"Tube Drive", "tube overdrive valve amp", EffectType::Distortion,
                       {0.5f, 0.55f, 0.7f, 0.5f}, juce::Time()});
    factoryPresets_.push_back({"Fuzz Chaos", "fuzz destroyed chaos broken", EffectType::Distortion,
                       {0.9f, 0.4f, 0.8f, 1.0f}, juce::Time()});
    factoryPresets_.push_back({"Edge", "crunch gritty edge overdrive", EffectType::Distortion,
                       {0.5f, 0.6f, 0.65f, 0.5f}, juce::Time()});
}

void PresetManager::loadPreset(const Preset& preset, IncantProcessor& processor) {
    processor.setEffectType(preset.effectType);

    auto* effect = processor.getCurrentEffect();
    if (effect) {
        for (size_t i = 0; i < preset.parameters.size() && i < static_cast<size_t>(effect->getNumParameters()); ++i) {
            effect->setParameter(static_cast<int>(i), preset.parameters[i]);
        }
    }
}

void PresetManager::saveToFile() {
    juce::XmlElement root("IncantPresets");

    for (const auto& preset : presets_) {
        auto* elem = root.createNewChildElement("Preset");
        elem->setAttribute("name", preset.name);
        elem->setAttribute("description", preset.description);
        elem->setAttribute("effectType", static_cast<int>(preset.effectType));

        juce::String paramStr;
        for (size_t i = 0; i < preset.parameters.size(); ++i) {
            if (i > 0) paramStr += ",";
            paramStr += juce::String(preset.parameters[i]);
        }
        elem->setAttribute("parameters", paramStr);
    }

    root.writeTo(presetsFile_);
}

void PresetManager::loadFromFile() {
    if (!presetsFile_.existsAsFile()) return;

    auto xml = juce::XmlDocument::parse(presetsFile_);
    if (!xml || !xml->hasTagName("IncantPresets")) return;

    presets_.clear();

    for (auto* elem : xml->getChildIterator()) {
        if (elem->hasTagName("Preset")) {
            Preset preset;
            preset.name = elem->getStringAttribute("name");
            preset.description = elem->getStringAttribute("description");
            preset.effectType = static_cast<EffectType>(elem->getIntAttribute("effectType"));

            juce::StringArray tokens;
            tokens.addTokens(elem->getStringAttribute("parameters"), ",", "");
            for (const auto& token : tokens) {
                preset.parameters.push_back(token.getFloatValue());
            }

            presets_.push_back(preset);
        }
    }
}

std::vector<int> PresetManager::getPresetsForEffect(EffectType type) const {
    std::vector<int> result;
    for (size_t i = 0; i < presets_.size(); ++i) {
        if (presets_[i].effectType == type) {
            result.push_back(static_cast<int>(i));
        }
    }
    return result;
}

} // namespace incant
