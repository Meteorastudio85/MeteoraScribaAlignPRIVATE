#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "AlignRegistry.h"
#include "PhaseAnalyzer.h"

class ScribaAlignProcessor : public juce::AudioProcessor
{
public:
    ScribaAlignProcessor();
    ~ScribaAlignProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Scriba Align"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    void updateTrackProperties (const TrackProperties&) override;

    // --- interfaccia usata da registro e GUI (message thread) ---
    AlignRegistry& getRegistry() { return *registry; }
    int  getInstanceId() const noexcept { return instanceId; }
    juce::String getTrackName() const;
    void setTrackName (const juce::String&);
    juce::Colour getTrackColour() const;

    double getShiftMs() const;
    void   setShiftMs (double ms);
    bool   isInverted() const;
    void   setInverted (bool);

    bool  isCaptureComplete() const noexcept { return captureDone.load(); }
    int   getCapturedLength() const noexcept { return captureWritten.load(); }
    float getCaptureProgress() const noexcept;
    const float* getCaptureData() const noexcept { return capture.data(); }
    juce::int64 getCaptureStart() const noexcept { return captureStart.load(); }

    void setLastResult (const AlignResult& r) { lastResult = r; }
    const AlignResult& getLastResult() const { return lastResult; }

    // Frequenza di campionamento salvata in prepareToPlay (non dipende dall'host)
    double getCurrentSampleRate() const noexcept { return currentSampleRate; }

    // Identità persistente (salvata nel progetto) e traccia a cui allinearsi.
    // alignTo vuoto = reference globale.
    juce::String getUid() const { return uid; }
    juce::String getAlignTo() const { return alignTo; }
    void setAlignTo (const juce::String& targetUid) { alignTo = targetUid; }

    // "In coppia con": la traccia segue spostamento e polarità di un'altra
    // (es. OH R in coppia con OH L), così la coppia stereo si sposta come un blocco unico.
    // linkTo vuoto = traccia indipendente.
    juce::String getLinkTo() const { return linkTo; }
    void setLinkTo (const juce::String& leaderUid) { linkTo = leaderUid; }

    // Colpi accumulati tra più analisi verso la stessa destinazione
    void addHits (const juce::String& key, const std::vector<HitMeasure>& hits);
    const std::vector<HitMeasure>& getHitPool() const { return hitPool; }
    void clearHits() { hitPool.clear(); hitPoolKey.clear(); }

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    juce::SharedResourcePointer<AlignRegistry> registry;
    int instanceId = 0;

    std::atomic<float>* shiftParam = nullptr;
    std::atomic<float>* invertParam = nullptr;
    juce::SmoothedValue<float> polaritySmoothed;   // +1 / -1 con breve dissolvenza

    // Delay: tutte le istanze dichiarano la stessa latenza fissa (maxShiftMs),
    // la DAW la compensa, e ogni traccia può così essere sia anticipata che ritardata.
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd> delayLine { 8192 };
    juce::SmoothedValue<float> delaySmoothed;
    int latencySamples = 0;

    // Cattura (scritta dall'audio thread, letta dal message thread a cattura finita)
    std::vector<float> capture;
    std::atomic<int>  captureWritten { 0 };
    std::atomic<bool> captureActive { false }, captureDone { false };
    std::atomic<juce::int64> captureStart { 0 };
    bool captureStarted = false;
    int  seenGeneration = 0;
    juce::int64 fallbackClock = 0;

    AlignResult lastResult;
    double currentSampleRate = 0.0;
    juce::String uid { juce::Uuid().toString() }, alignTo, linkTo;
    juce::String hitPoolKey;
    std::vector<HitMeasure> hitPool;

    mutable juce::CriticalSection nameLock;
    juce::String trackName;
    juce::Colour trackColour;
    bool nameFromHost = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ScribaAlignProcessor)
};
