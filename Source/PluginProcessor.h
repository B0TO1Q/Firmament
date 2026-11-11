#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "dsp/ReverbEngine.h"

class FirmamentProcessor final : public juce::AudioProcessor
{
public:
    FirmamentProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Firmament"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override;

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    fm::Params readParams() const;

    fm::ReverbEngine engine;
    std::vector<float> scratch;

    std::atomic<float>* pMode = nullptr, *pDecay = nullptr, *pPre = nullptr, *pTone = nullptr,
                      *pMod = nullptr, *pMix = nullptr, *pChar = nullptr, *pFreeze = nullptr,
                      *pRise = nullptr, *pFall = nullptr, *pFine = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FirmamentProcessor)
};
