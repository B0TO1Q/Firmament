#include "PluginEditor.h"

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

//==============================================================================
FirmamentEditor::FirmamentEditor (FirmamentProcessor& p) : AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&look);

    for (int i = 0; i < fm::kNumModes; ++i)
    {
        auto* b = modeButtons.add (new juce::TextButton (juce::String::formatted ("%02d\n", i + 1) + fm::modeName (i)));
        b->setRadioGroupId (1);
        b->onClick = [this, i] { if (modeAtt != nullptr) modeAtt->setValueAsCompleteGesture ((float) i); };
        addAndMakeVisible (b);
    }

    const auto attach = [this] (ui::Knob& k, const char* id)
    {
        addAndMakeVisible (k);
        sliderAtts.add (new SliderAtt (proc.apvts, id, k.slider));
    };
    attach (decay, "decay");
    attach (preDelay, "predelay");
    attach (tone, "tone");
    attach (mod, "mod");
    attach (character, "character");
    attach (mix, "mix");
    attach (rise, "rise");
    attach (fall, "fall");
    attach (fine, "fine");

    // Show DECAY as real seconds for the current mode.
    decay.slider.textFromValueFunction = [this] (double v)
    {
        const float sec = fm::decaySeconds (mode, (float) v);
        return (sec < 10.0f ? juce::String (sec, 1) : juce::String (juce::roundToInt (sec))) + " s";
    };

    freeze.setClickingTogglesState (true);
    addAndMakeVisible (freeze);
    freezeAtt = std::make_unique<ButtonAtt> (proc.apvts, "freeze", freeze);

    modeAtt = std::make_unique<juce::ParameterAttachment> (*proc.apvts.getParameter ("mode"),
                                                           [this] (float v) { modeChanged (juce::roundToInt (v)); });
    modeAtt->sendInitialUpdate();

    setSize (780, 470);
}

FirmamentEditor::~FirmamentEditor()
{
    setLookAndFeel (nullptr);
}

void FirmamentEditor::modeChanged (int newMode)
{
    mode = juce::jlimit (0, fm::kNumModes - 1, newMode);
    for (int i = 0; i < modeButtons.size(); ++i)
        modeButtons[i]->setToggleState (i == mode, juce::dontSendNotification);

    character.setLabel (fm::characterName (mode));
    const bool isLadder = mode == fm::Ladder;
    for (auto* k : { &rise, &fall, &fine })
        k->setEnabled (isLadder);
    decay.repaint();
    repaint();
}

void FirmamentEditor::paint (juce::Graphics& g)
{
    using namespace ui;
    g.fillAll (bg);

    // header
    g.setColour (bone);
    g.setFont (font (34.0f, true, 0.22f));
    g.drawText ("FIRMAMENT", 24, 14, 420, 38, juce::Justification::centredLeft);
    g.setColour (dim);
    g.setFont (font (11.0f, false, 0.2f));
    g.drawText ("SEVEN-MODE REVERB  //  V0.1", 26, 52, 420, 14, juce::Justification::centredLeft);

    const juce::Rectangle<int> tag (getWidth() - 150, 20, 126, 40);
    g.setColour (line);
    g.drawRect (tag, 1);
    g.setColour (accent);
    g.setFont (font (11.0f));
    g.drawText ("MODE " + juce::String::formatted ("%02d", mode + 1), tag.withTrimmedBottom (20), juce::Justification::centred);
    g.setColour (bone);
    g.setFont (font (13.0f));
    g.drawText (fm::modeName (mode), tag.withTrimmedTop (18), juce::Justification::centred);

    g.setColour (line);
    g.fillRect (24, 76, getWidth() - 48, 1);
    g.fillRect (24, 318, getWidth() - 48, 1);

    // Ladder fine-tune box
    const bool isLadder = mode == fm::Ladder;
    g.setColour (isLadder ? accent.withAlpha (0.7f) : line);
    g.drawRect (ladderBox, 1);
    g.setColour (isLadder ? accent : dim.withAlpha (0.5f));
    g.setFont (font (10.0f));
    const juce::String title = "  LADDER FINE TUNE  ";
    const int tw = 150;
    g.setColour (bg);
    g.fillRect (ladderBox.getX() + 12, ladderBox.getY() - 6, tw, 12);
    g.setColour (isLadder ? accent : dim.withAlpha (0.5f));
    g.drawText (title, ladderBox.getX() + 12, ladderBox.getY() - 7, tw, 14, juce::Justification::centred);
}

void FirmamentEditor::resized()
{
    auto area = getLocalBounds().reduced (24, 0);

    auto modes = area.withY (88).withHeight (44);
    const int gap = 6;
    const int bw = (modes.getWidth() - gap * (fm::kNumModes - 1)) / fm::kNumModes;
    for (int i = 0; i < modeButtons.size(); ++i)
        modeButtons[i]->setBounds (modes.getX() + i * (bw + gap), modes.getY(), bw, modes.getHeight());

    auto row = area.withY (150).withHeight (156);
    ui::Knob* main[] = { &decay, &preDelay, &tone, &mod, &character, &mix };
    const int kw = row.getWidth() / 6;
    for (int i = 0; i < 6; ++i)
        main[i]->setBounds (row.getX() + i * kw, row.getY(), kw, row.getHeight());

    auto bottom = area.withY (338).withHeight (112);
    ladderBox = bottom.removeFromLeft (430);
    auto lk = ladderBox.reduced (10, 8).withTrimmedTop (4);
    ui::Knob* sub[] = { &rise, &fall, &fine };
    const int sw = lk.getWidth() / 3;
    for (int i = 0; i < 3; ++i)
        sub[i]->setBounds (lk.getX() + i * sw, lk.getY(), sw, lk.getHeight());

    freeze.setBounds (bottom.withTrimmedLeft (24).reduced (0, 18));
}
