// Look and controls shared by every plugin in this repo.
#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

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
