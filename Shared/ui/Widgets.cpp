#include "Widgets.h"

namespace ui
{
juce::Font font (float height, bool bold, float kerning)
{
    return juce::Font (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain))
        .withExtraKerningFactor (kerning);
}

void Look::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                             float startAngle, float endAngle, juce::Slider& s)
{
    const auto area = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (4.0f);
    const float r = juce::jmin (area.getWidth(), area.getHeight()) * 0.5f;
    const auto c = area.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);
    const float alpha = s.isEnabled() ? 1.0f : 0.3f;

    // range ticks, like a compass bezel
    const int ticks = 11;
    for (int i = 0; i < ticks; ++i)
    {
        const float a = startAngle + (endAngle - startAngle) * (float) i / (float) (ticks - 1);
        const bool major = i % 5 == 0;
        const auto p1 = c.getPointOnCircumference (r - (major ? 7.0f : 4.0f), a);
        const auto p2 = c.getPointOnCircumference (r, a);
        g.setColour ((major ? bone : dim).withAlpha (alpha * (major ? 0.9f : 0.6f)));
        g.drawLine ({ p1, p2 }, major ? 1.6f : 1.0f);
    }

    const float bodyR = r - 11.0f;
    g.setColour (panel.brighter (0.06f).withAlpha (alpha));
    g.fillEllipse (c.x - bodyR, c.y - bodyR, bodyR * 2.0f, bodyR * 2.0f);
    g.setColour (line.withAlpha (alpha));
    g.drawEllipse (c.x - bodyR, c.y - bodyR, bodyR * 2.0f, bodyR * 2.0f, 1.5f);

    juce::Path arc;
    arc.addCentredArc (c.x, c.y, bodyR + 3.5f, bodyR + 3.5f, 0.0f, startAngle, angle, true);
    g.setColour (accent.withAlpha (alpha));
    g.strokePath (arc, juce::PathStrokeType (2.5f));

    const auto tip = c.getPointOnCircumference (bodyR - 3.0f, angle);
    const auto tail = c.getPointOnCircumference (bodyR * 0.35f, angle);
    g.setColour (bone.withAlpha (alpha));
    g.drawLine ({ tail, tip }, 2.5f);
}

void Look::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    const auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState();
    if (on)
    {
        g.setColour (accent.withMultipliedBrightness (down ? 0.85f : 1.0f));
        g.fillRect (r);
    }
    else
    {
        g.setColour (panel.brighter (over ? 0.08f : 0.0f));
        g.fillRect (r);
        g.setColour (over ? dim : line);
        g.drawRect (r, 1.0f);
    }
}

void Look::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    const bool on = b.getToggleState();
    g.setColour (on ? bg : (b.isEnabled() ? bone.withAlpha (0.8f) : dim));
    g.setFont (font (juce::jmin (13.0f, (float) b.getHeight() * 0.36f), true, 0.1f));
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (4, 2), juce::Justification::centred, 2, 0.85f);
}

Knob::Knob (const juce::String& labelText) : label (labelText)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    slider.onValueChange = [this] { repaint(); };
    addAndMakeVisible (slider);
}

void Knob::paint (juce::Graphics& g)
{
    const float a = isEnabled() ? 1.0f : 0.3f;
    auto r = getLocalBounds();
    g.setColour (bone.withAlpha (a));
    g.setFont (font (12.0f));
    g.drawFittedText (label, r.removeFromTop (18), juce::Justification::centred, 1);
    g.setColour (accent.withAlpha (a));
    g.setFont (font (12.0f, false, 0.05f));
    g.drawFittedText (slider.getTextFromValue (slider.getValue()), r.removeFromBottom (16), juce::Justification::centred, 1);
}

void Knob::resized()
{
    slider.setBounds (getLocalBounds().withTrimmedTop (18).withTrimmedBottom (16));
}
}
