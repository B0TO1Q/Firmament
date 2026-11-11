#include "PluginProcessor.h"
#include "PluginEditor.h"

using APF = juce::AudioParameterFloat;

static juce::AudioParameterFloatAttributes percentAttr()
{
    return juce::AudioParameterFloatAttributes()
        .withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; })
        .withValueFromStringFunction ([] (const juce::String& s) { return s.getFloatValue() / 100.0f; });
}

juce::AudioProcessorValueTreeState::ParameterLayout FirmamentProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    const juce::NormalisableRange<float> unit (0.0f, 1.0f, 0.001f);

    juce::StringArray modes;
    for (int i = 0; i < fm::kNumModes; ++i)
        modes.add (juce::String (fm::modeName (i)).substring (0, 1) + juce::String (fm::modeName (i)).substring (1).toLowerCase());

    l.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "mode", 1 }, "Mode", modes, (int) fm::Sanctuary));
    l.add (std::make_unique<APF> (juce::ParameterID { "decay", 1 }, "Decay", unit, 0.5f, percentAttr()));

    juce::NormalisableRange<float> preRange (0.0f, 250.0f, 0.1f);
    preRange.setSkewForCentre (50.0f);
    l.add (std::make_unique<APF> (juce::ParameterID { "predelay", 1 }, "Pre-Delay", preRange, 20.0f,
            juce::AudioParameterFloatAttributes()
                .withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v)) + " ms"; })
                .withValueFromStringFunction ([] (const juce::String& s) { return s.getFloatValue(); })));

    l.add (std::make_unique<APF> (juce::ParameterID { "tone", 1 }, "Tone", unit, 0.6f, percentAttr()));
    l.add (std::make_unique<APF> (juce::ParameterID { "mod", 1 }, "Mod", unit, 0.3f, percentAttr()));
    l.add (std::make_unique<APF> (juce::ParameterID { "mix", 1 }, "Mix", unit, 0.35f, percentAttr()));
    l.add (std::make_unique<APF> (juce::ParameterID { "character", 1 }, "Character", unit, 0.5f, percentAttr()));
    l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "freeze", 1 }, "Freeze", false));

    l.add (std::make_unique<APF> (juce::ParameterID { "rise", 1 }, "Ladder Rise", unit, 0.7f, percentAttr()));
    l.add (std::make_unique<APF> (juce::ParameterID { "fall", 1 }, "Ladder Fall", unit, 0.0f, percentAttr()));
    l.add (std::make_unique<APF> (juce::ParameterID { "fine", 1 }, "Ladder Fine",
            juce::NormalisableRange<float> (-50.0f, 50.0f, 0.1f), 0.0f,
            juce::AudioParameterFloatAttributes()
                .withStringFromValueFunction ([] (float v, int) { return (v > 0.05f ? "+" : "") + juce::String (v, 1) + " ct"; })
                .withValueFromStringFunction ([] (const juce::String& s) { return s.getFloatValue(); })));
    return l;
}

FirmamentProcessor::FirmamentProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "FIRMAMENT", createLayout())
{
    pMode   = apvts.getRawParameterValue ("mode");
    pDecay  = apvts.getRawParameterValue ("decay");
    pPre    = apvts.getRawParameterValue ("predelay");
    pTone   = apvts.getRawParameterValue ("tone");
    pMod    = apvts.getRawParameterValue ("mod");
    pMix    = apvts.getRawParameterValue ("mix");
    pChar   = apvts.getRawParameterValue ("character");
    pFreeze = apvts.getRawParameterValue ("freeze");
    pRise   = apvts.getRawParameterValue ("rise");
    pFall   = apvts.getRawParameterValue ("fall");
    pFine   = apvts.getRawParameterValue ("fine");
}

fm::Params FirmamentProcessor::readParams() const
{
    fm::Params p;
    p.mode       = juce::jlimit (0, fm::kNumModes - 1, juce::roundToInt (pMode->load()));
    p.decay      = pDecay->load();
    p.preDelayMs = pPre->load();
    p.tone       = pTone->load();
    p.mod        = pMod->load();
    p.mix        = pMix->load();
    p.character  = pChar->load();
    p.freeze     = pFreeze->load() > 0.5f;
    p.rise       = pRise->load();
    p.fall       = pFall->load();
    p.fineCents  = pFine->load();
    return p;
}

void FirmamentProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate);
    engine.setParams (readParams());
    scratch.assign ((size_t) juce::jmax (16, samplesPerBlock), 0.0f);
}

bool FirmamentProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono())
        return false;
    if (in == juce::AudioChannelSet::stereo())
        return out == juce::AudioChannelSet::stereo();
    return in == juce::AudioChannelSet::mono();   // mono->mono or mono->stereo
}

double FirmamentProcessor::getTailLengthSeconds() const
{
    const auto p = readParams();
    return p.freeze ? std::numeric_limits<double>::infinity()
                    : (double) fm::decaySeconds (p.mode, p.decay) + 0.5;
}

void FirmamentProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
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
            juce::FloatVectorOperations::copy (R, L, n);     // mono in, stereo out
    }
    else
    {
        if ((int) scratch.size() < n)
            scratch.resize ((size_t) n);                     // host exceeded its promised block size
        R = scratch.data();
        juce::FloatVectorOperations::copy (R, L, n);
    }

    engine.process (L, R, n);

    if (numCh < 2)                                           // mono out: fold the stereo tail down
        for (int i = 0; i < n; ++i)
            L[i] = 0.5f * (L[i] + R[i]);
}

juce::AudioProcessorEditor* FirmamentProcessor::createEditor()
{
    return new FirmamentEditor (*this);
}

void FirmamentProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void FirmamentProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new FirmamentProcessor();
}
