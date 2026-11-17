#include "PluginEditor.h"

namespace
{
constexpr int kWidth = 940, kHeight = 600;

const char* const knobNames[6] = { "DECAY", "PRE-DELAY", "TONE", "MOD", "", "MIX" };
const char* const modeTitles[fm::kNumModes] = { "Quarters", "Sanctuary", "Steel", "Coil", "Ladder", "Vapor", "Tide" };
const char* const modeHints[fm::kNumModes] = { "room", "hall", "plate", "spring", "octaves", "cloud", "swell" };

// White enamel with tiny blue flakes.
void fillSparkle (juce::Graphics& g, juce::Rectangle<float> r, float corner, int seed)
{
    juce::Path clip;
    clip.addRoundedRectangle (r, corner);
    juce::Graphics::ScopedSaveState save (g);
    g.reduceClipRegion (clip);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff6f4ee), 0.0f, r.getY(), juce::Colour (0xffe3e0d6), 0.0f, r.getBottom(), false));
    g.fillRect (r);
    juce::Random rnd (seed);
    const int flakes = juce::roundToInt (r.getWidth() * r.getHeight() / 70.0f);
    for (int i = 0; i < flakes; ++i)
    {
        const float x = r.getX() + rnd.nextFloat() * r.getWidth();
        const float y = r.getY() + rnd.nextFloat() * r.getHeight();
        const float b = rnd.nextFloat();
        const float strength = b * b;
        const float size = 0.5f + 0.9f * rnd.nextFloat();
        const auto c = juce::Colour::fromHSV (0.58f + 0.05f * rnd.nextFloat(), 0.55f + 0.4f * rnd.nextFloat(), 0.55f + 0.4f * rnd.nextFloat(), 1.0f);
        g.setColour (c.withAlpha (0.15f + 0.75f * strength));
        g.fillEllipse (x, y, size, size);
        if (strength > 0.85f)   // a few flakes catch the light
        {
            g.setColour (fw::sky.withAlpha (0.45f));
            g.fillRect (x - 1.4f, y + size * 0.5f - 0.25f, 2.8f + size, 0.5f);
            g.fillRect (x + size * 0.5f - 0.25f, y - 1.4f, 0.5f, 2.8f + size);
        }
    }
    // a soft sheen across the top
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.5f), 0.0f, r.getY(),
                                             juce::Colours::transparentWhite, 0.0f, r.getY() + r.getHeight() * 0.3f, false));
    g.fillRect (r);
}

void drawScrew (juce::Graphics& g, float cx, float cy, float r)
{
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillEllipse (cx - r - 0.5f, cy - r + 0.5f, r * 2.0f + 1.0f, r * 2.0f + 1.5f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff0f1f3), cx - r * 0.5f, cy - r * 0.6f,
                                             juce::Colour (0xff7d8086), cx + r, cy + r, true));
    g.fillEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f);
    g.setColour (juce::Colour (0xff3a3d42));
    g.drawLine (cx - r * 0.55f, cy - r * 0.55f, cx + r * 0.55f, cy + r * 0.55f, 1.6f);
    g.drawLine (cx - r * 0.55f, cy + r * 0.55f, cx + r * 0.55f, cy - r * 0.55f, 1.6f);
}

struct Vu
{
    juce::Rectangle<float> face;
    juce::Point<float> pivot;
    float radius, half = 0.62f;
    explicit Vu (juce::Rectangle<int> box)
    {
        face = box.toFloat().reduced (5.0f);
        radius = (face.getWidth() * 0.5f - 30.0f) / std::sin (half);
        pivot = { face.getCentreX(), face.getY() + 27.0f + radius };
    }
    float angle (float v) const { return -half + 2.0f * half * juce::jlimit (0.0f, 1.0f, v); }
};
}

//==============================================================================
namespace fw
{
juce::Font font (float height, bool bold, float kerning)
{
    return juce::Font (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain)).withExtraKerningFactor (kerning);
}

void Look::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                             float startAngle, float endAngle, juce::Slider& s)
{
    const auto area = juce::Rectangle<int> (x, y, w, h).toFloat();
    const float r = juce::jmin (area.getWidth(), area.getHeight()) * 0.5f - 1.0f;
    const auto c = area.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);
    const bool big = r > 48.0f;
    const float base = r * (big ? 0.48f : 0.56f);
    const float alpha = s.isEnabled() ? 1.0f : 0.35f;

    // printed scale
    g.setFont (font (big ? 10.0f : 8.5f, true, 0.0f));
    for (int i = 0; i <= 10; ++i)
    {
        const float a = startAngle + (endAngle - startAngle) * (float) i / 10.0f;
        g.setColour (ink.withAlpha (alpha));
        if (big)
        {
            const auto tp = c.getPointOnCircumference (r - 7.0f, a);
            g.drawText (juce::String (i), juce::roundToInt (tp.x) - 10, juce::roundToInt (tp.y) - 6, 20, 12, juce::Justification::centred);
            g.fillEllipse (juce::Rectangle<float> (2.2f, 2.2f).withCentre (c.getPointOnCircumference (r - 17.0f, a)));
        }
        else
            g.drawLine ({ c.getPointOnCircumference (r - 6.0f, a), c.getPointOnCircumference (r - 1.0f, a) }, i % 5 == 0 ? 1.6f : 1.0f);
    }

    // shadow cast on the enamel
    g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.35f * alpha), c.x + 2.0f, c.y + base * 0.5f,
                                             juce::Colours::transparentBlack, c.x + 2.0f, c.y + base * 1.9f, true));
    g.fillEllipse (c.x - base * 1.4f, c.y - base * 0.9f, base * 2.9f, base * 2.9f);

    // chrome ring and bakelite skirt
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffeef0f3).withAlpha (alpha), c.x - base, c.y - base,
                                             juce::Colour (0xff6c7077).withAlpha (alpha), c.x + base, c.y + base, false));
    g.fillEllipse (c.x - base - 2.5f, c.y - base - 2.5f, (base + 2.5f) * 2.0f, (base + 2.5f) * 2.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a3a3d).withAlpha (alpha), c.x - base * 0.5f, c.y - base,
                                             juce::Colour (0xff050506).withAlpha (alpha), c.x + base * 0.5f, c.y + base, false));
    g.fillEllipse (c.x - base, c.y - base, base * 2.0f, base * 2.0f);

    // pointer head
    const float len = base * 1.40f;
    juce::Path wedge;
    wedge.startNewSubPath (-base * 0.50f, base * 0.60f);
    wedge.lineTo (base * 0.50f, base * 0.60f);
    wedge.lineTo (base * 0.14f, -len);
    wedge.lineTo (-base * 0.14f, -len);
    wedge.closeSubPath();
    wedge = wedge.createPathWithRoundedCorners (base * 0.16f);
    const auto t = juce::AffineTransform::rotation (angle).translated (c.x, c.y);
    g.setColour (juce::Colours::black.withAlpha (0.35f * alpha));
    g.fillPath (wedge, t.translated (1.5f, 2.5f));
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4a4a4e).withAlpha (alpha), c.x - base, c.y - base,
                                             juce::Colour (0xff0a0a0b).withAlpha (alpha), c.x + base, c.y + base, false));
    g.fillPath (wedge, t);
    g.setColour (juce::Colours::white.withAlpha (0.12f * alpha));
    g.strokePath (wedge, juce::PathStrokeType (1.0f), t);

    // white indicator line
    juce::Path mark;
    mark.startNewSubPath (0.0f, -len + 3.0f);
    mark.lineTo (0.0f, -len * 0.30f);
    g.setColour (juce::Colour (0xfff4f1e8).withAlpha (alpha));
    g.strokePath (mark, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded), t);
}

void Key::paintButton (juce::Graphics& g, bool over, bool down)
{
    const bool on = getToggleState();
    auto r = getLocalBounds().toFloat().reduced (5.0f);
    if (down) r = r.reduced (1.0f);

    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillRoundedRectangle (r.expanded (3.0f).translated (0.0f, 1.5f), 8.0f);
    if (on)
    {
        for (float e = 6.0f; e > 0.0f; e -= 1.5f)
        {
            g.setColour (sky.withAlpha (0.07f));
            g.fillRoundedRectangle (r.expanded (e), 7.0f + e);
        }
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffa9d0ff), r.getCentreX(), r.getY(),
                                                 juce::Colour (0xff2f6fc4), r.getCentreX(), r.getBottom(), false));
    }
    else
        g.setGradientFill (juce::ColourGradient (juce::Colour (over ? 0xff3e3f44 : 0xff2e2f33), r.getCentreX(), r.getY(),
                                                 juce::Colour (0xff0b0b0d), r.getCentreX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (juce::Colours::white.withAlpha (on ? 0.6f : 0.14f));
    g.drawRoundedRectangle (r.reduced (1.0f), 5.0f, 1.0f);

    const auto txt = on ? juce::Colour (0xff0d1a33) : juce::Colour (0xfff1ede2);
    if (subLegend.isEmpty())
    {
        g.setColour (txt);
        g.setFont (font (13.0f, true, 0.14f));
        g.drawText (getButtonText(), r, juce::Justification::centred);
    }
    else
    {
        g.setColour (txt);
        g.setFont (font (20.0f, true, 0.0f));
        g.drawText (getButtonText(), r.withTrimmedBottom (r.getHeight() * 0.32f), juce::Justification::centredBottom);
        g.setColour (txt.withAlpha (0.75f));
        g.setFont (font (9.5f, true, 0.14f));
        g.drawText (subLegend, r.withTrimmedTop (r.getHeight() * 0.64f), juce::Justification::centredTop);
    }
}
}

//==============================================================================
FirmamentEditor::FirmamentEditor (FirmamentProcessor& p) : AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&look);

    for (int i = 0; i < fm::kNumModes; ++i)
    {
        auto* k = modeKeys.add (new fw::Key (juce::String::formatted ("%02d", i + 1), juce::String (modeHints[i]).toUpperCase()));
        k->setRadioGroupId (1);
        k->onClick = [this, i] { if (modeAtt != nullptr) modeAtt->setValueAsCompleteGesture ((float) i); };
        addAndMakeVisible (k);
    }

    const auto setup = [this] (juce::Slider& s, const char* id)
    {
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        s.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        s.setPopupDisplayEnabled (true, true, this);
        s.onValueChange = [this] { repaint(); };
        addAndMakeVisible (s);
        sliderAtts.add (new SliderAtt (proc.apvts, id, s));
    };
    const char* ids[6] = { "decay", "predelay", "tone", "mod", "character", "mix" };
    for (int i = 0; i < 6; ++i)
        setup (knobs[i], ids[i]);
    setup (ladder[0], "rise");
    setup (ladder[1], "fall");
    setup (ladder[2], "fine");

    // DECAY reads in real seconds for the current mode
    knobs[0].textFromValueFunction = [this] (double v)
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

    setSize (kWidth, kHeight);
    startTimerHz (30);
}

FirmamentEditor::~FirmamentEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void FirmamentEditor::modeChanged (int newMode)
{
    mode = juce::jlimit (0, fm::kNumModes - 1, newMode);
    for (int i = 0; i < modeKeys.size(); ++i)
        modeKeys[i]->setToggleState (i == mode, juce::dontSendNotification);
    for (auto& k : ladder)
        k.setEnabled (mode == fm::Ladder);
    if (getWidth() > 0)
        renderBackground();    // the CHARACTER label and ladder panel are printed on the face
    repaint();
}

void FirmamentEditor::timerCallback()
{
    const float db = juce::Decibels::gainToDecibels (proc.outputPeak.load (std::memory_order_relaxed), -90.0f);
    const float target = juce::jlimit (0.0f, 1.0f, (db + 30.0f) / 36.0f);
    needle += (target > needle ? 0.45f : 0.12f) * (target - needle);
    frozenLamp = freeze.getToggleState();
    repaint (vuBox);
    repaint (lampBox);
}

void FirmamentEditor::resized()
{
    face  = { 58, 14, kWidth - 116, kHeight - 28 };
    panel = { face.getX() + 22, 196, face.getWidth() - 44, 202 };

    // seven mode keys, edges lined up with the panel below
    const int ks = 74;
    const float pitch = (float) (panel.getWidth() + 4 - ks) / (float) (fm::kNumModes - 1);
    for (int i = 0; i < modeKeys.size(); ++i)
        modeKeys[i]->setBounds (panel.getX() - 2 + juce::roundToInt (pitch * (float) i), 96, ks, ks);

    // six main knobs in equal columns
    const int pad = 20, kSize = 118;
    const float col = (float) (panel.getWidth() - 2 * pad) / 6.0f;
    for (int i = 0; i < 6; ++i)
        knobs[i].setBounds (panel.getX() + pad + juce::roundToInt (col * ((float) i + 0.5f)) - kSize / 2, panel.getY() + 22, kSize, kSize);

    // bottom row: ladder fine-tune box, freeze, meter
    const int rowY = panel.getBottom() + 16, rowH = face.getBottom() - 18 - rowY;
    ladderBox = { panel.getX(), rowY, 360, rowH };
    const int lk = 78;
    const float lcol = (float) (ladderBox.getWidth() - 20) / 3.0f;
    for (int i = 0; i < 3; ++i)
        ladder[i].setBounds (ladderBox.getX() + 10 + juce::roundToInt (lcol * ((float) i + 0.5f)) - lk / 2, ladderBox.getY() + 26, lk, lk);

    vuBox = { panel.getRight() - 250, rowY + 4, 250, 110 };
    const int fx = ladderBox.getRight() + 24, fw2 = vuBox.getX() - 24 - fx;
    freeze.setBounds (fx + (fw2 - 112) / 2, rowY + 58, 112, 52);
    lampBox = { fx + (fw2 - 64) / 2, rowY - 4, 64, 64 };   // room for the glow

    renderBackground();
}

void FirmamentEditor::renderBackground()
{
    const float scale = 2.0f;
    background = juce::Image (juce::Image::RGB, juce::roundToInt ((float) getWidth() * scale), juce::roundToInt ((float) getHeight() * scale), true);
    juce::Graphics g (background);
    g.addTransform (juce::AffineTransform::scale (scale));
    drawFace (g);
}

void FirmamentEditor::drawFace (juce::Graphics& g)
{
    using namespace fw;
    const int W = getWidth(), H = getHeight();
    g.fillAll (rack);

    // rack ears
    for (int side = 0; side < 2; ++side)
    {
        const juce::Rectangle<float> ear (side == 0 ? 8.0f : (float) W - 62.0f, 14.0f, 54.0f, (float) H - 28.0f);
        fillSparkle (g, ear, 4.0f, 21 + side);
        g.setColour (juce::Colours::black.withAlpha (0.4f));
        g.drawRoundedRectangle (ear, 4.0f, 1.0f);
        for (float fy : { 0.30f, 0.70f })
        {
            const juce::Rectangle<float> slot (ear.getCentreX() - 8.0f, ear.getY() + ear.getHeight() * fy - 34.0f, 16.0f, 68.0f);
            g.setColour (juce::Colour (0xff050506));
            g.fillRoundedRectangle (slot, 8.0f);
        }
        drawScrew (g, ear.getCentreX(), ear.getY() + 26.0f, 8.0f);
        drawScrew (g, ear.getCentreX(), ear.getBottom() - 26.0f, 8.0f);
    }

    // faceplate: white enamel, blue flakes, navy pinstripe
    const auto f = face.toFloat();
    fillSparkle (g, f, 5.0f, 7);
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.drawRoundedRectangle (f, 5.0f, 1.2f);
    g.setColour (navy.withAlpha (0.55f));
    g.drawRoundedRectangle (f.reduced (4.0f), 4.0f, 1.0f);

    // nameplate
    const juce::Font serif (juce::FontOptions (juce::Font::getDefaultSerifFontName(), 50.0f, juce::Font::bold | juce::Font::italic));
    g.setFont (serif);
    g.setColour (juce::Colours::white);
    g.drawText ("Firmament", panel.getX() + 1, 22, 400, 52, juce::Justification::centredLeft);
    g.setColour (navy);
    g.drawText ("Firmament", panel.getX(), 20, 400, 52, juce::Justification::centredLeft);
    g.setFont (font (11.0f, true, 0.28f));
    g.setColour (navy.withAlpha (0.8f));
    g.drawText ("SEVEN-MODE REVERBERATION UNIT", panel.getX() + 3, 70, 400, 14, juce::Justification::centredLeft);

    // mode names under the keys
    g.setFont (font (12.5f, true, 0.06f));
    for (int i = 0; i < modeKeys.size(); ++i)
    {
        const auto b = modeKeys[i]->getBounds();
        g.setColour (i == mode ? navy : ink.withAlpha (0.75f));
        g.drawText (juce::String (modeTitles[i]).toUpperCase(), b.getX() - 14, b.getBottom() + 2, b.getWidth() + 28, 16, juce::Justification::centred);
    }

    // silk-screened outlines
    const auto outline = [&g] (juce::Rectangle<int> r, float a, juce::Rectangle<int> gap = {})
    {
        juce::Graphics::ScopedSaveState save (g);
        if (! gap.isEmpty()) g.excludeClipRegion (gap);
        g.setColour (navy.withAlpha (a));
        g.drawRoundedRectangle (r.toFloat(), 8.0f, 1.0f);
    };
    outline (panel, 0.45f);

    // knob legends
    for (int i = 0; i < 6; ++i)
    {
        const auto k = knobs[i].getBounds();
        g.setFont (font (14.0f, true, 0.10f));
        g.setColour (navy);
        g.drawText (i == 4 ? juce::String (fm::characterName (mode)) : juce::String (knobNames[i]),
                    k.getX() - 20, k.getBottom() + 4, k.getWidth() + 40, 18, juce::Justification::centred);
    }

    // ladder fine-tune box
    const bool isLadder = mode == fm::Ladder;
    const auto tr = juce::Rectangle<int> (ladderBox.getX() + 14, ladderBox.getY() - 7, 150, 14);
    outline (ladderBox, isLadder ? 0.8f : 0.3f, tr);
    {
        g.setFont (font (10.5f, true, 0.18f));
        g.setColour (isLadder ? navy : faint.withAlpha (0.6f));
        g.drawText ("LADDER  FINE TUNE", tr, juce::Justification::centred);
        const char* sub[3] = { "+ OCTAVE", "- OCTAVE", "FINE" };
        for (int i = 0; i < 3; ++i)
        {
            const auto k = ladder[i].getBounds();
            g.setFont (font (11.0f, true, 0.10f));
            g.setColour (isLadder ? navy : faint.withAlpha (0.55f));
            g.drawText (sub[i], k.getX() - 20, k.getBottom() + 2, k.getWidth() + 40, 14, juce::Justification::centred);
        }
    }

    // freeze label and lamp bezel
    g.setFont (font (10.0f, true, 0.14f));
    g.setColour (navy);
    g.drawText ("HOLD THE TAIL", freeze.getX() - 20, freeze.getBottom() + 4, freeze.getWidth() + 40, 12, juce::Justification::centred);
    {
        const auto c = lampBox.toFloat().getCentre();
        const float r = 16.0f;
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff2f3f5), c.x - r, c.y - r, juce::Colour (0xff6f7379), c.x + r, c.y + r, false));
        g.fillEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f);
        g.setColour (juce::Colours::black);
        g.fillEllipse (c.x - r + 3.5f, c.y - r + 3.5f, (r - 3.5f) * 2.0f, (r - 3.5f) * 2.0f);
    }

    // VU meter face
    {
        const Vu v (vuBox);
        const auto outer = vuBox.toFloat();
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff0f1f3), 0.0f, outer.getY(), juce::Colour (0xff6c7077), 0.0f, outer.getBottom(), false));
        g.fillRoundedRectangle (outer, 6.0f);
        g.setColour (juce::Colours::black);
        g.fillRoundedRectangle (outer.reduced (2.5f), 4.5f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xfffbf0c9), v.face.getCentreX(), v.face.getY(),
                                                 juce::Colour (0xffd9bd78), v.face.getCentreX(), v.face.getBottom() + 30.0f, true));
        g.fillRoundedRectangle (v.face, 3.0f);

        juce::Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (v.face.toNearestInt());
        const juce::Colour vin (0xff231a10), red (0xffb3261a);
        const float redFrom = 5.0f / 6.0f;
        juce::Path arc, hot;
        arc.addCentredArc (v.pivot.x, v.pivot.y, v.radius, v.radius, 0.0f, v.angle (0.0f), v.angle (redFrom), true);
        g.setColour (vin);
        g.strokePath (arc, juce::PathStrokeType (1.4f));
        hot.addCentredArc (v.pivot.x, v.pivot.y, v.radius + 1.5f, v.radius + 1.5f, 0.0f, v.angle (redFrom), v.angle (1.0f), true);
        g.setColour (red);
        g.strokePath (hot, juce::PathStrokeType (4.0f));
        const char* marks[7] = { "-30", "-24", "-18", "-12", "-6", "0", "+3" };
        g.setFont (font (9.5f, true, 0.0f));
        for (int i = 0; i < 7; ++i)
        {
            const float fr = (float) i / 6.0f;
            const float a = v.angle (fr);
            g.setColour (fr >= redFrom - 0.001f ? red : vin);
            g.drawLine ({ v.pivot.getPointOnCircumference (v.radius, a), v.pivot.getPointOnCircumference (v.radius + 6.0f, a) }, 1.4f);
            const auto tp = v.pivot.getPointOnCircumference (v.radius + 14.0f, a);
            g.drawText (marks[i], juce::roundToInt (tp.x) - 16, juce::roundToInt (tp.y) - 6, 32, 12, juce::Justification::centred);
        }
        g.setColour (vin);
        g.setFont (font (9.5f, true, 0.16f));
        g.drawText ("OUTPUT", v.face.withTrimmedTop (v.face.getHeight() - 17.0f), juce::Justification::centred);
    }

    // faceplate screws
    drawScrew (g, f.getX() + 14.0f, f.getY() + 14.0f, 5.5f);
    drawScrew (g, f.getRight() - 14.0f, f.getY() + 14.0f, 5.5f);
    drawScrew (g, f.getX() + 14.0f, f.getBottom() - 14.0f, 5.5f);
    drawScrew (g, f.getRight() - 14.0f, f.getBottom() - 14.0f, 5.5f);
}

void FirmamentEditor::paint (juce::Graphics& g)
{
    using namespace fw;
    g.drawImage (background, getLocalBounds().toFloat());

    // live value readouts under each knob
    g.setFont (font (11.0f, false, 0.02f));
    g.setColour (ink.withAlpha (0.8f));
    for (auto& k : knobs)
    {
        const auto b = k.getBounds();
        g.drawText (k.getTextFromValue (k.getValue()), b.getX() - 20, b.getBottom() + 22, b.getWidth() + 40, 14, juce::Justification::centred);
    }
    if (mode == fm::Ladder)
        for (auto& k : ladder)
        {
            const auto b = k.getBounds();
            g.drawText (k.getTextFromValue (k.getValue()), b.getX() - 20, b.getBottom() + 16, b.getWidth() + 40, 13, juce::Justification::centred);
        }

    // needle
    {
        const Vu v (vuBox);
        juce::Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (v.face.withTrimmedBottom (17.0f).toNearestInt());
        const float a = v.angle (needle);
        g.setColour (juce::Colours::black.withAlpha (0.18f));
        g.drawLine ({ v.pivot.translated (2.0f, 2.0f), v.pivot.getPointOnCircumference (v.radius + 7.0f, a).translated (2.0f, 2.0f) }, 2.0f);
        g.setColour (juce::Colour (0xff1a1410));
        g.drawLine ({ v.pivot, v.pivot.getPointOnCircumference (v.radius + 7.0f, a) }, 1.6f);
    }

    // blue jewel: lit while frozen
    {
        const auto c = lampBox.toFloat().getCentre();
        const float r = 11.0f;
        if (frozenLamp)
        {
            g.setColour (sky.withAlpha (0.30f));
            g.fillEllipse (c.x - r * 2.4f, c.y - r * 2.4f, r * 4.8f, r * 4.8f);
        }
        g.setGradientFill (juce::ColourGradient (frozenLamp ? juce::Colour (0xffd8ecff) : juce::Colour (0xff23426e), c.x - 3.0f, c.y - 4.0f,
                                                 frozenLamp ? juce::Colour (0xff2b7be8) : juce::Colour (0xff0a1628), c.x + r, c.y + r, true));
        g.fillEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f);
        g.setColour (juce::Colours::white.withAlpha (frozenLamp ? 0.75f : 0.3f));
        g.fillEllipse (c.x - r * 0.55f, c.y - r * 0.60f, r * 0.5f, r * 0.35f);
    }
}
