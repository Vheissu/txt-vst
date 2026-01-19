#include "PluginEditor.h"
#include <cmath>

namespace incant {

//==============================================================================
// MysticalKnob Implementation
//==============================================================================

MysticalKnob::MysticalKnob() {
    setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    setRange(0.0, 1.0, 0.001);
}

void MysticalKnob::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds().toFloat().reduced(4.0f);
    auto radius = std::min(bounds.getWidth(), bounds.getHeight()) / 2.0f;
    auto cx = bounds.getCentreX();
    auto cy = bounds.getCentreY();

    auto value = static_cast<float>(getValue());
    auto startAngle = juce::MathConstants<float>::pi * 1.25f;
    auto endAngle = juce::MathConstants<float>::pi * 2.75f;
    auto angle = startAngle + value * (endAngle - startAngle);

    // Outer glow
    juce::ColourGradient glowGradient(
        juce::Colour(0x40d4a558), cx, cy,
        juce::Colour(0x00d4a558), cx - radius * 1.2f, cy,
        true
    );
    g.setGradientFill(glowGradient);
    g.fillEllipse(bounds.expanded(8.0f));

    // Background circle
    g.setColour(juce::Colour(0xFF1a1a24));
    g.fillEllipse(cx - radius, cy - radius, radius * 2.0f, radius * 2.0f);

    // Outer ring
    g.setColour(juce::Colour(0xFF2a2a38));
    g.drawEllipse(cx - radius, cy - radius, radius * 2.0f, radius * 2.0f, 2.0f);

    // Track (unfilled portion)
    juce::Path trackPath;
    trackPath.addCentredArc(cx, cy, radius * 0.75f, radius * 0.75f,
                            0.0f, startAngle, endAngle, true);
    g.setColour(juce::Colour(0xFF3a3a4a));
    g.strokePath(trackPath, juce::PathStrokeType(4.0f, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));

    // Value arc (filled portion) - gold gradient
    if (value > 0.001f) {
        juce::Path valuePath;
        valuePath.addCentredArc(cx, cy, radius * 0.75f, radius * 0.75f,
                                0.0f, startAngle, angle, true);

        juce::ColourGradient arcGradient(
            juce::Colour(0xFFd4a558), cx, cy - radius,
            juce::Colour(0xFFf4c878), cx, cy + radius,
            false
        );
        g.setGradientFill(arcGradient);
        g.strokePath(valuePath, juce::PathStrokeType(4.0f, juce::PathStrokeType::curved,
                                                      juce::PathStrokeType::rounded));
    }

    // Rune tick marks
    g.setColour(juce::Colour(0xFF6b4c9a));
    for (int i = 0; i <= 10; ++i) {
        float tickAngle = startAngle + (static_cast<float>(i) / 10.0f) * (endAngle - startAngle);
        float innerR = radius * 0.55f;
        float outerR = radius * 0.62f;

        float x1 = cx + innerR * std::cos(tickAngle);
        float y1 = cy + innerR * std::sin(tickAngle);
        float x2 = cx + outerR * std::cos(tickAngle);
        float y2 = cy + outerR * std::sin(tickAngle);

        g.drawLine(x1, y1, x2, y2, 1.5f);
    }

    // Pointer indicator
    float pointerLength = radius * 0.5f;
    float px = cx + pointerLength * std::cos(angle);
    float py = cy + pointerLength * std::sin(angle);

    g.setColour(juce::Colour(0xFFf4c878));
    g.drawLine(cx, cy, px, py, 3.0f);

    // Center dot
    g.setColour(juce::Colour(0xFFd4a558));
    g.fillEllipse(cx - 4.0f, cy - 4.0f, 8.0f, 8.0f);
}

//==============================================================================
// LevelMeter Implementation
//==============================================================================

LevelMeter::LevelMeter() {
    startTimerHz(30);
}

void LevelMeter::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds().toFloat().reduced(2.0f);

    // Background
    g.setColour(juce::Colour(0xFF1a1a24));
    g.fillRoundedRectangle(bounds, 4.0f);

    // Border
    g.setColour(juce::Colour(0xFF2a2a38));
    g.drawRoundedRectangle(bounds, 4.0f, 1.0f);

    // Level bar
    float levelHeight = bounds.getHeight() * currentLevel_;
    if (levelHeight > 0.0f) {
        juce::Rectangle<float> levelRect(
            bounds.getX() + 2.0f,
            bounds.getBottom() - levelHeight - 2.0f,
            bounds.getWidth() - 4.0f,
            levelHeight
        );

        // Gradient from green to gold to red
        juce::ColourGradient levelGradient(
            juce::Colour(0xFF4a9f7f), levelRect.getX(), levelRect.getBottom(),
            juce::Colour(0xFFd4a558), levelRect.getX(), levelRect.getY(),
            false
        );
        if (currentLevel_ > 0.7f) {
            levelGradient.addColour(0.7, juce::Colour(0xFFd4a558));
            levelGradient.addColour(1.0, juce::Colour(0xFFc45c5c));
        }

        g.setGradientFill(levelGradient);
        g.fillRoundedRectangle(levelRect, 2.0f);

        // Glow effect
        g.setColour(juce::Colour(0x30f4c878));
        g.fillRoundedRectangle(levelRect.expanded(2.0f, 0.0f), 3.0f);
    }

    // Peak indicator
    if (peakLevel_ > 0.01f) {
        float peakY = bounds.getBottom() - bounds.getHeight() * peakLevel_ - 2.0f;
        g.setColour(peakLevel_ > 0.9f ? juce::Colour(0xFFc45c5c) : juce::Colour(0xFFf4c878));
        g.fillRect(bounds.getX() + 2.0f, peakY, bounds.getWidth() - 4.0f, 2.0f);
    }
}

void LevelMeter::timerCallback() {
    // Smooth decay
    currentLevel_ += (targetLevel_ - currentLevel_) * 0.3f;

    // Peak hold and decay
    if (targetLevel_ > peakLevel_) {
        peakLevel_ = targetLevel_;
        peakHoldCounter_ = 30; // Hold for ~1 second
    } else if (peakHoldCounter_ > 0) {
        --peakHoldCounter_;
    } else {
        peakLevel_ *= 0.95f;
    }

    repaint();
}

void LevelMeter::setLevel(float level) {
    targetLevel_ = juce::jlimit(0.0f, 1.0f, level);
}

//==============================================================================
// IncantEditor Implementation
//==============================================================================

IncantEditor::IncantEditor(IncantProcessor& processor)
    : AudioProcessorEditor(processor), processor_(processor)
{
    setSize(700, 550);
    startTimerHz(30);

    // Title
    titleLabel_.setText("INCANT", juce::dontSendNotification);
    titleLabel_.setFont(juce::FontOptions(36.0f).withStyle("Bold"));
    titleLabel_.setColour(juce::Label::textColourId, Colors::accent);
    titleLabel_.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(titleLabel_);

    // Subtitle
    subtitleLabel_.setText("speak your sound into existence", juce::dontSendNotification);
    subtitleLabel_.setFont(juce::FontOptions(12.0f).withStyle("Italic"));
    subtitleLabel_.setColour(juce::Label::textColourId, Colors::textDim);
    subtitleLabel_.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(subtitleLabel_);

    // Effect selector
    effectSelector_.addItem("Equalizer", 1);
    effectSelector_.addItem("Compressor", 2);
    effectSelector_.addItem("Reverb", 3);
    effectSelector_.addItem("Distortion", 4);
    effectSelector_.addItem("Delay", 5);
    effectSelector_.addItem("Glitch", 6);
    effectSelector_.addItem("Overdrive", 7);
    effectSelector_.addItem("Chorus", 8);
    effectSelector_.addItem("Phaser", 9);
    effectSelector_.addItem("Tremolo", 10);
    effectSelector_.addItem("Filter", 11);
    effectSelector_.setSelectedId(static_cast<int>(processor_.getEffectType()) + 1);
    effectSelector_.onChange = [this] { onEffectTypeChanged(); };
    effectSelector_.setColour(juce::ComboBox::backgroundColourId, Colors::backgroundLight);
    effectSelector_.setColour(juce::ComboBox::textColourId, Colors::text);
    effectSelector_.setColour(juce::ComboBox::outlineColourId, Colors::purple);
    addAndMakeVisible(effectSelector_);

    // Preset selector
    presetSelector_.addItem("-- Factory Presets --", 1);
    auto& presets = processor_.getPresetManager().getFactoryPresets();
    int id = 2;
    for (const auto& preset : presets) {
        presetSelector_.addItem(preset.name, id++);
    }
    presetSelector_.setSelectedId(1);
    presetSelector_.onChange = [this] {
        int sel = presetSelector_.getSelectedId();
        if (sel > 1) {
            auto& presets = processor_.getPresetManager().getFactoryPresets();
            if (sel - 2 < static_cast<int>(presets.size())) {
                processor_.getPresetManager().loadPreset(presets[static_cast<size_t>(sel - 2)], processor_);
                effectSelector_.setSelectedId(static_cast<int>(processor_.getEffectType()) + 1, juce::dontSendNotification);
                updateKnobsForEffect();
            }
        }
    };
    presetSelector_.setColour(juce::ComboBox::backgroundColourId, Colors::backgroundLight);
    presetSelector_.setColour(juce::ComboBox::textColourId, Colors::text);
    presetSelector_.setColour(juce::ComboBox::outlineColourId, Colors::purple);
    addAndMakeVisible(presetSelector_);

    // Incantation input
    incantationInput_.setMultiLine(false);
    incantationInput_.setReturnKeyStartsNewLine(false);
    incantationInput_.setTextToShowWhenEmpty("Type your incantation here...", Colors::textDim);
    incantationInput_.setColour(juce::TextEditor::backgroundColourId, Colors::backgroundLight);
    incantationInput_.setColour(juce::TextEditor::textColourId, Colors::text);
    incantationInput_.setColour(juce::TextEditor::outlineColourId, Colors::purple);
    incantationInput_.setColour(juce::TextEditor::focusedOutlineColourId, Colors::accent);
    incantationInput_.setFont(juce::FontOptions(16.0f));
    incantationInput_.onReturnKey = [this] { onCastSpell(); };
    addAndMakeVisible(incantationInput_);

    // Cast button
    castButton_.setButtonText("CAST");
    castButton_.onClick = [this] { onCastSpell(); };
    castButton_.setColour(juce::TextButton::buttonColourId, Colors::purple);
    castButton_.setColour(juce::TextButton::buttonOnColourId, Colors::accent);
    castButton_.setColour(juce::TextButton::textColourOffId, Colors::text);
    addAndMakeVisible(castButton_);

    // Status label
    statusLabel_.setText("Ready to cast", juce::dontSendNotification);
    statusLabel_.setFont(juce::FontOptions(12.0f));
    statusLabel_.setColour(juce::Label::textColourId, Colors::success);
    statusLabel_.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(statusLabel_);

    // Knobs
    for (int i = 0; i < NUM_KNOBS; ++i) {
        addAndMakeVisible(knobs_[static_cast<size_t>(i)]);
        knobs_[static_cast<size_t>(i)].onValueChange = [this, i] {
            auto* effect = processor_.getCurrentEffect();
            if (effect && i < effect->getNumParameters()) {
                effect->setParameter(i, static_cast<float>(knobs_[static_cast<size_t>(i)].getValue()));
            }
        };

        knobLabels_[static_cast<size_t>(i)].setFont(juce::FontOptions(11.0f));
        knobLabels_[static_cast<size_t>(i)].setColour(juce::Label::textColourId, Colors::textDim);
        knobLabels_[static_cast<size_t>(i)].setJustificationType(juce::Justification::centred);
        addAndMakeVisible(knobLabels_[static_cast<size_t>(i)]);

        knobValueLabels_[static_cast<size_t>(i)].setFont(juce::FontOptions(10.0f));
        knobValueLabels_[static_cast<size_t>(i)].setColour(juce::Label::textColourId, Colors::accent);
        knobValueLabels_[static_cast<size_t>(i)].setJustificationType(juce::Justification::centred);
        addAndMakeVisible(knobValueLabels_[static_cast<size_t>(i)]);
    }

    // Meters
    addAndMakeVisible(inputMeter_);
    addAndMakeVisible(outputMeter_);

    inputLabel_.setText("IN", juce::dontSendNotification);
    inputLabel_.setFont(juce::FontOptions(10.0f));
    inputLabel_.setColour(juce::Label::textColourId, Colors::textDim);
    inputLabel_.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(inputLabel_);

    outputLabel_.setText("OUT", juce::dontSendNotification);
    outputLabel_.setFont(juce::FontOptions(10.0f));
    outputLabel_.setColour(juce::Label::textColourId, Colors::textDim);
    outputLabel_.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(outputLabel_);

    updateKnobsForEffect();
}

IncantEditor::~IncantEditor() {
    stopTimer();
}

void IncantEditor::paint(juce::Graphics& g) {
    drawMysticalBackground(g);

    // Decorative rune circle
    auto bounds = getLocalBounds().toFloat();
    drawRuneCircle(g, bounds.getCentreX(), bounds.getHeight() * 0.65f, 180.0f);
}

void IncantEditor::drawMysticalBackground(juce::Graphics& g) {
    auto bounds = getLocalBounds().toFloat();

    // Base gradient
    juce::ColourGradient bgGradient(
        Colors::background, bounds.getCentreX(), 0.0f,
        Colors::purpleDark, bounds.getCentreX(), bounds.getHeight(),
        false
    );
    g.setGradientFill(bgGradient);
    g.fillRect(bounds);

    // Subtle radial glow from center
    float glowPhase = backgroundPhase_ * 0.5f;
    float glowIntensity = 0.15f + 0.05f * std::sin(glowPhase);

    juce::ColourGradient centerGlow(
        Colors::purple.withAlpha(glowIntensity), bounds.getCentreX(), bounds.getHeight() * 0.6f,
        juce::Colours::transparentBlack, bounds.getCentreX(), 0.0f,
        true
    );
    g.setGradientFill(centerGlow);
    g.fillRect(bounds);

    // Subtle noise texture (simulated with circles)
    g.setColour(juce::Colour(0x08ffffff));
    juce::Random rand(42);
    for (int i = 0; i < 100; ++i) {
        float x = rand.nextFloat() * bounds.getWidth();
        float y = rand.nextFloat() * bounds.getHeight();
        float size = rand.nextFloat() * 2.0f + 0.5f;
        g.fillEllipse(x, y, size, size);
    }
}

void IncantEditor::drawRuneCircle(juce::Graphics& g, float cx, float cy, float radius) {
    float phase = backgroundPhase_;

    // Outer circle
    g.setColour(Colors::purple.withAlpha(0.3f));
    g.drawEllipse(cx - radius, cy - radius, radius * 2.0f, radius * 2.0f, 1.5f);

    // Inner circle
    float innerRadius = radius * 0.7f;
    g.setColour(Colors::purple.withAlpha(0.2f));
    g.drawEllipse(cx - innerRadius, cy - innerRadius, innerRadius * 2.0f, innerRadius * 2.0f, 1.0f);

    // Rotating rune marks
    g.setColour(Colors::accent.withAlpha(0.4f));
    int numMarks = 12;
    for (int i = 0; i < numMarks; ++i) {
        float angle = phase * 0.2f + (static_cast<float>(i) / numMarks) * juce::MathConstants<float>::twoPi;
        float innerR = radius * 0.72f;
        float outerR = radius * 0.98f;

        float x1 = cx + innerR * std::cos(angle);
        float y1 = cy + innerR * std::sin(angle);
        float x2 = cx + outerR * std::cos(angle);
        float y2 = cy + outerR * std::sin(angle);

        g.drawLine(x1, y1, x2, y2, 2.0f);
    }

    // Decorative symbols at cardinal points
    g.setColour(Colors::accent.withAlpha(0.6f));
    float symbolRadius = radius * 0.85f;
    for (int i = 0; i < 4; ++i) {
        float angle = -phase * 0.1f + (static_cast<float>(i) / 4.0f) * juce::MathConstants<float>::twoPi;
        float sx = cx + symbolRadius * std::cos(angle);
        float sy = cy + symbolRadius * std::sin(angle);

        // Small diamond shape
        juce::Path diamond;
        diamond.startNewSubPath(sx, sy - 6.0f);
        diamond.lineTo(sx + 4.0f, sy);
        diamond.lineTo(sx, sy + 6.0f);
        diamond.lineTo(sx - 4.0f, sy);
        diamond.closeSubPath();
        g.strokePath(diamond, juce::PathStrokeType(1.5f));
    }
}

void IncantEditor::resized() {
    auto bounds = getLocalBounds().reduced(20);

    // Title area
    titleLabel_.setBounds(bounds.removeFromTop(45));
    subtitleLabel_.setBounds(bounds.removeFromTop(20));
    bounds.removeFromTop(15);

    // Selectors row
    auto selectorRow = bounds.removeFromTop(30);
    effectSelector_.setBounds(selectorRow.removeFromLeft(150));
    selectorRow.removeFromLeft(20);
    presetSelector_.setBounds(selectorRow.removeFromLeft(200));
    bounds.removeFromTop(20);

    // Incantation input row
    auto inputRow = bounds.removeFromTop(40);
    castButton_.setBounds(inputRow.removeFromRight(80));
    inputRow.removeFromRight(10);
    incantationInput_.setBounds(inputRow);

    // Status
    statusLabel_.setBounds(bounds.removeFromTop(25));
    bounds.removeFromTop(20);

    // Meters on the sides
    auto meterWidth = 25;
    auto meterArea = bounds.reduced(0, 20);

    auto leftMeterArea = meterArea.removeFromLeft(meterWidth + 20);
    inputLabel_.setBounds(leftMeterArea.removeFromTop(15));
    inputMeter_.setBounds(leftMeterArea.reduced(10, 0));

    auto rightMeterArea = meterArea.removeFromRight(meterWidth + 20);
    outputLabel_.setBounds(rightMeterArea.removeFromTop(15));
    outputMeter_.setBounds(rightMeterArea.reduced(10, 0));

    // Knobs in center
    auto knobArea = meterArea.reduced(30, 30);
    int knobSize = 80;
    int knobSpacing = (knobArea.getWidth() - knobSize * NUM_KNOBS) / (NUM_KNOBS - 1);

    auto* effect = processor_.getCurrentEffect();
    int numParams = effect ? effect->getNumParameters() : 0;

    int startX = knobArea.getX() + (knobArea.getWidth() - (numParams * knobSize + (numParams - 1) * knobSpacing)) / 2;

    for (int i = 0; i < NUM_KNOBS; ++i) {
        bool visible = i < numParams;
        knobs_[static_cast<size_t>(i)].setVisible(visible);
        knobLabels_[static_cast<size_t>(i)].setVisible(visible);
        knobValueLabels_[static_cast<size_t>(i)].setVisible(visible);

        if (visible) {
            int x = startX + i * (knobSize + knobSpacing);
            int y = knobArea.getCentreY() - knobSize / 2 - 10;

            knobs_[static_cast<size_t>(i)].setBounds(x, y, knobSize, knobSize);
            knobLabels_[static_cast<size_t>(i)].setBounds(x, y - 18, knobSize, 16);
            knobValueLabels_[static_cast<size_t>(i)].setBounds(x, y + knobSize + 2, knobSize, 16);
        }
    }
}

void IncantEditor::timerCallback() {
    // Update meters
    inputMeter_.setLevel(processor_.getInputLevel() * 3.0f); // Scale for visibility
    outputMeter_.setLevel(processor_.getOutputLevel() * 3.0f);

    // Update knob value displays
    auto* effect = processor_.getCurrentEffect();
    if (effect) {
        for (int i = 0; i < effect->getNumParameters(); ++i) {
            float value = effect->getParameter(i);
            knobValueLabels_[static_cast<size_t>(i)].setText(
                juce::String(value, 2), juce::dontSendNotification);

            // Sync knob position if it changed externally
            if (std::abs(static_cast<float>(knobs_[static_cast<size_t>(i)].getValue()) - value) > 0.001f) {
                knobs_[static_cast<size_t>(i)].setValue(value, juce::dontSendNotification);
            }
        }
    }

    // Update status based on LLM state
    auto status = processor_.getLLMStatus();
    if (status == LLMEngine::Status::Processing) {
        statusLabel_.setText("Channeling the arcane...", juce::dontSendNotification);
        statusLabel_.setColour(juce::Label::textColourId, Colors::accent);
        isGenerating_ = true;
    } else if (isGenerating_) {
        statusLabel_.setText("Spell complete!", juce::dontSendNotification);
        statusLabel_.setColour(juce::Label::textColourId, Colors::success);
        isGenerating_ = false;
    }

    // Animate background
    backgroundPhase_ += 0.02f;
    repaint();
}

void IncantEditor::onCastSpell() {
    auto text = incantationInput_.getText().toStdString();
    if (!text.empty()) {
        statusLabel_.setText("Casting...", juce::dontSendNotification);
        statusLabel_.setColour(juce::Label::textColourId, Colors::accent);
        processor_.generateFromText(text);
    }
}

void IncantEditor::onEffectTypeChanged() {
    int selected = effectSelector_.getSelectedId();
    if (selected > 0) {
        processor_.setEffectType(static_cast<EffectType>(selected - 1));
        updateKnobsForEffect();
    }
}

void IncantEditor::updateKnobsForEffect() {
    auto* effect = processor_.getCurrentEffect();
    if (!effect) return;

    int numParams = effect->getNumParameters();

    for (int i = 0; i < NUM_KNOBS; ++i) {
        if (i < numParams) {
            knobLabels_[static_cast<size_t>(i)].setText(
                effect->getParameterName(i), juce::dontSendNotification);
            knobs_[static_cast<size_t>(i)].setValue(
                effect->getParameter(i), juce::dontSendNotification);
        }
    }

    resized();
}

} // namespace incant
