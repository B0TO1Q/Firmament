#pragma once

#include "PluginProcessor.h"

namespace fw
{
const juce::Colour rack   { 0xff0b0b0c };
const juce::Colour navy   { 0xff1b2a4a };
const juce::Colour ink    { 0xff22314f };
const juce::Colour sky    { 0xff4f8fe0 };
const juce::Colour faint  { 0xff6b7790 };

juce::Font font (float height, bool bold = true, float kerning = 0.06f);

// Black bakelite pointer knob with a printed scale.
class Look final : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
};

// Black push-key with a printed legend; glows blue when on.
class Key final : public juce::Button
{
public:
    Key (const juce::String& legend, const juce::String& sub = {}) : juce::Button (legend), subLegend (sub) {}
    void paintButton (juce::Graphics&, bool over, bool down) override;

private:
    juce::String subLegend;
};
}

class FirmamentEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit FirmamentEditor (FirmamentProcessor&);
    ~FirmamentEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;

    void timerCallback() override;
    void modeChanged (int newMode);
    void renderBackground();
    void drawFace (juce::Graphics&);

    FirmamentProcessor& proc;
    fw::Look look;

    juce::OwnedArray<fw::Key> modeKeys;
    juce::Slider knobs[6], ladder[3];
    fw::Key freeze { "FREEZE" };

    juce::OwnedArray<SliderAtt> sliderAtts;
    std::unique_ptr<ButtonAtt> freezeAtt;
    std::unique_ptr<juce::ParameterAttachment> modeAtt;

    int mode = 1;
    float needle = 0.0f;
    bool frozenLamp = false;

    juce::Image background;
    juce::Rectangle<int> face, panel, ladderBox, vuBox, lampBox;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FirmamentEditor)
};
