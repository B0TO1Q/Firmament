#include "PluginEditor.h"

namespace
{
constexpr int kWidth = 1000, kHeight = 500;

const char* const fxIds[7]   = { "drive", "glow", "orchestra", "stutter", "shimmer", "nebula", "pump" };
const char* const fxNames[7] = { "DRIVE", "GLOW", "ORCHESTRA", "STUTTER", "SHIMMER", "NEBULA", "PUMP" };
const char* const fxHints[7] = { "grit to fuzz", "sparkle and air", "one part to a section", "beat repeat",
                                 "climbing octaves", "endless space", "house sidechain" };

void drawStar (juce::Graphics& g, juce::Point<float> c, float r, juce::Colour col)
{
    juce::Path s;
    s.startNewSubPath (c.x, c.y - r);
    s.quadraticTo (c.x, c.y, c.x + r, c.y);
    s.quadraticTo (c.x, c.y, c.x, c.y + r);
    s.quadraticTo (c.x, c.y, c.x - r, c.y);
    s.quadraticTo (c.x, c.y, c.x, c.y - r);
    s.closeSubPath();
    g.setColour (col);
    g.fillPath (s);
}

// Midnight metal-flake: silver, blue and violet flecks, a few full stars.
void fillCosmic (juce::Graphics& g, juce::Rectangle<float> r, float corner, int seed)
{
    juce::Path clip;
    clip.addRoundedRectangle (r, corner);
    juce::Graphics::ScopedSaveState save (g);
    g.reduceClipRegion (clip);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1b1a36), r.getX() + r.getWidth() * 0.3f, r.getY(),
                                             juce::Colour (0xff08080f), r.getRight(), r.getBottom(), true));
    g.fillRect (r);
    juce::Random rnd (seed);
    const int flakes = juce::roundToInt (r.getWidth() * r.getHeight() / 60.0f);
    const juce::Colour pal[4] = { st::silver, st::silver, st::violet, juce::Colour (0xff8fb4ff) };
    for (int i = 0; i < flakes; ++i)
    {
        const float x = r.getX() + rnd.nextFloat() * r.getWidth();
        const float y = r.getY() + rnd.nextFloat() * r.getHeight();
        const float b = rnd.nextFloat();
        const float bright = b * b * b;
        const float size = 0.5f + 0.9f * rnd.nextFloat();
        const auto c = pal[rnd.nextInt (4)];
        g.setColour (c.withAlpha (0.10f + 0.85f * bright));
        g.fillEllipse (x, y, size, size);
        if (bright > 0.95f)
            drawStar (g, { x + size * 0.5f, y + size * 0.5f }, 2.5f + 2.5f * rnd.nextFloat(), c.withAlpha (0.8f));
    }
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.06f), 0.0f, r.getY(),
                                             juce::Colours::transparentWhite, 0.0f, r.getY() + r.getHeight() * 0.35f, false));
    g.fillRect (r);
}

void drawScrew (juce::Graphics& g, float cx, float cy, float r)
{
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillEllipse (cx - r - 1.0f, cy - r, r * 2.0f + 2.0f, r * 2.0f + 2.5f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffd0d2da), cx - r * 0.5f, cy - r * 0.6f,
                                             juce::Colour (0xff3a3c46), cx + r, cy + r, true));
    g.fillEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f);
    g.setColour (juce::Colour (0xff15151c));
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
namespace st
{
juce::Font font (float height, bool bold, float kerning)
{
    return juce::Font (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain)).withExtraKerningFactor (kerning);
}

void Look::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                             float startAngle, float endAngle, juce::Slider& s)
{
    const auto area = juce::Rectangle<int> (x, y, w, h).toFloat();
    const float r = juce::jmin (area.getWidth(), area.getHeight()) * 0.5f - 2.0f;
    const auto c = area.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);
    const bool bipolar = s.getMinimum() < 0.0;
    const float body = r * 0.66f;

    // tick ring
    for (int i = 0; i <= 10; ++i)
    {
        const float a = startAngle + (endAngle - startAngle) * (float) i / 10.0f;
        const bool major = i % 5 == 0;
        g.setColour (silver.withAlpha (major ? 0.8f : 0.35f));
        g.drawLine ({ c.getPointOnCircumference (r - (major ? 5.0f : 3.0f), a), c.getPointOnCircumference (r, a) }, major ? 1.5f : 1.0f);
    }

    // violet glow ring showing the setting
    const float ringR = body + 6.0f;
    const float from = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;
    juce::Path track, arc;
    track.addCentredArc (c.x, c.y, ringR, ringR, 0.0f, startAngle, endAngle, true);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    if (std::abs (angle - from) > 0.01f)
    {
        arc.addCentredArc (c.x, c.y, ringR, ringR, 0.0f, juce::jmin (from, angle), juce::jmax (from, angle), true);
        for (float wdt : { 10.0f, 6.0f })
        {
            g.setColour (violet.withAlpha (wdt > 8.0f ? 0.08f : 0.18f));
            g.strokePath (arc, juce::PathStrokeType (wdt, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        g.setColour (violet.brighter (0.3f));
        g.strokePath (arc, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // shadow
    g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.7f), c.x, c.y + body * 0.3f,
                                             juce::Colours::transparentBlack, c.x, c.y + body * 1.6f, true));
    g.fillEllipse (c.x - body * 1.3f, c.y - body * 0.9f, body * 2.6f, body * 2.6f);

    // black ribbed body
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a3a40), c.x - body * 0.4f, c.y - body,
                                             juce::Colour (0xff060608), c.x + body * 0.4f, c.y + body, false));
    g.fillEllipse (c.x - body, c.y - body, body * 2.0f, body * 2.0f);
    for (int i = 0; i < 36; ++i)
    {
        const float a = angle + juce::MathConstants<float>::twoPi * (float) i / 36.0f;
        g.setColour (juce::Colours::white.withAlpha (0.07f));
        g.drawLine ({ c.getPointOnCircumference (body * 0.80f, a), c.getPointOnCircumference (body * 0.99f, a) }, 1.6f);
    }
    g.setColour (juce::Colours::black);
    g.drawEllipse (c.x - body, c.y - body, body * 2.0f, body * 2.0f, 1.0f);

    // brushed aluminium cap
    const float cap = body * 0.74f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffe6e7ec), c.x - cap, c.y - cap,
                                             juce::Colour (0xff8a8c96), c.x + cap, c.y + cap, false));
    g.fillEllipse (c.x - cap, c.y - cap, cap * 2.0f, cap * 2.0f);
    for (float rr = cap * 0.15f; rr < cap; rr += 2.0f)   // concentric machining rings
    {
        g.setColour (juce::Colours::black.withAlpha (0.05f));
        g.drawEllipse (c.x - rr, c.y - rr, rr * 2.0f, rr * 2.0f, 0.6f);
    }
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.drawEllipse (c.x - cap, c.y - cap, cap * 2.0f, cap * 2.0f, 1.0f);

    // index line across the cap and onto the body
    g.setColour (juce::Colour (0xff101014));
    g.drawLine ({ c.getPointOnCircumference (cap * 0.15f, angle), c.getPointOnCircumference (cap * 0.92f, angle) }, 2.6f);
    g.setColour (cream);
    g.drawLine ({ c.getPointOnCircumference (cap * 1.04f, angle), c.getPointOnCircumference (body * 0.96f, angle) }, 2.4f);
}

void Key::paintButton (juce::Graphics& g, bool over, bool down)
{
    const bool on = getToggleState();
    auto r = getLocalBounds().toFloat().reduced (4.0f);
    if (down) r = r.reduced (0.8f);
    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.fillRoundedRectangle (r.expanded (2.5f), 6.0f);
    if (on)
    {
        for (float e = 6.0f; e > 0.0f; e -= 1.5f)
        {
            g.setColour (violet.withAlpha (0.08f));
            g.fillRoundedRectangle (r.expanded (e), 5.0f + e);
        }
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffd6bcff), r.getCentreX(), r.getY(),
                                                 juce::Colour (0xff6f37d6), r.getCentreX(), r.getBottom(), false));
    }
    else
        g.setGradientFill (juce::ColourGradient (juce::Colour (over ? 0xff34344a : 0xff292a3a), r.getCentreX(), r.getY(),
                                                 juce::Colour (0xff0d0d16), r.getCentreX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, 4.5f);
    g.setColour (juce::Colours::white.withAlpha (on ? 0.55f : 0.12f));
    g.drawRoundedRectangle (r.reduced (1.0f), 4.0f, 1.0f);
    g.setColour (on ? juce::Colour (0xff1a0b38) : silver.withAlpha (0.9f));
    g.setFont (font (12.0f, true, 0.10f));
    g.drawText (getButtonText(), r, juce::Justification::centred);
}
}

//==============================================================================
StardustEditor::StardustEditor (StardustProcessor& p) : AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&look);

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
    for (int i = 0; i < 7; ++i) setup (fx[i], fxIds[i]);
    setup (filter, "filter");
    setup (mix, "mix");
    setup (output, "output");
    filter.setDoubleClickReturnValue (true, 0.0);

    const char* rates[4] = { "1/4", "1/8", "1/16", "1/32" };
    for (int i = 0; i < 4; ++i)
    {
        auto* k = rateKeys.add (new st::Key (rates[i]));
        k->setRadioGroupId (1);
        k->onClick = [this, i] { if (rateAtt != nullptr) rateAtt->setValueAsCompleteGesture ((float) i); };
        addAndMakeVisible (k);
    }
    const char* dirs[3] = { "UP", "DOWN", "BOTH" };
    for (int i = 0; i < 3; ++i)
    {
        auto* k = dirKeys.add (new st::Key (dirs[i]));
        k->setRadioGroupId (2);
        k->onClick = [this, i] { if (dirAtt != nullptr) dirAtt->setValueAsCompleteGesture ((float) i); };
        addAndMakeVisible (k);
    }
    rateAtt = std::make_unique<juce::ParameterAttachment> (*proc.apvts.getParameter ("rate"), [this] (float v)
              { for (int i = 0; i < rateKeys.size(); ++i) rateKeys[i]->setToggleState (i == juce::roundToInt (v), juce::dontSendNotification); });
    dirAtt = std::make_unique<juce::ParameterAttachment> (*proc.apvts.getParameter ("direction"), [this] (float v)
              { for (int i = 0; i < dirKeys.size(); ++i) dirKeys[i]->setToggleState (i == juce::roundToInt (v), juce::dontSendNotification); });
    rateAtt->sendInitialUpdate();
    dirAtt->sendInitialUpdate();

    freeze.setClickingTogglesState (true);
    addAndMakeVisible (freeze);
    freezeAtt = std::make_unique<ButtonAtt> (proc.apvts, "freeze", freeze);

    setSize (kWidth, kHeight);
    startTimerHz (30);
}

StardustEditor::~StardustEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void StardustEditor::timerCallback()
{
    const float db = juce::Decibels::gainToDecibels (proc.engine.outputPeak(), -90.0f);
    const float target = juce::jlimit (0.0f, 1.0f, (db + 30.0f) / 36.0f);
    needle += (target > needle ? 0.45f : 0.12f) * (target - needle);
    repaint (vuBox);
    const int bpm = juce::roundToInt (proc.hostBpm.load (std::memory_order_relaxed));
    if (bpm != bpmShown) { bpmShown = bpm; repaint (bpmBox); }
}

void StardustEditor::resized()
{
    face = { 58, 14, kWidth - 116, kHeight - 28 };
    topPanel = { face.getX() + 20, 108, face.getWidth() - 40, 214 };
    bottomPanel = { topPanel.getX(), topPanel.getBottom() + 14, topPanel.getWidth(), face.getBottom() - 18 - topPanel.getBottom() - 14 };

    vuBox  = { topPanel.getRight() - 220, 24, 220, 78 };
    bpmBox = { vuBox.getX() - 112, 36, 96, 46 };

    // seven effect knobs in equal columns
    const int kSize = 108;
    const float col = (float) topPanel.getWidth() / 7.0f;
    for (int i = 0; i < 7; ++i)
    {
        const int cx = topPanel.getX() + juce::roundToInt (col * ((float) i + 0.5f));
        lamps[i] = { cx - 9, topPanel.getY() + 22, 18, 18 };
        fx[i].setBounds (cx - kSize / 2, topPanel.getY() + 46, kSize, kSize);
    }

    // bottom row: stutter rate, shimmer direction, freeze, filter, mix, output
    const int by = bottomPanel.getY() + 44;
    for (int i = 0; i < 4; ++i)
        rateKeys[i]->setBounds (bottomPanel.getX() + 18 + i * 52, by, 52, 40);
    const int dx = bottomPanel.getX() + 18 + 4 * 52 + 30;
    for (int i = 0; i < 3; ++i)
        dirKeys[i]->setBounds (dx + i * 62, by, 62, 40);
    freeze.setBounds (dx + 3 * 62 + 30, by, 96, 40);

    const int kx0 = freeze.getRight() + 24, kw = (bottomPanel.getRight() - 12 - kx0) / 3;
    juce::Slider* small[3] = { &filter, &mix, &output };
    for (int i = 0; i < 3; ++i)
        small[i]->setBounds (kx0 + i * kw + (kw - 80) / 2, bottomPanel.getY() + 8, 80, 80);

    renderBackground();
}

void StardustEditor::renderBackground()
{
    const float scale = 2.0f;
    background = juce::Image (juce::Image::RGB, juce::roundToInt ((float) getWidth() * scale), juce::roundToInt ((float) getHeight() * scale), true);
    juce::Graphics g (background);
    g.addTransform (juce::AffineTransform::scale (scale));
    drawFace (g);
}

void StardustEditor::drawFace (juce::Graphics& g)
{
    using namespace st;
    const int W = getWidth(), H = getHeight();
    g.fillAll (juce::Colour (0xff050508));

    for (int side = 0; side < 2; ++side)
    {
        const juce::Rectangle<float> ear (side == 0 ? 8.0f : (float) W - 62.0f, 14.0f, 54.0f, (float) H - 28.0f);
        fillCosmic (g, ear, 4.0f, 31 + side);
        g.setColour (juce::Colours::white.withAlpha (0.10f));
        g.drawRoundedRectangle (ear, 4.0f, 1.0f);
        for (float fy : { 0.30f, 0.70f })
        {
            const juce::Rectangle<float> slot (ear.getCentreX() - 8.0f, ear.getY() + ear.getHeight() * fy - 34.0f, 16.0f, 68.0f);
            g.setColour (juce::Colour (0xff020203));
            g.fillRoundedRectangle (slot, 8.0f);
        }
        drawScrew (g, ear.getCentreX(), ear.getY() + 26.0f, 8.0f);
        drawScrew (g, ear.getCentreX(), ear.getBottom() - 26.0f, 8.0f);
    }

    const auto f = face.toFloat();
    fillCosmic (g, f, 5.0f, 9);
    g.setColour (juce::Colours::white.withAlpha (0.12f));
    g.drawRoundedRectangle (f, 5.0f, 1.0f);
    g.setColour (silver.withAlpha (0.45f));
    g.drawRoundedRectangle (f.reduced (4.0f), 4.0f, 1.0f);

    // nameplate: chrome lettering, double rule underneath
    g.setFont (font (44.0f, true, 0.46f));
    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.drawText ("STARDUST", topPanel.getX() + 2, 22, 470, 50, juce::Justification::centredLeft);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffffffff), 0.0f, 26.0f, juce::Colour (0xff7d8090), 0.0f, 66.0f, false));
    g.drawText ("STARDUST", topPanel.getX(), 20, 470, 50, juce::Justification::centredLeft);
    g.setColour (silver.withAlpha (0.55f));
    g.fillRect ((float) topPanel.getX() + 2.0f, 72.0f, 440.0f, 1.0f);
    g.fillRect ((float) topPanel.getX() + 2.0f, 75.0f, 440.0f, 0.6f);
    drawStar (g, { (float) topPanel.getX() + 458.0f, 40.0f }, 8.0f, cream);
    g.setFont (font (10.5f, true, 0.34f));
    g.setColour (cream.withAlpha (0.75f));
    g.drawText ("ATMOSPHERIC EFFECTS SYSTEM", topPanel.getX() + 3, 82, 460, 14, juce::Justification::centredLeft);

    // panels
    for (auto r : { topPanel, bottomPanel })
    {
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillRoundedRectangle (r.toFloat(), 8.0f);
        g.setColour (silver.withAlpha (0.30f));
        g.drawRoundedRectangle (r.toFloat(), 8.0f, 1.0f);
    }

    // numbers, names and hints for the seven effects
    for (int i = 0; i < 7; ++i)
    {
        const auto k = fx[i].getBounds();
        const auto l = lamps[i].toFloat();
        g.setColour (juce::Colours::black);
        g.fillEllipse (l.expanded (2.5f));
        g.setColour (silver.withAlpha (0.35f));
        g.drawEllipse (l.expanded (2.5f), 1.0f);
        g.setFont (font (10.0f, true, 0.1f));
        g.setColour (silver.withAlpha (0.6f));
        g.drawText (juce::String (i + 1), k.getX(), topPanel.getY() + 6, k.getWidth(), 12, juce::Justification::centred);
        g.setFont (font (14.0f, true, 0.16f));
        g.setColour (cream);
        g.drawText (fxNames[i], k.getX() - 30, k.getBottom() + 4, k.getWidth() + 60, 18, juce::Justification::centred);
        g.setFont (font (10.5f, false, 0.02f));
        g.setColour (silver.withAlpha (0.65f));
        g.drawText (fxHints[i], k.getX() - 30, k.getBottom() + 23, k.getWidth() + 60, 14, juce::Justification::centred);
    }

    // bottom row legends
    g.setFont (font (10.5f, true, 0.2f));
    g.setColour (cream.withAlpha (0.85f));
    g.drawText ("STUTTER RATE", rateKeys[0]->getX(), bottomPanel.getY() + 22, 208, 14, juce::Justification::centred);
    g.drawText ("SHIMMER", dirKeys[0]->getX(), bottomPanel.getY() + 22, 186, 14, juce::Justification::centred);
    g.drawText ("NEBULA", freeze.getX(), bottomPanel.getY() + 22, freeze.getWidth(), 14, juce::Justification::centred);
    const char* smallNames[3] = { "FILTER", "MIX", "OUTPUT" };
    juce::Slider* small[3] = { &filter, &mix, &output };
    for (int i = 0; i < 3; ++i)
    {
        const auto k = small[i]->getBounds();
        g.setColour (cream);
        g.setFont (font (11.5f, true, 0.14f));
        g.drawText (smallNames[i], k.getX() - 20, k.getBottom() + 2, k.getWidth() + 40, 14, juce::Justification::centred);
    }
    g.setFont (font (9.0f, true, 0.1f));
    g.setColour (silver.withAlpha (0.6f));
    g.drawText ("LP", filter.getX() - 4, filter.getBottom() - 14, 20, 10, juce::Justification::centredLeft);
    g.drawText ("HP", filter.getRight() - 16, filter.getBottom() - 14, 20, 10, juce::Justification::centredLeft);

    // tempo window bezel
    {
        const auto b = bpmBox.toFloat();
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffd8dae2), 0.0f, b.getY(), juce::Colour (0xff4a4c56), 0.0f, b.getBottom(), false));
        g.fillRoundedRectangle (b, 5.0f);
        g.setColour (juce::Colour (0xff050c0c));
        g.fillRoundedRectangle (b.reduced (3.0f), 3.5f);
        g.setFont (font (9.0f, true, 0.16f));
        g.setColour (silver.withAlpha (0.8f));
        g.drawText ("TEMPO", bpmBox.getX(), bpmBox.getBottom() + 3, bpmBox.getWidth(), 12, juce::Justification::centred);
    }

    // VU face
    {
        const Vu v (vuBox);
        const auto outer = vuBox.toFloat();
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffd8dae2), 0.0f, outer.getY(), juce::Colour (0xff4a4c56), 0.0f, outer.getBottom(), false));
        g.fillRoundedRectangle (outer, 6.0f);
        g.setColour (juce::Colours::black);
        g.fillRoundedRectangle (outer.reduced (2.5f), 4.5f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffeef5ff), v.face.getCentreX(), v.face.getY(),
                                                 juce::Colour (0xff8fb2e0), v.face.getCentreX(), v.face.getBottom() + 30.0f, true));
        g.fillRoundedRectangle (v.face, 3.0f);
        juce::Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (v.face.toNearestInt());
        const juce::Colour vin (0xff13213d), red (0xffc0302a);
        const float redFrom = 5.0f / 6.0f;
        juce::Path arc, hot;
        arc.addCentredArc (v.pivot.x, v.pivot.y, v.radius, v.radius, 0.0f, v.angle (0.0f), v.angle (redFrom), true);
        g.setColour (vin);
        g.strokePath (arc, juce::PathStrokeType (1.4f));
        hot.addCentredArc (v.pivot.x, v.pivot.y, v.radius + 1.5f, v.radius + 1.5f, 0.0f, v.angle (redFrom), v.angle (1.0f), true);
        g.setColour (red);
        g.strokePath (hot, juce::PathStrokeType (4.0f));
        const char* marks[7] = { "-30", "-24", "-18", "-12", "-6", "0", "+3" };
        g.setFont (font (9.0f, true, 0.0f));
        for (int i = 0; i < 7; ++i)
        {
            const float fr = (float) i / 6.0f;
            const float a = v.angle (fr);
            g.setColour (fr >= redFrom - 0.001f ? red : vin);
            g.drawLine ({ v.pivot.getPointOnCircumference (v.radius, a), v.pivot.getPointOnCircumference (v.radius + 5.0f, a) }, 1.3f);
            const auto tp = v.pivot.getPointOnCircumference (v.radius + 12.0f, a);
            g.drawText (marks[i], juce::roundToInt (tp.x) - 16, juce::roundToInt (tp.y) - 6, 32, 12, juce::Justification::centred);
        }
        g.setColour (vin);
        g.setFont (font (9.0f, true, 0.16f));
        g.drawText ("OUTPUT", v.face.withTrimmedTop (v.face.getHeight() - 15.0f), juce::Justification::centred);
    }

    drawScrew (g, f.getX() + 14.0f, f.getY() + 14.0f, 5.5f);
    drawScrew (g, f.getRight() - 14.0f, f.getY() + 14.0f, 5.5f);
    drawScrew (g, f.getX() + 14.0f, f.getBottom() - 14.0f, 5.5f);
    drawScrew (g, f.getRight() - 14.0f, f.getBottom() - 14.0f, 5.5f);
}

void StardustEditor::paint (juce::Graphics& g)
{
    using namespace st;
    g.drawImage (background, getLocalBounds().toFloat());

    // a jewel above each effect lights violet when it is in use
    for (int i = 0; i < 7; ++i)
    {
        const float v = (float) fx[i].getValue();
        const auto l = lamps[i].toFloat();
        const auto c = l.getCentre();
        if (v > 0.001f)
        {
            g.setColour (violet.withAlpha (0.12f + 0.25f * v));
            g.fillEllipse (l.expanded (6.0f + 4.0f * v));
        }
        g.setGradientFill (juce::ColourGradient (v > 0.001f ? juce::Colour (0xffeedcff) : juce::Colour (0xff3a2a5c), c.x - 2.0f, c.y - 3.0f,
                                                 v > 0.001f ? juce::Colour (0xff7a3cf0) : juce::Colour (0xff120a22), c.x + 9.0f, c.y + 9.0f, true));
        g.fillEllipse (l);
        g.setColour (juce::Colours::white.withAlpha (v > 0.001f ? 0.7f : 0.25f));
        g.fillEllipse (c.x - 4.5f, c.y - 5.0f, 4.5f, 3.0f);
    }

    // live readouts for the small knobs
    g.setFont (font (10.5f, false, 0.02f));
    g.setColour (silver.withAlpha (0.75f));
    for (auto* s : { &filter, &mix, &output })
        g.drawText (s->getTextFromValue (s->getValue()), s->getX() - 20, s->getBottom() + 17, s->getWidth() + 40, 12, juce::Justification::centred);

    // tempo in amber digits
    g.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 24.0f, juce::Font::bold)));
    g.setColour (juce::Colour (0xff5ff2d6).withAlpha (0.12f));
    g.drawText ("888", bpmBox.reduced (6, 4), juce::Justification::centred);
    g.setColour (juce::Colour (0xff5ff2d6).withAlpha (0.25f));
    g.drawText (juce::String (bpmShown).paddedLeft ('0', 3), bpmBox.reduced (6, 4).translated (0, 1), juce::Justification::centred);
    g.setColour (juce::Colour (0xff8afff0));
    g.drawText (juce::String (bpmShown).paddedLeft ('0', 3), bpmBox.reduced (6, 4), juce::Justification::centred);

    // needle
    {
        const Vu v (vuBox);
        juce::Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (v.face.withTrimmedBottom (15.0f).toNearestInt());
        const float a = v.angle (needle);
        g.setColour (juce::Colours::black.withAlpha (0.18f));
        g.drawLine ({ v.pivot.translated (2.0f, 2.0f), v.pivot.getPointOnCircumference (v.radius + 6.0f, a).translated (2.0f, 2.0f) }, 2.0f);
        g.setColour (juce::Colour (0xff0f1526));
        g.drawLine ({ v.pivot, v.pivot.getPointOnCircumference (v.radius + 6.0f, a) }, 1.6f);
    }
}
