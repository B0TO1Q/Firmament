#include "PluginProcessor.h"
#include "PluginEditor.h"

using APF = juce::AudioParameterFloat;

static const char* const fxIds[7]   = { "drive", "glow", "orchestra", "stutter", "shimmer", "nebula", "pump" };
static const char* const fxNames[7] = { "Drive", "Glow", "Orchestra", "Stutter", "Shimmer", "Nebula", "Pump" };

juce::AudioProcessorValueTreeState::ParameterLayout StardustProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    const auto tenAttr = juce::AudioParameterFloatAttributes()
        .withStringFromValueFunction ([] (float v, int) { return juce::String (v * 10.0f, 1); })
        .withValueFromStringFunction ([] (const juce::String& s) { return s.getFloatValue() / 10.0f; });

    for (int i = 0; i < 7; ++i)
        l.add (std::make_unique<APF> (juce::ParameterID { fxIds[i], 1 }, fxNames[i], juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f, tenAttr));

    l.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "rate", 1 }, "Stutter Rate", juce::StringArray { "1/4", "1/8", "1/16", "1/32" }, 2));
    l.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "direction", 1 }, "Shimmer Direction", juce::StringArray { "Up", "Down", "Both" }, 0));
    l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "freeze", 1 }, "Freeze", false));
    l.add (std::make_unique<APF> (juce::ParameterID { "filter", 1 }, "Filter", juce::NormalisableRange<float> (-1.0f, 1.0f, 0.001f), 0.0f,
            juce::AudioParameterFloatAttributes()
                .withStringFromValueFunction ([] (float v, int)
                {
                    if (std::abs (v) <= 0.01f) return juce::String ("Off");
                    const float hz = v < 0.0f ? 20000.0f * std::pow (60.0f / 20000.0f, -v) : 20.0f * std::pow (9000.0f / 20.0f, v);
                    const auto f = hz >= 1000.0f ? juce::String (hz / 1000.0f, 1) + "k" : juce::String (juce::roundToInt (hz));
                    return juce::String (v < 0.0f ? "LP " : "HP ") + f;
                })));
    l.add (std::make_unique<APF> (juce::ParameterID { "mix", 1 }, "Mix", juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 1.0f,
            juce::AudioParameterFloatAttributes()
                .withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; })
                .withValueFromStringFunction ([] (const juce::String& s) { return s.getFloatValue() / 100.0f; })));
    l.add (std::make_unique<APF> (juce::ParameterID { "output", 1 }, "Output", juce::NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f,
            juce::AudioParameterFloatAttributes()
                .withStringFromValueFunction ([] (float v, int) { return (v > 0.05f ? "+" : "") + juce::String (v, 1) + " dB"; })
                .withValueFromStringFunction ([] (const juce::String& s) { return s.getFloatValue(); })));
    return l;
}

StardustProcessor::StardustProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STARDUST", createLayout())
{
    for (int i = 0; i < 7; ++i) pFx[i] = apvts.getRawParameterValue (fxIds[i]);
    pRate   = apvts.getRawParameterValue ("rate");
    pDir    = apvts.getRawParameterValue ("direction");
    pFreeze = apvts.getRawParameterValue ("freeze");
    pFilter = apvts.getRawParameterValue ("filter");
    pMix    = apvts.getRawParameterValue ("mix");
    pOut    = apvts.getRawParameterValue ("output");
}

sd::Params StardustProcessor::readParams() const
{
    sd::Params p;
    float* fx[7] = { &p.drive, &p.glow, &p.orchestra, &p.stutter, &p.shimmer, &p.nebula, &p.pump };
    for (int i = 0; i < 7; ++i) *fx[i] = pFx[i]->load();
    p.stutterRate = juce::jlimit (0, 3, juce::roundToInt (pRate->load()));
    p.shimmerDir  = juce::jlimit (0, 2, juce::roundToInt (pDir->load()));
    p.freeze      = pFreeze->load() > 0.5f;
    p.filter      = pFilter->load();
    p.mix         = pMix->load();
    p.outputDb    = pOut->load();
    return p;
}

void StardustProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate);
    engine.setParams (readParams());
    scratch.assign ((size_t) juce::jmax (16, samplesPerBlock), 0.0f);
    setLatencySamples (0);
}

bool StardustProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono())
        return false;
    if (in == juce::AudioChannelSet::stereo())
        return out == juce::AudioChannelSet::stereo();
    return in == juce::AudioChannelSet::mono();
}

double StardustProcessor::getTailLengthSeconds() const
{
    return pFreeze->load() > 0.5f ? std::numeric_limits<double>::infinity() : 30.0;
}

void StardustProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int numIn = getTotalNumInputChannels();
    const int numCh = buffer.getNumChannels();
    if (n == 0 || numCh == 0)
        return;

    sd::Transport t;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto bpm = pos->getBpm()) t.bpm = *bpm;
            if (auto ppq = pos->getPpqPosition()) t.ppq = *ppq;
            t.playing = pos->getIsPlaying();
        }
    hostBpm.store (t.bpm, std::memory_order_relaxed);

    engine.setParams (readParams());

    float* L = buffer.getWritePointer (0);
    float* R = nullptr;
    if (numCh >= 2)
    {
        R = buffer.getWritePointer (1);
        if (numIn < 2)
            juce::FloatVectorOperations::copy (R, L, n);
    }
    else
    {
        if ((int) scratch.size() < n)
            scratch.resize ((size_t) n);
        R = scratch.data();
        juce::FloatVectorOperations::copy (R, L, n);
    }

    engine.process (L, R, n, t);

    if (numCh < 2)
        for (int i = 0; i < n; ++i)
            L[i] = 0.5f * (L[i] + R[i]);
}

juce::AudioProcessorEditor* StardustProcessor::createEditor()
{
    return new StardustEditor (*this);
}

void StardustProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void StardustProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new StardustProcessor();
}
