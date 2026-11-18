#pragma once

#include "PluginProcessor.h"

namespace st
{
const juce::Colour space  { 0xff0a0b18 };
const juce::Colour gold   { 0xffe2b866 };
const juce::Colour silver { 0xffd6d8e4 };
const juce::Colour violet { 0xffa36bff };
const juce::Colour dim    { 0xff8a88a8 };

juce::Font font (float height, bool bold = true, float kerning = 0.06f);

// Chrome knob with a glowing violet ring; bipolar sliders glow out from the centre.
class Look final : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
};

// Dark key with a legend; glows violet when on.
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
