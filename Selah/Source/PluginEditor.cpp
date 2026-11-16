#include "PluginEditor.h"

namespace
{
constexpr int kWidth = 940, kHeightClosed = 552, kDrawerHeight = 140;

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

// Matte black finish with tiny white flakes, like vintage sparkle covering.
void fillSparkle (juce::Graphics& g, juce::Rectangle<float> r, float corner, int seed)
{
    juce::Path clip;
    clip.addRoundedRectangle (r, corner);
    juce::Graphics::ScopedSaveState save (g);
    g.reduceClipRegion (clip);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff171717), 0.0f, r.getY(), juce::Colour (0xff0c0c0c), 0.0f, r.getBottom(), false));
    g.fillRect (r);
    juce::Random rnd (seed);
    const int flakes = juce::roundToInt (r.getWidth() * r.getHeight() / 75.0f);
    for (int i = 0; i < flakes; ++i)
    {
        const float x = r.getX() + rnd.nextFloat() * r.getWidth();
        const float y = r.getY() + rnd.nextFloat() * r.getHeight();
        const float b = rnd.nextFloat();
        const float bright = b * b * b;                       // most flakes are faint, a few catch the light
        const float size = 0.5f + 0.9f * rnd.nextFloat();
        g.setColour (juce::Colours::white.withAlpha (0.10f + 0.80f * bright));
        g.fillEllipse (x, y, size, size);
        if (bright > 0.80f)
        {
            g.setColour (juce::Colours::white.withAlpha (0.35f));
            g.fillRect (x - 1.6f, y + size * 0.5f - 0.25f, 3.2f + size, 0.5f);
            g.fillRect (x + size * 0.5f - 0.25f, y - 1.6f, 0.5f, 3.2f + size);
        }
    }
}

struct VuGeometry
{
    juce::Rectangle<float> face;
    juce::Point<float> pivot;
    float radius, halfAngle;
    explicit VuGeometry (juce::Rectangle<int> box)
    {
        face = box.toFloat().reduced (5.0f);
        halfAngle = 0.62f;
        radius = (face.getWidth() * 0.5f - 30.0f) / std::sin (halfAngle);
        pivot = { face.getCentreX(), face.getY() + 27.0f + radius };
    }
    float angleFor (float v01) const { return -halfAngle + 2.0f * halfAngle * juce::jlimit (0.0f, 1.0f, v01); }
};

// The printed face of a needle meter (drawn once into the cached background).
void drawVuFace (juce::Graphics& g, juce::Rectangle<int> box, const juce::String& title,
                 const juce::StringArray& marks, float redFrom01)
{
    const VuGeometry v (box);
    const auto outer = box.toFloat();
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffd8d9db), 0.0f, outer.getY(), juce::Colour (0xff55575a), 0.0f, outer.getBottom(), false));
    g.fillRoundedRectangle (outer, 6.0f);                      // chrome bezel
    g.setColour (juce::Colours::black);
    g.fillRoundedRectangle (outer.reduced (2.5f), 4.5f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xfffbf0c9), v.face.getCentreX(), v.face.getY(),
                                             juce::Colour (0xffd9bd78), v.face.getCentreX(), v.face.getBottom() + 30.0f, true));
    g.fillRoundedRectangle (v.face, 3.0f);                     // warm back-lit card

    juce::Graphics::ScopedSaveState save (g);
    g.reduceClipRegion (v.face.toNearestInt());
    const juce::Colour ink (0xff231a10), red (0xffb3261a);

    juce::Path arc, hot;
    arc.addCentredArc (v.pivot.x, v.pivot.y, v.radius, v.radius, 0.0f, v.angleFor (0.0f), v.angleFor (redFrom01), true);
    g.setColour (ink);
    g.strokePath (arc, juce::PathStrokeType (1.4f));
    if (redFrom01 < 1.0f)
    {
        hot.addCentredArc (v.pivot.x, v.pivot.y, v.radius + 1.5f, v.radius + 1.5f, 0.0f, v.angleFor (redFrom01), v.angleFor (1.0f), true);
        g.setColour (red);
        g.strokePath (hot, juce::PathStrokeType (4.0f));
    }
    const int n = marks.size();
    g.setFont (hw::font (9.5f, true, 0.0f));
    for (int i = 0; i < n; ++i)
    {
        const float f = (float) i / (float) (n - 1);
        const float a = v.angleFor (f);
        g.setColour (redFrom01 < 1.0f && f >= redFrom01 - 0.001f ? red : ink);
        g.drawLine ({ v.pivot.getPointOnCircumference (v.radius, a), v.pivot.getPointOnCircumference (v.radius + 6.0f, a) }, 1.4f);
        const auto tp = v.pivot.getPointOnCircumference (v.radius + 14.0f, a);
        g.drawText (marks[i], juce::roundToInt (tp.x) - 16, juce::roundToInt (tp.y) - 6, 32, 12, juce::Justification::centred);
        if (i < n - 1)
            for (int k = 1; k < 3; ++k)
            {
                const float am = v.angleFor (f + (float) k / (3.0f * (float) (n - 1)));
                g.setColour (ink.withAlpha (0.6f));
                g.drawLine ({ v.pivot.getPointOnCircumference (v.radius, am), v.pivot.getPointOnCircumference (v.radius + 3.5f, am) }, 1.0f);
            }
    }
    g.setColour (ink);
    g.setFont (hw::font (9.5f, true, 0.16f));
    g.drawText (title, v.face.withTrimmedTop (v.face.getHeight() - 17.0f), juce::Justification::centred);
}

void drawVuNeedle (juce::Graphics& g, juce::Rectangle<int> box, float v01)
{
    const VuGeometry v (box);
    juce::Graphics::ScopedSaveState save (g);
    g.reduceClipRegion (v.face.withTrimmedBottom (17.0f).toNearestInt());
    const float a = v.angleFor (v01);
    g.setColour (juce::Colours::black.withAlpha (0.18f));
    g.drawLine ({ v.pivot.translated (2.0f, 2.0f), v.pivot.getPointOnCircumference (v.radius + 7.0f, a).translated (2.0f, 2.0f) }, 2.0f);
    g.setColour (juce::Colour (0xff1a1410));
    g.drawLine ({ v.pivot, v.pivot.getPointOnCircumference (v.radius + 7.0f, a) }, 1.6f);
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
    const float r = juce::jmin (area.getWidth(), area.getHeight()) * 0.5f - 1.0f;
    const auto c = area.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);
    const bool numbered = r > 48.0f;
    const float base = r * (numbered ? 0.46f : 0.52f);

    // printed 0-10 scale
    g.setFont (font (10.0f, true, 0.0f));
    for (int i = 0; i <= 10; ++i)
    {
        const float a = startAngle + (endAngle - startAngle) * (float) i / 10.0f;
        g.setColour (cream.withAlpha (0.9f));
        if (numbered)
        {
            const auto tp = c.getPointOnCircumference (r - 7.0f, a);
            g.drawText (juce::String (i), juce::roundToInt (tp.x) - 10, juce::roundToInt (tp.y) - 6, 20, 12, juce::Justification::centred);
            g.fillEllipse (juce::Rectangle<float> (2.2f, 2.2f).withCentre (c.getPointOnCircumference (r - 17.0f, a)));
        }
        else
            g.drawLine ({ c.getPointOnCircumference (r - 5.0f, a), c.getPointOnCircumference (r - 1.0f, a) }, 1.2f);
    }

    // shadow on the panel
    g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.8f), c.x, c.y + base * 0.4f,
                                             juce::Colours::transparentBlack, c.x, c.y + base * 1.9f, true));
    g.fillEllipse (c.x - base * 1.5f, c.y - base * 1.0f, base * 3.0f, base * 3.0f);

    // skirt
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff7efd9), c.x - base * 0.4f, c.y - base,
                                             juce::Colour (0xffa99d80), c.x + base * 0.5f, c.y + base, false));
    g.fillEllipse (c.x - base, c.y - base, base * 2.0f, base * 2.0f);
    g.setColour (juce::Colour (0xff3b3324).withAlpha (0.7f));
    g.drawEllipse (c.x - base, c.y - base, base * 2.0f, base * 2.0f, 1.0f);

    // pointer wedge (the "chicken head")
    const float len = base * (numbered ? 1.42f : 1.30f);
    juce::Path wedge;
    wedge.startNewSubPath (-base * 0.50f, base * 0.62f);
    wedge.lineTo (base * 0.50f, base * 0.62f);
    wedge.lineTo (base * 0.15f, -len);
    wedge.lineTo (-base * 0.15f, -len);
    wedge.closeSubPath();
    wedge = wedge.createPathWithRoundedCorners (base * 0.16f);
    const auto t = juce::AffineTransform::rotation (angle).translated (c.x, c.y);
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillPath (wedge, t.translated (1.5f, 2.5f));
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xfffffaea), c.x - base, c.y - base,
                                             juce::Colour (0xffc7bb9c), c.x + base, c.y + base, false));
    g.fillPath (wedge, t);
    g.setColour (juce::Colour (0xff3b3324).withAlpha (0.75f));
    g.strokePath (wedge, juce::PathStrokeType (1.0f), t);

    // indicator line
    juce::Path mark;
    mark.startNewSubPath (0.0f, -len + 3.0f);
    mark.lineTo (0.0f, -len * 0.35f);
    g.setColour (juce::Colour (0xff1c160d));
    g.strokePath (mark, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded), t);
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
        g.setGradientFill (juce::ColourGradient (juce::Colour (over ? 0xfffbf3de : 0xfff1e8d0), r.getCentreX(), r.getY(),
                                                 juce::Colour (0xffb0a487), r.getCentreX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, 7.0f);
    g.setColour (juce::Colours::white.withAlpha (on ? 0.45f : 0.55f));
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
    const auto ink = on ? juce::Colour (0xff2a1803) : juce::Colour (0xff2b241a);
    fill.applyTransform (t);
    stroke.applyTransform (t);
    cut.applyTransform (t);
    g.setColour (ink);
    g.fillPath (fill);
    g.strokePath (stroke, juce::PathStrokeType (juce::jmax (1.6f, box.getWidth() * 0.075f), juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    if (! cut.isEmpty())
    {
        g.setColour (on ? juce::Colour (0xffe09a2c) : juce::Colour (0xffd6cbb0));
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
    const float lvTarget = (juce::Decibels::gainToDecibels (level, -90.0f) + 30.0f) / 36.0f;   // -30 .. +3 dB plus a little over-travel
    const float grTarget = gr / 16.0f;
    needleLevel += (lvTarget > needleLevel ? 0.45f : 0.12f) * (juce::jlimit (0.0f, 1.0f, lvTarget) - needleLevel);
    needleGr    += 0.30f * (juce::jlimit (0.0f, 1.0f, grTarget) - needleGr);
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

    inset = { face.getX() + 22, 208, face.getWidth() - 44, 300 };
    ledBox  = { inset.getX() + 22, inset.getY() + 16, 300, 92 };
    grBox   = { ledBox.getRight() + 16, ledBox.getY(), 210, 92 };
    lampBox = { grBox.getRight() + 26, ledBox.getY() + 12, 76, 68 };
    advancedSwitch.setBounds (inset.getRight() - 24 - 46, ledBox.getY() + 10, 40, 50);

    const int kSize = 118;
    const int kPitch = (inset.getWidth() - 40 - kSize) / 5;
    for (int i = 0; i < 6; ++i)
        knobs[i].setBounds (inset.getX() + 20 + i * kPitch, inset.getY() + 116, kSize, kSize);

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
        fillSparkle (g, ear, 4.0f, 11 + side);
        g.setColour (juce::Colours::white.withAlpha (0.10f));
        g.drawRoundedRectangle (ear, 4.0f, 1.0f);
        for (float fy : { 0.30f, 0.70f })
        {
            const juce::Rectangle<float> slot (ear.getCentreX() - 8.0f, ear.getY() + ear.getHeight() * fy - 34.0f, 16.0f, 68.0f);
            g.setColour (juce::Colour (0xff030303));
            g.fillRoundedRectangle (slot, 8.0f);
            g.setColour (juce::Colours::white.withAlpha (0.14f));
            g.drawRoundedRectangle (slot.expanded (0.5f), 8.0f, 1.0f);
        }
        drawScrew (g, ear.getCentreX(), ear.getY() + 26.0f, 8.0f);
        drawScrew (g, ear.getCentreX(), ear.getBottom() - 26.0f, 8.0f);
    }

    // faceplate: matte black with white flakes, cream pinstripe
    const auto f = face.toFloat();
    fillSparkle (g, f, 5.0f, 5);
    g.setColour (juce::Colours::white.withAlpha (0.12f));
    g.drawRoundedRectangle (f, 5.0f, 1.0f);
    g.setColour (cream.withAlpha (0.55f));
    g.drawRoundedRectangle (f.reduced (7.0f), 4.0f, 1.0f);

    // nameplate
    const juce::Font serif (juce::FontOptions (juce::Font::getDefaultSerifFontName(), 50.0f, juce::Font::bold | juce::Font::italic));
    g.setFont (serif);
    g.setColour (juce::Colours::black.withAlpha (0.8f));
    g.drawText ("Selah", face.getX() + 32, 22, 300, 52, juce::Justification::centredLeft);
    g.setColour (gold);
    g.drawText ("Selah", face.getX() + 30, 20, 300, 52, juce::Justification::centredLeft);
    g.setFont (font (11.0f, true, 0.28f));
    g.setColour (cream.withAlpha (0.85f));
    g.drawText ("SOUND THAT SERVES", face.getX() + 33, 70, 300, 14, juce::Justification::centredLeft);

    g.setFont (font (10.5f, true, 0.18f));
    g.setColour (gold.withAlpha (0.9f));
    g.drawText ("CHURCH CHANNEL  -  ZERO DELAY", face.getRight() - 340, 28, 310, 14, juce::Justification::centredRight);
    g.setColour (cream.withAlpha (0.75f));
    g.drawText ("1  PICK WHAT IS ON THIS CHANNEL", face.getRight() - 340, 48, 310, 14, juce::Justification::centredRight);
    g.drawText ("2  TURN UP ONLY WHAT YOU NEED", face.getRight() - 340, 64, 310, 14, juce::Justification::centredRight);

    // source labels
    g.setFont (font (12.0f, false, 0.02f));
    g.setColour (cream.withAlpha (0.9f));
    for (int i = 0; i < sourceButtons.size(); ++i)
    {
        const auto b = sourceButtons[i]->getBounds();
        g.drawFittedText (sourceLabels[i], b.getX() - 10, b.getBottom() + 1, b.getWidth() + 20, 30, juce::Justification::centredTop, 2);
    }

    // silk-screened section outline
    const auto outline = [&g] (juce::Rectangle<int> r)
    {
        g.setColour (cream.withAlpha (0.45f));
        g.drawRoundedRectangle (r.toFloat(), 8.0f, 1.0f);
    };
    outline (inset);

    drawVuFace (g, ledBox, "OUTPUT LEVEL", { "-30", "-24", "-18", "-12", "-6", "0", "+3" }, 5.0f / 6.0f);
    drawVuFace (g, grBox, "EVENING OUT", { "0", "4", "8", "12", "16" }, 1.0f);

    // knob names and plain-English hints
    for (int i = 0; i < 6; ++i)
    {
        const auto k = knobs[i].getBounds();
        g.setFont (font (14.0f, true, 0.10f));
        g.setColour (cream);
        g.drawText (knobNames[i], k.getX() - 20, k.getBottom() + 2, k.getWidth() + 40, 18, juce::Justification::centred);
        g.setFont (font (11.0f, false, 0.01f));
        g.setColour (cream.withAlpha (0.6f));
        g.drawFittedText (knobHints[i], k.getX() - 14, k.getBottom() + 22, k.getWidth() + 28, 30, juce::Justification::centredTop, 2);
    }

    g.setFont (font (10.0f, true, 0.12f));
    g.setColour (cream);
    g.drawText ("TOO LOUD", lampBox.getX() - 6, lampBox.getBottom() - 13, lampBox.getWidth() + 12, 12, juce::Justification::centred);
    g.drawText ("ADVANCED", advancedSwitch.getX() - 20, lampBox.getBottom() - 13, advancedSwitch.getWidth() + 40, 12, juce::Justification::centred);

    // jewel lamp bezel
    {
        const auto c = lampBox.toFloat().withTrimmedBottom (18.0f).getCentre();
        const float r = 16.0f;
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffe6e7e9), c.x - r, c.y - r, juce::Colour (0xff4a4c4f), c.x + r, c.y + r, false));
        g.fillEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f);
        g.setColour (juce::Colours::black);
        g.fillEllipse (c.x - r + 3.5f, c.y - r + 3.5f, (r - 3.5f) * 2.0f, (r - 3.5f) * 2.0f);
    }

    // faceplate screws
    drawScrew (g, f.getX() + 18.0f, f.getY() + 18.0f, 5.5f);
    drawScrew (g, f.getRight() - 18.0f, f.getY() + 18.0f, 5.5f);
    drawScrew (g, f.getX() + 18.0f, f.getBottom() - 18.0f, 5.5f);
    drawScrew (g, f.getRight() - 18.0f, f.getBottom() - 18.0f, 5.5f);

    if (advanced)
    {
        outline (drawer);
        g.setFont (font (10.0f, true, 0.12f));
        g.setColour (cream);
        g.drawText ("REVERB LENGTH", revLength.getX() - 30, revLength.getBottom() + 2, revLength.getWidth() + 60, 12, juce::Justification::centred);
        g.drawText ("SAFETY LIMITER", limiterSwitch.getX() - 36, limiterSwitch.getBottom() + 4, limiterSwitch.getWidth() + 72, 12, juce::Justification::centred);
    }
}

void SelahEditor::paint (juce::Graphics& g)
{
    using namespace hw;
    g.drawImage (background, getLocalBounds().toFloat());

    drawVuNeedle (g, ledBox, needleLevel);
    drawVuNeedle (g, grBox, needleGr);

    // TOO LOUD jewel
    {
        const bool hot = limiting > 0.05f;
        const auto c = lampBox.toFloat().withTrimmedBottom (18.0f).getCentre();
        const float r = 11.0f;
        if (hot)
        {
            g.setColour (juce::Colour (0xffff3020).withAlpha (0.28f));
            g.fillEllipse (c.x - r * 2.4f, c.y - r * 2.4f, r * 4.8f, r * 4.8f);
        }
        g.setGradientFill (juce::ColourGradient (hot ? juce::Colour (0xffffb0a0) : juce::Colour (0xff7a2018), c.x - 3.0f, c.y - 4.0f,
                                                 hot ? juce::Colour (0xffd81808) : juce::Colour (0xff2c0906), c.x + r, c.y + r, true));
        g.fillEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f);
        g.setColour (juce::Colours::white.withAlpha (hot ? 0.7f : 0.25f));     // facet glint
        g.fillEllipse (c.x - r * 0.55f, c.y - r * 0.60f, r * 0.5f, r * 0.35f);
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
        g.setColour (cream.withAlpha (0.9f));
        int y = techBox.getY();
        for (const auto& s : lines)
        {
            g.drawText (s, techBox.getX(), y, techBox.getWidth(), 16, juce::Justification::centredLeft);
            y += 20;
        }
    }
}
