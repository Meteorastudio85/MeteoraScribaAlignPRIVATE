#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    const juce::Colour defaultColours[] = {
        juce::Colour (0xffe0a458), juce::Colour (0xff6fb3d2), juce::Colour (0xffd36a6a),
        juce::Colour (0xff8fc98b), juce::Colour (0xffb28fd6), juce::Colour (0xffd6c46f),
        juce::Colour (0xff6fd6c0), juce::Colour (0xffd68fb8)
    };
}

ScribaAlignProcessor::ScribaAlignProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    shiftParam = apvts.getRawParameterValue ("shift");
    invertParam = apvts.getRawParameterValue ("invert");

    instanceId  = registry->add (this);
    trackName   = "Traccia " + juce::String (instanceId);
    trackColour = defaultColours[(size_t) (instanceId - 1) % std::size (defaultColours)];
}

ScribaAlignProcessor::~ScribaAlignProcessor()
{
    registry->remove (this);
}

juce::AudioProcessorValueTreeState::ParameterLayout ScribaAlignProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "shift", 1 }, "Correzione",
        juce::NormalisableRange<float> ((float) -AlignRegistry::maxShiftMs, (float) AlignRegistry::maxShiftMs, 0.001f),
        0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("ms")));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "invert", 1 }, juce::String::fromUTF8 ("Inverti polarit\xc3\xa0"), false));

    return layout;
}

bool ScribaAlignProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void ScribaAlignProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    latencySamples = juce::roundToInt (AlignRegistry::maxShiftMs * sampleRate / 1000.0);
    setLatencySamples (latencySamples);

    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock,
                                  (juce::uint32) juce::jmax (1, getTotalNumOutputChannels()) };
    delayLine.setMaximumDelayInSamples (2 * latencySamples + 8);
    delayLine.prepare (spec);
    delayLine.reset();

    delaySmoothed.reset (sampleRate, 0.05);
    delaySmoothed.setCurrentAndTargetValue ((float) latencySamples);

    polaritySmoothed.reset (sampleRate, 0.02);
    polaritySmoothed.setCurrentAndTargetValue (invertParam->load() > 0.5f ? -1.0f : 1.0f);

    capture.assign ((size_t) (AlignRegistry::captureSeconds * sampleRate), 0.0f);
    captureWritten = 0;
    captureDone = false;
    captureActive = false;
}

void ScribaAlignProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numIn  = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    const int n      = buffer.getNumSamples();

    for (int ch = numIn; ch < numOut; ++ch)
        buffer.clear (ch, 0, n);

    // ---- posizione sulla timeline ----
    juce::int64 now = fallbackClock;
    bool playing = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            playing = pos->getIsPlaying();
            if (auto t = pos->getTimeInSamples())
                now = *t;
        }
    fallbackClock = now + n;

    // ---- cattura (sul segnale non ancora corretto) ----
    const int gen = registry->getCaptureGeneration();
    if (gen != seenGeneration)
    {
        seenGeneration = gen;
        captureWritten = 0;
        captureDone    = false;
        captureStarted = false;
        captureActive  = registry->isCaptureRequested();
    }

    if (captureActive.load() && playing && numIn > 0 && ! capture.empty())
    {
        int written = captureWritten.load();

        if (! captureStarted || now != captureStart.load() + written)
        {
            // primo blocco, oppure l'utente ha saltato sulla timeline: si riparte
            captureStart   = now;
            written        = 0;
            captureStarted = true;
        }

        const int toCopy = juce::jmin (n, (int) capture.size() - written);
        const float gain = 1.0f / (float) numIn;

        for (int i = 0; i < toCopy; ++i)
        {
            float s = 0.0f;
            for (int ch = 0; ch < numIn; ++ch)
                s += buffer.getSample (ch, i);
            capture[(size_t) (written + i)] = s * gain;
        }

        written += toCopy;
        captureWritten = written;

        if (written >= (int) capture.size())
        {
            captureActive = false;
            captureDone   = true;
        }
    }

    // ---- correzione: delay = latenza fissa - spostamento ----
    const float shiftSamples = shiftParam->load() * (float) currentSampleRate / 1000.0f;
    const float target = juce::jlimit (0.0f, (float) (2 * latencySamples),
                                       (float) latencySamples - shiftSamples);
    delaySmoothed.setTargetValue (target);

    // ---- polarità (impostata dall'analisi insieme allo spostamento) ----
    polaritySmoothed.setTargetValue (invertParam->load() > 0.5f ? -1.0f : 1.0f);

    for (int i = 0; i < n; ++i)
    {
        const float d   = delaySmoothed.getNextValue();
        const float pol = polaritySmoothed.getNextValue();
        for (int ch = 0; ch < numOut; ++ch)
        {
            delayLine.pushSample (ch, buffer.getSample (ch, i));
            buffer.setSample (ch, i, pol * delayLine.popSample (ch, d));
        }
    }
}

float ScribaAlignProcessor::getCaptureProgress() const noexcept
{
    if (capture.empty()) return 0.0f;
    return (float) captureWritten.load() / (float) capture.size();
}

// ---------------------------------------------------------------------------

juce::String ScribaAlignProcessor::getTrackName() const
{
    const juce::ScopedLock sl (nameLock);
    return trackName;
}

void ScribaAlignProcessor::setTrackName (const juce::String& s)
{
    const juce::ScopedLock sl (nameLock);
    trackName = s;
}

juce::Colour ScribaAlignProcessor::getTrackColour() const
{
    const juce::ScopedLock sl (nameLock);
    return trackColour;
}

void ScribaAlignProcessor::updateTrackProperties (const TrackProperties& props)
{
    const juce::ScopedLock sl (nameLock);
    if (props.name.isNotEmpty())
    {
        trackName = props.name;
        nameFromHost = true;
    }
    if (props.colour != juce::Colours::transparentBlack)
        trackColour = props.colour;
}

double ScribaAlignProcessor::getShiftMs() const { return shiftParam->load(); }

void ScribaAlignProcessor::setShiftMs (double ms)
{
    if (auto* p = apvts.getParameter ("shift"))
        p->setValueNotifyingHost (p->convertTo0to1 ((float) ms));
}

bool ScribaAlignProcessor::isInverted() const { return invertParam->load() > 0.5f; }

void ScribaAlignProcessor::setInverted (bool on)
{
    if (auto* p = apvts.getParameter ("invert"))
        p->setValueNotifyingHost (on ? 1.0f : 0.0f);
}

// ---------------------------------------------------------------------------

void ScribaAlignProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("uid", uid, nullptr);
    state.setProperty ("alignTo", alignTo, nullptr);
    state.setProperty ("linkTo", linkTo, nullptr);
    if (! nameFromHost)
        state.setProperty ("trackName", getTrackName(), nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void ScribaAlignProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
    {
        auto state = juce::ValueTree::fromXml (*xml);
        if (! state.isValid()) return;

        apvts.replaceState (state);

        if (state.hasProperty ("uid"))     uid     = state["uid"].toString();
        if (state.hasProperty ("alignTo")) alignTo = state["alignTo"].toString();
        if (state.hasProperty ("linkTo"))  linkTo  = state["linkTo"].toString();

        if (state.hasProperty ("trackName") && ! nameFromHost)
            setTrackName (state["trackName"].toString());
        // la reference non si salva: è sempre la cassa, scelta in automatico
    }
}

void ScribaAlignProcessor::addHits (const juce::String& key, const std::vector<HitMeasure>& hits)
{
    if (key != hitPoolKey)
    {
        hitPool.clear();
        hitPoolKey = key;
    }
    hitPool.insert (hitPool.end(), hits.begin(), hits.end());
    if ((int) hitPool.size() > PhaseAnalyzer::maxPool)
        hitPool.erase (hitPool.begin(), hitPool.end() - PhaseAnalyzer::maxPool);
}

juce::AudioProcessorEditor* ScribaAlignProcessor::createEditor()
{
    return new ScribaAlignEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ScribaAlignProcessor();
}
