#include "PluginEditor.h"

namespace
{
constexpr int kWidth = 940, kHeightClosed = 524, kDrawerHeight = 140;

const char* const knobNames[6] = { "CLEAN UP", "WARMTH", "CLARITY", "EVEN OUT", "SPACE", "VOLUME" };
const char* const knobHints[6] = {
    "Rumble, hiss,\nharsh S sounds",
    "Body and\nfullness",
    "Less mud,\nmore presence",
    "Keeps the level\nsteady",
    "Room sound\naround it",
    "Final\nloudness",
};
const char* const sourceLabels[sl::kNumSources] = {
    "Pastor\nMic", "Lead\nVocal", "Choir", "Acoustic\nGuitar", "Electric\nGuitar", "Bass", "Keys", "Drums", "Full\nMix"
};

void drawScrew (juce::Graphics& g, float cx, float cy, float r)
{
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillEllipse (cx - r - 1.0f, cy - r, r * 2.0f + 2.0f, r * 2.0f + 2.5f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff8d8f92), cx - r * 0.5f, cy - r * 0.6f,
                                             juce::Colour (0xff2c2d2f), cx + r, cy + r, true));
    g.fillEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f);
    g.setColour (juce::Colour (0xff111112));
    g.drawLine (cx - r * 0.55f, cy - r * 0.55f, cx + r * 0.55f, cy + r * 0.55f, 1.8f);
    g.drawLine (cx - r * 0.55f, cy + r * 0.55f, cx + r * 0.55f, cy - r * 0.55f, 1.8f);
}

// Brushed metal: a vertical gradient with fine horizontal streaks.
void fillBrushed (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour top, juce::Colour bottom, float corner, int seed)
{
    juce::Path clip;
    clip.addRoundedRectangle (r, corner);
    juce::Graphics::ScopedSaveState save (g);
    g.reduceClipRegion (clip);
    g.setGradientFill (juce::ColourGradient (top, 0.0f, r.getY(), bottom, 0.0f, r.getBottom(), false));
    g.fillRect (r);
    juce::Random rnd (seed);
    for (float y = r.getY(); y < r.getBottom(); y += 1.0f)
    {
        const float a = rnd.nextFloat();
        g.setColour ((a > 0.5f ? juce::Colours::white : juce::Colours::black).withAlpha (0.018f + 0.03f * std::abs (a - 0.5f)));
        const float x0 = r.getX() + rnd.nextFloat() * r.getWidth() * 0.3f;
        g.fillRect (x0, y, r.getWidth() * (0.5f + 0.5f * rnd.nextFloat()), 1.0f);
    }
    // soft sheen across the top third
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.07f), 0.0f, r.getY(),
                                             juce::Colours::transparentWhite, 0.0f, r.getY() + r.getHeight() * 0.35f, false));
    g.fillRect (r);
}

void drawLed (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c, bool on)
{
    if (on)
    {
        g.setColour (c.withAlpha (0.22f));
        g.fillRoundedRectangle (r.expanded (3.0f), 4.0f);
        g.setGradientFill (juce::ColourGradient (c.brighter (0.6f), r.getCentreX(), r.getY(), c, r.getCentreX(), r.getBottom(), false));
    }
    else
        g.setColour (c.withMultipliedSaturation (0.5f).withMultipliedBrightness (0.2f));
    g.fillRoundedRectangle (r, 2.0f);
}
}

//==============================================================================
namespace hw
{
juce::Font font (float height, bool bold, float kerning)
{
    return juce::Font (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain))
        .withExtraKerningFactor (kerning);
}

void Look::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                             float startAngle, float endAngle, juce::Slider&)
{
    const auto area = juce::Rectangle<int> (x, y, w, h).toFloat();
    const float r = juce::jmin (area.getWidth(), area.getHeight()) * 0.5f - 2.0f;
    const auto c = area.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);
    const float bodyR = r * 0.70f;

    // scale ticks
    for (int i = 0; i <= 10; ++i)
    {
        const float a = startAngle + (endAngle - startAngle) * (float) i / 10.0f;
        const bool major = i % 5 == 0;
        g.setColour (engrave.withAlpha (major ? 0.9f : 0.45f));
        g.drawLine ({ c.getPointOnCircumference (r - (major ? 5.0f : 3.0f), a), c.getPointOnCircumference (r, a) }, major ? 1.6f : 1.0f);
    }

    // lit value arc
    juce::Path track, arc;
    const float arcR = bodyR + 5.5f;
    track.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    if (pos > 0.004f)
    {
        arc.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, angle, true);
        g.setColour (amber.withAlpha (0.22f));
        g.strokePath (arc, juce::PathStrokeType (8.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (amber);
        g.strokePath (arc, juce::PathStrokeType (2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // drop shadow
    g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.65f), c.x, c.y + bodyR * 0.3f,
                                             juce::Colours::transparentBlack, c.x, c.y + bodyR * 1.55f, true));
    g.fillEllipse (c.x - bodyR * 1.3f, c.y - bodyR * 0.9f, bodyR * 2.6f, bodyR * 2.6f);

    // knurled skirt
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff5a5c5f), c.x, c.y - bodyR, juce::Colour (0xff17181a), c.x, c.y + bodyR, false));
    g.fillEllipse (c.x - bodyR, c.y - bodyR, bodyR * 2.0f, bodyR * 2.0f);
    for (int i = 0; i < 48; ++i)
    {
        const float a = angle + juce::MathConstants<float>::twoPi * (float) i / 48.0f;
        g.setColour ((i % 2 == 0 ? juce::Colours::black : juce::Colours::white).withAlpha (i % 2 == 0 ? 0.45f : 0.10f));
        g.drawLine ({ c.getPointOnCircumference (bodyR * 0.80f, a), c.getPointOnCircumference (bodyR * 0.99f, a) }, 1.2f);
    }
    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.drawEllipse (c.x - bodyR, c.y - bodyR, bodyR * 2.0f, bodyR * 2.0f, 1.2f);

    // brushed cap
    const float capR = bodyR * 0.78f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff8a8c8f), c.x - capR * 0.6f, c.y - capR * 0.8f,
                                             juce::Colour (0xff2b2c2e), c.x + capR * 0.7f, c.y + capR, false));
    g.fillEllipse (c.x - capR, c.y - capR, capR * 2.0f, capR * 2.0f);
    g.setColour (juce::Colours::white.withAlpha (0.18f));
    g.drawEllipse (c.x - capR, c.y - capR, capR * 2.0f, capR * 2.0f, 1.0f);
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.drawEllipse (c.x - capR * 0.55f, c.y - capR * 0.55f, capR * 1.1f, capR * 1.1f, 0.8f);

    // pointer
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawLine ({ c.getPointOnCircumference (capR * 0.35f, angle), c.getPointOnCircumference (bodyR * 0.96f, angle) }, 5.0f);
    g.setColour (amber);
    g.drawLine ({ c.getPointOnCircumference (capR * 0.40f, angle), c.getPointOnCircumference (bodyR * 0.94f, angle) }, 3.0f);
}

//------------------------------------------------------------------------------
SourceButton::SourceButton (int sourceIndex) : juce::Button (sl::sourceName (sourceIndex)), index (sourceIndex) {}

void SourceButton::paintButton (juce::Graphics& g, bool over, bool down)
{
    const bool on = getToggleState();
    auto r = getLocalBounds().toFloat().reduced (5.0f);
    if (down) r = r.reduced (1.0f);

    // well the button sits in
    g.setColour (juce::Colours::black.withAlpha (0.75f));
    g.fillRoundedRectangle (r.expanded (3.0f), 9.0f);

    if (on)
    {
        for (float e = 5.0f; e > 0.0f; e -= 1.0f)
        {
            g.setColour (amber.withAlpha (0.05f));
            g.fillRoundedRectangle (r.expanded (e), 8.0f + e);
        }
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffffc266), r.getCentreX(), r.getY(),
                                                 juce::Colour (0xffb8700f), r.getCentreX(), r.getBottom(), false));
    }
    else
        g.setGradientFill (juce::ColourGradient (juce::Colour (over ? 0xff3d4043 : 0xff34373a), r.getCentreX(), r.getY(),
                                                 juce::Colour (0xff1a1b1d), r.getCentreX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, 7.0f);
    g.setColour (juce::Colours::white.withAlpha (on ? 0.45f : 0.10f));
    g.drawRoundedRectangle (r.reduced (1.0f), 6.0f, 1.0f);

    // icon, drawn in a unit square
    juce::Path fill, stroke, cut;
    switch (index)
    {
        case sl::Pastor:
            fill.addRoundedRectangle (0.38f, 0.08f, 0.24f, 0.44f, 0.12f);
            stroke.startNewSubPath (0.25f, 0.40f); stroke.cubicTo (0.25f, 0.72f, 0.75f, 0.72f, 0.75f, 0.40f);
            stroke.startNewSubPath (0.50f, 0.64f); stroke.lineTo (0.50f, 0.86f);
            stroke.startNewSubPath (0.32f, 0.88f); stroke.lineTo (0.68f, 0.88f);
            break;
        case sl::LeadVocal:
            fill.addEllipse (0.34f, 0.04f, 0.32f, 0.32f);
            fill.addQuadrilateral (0.41f, 0.38f, 0.59f, 0.38f, 0.545f, 0.96f, 0.455f, 0.96f);
            fill.applyTransform (juce::AffineTransform::rotation (0.62f, 0.5f, 0.5f));
            break;
        case sl::Choir:
            fill.addEllipse (0.38f, 0.14f, 0.24f, 0.24f);
            fill.addEllipse (0.10f, 0.26f, 0.20f, 0.20f);
            fill.addEllipse (0.70f, 0.26f, 0.20f, 0.20f);
            fill.addPieSegment (0.28f, 0.44f, 0.44f, 0.80f, -1.5708f, 1.5708f, 0.0f);
            fill.addPieSegment (0.02f, 0.52f, 0.34f, 0.64f, -1.5708f, 1.5708f, 0.0f);
            fill.addPieSegment (0.64f, 0.52f, 0.34f, 0.64f, -1.5708f, 1.5708f, 0.0f);
            break;
        case sl::Acoustic:
            fill.addEllipse (0.24f, 0.50f, 0.52f, 0.44f);
            fill.addEllipse (0.31f, 0.33f, 0.38f, 0.32f);
            fill.addRectangle (0.455f, 0.02f, 0.09f, 0.36f);
            cut.addEllipse (0.42f, 0.52f, 0.16f, 0.16f);
            fill.applyTransform (juce::AffineTransform::rotation (0.55f, 0.5f, 0.5f));
            cut.applyTransform (juce::AffineTransform::rotation (0.55f, 0.5f, 0.5f));
            break;
        case sl::Electric:
            fill.startNewSubPath (0.60f, 0.04f); fill.lineTo (0.22f, 0.56f); fill.lineTo (0.46f, 0.56f);
            fill.lineTo (0.36f, 0.96f); fill.lineTo (0.78f, 0.42f); fill.lineTo (0.54f, 0.42f); fill.closeSubPath();
            break;
        case sl::Bass:
            fill.addEllipse (0.20f, 0.30f, 0.17f, 0.17f);
            stroke.startNewSubPath (0.24f, 0.38f); stroke.cubicTo (0.22f, 0.08f, 0.66f, 0.06f, 0.66f, 0.38f);
            stroke.cubicTo (0.66f, 0.62f, 0.44f, 0.80f, 0.22f, 0.90f);
            fill.addEllipse (0.76f, 0.20f, 0.10f, 0.10f);
            fill.addEllipse (0.76f, 0.44f, 0.10f, 0.10f);
            break;
        case sl::Keys:
            stroke.addRoundedRectangle (0.08f, 0.24f, 0.84f, 0.52f, 0.04f);
            for (float kx : { 0.29f, 0.50f, 0.71f }) { stroke.startNewSubPath (kx, 0.24f); stroke.lineTo (kx, 0.76f); }
            for (float kx : { 0.235f, 0.445f, 0.655f }) fill.addRectangle (kx, 0.24f, 0.11f, 0.30f);
            break;
        case sl::Drums:
            stroke.addEllipse (0.16f, 0.38f, 0.68f, 0.20f);
            stroke.startNewSubPath (0.16f, 0.48f); stroke.lineTo (0.16f, 0.76f);
            stroke.cubicTo (0.16f, 0.92f, 0.84f, 0.92f, 0.84f, 0.76f); stroke.lineTo (0.84f, 0.48f);
            stroke.startNewSubPath (0.08f, 0.08f); stroke.lineTo (0.44f, 0.44f);
            stroke.startNewSubPath (0.92f, 0.08f); stroke.lineTo (0.56f, 0.44f);
            break;
        default: // Full mix: three faders
            for (float fx : { 0.24f, 0.50f, 0.76f }) { stroke.startNewSubPath (fx, 0.12f); stroke.lineTo (fx, 0.88f); }
            fill.addRoundedRectangle (0.14f, 0.56f, 0.20f, 0.13f, 0.03f);
            fill.addRoundedRectangle (0.40f, 0.24f, 0.20f, 0.13f, 0.03f);
            fill.addRoundedRectangle (0.66f, 0.42f, 0.20f, 0.13f, 0.03f);
            break;
    }
    const auto box = r.reduced (r.getWidth() * 0.24f);
    const auto t = juce::AffineTransform::scale (box.getWidth(), box.getHeight()).translated (box.getX(), box.getY());
    const auto ink = on ? juce::Colour (0xff2a1803) : juce::Colour (0xffa9acb0).withAlpha (over ? 1.0f : 0.85f);
    fill.applyTransform (t);
    stroke.applyTransform (t);
    cut.applyTransform (t);
    g.setColour (ink);
    g.fillPath (fill);
    g.strokePath (stroke, juce::PathStrokeType (juce::jmax (1.6f, box.getWidth() * 0.075f), juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    if (! cut.isEmpty())
    {
        g.setColour (on ? juce::Colour (0xffe09a2c) : juce::Colour (0xff27292b));
        g.fillPath (cut);
    }
}

//------------------------------------------------------------------------------
ToggleSwitch::ToggleSwitch() : juce::Button ("switch") { setClickingTogglesState (true); }

void ToggleSwitch::paintButton (juce::Graphics& g, bool, bool)
{
    const auto r = getLocalBounds().toFloat().reduced (2.0f);
    const auto c = r.getCentre();
    const bool on = getToggleState();

    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a3c3f), c.x, r.getY(), juce::Colour (0xff1c1d1f), c.x, r.getBottom(), false));
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (juce::Colours::black.withAlpha (0.8f));
    g.drawRoundedRectangle (r, 5.0f, 1.2f);

    const float nut = juce::jmin (r.getWidth(), r.getHeight()) * 0.30f;
    juce::Path hex;
    hex.addPolygon (c, 6, nut, 0.5236f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffa6a8ab), c.x - nut, c.y - nut, juce::Colour (0xff3f4144), c.x + nut, c.y + nut, false));
    g.fillPath (hex);
    g.setColour (juce::Colours::black);
    g.fillEllipse (c.x - nut * 0.5f, c.y - nut * 0.5f, nut, nut);

    const float len = r.getHeight() * 0.36f;
    const juce::Point<float> tip (c.x + (on ? 2.0f : -2.0f), c.y + (on ? -len : len));
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.drawLine ({ c.translated (2.0f, 3.0f), tip.translated (2.0f, 3.0f) }, 7.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffe4e5e7), c.x - 4.0f, 0.0f, juce::Colour (0xff6c6e71), c.x + 4.0f, 0.0f, false));
    juce::Path lever;
    lever.startNewSubPath (c); lever.lineTo (tip);
    g.strokePath (lever, juce::PathStrokeType (6.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.fillEllipse (tip.x - 5.0f, tip.y - 5.0f, 10.0f, 10.0f);
}
}

//==============================================================================
SelahEditor::SelahEditor (SelahProcessor& p) : AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&look);

    for (int i = 0; i < sl::kNumSources; ++i)
    {
        auto* b = sourceButtons.add (new hw::SourceButton (i));
        b->setRadioGroupId (1);
        b->onClick = [this, i] { if (sourceAtt != nullptr) sourceAtt->setValueAsCompleteGesture ((float) i); };
        addAndMakeVisible (b);
    }

    const char* ids[6] = { "cleanup", "warmth", "clarity", "evenout", "space", "volume" };
    const auto setup = [this] (juce::Slider& s, const char* id)
    {
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        s.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        s.setPopupDisplayEnabled (true, true, this);
        addAndMakeVisible (s);
        sliderAtts.add (new SliderAtt (proc.apvts, id, s));
    };
    for (int i = 0; i < 6; ++i)
        setup (knobs[i], ids[i]);
    setup (revLength, "revlength");

    addAndMakeVisible (limiterSwitch);
    limiterAtt = std::make_unique<ButtonAtt> (proc.apvts, "limiter", limiterSwitch);

    advancedSwitch.onClick = [this] { setAdvanced (advancedSwitch.getToggleState()); };
    addAndMakeVisible (advancedSwitch);

    sourceAtt = std::make_unique<juce::ParameterAttachment> (*proc.apvts.getParameter ("source"),
                                                             [this] (float v) { sourceChanged (juce::roundToInt (v)); });
    sourceAtt->sendInitialUpdate();

    advanced = (bool) proc.apvts.state.getProperty ("advancedOpen", false);
    advancedSwitch.setToggleState (advanced, juce::dontSendNotification);
    revLength.setVisible (advanced);
    limiterSwitch.setVisible (advanced);

    setSize (kWidth, kHeightClosed + (advanced ? kDrawerHeight : 0));
    startTimerHz (30);
}

SelahEditor::~SelahEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void SelahEditor::sourceChanged (int s)
{
    source = juce::jlimit (0, sl::kNumSources - 1, s);
    for (int i = 0; i < sourceButtons.size(); ++i)
        sourceButtons[i]->setToggleState (i == source, juce::dontSendNotification);
    repaint();
}

void SelahEditor::setAdvanced (bool open)
{
    advanced = open;
    proc.apvts.state.setProperty ("advancedOpen", open, nullptr);
    revLength.setVisible (open);
    limiterSwitch.setVisible (open);
    setSize (kWidth, kHeightClosed + (open ? kDrawerHeight : 0));
}

void SelahEditor::timerCallback()
{
    level = proc.engine.outputPeak();
    gr = proc.engine.gainReductionDb();
    limiting = proc.engine.limiting();
    repaint (ledBox);
    repaint (grBox);
    repaint (lampBox);
    if (advanced)
        repaint (techBox);
}

void SelahEditor::resized()
{
    const int H = getHeight();
    face = { 58, 14, kWidth - 116, H - 28 };

    // nine source buttons across the faceplate
    const int bs = 66;
    const int pitch = (face.getWidth() - 48 - bs) / (sl::kNumSources - 1);
    for (int i = 0; i < sourceButtons.size(); ++i)
        sourceButtons[i]->setBounds (face.getX() + 24 + i * pitch, 96, bs, bs);

    inset = { face.getX() + 22, 208, face.getWidth() - 44, 272 };
    ledBox  = { inset.getX() + 24, inset.getY() + 18, 386, 60 };
    grBox   = { ledBox.getRight() + 14, ledBox.getY(), 160, 60 };
    lampBox = { grBox.getRight() + 14, ledBox.getY(), 72, 60 };
    advancedSwitch.setBounds (inset.getRight() - 24 - 42, ledBox.getY() - 2, 36, 42);

    const int kSize = 118;
    const int kPitch = (inset.getWidth() - 40 - kSize) / 5;
    for (int i = 0; i < 6; ++i)
        knobs[i].setBounds (inset.getX() + 20 + i * kPitch, inset.getY() + 90, kSize, kSize);

    drawer = { inset.getX(), inset.getBottom() + 14, inset.getWidth(), kDrawerHeight - 12 };
    techBox = drawer.reduced (18, 12).withTrimmedRight (270);
    revLength.setBounds (drawer.getRight() - 250, drawer.getY() + 12, 84, 84);
    limiterSwitch.setBounds (drawer.getRight() - 96, drawer.getY() + 22, 46, 62);

    renderBackground();
}

void SelahEditor::renderBackground()
{
    const float scale = 2.0f;   // render at 2x so it stays sharp on high-density screens
    background = juce::Image (juce::Image::RGB, juce::roundToInt ((float) getWidth() * scale),
                              juce::roundToInt ((float) getHeight() * scale), true);
    juce::Graphics g (background);
    g.addTransform (juce::AffineTransform::scale (scale));
    drawFace (g);
}

void SelahEditor::drawFace (juce::Graphics& g)
{
    using namespace hw;
    const int W = getWidth(), H = getHeight();
    g.fillAll (rack);

    // rack ears
    for (int side = 0; side < 2; ++side)
    {
        const juce::Rectangle<float> ear (side == 0 ? 8.0f : (float) W - 62.0f, 14.0f, 54.0f, (float) H - 28.0f);
        fillBrushed (g, ear, juce::Colour (0xff4a4c4f), juce::Colour (0xff2b2c2e), 4.0f, 11 + side);
        g.setColour (juce::Colours::black.withAlpha (0.7f));
        g.drawRoundedRectangle (ear, 4.0f, 1.5f);
        for (float fy : { 0.30f, 0.70f })
        {
            const juce::Rectangle<float> slot (ear.getCentreX() - 8.0f, ear.getY() + ear.getHeight() * fy - 34.0f, 16.0f, 68.0f);
            g.setColour (juce::Colour (0xff050505));
            g.fillRoundedRectangle (slot, 8.0f);
            g.setColour (juce::Colours::white.withAlpha (0.10f));
            g.drawRoundedRectangle (slot.expanded (0.5f), 8.0f, 1.0f);
        }
        drawScrew (g, ear.getCentreX(), ear.getY() + 26.0f, 8.0f);
        drawScrew (g, ear.getCentreX(), ear.getBottom() - 26.0f, 8.0f);
    }

    // faceplate
    const auto f = face.toFloat();
    fillBrushed (g, f, juce::Colour (0xff55585b), juce::Colour (0xff303235), 5.0f, 5);
    g.setColour (juce::Colours::black.withAlpha (0.8f));
    g.drawRoundedRectangle (f, 5.0f, 1.5f);
    g.setColour (juce::Colours::white.withAlpha (0.14f));
    g.drawLine (f.getX() + 4.0f, f.getY() + 1.5f, f.getRight() - 4.0f, f.getY() + 1.5f, 1.0f);

    // name plate
    g.setFont (font (46.0f, true, 0.10f));
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawText ("SELAH", face.getX() + 30, 23, 300, 46, juce::Justification::centredLeft);
    g.setColour (gold);
    g.drawText ("SELAH", face.getX() + 29, 21, 300, 46, juce::Justification::centredLeft);
    g.setFont (font (15.0f, false, 0.03f));
    g.setColour (engrave);
    g.drawText ("Sound that serves", face.getX() + 31, 66, 300, 18, juce::Justification::centredLeft);

    g.setFont (font (10.5f, true, 0.18f));
    g.setColour (faint);
    g.drawText ("CHURCH CHANNEL  /  ZERO DELAY", face.getRight() - 330, 30, 300, 14, juce::Justification::centredRight);
    g.setColour (engrave.withAlpha (0.8f));
    g.drawText ("1  PICK WHAT IS ON THIS CHANNEL", face.getRight() - 330, 50, 300, 14, juce::Justification::centredRight);
    g.drawText ("2  TURN UP ONLY WHAT YOU NEED", face.getRight() - 330, 66, 300, 14, juce::Justification::centredRight);

    // source labels
    g.setFont (font (12.0f, false, 0.02f));
    g.setColour (engrave);
    for (int i = 0; i < sourceButtons.size(); ++i)
    {
        const auto b = sourceButtons[i]->getBounds();
        g.drawFittedText (sourceLabels[i], b.getX() - 10, b.getBottom() + 1, b.getWidth() + 20, 30, juce::Justification::centredTop, 2);
    }

    // recessed control panel
    const auto drawInset = [&g] (juce::Rectangle<int> r)
    {
        const auto rf = r.toFloat();
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillRoundedRectangle (rf.expanded (2.0f), 9.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1b1c1e), 0.0f, rf.getY(), juce::Colour (0xff242527), 0.0f, rf.getBottom(), false));
        g.fillRoundedRectangle (rf, 8.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.5f), 0.0f, rf.getY(),
                                                 juce::Colours::transparentBlack, 0.0f, rf.getY() + 14.0f, false));
        g.fillRoundedRectangle (rf, 8.0f);
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.drawRoundedRectangle (rf.expanded (1.0f), 9.0f, 1.0f);
    };
    drawInset (inset);

    // meter windows
    for (auto box : { ledBox, grBox })
    {
        g.setColour (juce::Colour (0xff09090a));
        g.fillRoundedRectangle (box.toFloat(), 5.0f);
        g.setColour (juce::Colours::white.withAlpha (0.10f));
        g.drawRoundedRectangle (box.toFloat(), 5.0f, 1.0f);
    }
    g.setFont (font (10.0f, true, 0.12f));
    g.setColour (faint);
    const float ledPitch = (float) (ledBox.getWidth() - 36) / 15.0f;
    for (int i = 0; i <= 5; ++i)   // -30 .. 0 dB, one LED per 2 dB
    {
        const float x = (float) ledBox.getX() + 18.0f + ledPitch * ((float) i * 3.0f);
        g.drawText (i == 5 ? "0" : juce::String (-30 + i * 6), juce::roundToInt (x) - 20, ledBox.getY() + 28, 40, 12, juce::Justification::centred);
    }
    g.drawText ("OUTPUT LEVEL", ledBox.getX(), ledBox.getBottom() - 17, ledBox.getWidth(), 12, juce::Justification::centred);
    g.drawText ("EVENING OUT", grBox.getX(), grBox.getBottom() - 17, grBox.getWidth(), 12, juce::Justification::centred);

    // knob names and plain-English hints
    for (int i = 0; i < 6; ++i)
    {
        const auto k = knobs[i].getBounds();
        g.setFont (font (14.0f, true, 0.10f));
        g.setColour (engrave);
        g.drawText (knobNames[i], k.getX() - 20, k.getBottom() + 2, k.getWidth() + 40, 18, juce::Justification::centred);
        g.setFont (font (11.0f, false, 0.01f));
        g.setColour (faint);
        g.drawFittedText (knobHints[i], k.getX() - 14, k.getBottom() + 22, k.getWidth() + 28, 30, juce::Justification::centredTop, 2);
    }

    g.setFont (font (10.0f, true, 0.12f));
    g.setColour (engrave);
    g.drawText ("TOO LOUD", lampBox.getX() - 6, lampBox.getBottom() - 17, lampBox.getWidth() + 12, 12, juce::Justification::centred);
    g.drawText ("ADVANCED", advancedSwitch.getX() - 20, lampBox.getBottom() - 17, advancedSwitch.getWidth() + 40, 12, juce::Justification::centred);

    // faceplate screws
    drawScrew (g, f.getX() + 14.0f, f.getY() + 14.0f, 5.5f);
    drawScrew (g, f.getRight() - 14.0f, f.getY() + 14.0f, 5.5f);
    drawScrew (g, f.getX() + 14.0f, f.getBottom() - 12.0f, 5.5f);
    drawScrew (g, f.getRight() - 14.0f, f.getBottom() - 12.0f, 5.5f);

    if (advanced)
    {
        drawInset (drawer);
        g.setFont (font (10.0f, true, 0.12f));
        g.setColour (engrave);
        g.drawText ("REVERB LENGTH", revLength.getX() - 30, revLength.getBottom() + 2, revLength.getWidth() + 60, 12, juce::Justification::centred);
        g.drawText ("SAFETY LIMITER", limiterSwitch.getX() - 36, limiterSwitch.getBottom() + 4, limiterSwitch.getWidth() + 72, 12, juce::Justification::centred);
    }
}

void SelahEditor::paint (juce::Graphics& g)
{
    using namespace hw;
    g.drawImage (background, getLocalBounds().toFloat());

    // output level: 15 LEDs from -30 to 0 dB
    const float db = juce::Decibels::gainToDecibels (level, -90.0f);
    const float ledPitch = (float) (ledBox.getWidth() - 36) / 15.0f;
    for (int i = 0; i < 15; ++i)
    {
        const juce::Rectangle<float> r ((float) ledBox.getX() + 18.0f + ledPitch * (float) i + 3.0f, (float) ledBox.getY() + 10.0f, ledPitch - 6.0f, 13.0f);
        const auto c = i < 9 ? juce::Colour (0xff35d14a) : (i < 13 ? juce::Colour (0xfff2c22e) : juce::Colour (0xffe8402c));
        drawLed (g, r, c, db > -30.0f + 2.0f * (float) i);
    }

    // how hard EVEN OUT is working: 8 LEDs, 2 dB each
    const float grPitch = (float) (grBox.getWidth() - 28) / 8.0f;
    for (int i = 0; i < 8; ++i)
    {
        const juce::Rectangle<float> r ((float) grBox.getX() + 14.0f + grPitch * (float) i + 3.0f, (float) grBox.getY() + 10.0f, grPitch - 6.0f, 13.0f);
        drawLed (g, r, amber, gr > 0.5f + 2.0f * (float) i);
    }

    // TOO LOUD lamp
    {
        const bool hot = limiting > 0.05f;
        const auto c = lampBox.toFloat().withTrimmedBottom (22.0f).getCentre();
        const float r = 9.0f;
        g.setColour (juce::Colours::black);
        g.fillEllipse (c.x - r - 3.0f, c.y - r - 3.0f, (r + 3.0f) * 2.0f, (r + 3.0f) * 2.0f);
        if (hot)
        {
            g.setColour (juce::Colour (0xffff3020).withAlpha (0.25f));
            g.fillEllipse (c.x - r * 2.2f, c.y - r * 2.2f, r * 4.4f, r * 4.4f);
        }
        g.setGradientFill (juce::ColourGradient (hot ? juce::Colour (0xffff9a8a) : juce::Colour (0xff5a1a14), c.x - 3.0f, c.y - 3.0f,
                                                 hot ? juce::Colour (0xffe02010) : juce::Colour (0xff2a0b08), c.x + r, c.y + r, true));
        g.fillEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f);
    }

    if (advanced)
    {
        const auto t = sl::describe (proc.readParams());
        const auto& c = sl::sourceCfg (source);
        juce::StringArray lines;
        lines.add ("CLEAN UP  low cut " + juce::String (juce::roundToInt (t.hpHz)) + " Hz"
                   + (c.deEssMaxDb > 0 ? " / de-ess to " + juce::String (t.deEssDb, 1) + " dB" : juce::String())
                   + (c.gateThrDb > -80.0f ? " / gate " + juce::String (juce::roundToInt (t.gateThrDb)) + " dB" : juce::String()));
        lines.add ("WARMTH    +" + juce::String (t.warmDb, 1) + " dB @ " + juce::String (juce::roundToInt (c.warmHz)) + " Hz / transformer drive");
        lines.add ("CLARITY   " + juce::String (t.mudDb, 1) + " dB @ " + juce::String (juce::roundToInt (c.mudHz))
                   + " / +" + juce::String (t.presDb, 1) + " dB @ " + juce::String (c.presHz / 1000.0f, 1)
                   + "k / +" + juce::String (t.airDb, 1) + " dB @ " + juce::String (c.airHz / 1000.0f, 1) + "k");
        lines.add ("EVEN OUT  " + juce::String (t.ratio, 1) + ":1 / threshold " + juce::String (t.thrDb, 1) + " dB / now " + (gr > 0.05f ? "-" + juce::String (gr, 1) : juce::String ("0.0")) + " dB");
        lines.add ("SPACE     " + juce::String (fm::modeName (c.reverbMode)) + " / " + juce::String (t.revSeconds, 1) + " s");
        g.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 12.0f, juce::Font::plain)));
        g.setColour (amber.withAlpha (0.9f));
        int y = techBox.getY();
        for (const auto& s : lines)
        {
            g.drawText (s, techBox.getX(), y, techBox.getWidth(), 16, juce::Justification::centredLeft);
            y += 20;
        }
    }
}
