#pragma once

#include "PluginProcessor.h"

namespace st
{
const juce::Colour crimson { 0xffa8141f };
const juce::Colour ink     { 0xff17171a };   // silkscreen black
const juce::Colour paper   { 0xfff4f2ee };   // warm white
const juce::Colour grey    { 0xff5d5d63 };

juce::Font font (float height, bool bold = true, float kerning = 0.06f);

// Console knob: crimson cap for the seven effects, black cap for the utilities.
class Look final : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
};

// Black key with a white legend; lights crimson when on.
class Key final : public juce::Button
{
public:
    explicit Key (const juce::String& legend) : juce::Button (legend) {}
    void paintButton (juce::Graphics&, bool over, bool down) override;
};
}

class StardustEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit StardustEditor (StardustProcessor&);
    ~StardustEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;

    void timerCallback() override;
    void renderBackground();
    void drawFace (juce::Graphics&);

    StardustProcessor& proc;
    st::Look look;

    juce::Slider fx[7], filter, mix, output;
    juce::OwnedArray<st::Key> rateKeys, dirKeys;
    st::Key freeze { "FREEZE" };

    juce::OwnedArray<SliderAtt> sliderAtts;
    std::unique_ptr<ButtonAtt> freezeAtt;
    std::unique_ptr<juce::ParameterAttachment> rateAtt, dirAtt;

    float needle = 0.0f;
    int bpmShown = 120;

    juce::Image background;
    juce::Rectangle<int> face, topPanel, bottomPanel, vuBox, bpmBox, lamps[7];

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StardustEditor)
};
