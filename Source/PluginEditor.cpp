#include "PluginEditor.h"

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
