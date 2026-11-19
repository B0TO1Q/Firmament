#include "PluginProcessor.h"
#include "PluginEditor.h"

using APF = juce::AudioParameterFloat;

// Knobs read 0 to 10, like a guitar amp.
static juce::AudioParameterFloatAttributes tenAttr()
{
    return juce::AudioParameterFloatAttributes()
        .withStringFromValueFunction ([] (float v, int) { return juce::String (v * 10.0f, 1); })
        .withValueFromStringFunction ([] (const juce::String& s) { return s.getFloatValue() / 10.0f; });
}

juce::AudioProcessorValueTreeState::ParameterLayout SelahProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    const juce::NormalisableRange<float> unit (0.0f, 1.0f, 0.001f);

    juce::StringArray sources { "Speaker Mic", "Lead Vocal", "Choir", "Acoustic Guitar", "Electric Guitar", "Bass", "Keys", "Drums", "Full Mix" };
    l.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "source", 1 }, "Source", sources, 0));
    // six tools per source; each source keeps its own settings
    const char* srcTitles[sl::kNumSources] = { "Speaker Mic", "Lead Vocal", "Choir", "Acoustic", "Electric", "Bass", "Keys", "Drums", "Full Mix" };
    for (int s = 0; s < sl::kNumSources; ++s)
        for (int t = 0; t < sl::kTools; ++t)
        {
            const auto& ts = sl::toolSpec (s, t);
            l.add (std::make_unique<APF> (juce::ParameterID { toolParamId (s, t), 1 },
                                          juce::String (srcTitles[s]) + ": " + juce::String (ts.name).substring (0, 1) + juce::String (ts.name).substring (1).toLowerCase(),
                                          unit, ts.def, tenAttr()));
        }
    l.add (std::make_unique<APF> (juce::ParameterID { "volume", 1 }, "Volume",
            juce::NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f,
            juce::AudioParameterFloatAttributes()
                .withStringFromValueFunction ([] (float v, int) { return (v > 0.05f ? "+" : "") + juce::String (v, 1) + " dB"; })
                .withValueFromStringFunction ([] (const juce::String& s) { return s.getFloatValue(); })));
    l.add (std::make_unique<APF> (juce::ParameterID { "revlength", 1 }, "Reverb Length", unit, 0.5f, tenAttr()));
    l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "limiter", 1 }, "Safety Limiter", true));

    l.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "width", 1 }, "Mono / Stereo", juce::StringArray { "Mono", "Stereo" }, 1));
    l.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "eqmode", 1 }, "EQ Bands", juce::StringArray { "7", "14", "21" }, 0));
    l.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "eqpost", 1 }, "EQ Position", juce::StringArray { "Pre", "Post" }, 0));
    for (int m = 0; m < 3; ++m)
        for (int b = 0; b < sl::eqBandCount (m); ++b)
            l.add (std::make_unique<APF> (juce::ParameterID { eqParamId (m, b), 1 },
                    "EQ" + juce::String (sl::eqBandCount (m)) + " " + eqFreqText (sl::eqFreq (m, b)),
                    juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f,
                    juce::AudioParameterFloatAttributes()
                        .withStringFromValueFunction ([] (float v, int) { return (v > 0.05f ? "+" : "") + juce::String (v, 1) + " dB"; })
                        .withValueFromStringFunction ([] (const juce::String& t) { return t.getFloatValue(); })));
    return l;
}

juce::String SelahProcessor::eqFreqText (float hz)
{
    if (hz < 1000.0f) return juce::String (juce::roundToInt (hz));
    const float k = hz / 1000.0f;
    return (std::abs (k - std::round (k)) < 0.01f ? juce::String (juce::roundToInt (k)) : juce::String (k, k * 10.0f - std::floor (k * 10.0f) > 0.01f ? 2 : 1)) + "k";
}

SelahProcessor::SelahProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "SELAH", createLayout())
{
    pSource  = apvts.getRawParameterValue ("source");
    for (int s = 0; s < sl::kNumSources; ++s)
        for (int t = 0; t < sl::kTools; ++t)
            pTool[s][t] = apvts.getRawParameterValue (toolParamId (s, t));
    pVolume  = apvts.getRawParameterValue ("volume");
    pRevLen  = apvts.getRawParameterValue ("revlength");
    pLimiter = apvts.getRawParameterValue ("limiter");
    pEqMode  = apvts.getRawParameterValue ("eqmode");
    pEqPost  = apvts.getRawParameterValue ("eqpost");
    pWidth   = apvts.getRawParameterValue ("width");
    for (int m = 0; m < 3; ++m)
        for (int b = 0; b < sl::eqBandCount (m); ++b)
            pEq[m][b] = apvts.getRawParameterValue (eqParamId (m, b));
}

sl::Params SelahProcessor::readParams() const
{
    sl::Params p;
    p.source       = juce::jlimit (0, sl::kNumSources - 1, juce::roundToInt (pSource->load()));
    for (int s = 0; s < sl::kNumSources; ++s)
        for (int t = 0; t < sl::kTools; ++t)
            p.tool[s][t] = pTool[s][t]->load();
    p.volumeDb     = pVolume->load();
    p.reverbLength = pRevLen->load();
    p.limiter      = pLimiter->load() > 0.5f;
    p.eqMode       = juce::jlimit (0, 2, juce::roundToInt (pEqMode->load()));
    p.eqPost       = pEqPost->load() > 0.5f;
    p.mono         = pWidth->load() < 0.5f;
    for (int b = 0; b < sl::eqBandCount (p.eqMode); ++b)
        p.eqGain[b] = pEq[p.eqMode][b]->load();
    return p;
}

void SelahProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate);
    engine.setParams (readParams());
    scratch.assign ((size_t) juce::jmax (16, samplesPerBlock), 0.0f);
    setLatencySamples (0);
}

bool SelahProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono())
        return false;
    if (in == juce::AudioChannelSet::stereo())
        return out == juce::AudioChannelSet::stereo();
    return in == juce::AudioChannelSet::mono();
}

void SelahProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int numIn = getTotalNumInputChannels();
    const int numCh = buffer.getNumChannels();
    if (n == 0 || numCh == 0)
        return;

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

    engine.process (L, R, n);

    if (numCh < 2)
        for (int i = 0; i < n; ++i)
            L[i] = 0.5f * (L[i] + R[i]);
}

juce::AudioProcessorEditor* SelahProcessor::createEditor()
{
    return new SelahEditor (*this);
}

void SelahProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void SelahProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SelahProcessor();
}
