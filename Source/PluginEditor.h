#pragma once
#include "PluginProcessor.h"
#include "ScribaLookAndFeel.h"

namespace Scriba
{
    enum class Icon { kick, snare, snareBottom, tom, hihat, overhead, room, generic };
    Icon iconForName (const juce::String& trackName);
    juce::Path iconPath (Icon, juce::Rectangle<float> area);

    // Tile della reference (come le tile degli strumenti di Scriba)
    class TileButton : public juce::Button
    {
    public:
        TileButton() : juce::Button ({}) {}
        int    instanceId = 0;
        Icon   icon = Icon::generic;
        juce::String label;
        void paintButton (juce::Graphics&, bool over, bool down) override;
    };

    // Grande pulsante ANALIZZA
    class AnalyseButton : public juce::Button
    {
    public:
        AnalyseButton() : juce::Button ("Analizza") {}
        juce::String label { "ANALIZZA" };
        float progress = 0.0f;   // 0..1 durante la cattura
        void paintButton (juce::Graphics&, bool over, bool down) override;
    };

    // Pulsante secondario con icona
    class IconButton : public juce::Button
    {
    public:
        enum class Kind { check, copy, reset };
        IconButton (Kind k, const juce::String& buttonText) : juce::Button (buttonText), kind (k) {}
        void paintButton (juce::Graphics&, bool over, bool down) override;
    private:
        Kind kind;
    };

    // Segmento PLUGIN / CLIP (come WAVE / CHORDS)
    class SegmentButton : public juce::Button
    {
    public:
        explicit SegmentButton (const juce::String& buttonText) : juce::Button (buttonText) {}
        void paintButton (juce::Graphics&, bool over, bool down) override;
    };
}

// Tutta l'interfaccia è disegnata su 760x640 e poi scalata.
class ScribaAlignContent : public juce::Component, private juce::Timer
{
public:
    static constexpr int designW = 760, designH = 640;

    explicit ScribaAlignContent (ScribaAlignProcessor&);
    ~ScribaAlignContent() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct TrackInfo
    {
        int id = 0;
        juce::String name;
        double shiftMs = 0.0;
        bool isRef = false, isSelf = false, hasResult = false, inverted = false, follower = false, excluded = false;
        float agreement = 0.0f;
        int hits = 0;
        juce::String message, shortNote, uid, targetName, pairName;
    };

    void timerCallback() override;
    void refreshSnapshot();
    void rebuildTiles();
    const TrackInfo* self() const;
    const TrackInfo* reference() const;

    void paintHeader (juce::Graphics&);
    void paintWheel (juce::Graphics&);
    void paintRightPanel (juce::Graphics&);
    void paintTrackList (juce::Graphics&);

    ScribaAlignProcessor& proc;
    AlignRegistry& registry;
    ScribaLookAndFeel lnf;
    juce::Image logo;

    std::vector<TrackInfo> tracks;
    bool capturing = false;
    float captureProgress = 0.0f;
    juce::String status;
    double sampleRate = 48000.0;

    juce::OwnedArray<Scriba::TileButton> tiles;
    juce::Label nameLabel;
    juce::Slider shiftSlider;
    Scriba::AnalyseButton analyseButton;
    Scriba::IconButton resetButton { Scriba::IconButton::Kind::reset, "AZZERA TUTTO" };
    juce::TooltipWindow tooltips { this, 600 };

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> shiftAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ScribaAlignContent)
};

class ScribaAlignEditor : public juce::AudioProcessorEditor
{
public:
    explicit ScribaAlignEditor (ScribaAlignProcessor&);
    void resized() override;

private:
    ScribaAlignContent content;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ScribaAlignEditor)
};
