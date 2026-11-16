#pragma once

#include "PluginProcessor.h"
#include "../Shared/ui/Widgets.h"

class FirmamentEditor final : public juce::AudioProcessorEditor
{
public:
    explicit FirmamentEditor (FirmamentProcessor&);
    ~FirmamentEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;

    void modeChanged (int newMode);

    FirmamentProcessor& proc;
    ui::Look look;

    juce::OwnedArray<juce::TextButton> modeButtons;
    ui::Knob decay { "DECAY" }, preDelay { "PRE-DELAY" }, tone { "TONE" }, mod { "MOD" },
             character { "CHARACTER" }, mix { "MIX" };
    ui::Knob rise { "RISE  +OCT" }, fall { "FALL  -OCT" }, fine { "FINE" };
    juce::TextButton freeze { "FREEZE" };

    juce::OwnedArray<SliderAtt> sliderAtts;
    std::unique_ptr<ButtonAtt> freezeAtt;
    std::unique_ptr<juce::ParameterAttachment> modeAtt;

    int mode = 1;
    juce::Rectangle<int> ladderBox;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FirmamentEditor)
};
