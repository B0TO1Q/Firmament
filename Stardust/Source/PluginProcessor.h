#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "StardustEngine.h"

class StardustProcessor final : public juce::AudioProcessor
{
public:
    StardustProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "STARDUST"; }
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

    sd::Params readParams() const;

    juce::AudioProcessorValueTreeState apvts;
    sd::Engine engine;
    std::atomic<double> hostBpm { 120.0 };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    std::vector<float> scratch;
    std::atomic<float>* pFx[7] = {};
    std::atomic<float>* pRate = nullptr, *pDir = nullptr, *pFreeze = nullptr, *pFilter = nullptr, *pMix = nullptr, *pOut = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StardustProcessor)
};
