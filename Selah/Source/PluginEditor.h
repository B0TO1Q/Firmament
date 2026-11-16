#pragma once

#include "PluginProcessor.h"

namespace hw
{
const juce::Colour rack     { 0xff0b0b0c };
const juce::Colour gold     { 0xffd4ac62 };
const juce::Colour cream    { 0xffece3cb };
const juce::Colour amber    { 0xfff2a33a };
const juce::Colour engrave  { 0xffb9b6ae };
const juce::Colour faint    { 0xff85837d };

juce::Font font (float height, bool bold = true, float kerning = 0.06f);

// Cream pointer knob with a printed 0-10 scale.
class Look final : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
};

// Ivory push-button with an icon for each kind of source; lights amber when selected.
class SourceButton final : public juce::Button
{
public:
    explicit SourceButton (int sourceIndex);
    void paintButton (juce::Graphics&, bool over, bool down) override;

private:
    int index;
};

// Metal bat-handle toggle switch: up = on.
class ToggleSwitch final : public juce::Button
{
public:
    ToggleSwitch();
    void paintButton (juce::Graphics&, bool over, bool down) override;
};
}

class SelahEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit SelahEditor (SelahProcessor&);
    ~SelahEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;

    void timerCallback() override;
    void sourceChanged (int);
    void setAdvanced (bool);
    void renderBackground();
    void drawFace (juce::Graphics&);

    SelahProcessor& proc;
    hw::Look look;

    juce::OwnedArray<hw::SourceButton> sourceButtons;
    juce::Slider knobs[6], revLength;
    hw::ToggleSwitch advancedSwitch, limiterSwitch;

    juce::OwnedArray<SliderAtt> sliderAtts;
    std::unique_ptr<ButtonAtt> limiterAtt;
    std::unique_ptr<juce::ParameterAttachment> sourceAtt;

    int source = 0;
    bool advanced = false;
    float level = 0.0f, gr = 0.0f, limiting = 0.0f, needleLevel = 0.0f, needleGr = 0.0f;

    juce::Image background;
    juce::Rectangle<int> face, inset, ledBox, grBox, lampBox, drawer, techBox;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SelahEditor)
};
