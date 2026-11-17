#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "ChannelEngine.h"

class SelahProcessor final : public juce::AudioProcessor
{
public:
    SelahProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Selah"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 6.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    sl::Params readParams() const;
    static juce::String eqParamId (int mode, int band) { return "eq" + juce::String (sl::eqBandCount (mode)) + "_" + juce::String (band); }
    static juce::String eqFreqText (float hz);

    juce::AudioProcessorValueTreeState apvts;
    sl::ChannelEngine engine;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    std::vector<float> scratch;
    std::atomic<float>* pSource = nullptr, *pClean = nullptr, *pWarm = nullptr, *pClarity = nullptr,
                      *pEven = nullptr, *pSpace = nullptr, *pVolume = nullptr, *pRevLen = nullptr, *pLimiter = nullptr,
                      *pEqMode = nullptr, *pEqPost = nullptr, *pWidth = nullptr;
    std::atomic<float>* pEq[3][sl::kMaxEqBands] = {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SelahProcessor)
};
