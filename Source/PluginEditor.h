#pragma once

#include "PluginProcessor.h"

namespace ui
{
const juce::Colour bg      { 0xff12140e };
const juce::Colour panel   { 0xff1b1e15 };
const juce::Colour line    { 0xff3a402c };
const juce::Colour bone    { 0xffe6e0d2 };
const juce::Colour dim     { 0xff8b8a78 };
const juce::Colour accent  { 0xffd9912b };

juce::Font font (float height, bool bold = true, float kerning = 0.12f);

class Look final : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool over, bool down) override;
};

// Rotary dial with a stencil label above and a live value readout below.
class Knob final : public juce::Component
{
public:
    explicit Knob (const juce::String& labelText);
    void setLabel (const juce::String& t) { label = t; repaint(); }
    void paint (juce::Graphics&) override;
    void resized() override;

    juce::Slider slider;

private:
    juce::String label;
};
}

class FirmamentEditor final : public juce::AudioProcessorEditor
{
public:
    explicit FirmamentEditor (FirmamentProcessor&);
    ~FirmamentEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;

    void modeChanged (int newMode);

    FirmamentProcessor& proc;
    ui::Look look;

    juce::OwnedArray<juce::TextButton> modeButtons;
    ui::Knob decay { "DECAY" }, preDelay { "PRE-DELAY" }, tone { "TONE" }, mod { "MOD" },
             character { "CHARACTER" }, mix { "MIX" };
    ui::Knob rise { "RISE  +OCT" }, fall { "FALL  -OCT" }, fine { "FINE" };
    juce::TextButton freeze { "FREEZE" };

    juce::OwnedArray<SliderAtt> sliderAtts;
    std::unique_ptr<ButtonAtt> freezeAtt;
    std::unique_ptr<juce::ParameterAttachment> modeAtt;

    int mode = 1;
    juce::Rectangle<int> ladderBox;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FirmamentEditor)
};
