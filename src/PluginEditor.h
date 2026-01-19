#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

namespace incant {

//==============================================================================
// Custom knob with mystical rune aesthetic
class MysticalKnob : public juce::Slider {
public:
    MysticalKnob();
    void paint(juce::Graphics& g) override;

private:
    float glowIntensity_ = 0.0f;
};

//==============================================================================
// Level meter with magical glow
class LevelMeter : public juce::Component, public juce::Timer {
public:
    LevelMeter();
    void paint(juce::Graphics& g) override;
    void timerCallback() override;
    void setLevel(float level);

private:
    float currentLevel_ = 0.0f;
    float targetLevel_ = 0.0f;
    float peakLevel_ = 0.0f;
    int peakHoldCounter_ = 0;
};

//==============================================================================
// Main Editor
class IncantEditor : public juce::AudioProcessorEditor,
                     public juce::Timer {
public:
    explicit IncantEditor(IncantProcessor& processor);
    ~IncantEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;

private:
    void onCastSpell();
    void onEffectTypeChanged();
    void updateKnobsForEffect();
    void drawRuneCircle(juce::Graphics& g, float cx, float cy, float radius);
    void drawMysticalBackground(juce::Graphics& g);

    IncantProcessor& processor_;

    // Colors - mystical palette
    struct Colors {
        static inline const juce::Colour background{0xFF0d0d12};
        static inline const juce::Colour backgroundLight{0xFF1a1a24};
        static inline const juce::Colour accent{0xFFd4a558};       // Gold
        static inline const juce::Colour accentGlow{0xFFf4c878};
        static inline const juce::Colour purple{0xFF6b4c9a};
        static inline const juce::Colour purpleDark{0xFF2a1f3d};
        static inline const juce::Colour text{0xFFe8e4dc};
        static inline const juce::Colour textDim{0xFF8a8680};
        static inline const juce::Colour success{0xFF7cb87c};
        static inline const juce::Colour meter{0xFF4a9f7f};
    };

    // UI Components
    juce::Label titleLabel_;
    juce::Label subtitleLabel_;

    juce::ComboBox effectSelector_;
    juce::ComboBox presetSelector_;

    juce::TextEditor incantationInput_;
    juce::TextButton castButton_;
    juce::Label statusLabel_;

    // Knobs
    static constexpr int NUM_KNOBS = 5;
    std::array<MysticalKnob, NUM_KNOBS> knobs_;
    std::array<juce::Label, NUM_KNOBS> knobLabels_;
    std::array<juce::Label, NUM_KNOBS> knobValueLabels_;

    // Meters
    LevelMeter inputMeter_;
    LevelMeter outputMeter_;
    juce::Label inputLabel_;
    juce::Label outputLabel_;

    // Animation state
    float backgroundPhase_ = 0.0f;
    bool isGenerating_ = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(IncantEditor)
};

} // namespace incant
