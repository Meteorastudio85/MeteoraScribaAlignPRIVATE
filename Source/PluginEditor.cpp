#include "PluginEditor.h"
#include "BinaryData.h"

using namespace Scriba;

namespace
{
    constexpr float pi = juce::MathConstants<float>::pi;

    // Ruota della fase
    constexpr float wheelCx = 180.0f, wheelCy = 245.0f, wheelR = 80.0f;
    constexpr float wheelSpan = 150.0f;   // gradi per ±20 ms

    juce::Point<float> wheelPoint (double ms, float r)
    {
        const float a = juce::degreesToRadians ((float) (juce::jlimit (-20.0, 20.0, ms) / 20.0) * wheelSpan);
        return { wheelCx + r * std::sin (a), wheelCy - r * std::cos (a) };
    }

    juce::String signedMs (double ms, int decimals)
    {
        return (ms > 0.0005 ? "+" : "") + juce::String (ms, decimals);
    }

    void drawLabel (juce::Graphics& g, const juce::String& s, float x, float baseline,
                    const juce::Font& f, juce::Colour c,
                    juce::Justification j = juce::Justification::left)
    {
        g.setColour (c);
        g.setFont (f);
        const float w = 600.0f;
        float left = x;
        if (j == juce::Justification::right)            left = x - w;
        else if (j == juce::Justification::horizontallyCentred) left = x - w * 0.5f;
        g.drawText (s, juce::Rectangle<float> (left, baseline - f.getAscent(), w, f.getHeight()),
                    j.getOnlyHorizontalFlags() | juce::Justification::top, false);
    }

    void panel (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour fill, juce::Colour stroke, float radius)
    {
        g.setColour (fill);
        g.fillRoundedRectangle (r, radius);
        g.setColour (stroke);
        g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);
    }
}

// ============================================================================
//  Icone
// ============================================================================
Icon Scriba::iconForName (const juce::String& trackName)
{
    const auto n = " " + trackName.toLowerCase() + " ";
    auto has = [&] (const char* s) { return n.contains (s); };

    if (has ("kick") || has ("cassa") || has (" bd ") || has (" bd"))           return Icon::kick;
    if (has ("snare") || has ("rullante") || has (" sn"))
        return (has ("bot") || has ("sotto") || has ("btm") || has (" b ")) ? Icon::snareBottom : Icon::snare;
    if (has ("tom") || has ("floor") || has ("timpano"))                         return Icon::tom;
    if (has ("hat") || has (" hh") || has ("charleston"))                        return Icon::hihat;
    if (has ("overhead") || has (" oh") || has ("over") || has ("ride") || has ("crash")) return Icon::overhead;
    if (has ("room") || has ("amb") || has ("stanza"))                           return Icon::room;
    return Icon::generic;
}

juce::Path Scriba::iconPath (Icon icon, juce::Rectangle<float> a)
{
    juce::Path p;
    const float cx = a.getCentreX(), cy = a.getCentreY();

    auto drum = [&] (float w, float topY, float bottomY)
    {
        const float l = cx - w, r = cx + w;
        p.addEllipse (l, topY - 4.0f, 2.0f * w, 8.0f);
        p.startNewSubPath (r, topY);
        p.lineTo (r, bottomY);
        p.addCentredArc (cx, bottomY, w, 4.0f, 0.0f, pi * 0.5f, pi * 1.5f, false);
        p.lineTo (l, topY);
    };

    switch (icon)
    {
        case Icon::kick:
            p.addEllipse (cx - 12.0f, cy - 12.0f, 24.0f, 24.0f);
            p.addEllipse (cx - 6.0f,  cy - 6.0f,  12.0f, 12.0f);
            break;

        case Icon::snare:
            drum (12.0f, cy - 6.0f, cy + 6.0f);
            break;

        case Icon::snareBottom:
            drum (12.0f, cy - 8.0f, cy + 3.0f);
            p.startNewSubPath (cx - 8.0f, cy + 12.0f);
            for (int i = 1; i <= 4; ++i)
                p.lineTo (cx - 8.0f + (float) i * 4.0f, cy + (i % 2 == 1 ? 9.0f : 12.0f));
            break;

        case Icon::tom:
            drum (9.0f, cy - 6.0f, cy + 5.0f);
            p.startNewSubPath (cx - 6.0f, cy + 9.0f); p.lineTo (cx - 9.0f, cy + 13.0f);
            p.startNewSubPath (cx + 6.0f, cy + 9.0f); p.lineTo (cx + 9.0f, cy + 13.0f);
            break;

        case Icon::hihat:
            p.addEllipse (cx - 14.0f, cy - 7.0f, 28.0f, 5.0f);
            p.addEllipse (cx - 14.0f, cy - 1.0f, 28.0f, 5.0f);
            p.startNewSubPath (cx, cy - 13.0f); p.lineTo (cx, cy - 7.0f);
            p.startNewSubPath (cx, cy + 4.0f);  p.lineTo (cx, cy + 13.0f);
            break;

        case Icon::overhead:
            p.addEllipse (cx - 15.0f, cy - 4.0f, 30.0f, 7.0f);
            p.startNewSubPath (cx, cy - 4.0f); p.lineTo (cx, cy - 10.0f);
            p.startNewSubPath (cx, cy + 3.0f); p.lineTo (cx, cy + 14.0f);
            break;

        case Icon::room:
            p.startNewSubPath (cx - 12.0f, cy + 1.0f);
            p.lineTo (cx - 2.0f, cy - 9.0f);
            p.lineTo (cx + 8.0f, cy + 1.0f);
            p.lineTo (cx + 8.0f, cy + 11.0f);
            p.lineTo (cx - 12.0f, cy + 11.0f);
            p.closeSubPath();
            p.addCentredArc (cx + 10.0f, cy - 4.0f, 6.0f, 6.0f, 0.0f, 0.2f, pi - 0.2f, true);
            p.addCentredArc (cx + 10.0f, cy - 4.0f, 11.0f, 11.0f, 0.0f, 0.3f, pi - 0.3f, true);
            break;

        case Icon::generic:
        default:
            p.startNewSubPath (cx - 14.0f, cy);
            p.lineTo (cx - 9.0f, cy);  p.lineTo (cx - 6.0f, cy - 10.0f); p.lineTo (cx - 1.0f, cy + 11.0f);
            p.lineTo (cx + 3.0f, cy - 6.0f); p.lineTo (cx + 7.0f, cy + 5.0f); p.lineTo (cx + 10.0f, cy);
            p.lineTo (cx + 14.0f, cy);
            break;
    }
    return p;
}

// ============================================================================
//  Pulsanti
// ============================================================================
void TileButton::paintButton (juce::Graphics& g, bool over, bool down)
{
    const bool on = getToggleState();
    auto r = getLocalBounds().toFloat().reduced (1.0f);

    g.setColour (on ? Col::tileOn : (over ? Col::tile.brighter (0.06f) : Col::tile));
    g.fillRoundedRectangle (r, 12.0f);
    g.setColour (on ? Col::teal : Col::tileStroke);
    g.drawRoundedRectangle (r, 12.0f, on ? 2.0f : 1.0f);

    const auto ink = on ? Col::teal : Col::textSoft;
    const bool compact = r.getHeight() < 60.0f;

    if (! compact)
    {
        auto iconArea = r.withHeight (r.getHeight() * 0.62f).withSizeKeepingCentre (30.0f, 30.0f).translated (0.0f, 4.0f);
        g.setColour (ink);
        g.strokePath (iconPath (icon, iconArea),
                      juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    g.setColour (ink);
    g.setFont (font (compact ? 9.5f : 10.5f, true, 0.05f));
    auto textArea = compact ? r : r.removeFromBottom (r.getHeight() * 0.36f).translated (0.0f, -4.0f);
    g.drawFittedText (label, textArea.toNearestInt().reduced (4, 0), juce::Justification::centred, 1, 0.7f);

    if (down)
    {
        g.setColour (juce::Colours::black.withAlpha (0.15f));
        g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (1.0f), 12.0f);
    }
}

void AnalyseButton::paintButton (juce::Graphics& g, bool over, bool down)
{
    auto r = getLocalBounds().toFloat();
    juce::ColourGradient grad (Col::tealLight, r.getX(), r.getY(), Col::teal, r.getRight(), r.getY(), false);
    if (over) grad.multiplyOpacity (0.92f);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (r, 14.0f);

    if (progress > 0.0f)
    {
        g.setColour (Col::tealInk.withAlpha (0.18f));
        g.fillRoundedRectangle (r.withWidth (r.getWidth() * juce::jlimit (0.0f, 1.0f, progress)), 14.0f);
    }

    const auto f = font (20.0f, true, 0.12f);
    const float tw = textWidth (f, label);
    const float total = 16.0f + 12.0f + tw;
    const float x0 = r.getCentreX() - total * 0.5f;

    g.setColour (juce::Colour (0xff0b2f3a));
    g.fillEllipse (x0, r.getCentreY() - 8.0f, 16.0f, 16.0f);

    g.setColour (Col::tealInk);
    g.setFont (f);
    g.drawText (label, juce::Rectangle<float> (x0 + 28.0f, r.getY(), tw + 10.0f, r.getHeight()),
                juce::Justification::centredLeft, false);

    if (down)
    {
        g.setColour (juce::Colours::black.withAlpha (0.12f));
        g.fillRoundedRectangle (r, 14.0f);
    }
}

void IconButton::paintButton (juce::Graphics& g, bool over, bool down)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    panel (g, r, over ? Col::tile.brighter (0.07f) : Col::tile, Col::panelStroke, 10.0f);

    const auto f = font (12.0f, true, 0.07f);
    const float tw = textWidth (f, getButtonText());
    const float total = 18.0f + 10.0f + tw;
    const float x0 = r.getCentreX() - total * 0.5f, cy = r.getCentreY();

    juce::Path p;
    if (kind == Kind::check)
    {
        p.startNewSubPath (x0 + 1.0f, cy);
        p.lineTo (x0 + 6.0f, cy + 5.0f);
        p.lineTo (x0 + 16.0f, cy - 6.0f);
    }
    else if (kind == Kind::reset)
    {
        p.addCentredArc (x0 + 8.0f, cy, 7.0f, 7.0f, 0.0f, 0.5f, 2.0f * pi - 0.3f, true);
        p.startNewSubPath (x0 + 8.0f + 7.0f * std::sin (0.5f) - 4.0f, cy - 7.0f * std::cos (0.5f) - 1.0f);
        p.lineTo (x0 + 8.0f + 7.0f * std::sin (0.5f), cy - 7.0f * std::cos (0.5f));
        p.lineTo (x0 + 8.0f + 7.0f * std::sin (0.5f) - 1.0f, cy - 7.0f * std::cos (0.5f) + 4.0f);
    }
    else
    {
        p.addRoundedRectangle (x0, cy - 5.0f, 11.0f, 13.0f, 2.0f);
        p.startNewSubPath (x0 + 5.0f, cy - 5.0f);
        p.lineTo (x0 + 5.0f, cy - 9.0f);
        p.lineTo (x0 + 16.0f, cy - 9.0f);
        p.lineTo (x0 + 16.0f, cy + 4.0f);
        p.lineTo (x0 + 11.0f, cy + 4.0f);
    }

    const auto ink = isEnabled() ? Col::textSoft : Col::dimmer;
    g.setColour (ink);
    g.strokePath (p, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setFont (f);
    g.drawText (getButtonText(), juce::Rectangle<float> (x0 + 28.0f, r.getY(), tw + 10.0f, r.getHeight()),
                juce::Justification::centredLeft, false);

    if (down)
    {
        g.setColour (juce::Colours::black.withAlpha (0.15f));
        g.fillRoundedRectangle (r, 10.0f);
    }
}

void SegmentButton::paintButton (juce::Graphics& g, bool over, bool)
{
    const bool on = getToggleState();
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    panel (g, r, on ? Col::tealDark : (over ? Col::tile.brighter (0.08f) : juce::Colour (0xff1d2a3c)),
           on ? Col::teal : Col::panelLowLine, 8.0f);
    g.setColour (on ? juce::Colour (0xffbff7ec) : Col::textSoft);
    g.setFont (font (11.0f, true, 0.08f));
    g.drawText (getButtonText(), r, juce::Justification::centred, false);
}

// ============================================================================
//  Contenuto
// ============================================================================
ScribaAlignContent::ScribaAlignContent (ScribaAlignProcessor& p)
    : proc (p), registry (p.getRegistry())
{
    setLookAndFeel (&lnf);
    setSize (designW, designH);

    logo = juce::ImageCache::getFromMemory (BinaryData::logo_png, BinaryData::logo_pngSize);

    nameLabel.setEditable (false, true, false);
    nameLabel.setJustificationType (juce::Justification::centred);
    nameLabel.setFont (font (11.0f, true, 0.09f));
    nameLabel.setColour (juce::Label::textColourId, Col::teal);
    nameLabel.setTooltip ("Doppio click per rinominare. Il nome decide il ruolo: Kick, Snare, Snare Bottom, OH, Room...");
    nameLabel.onTextChange = [this] { proc.setTrackName (nameLabel.getText().trim()); };
    addAndMakeVisible (nameLabel);

    // la correzione si vede ma non si tocca: la decide l'analisi
    shiftSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    shiftSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    shiftSlider.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (shiftSlider);
    shiftAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "shift", shiftSlider);

    analyseButton.onClick = [this]
    {
        if (registry.isCaptureRequested()) registry.cancelCapture();
        else                               registry.startCapture();
    };
    addAndMakeVisible (analyseButton);

    resetButton.setTooltip (juce::String::fromUTF8 ("Riporta tutte le tracce a zero (spostamento e polarit\xc3\xa0)"));
    resetButton.onClick = [this]
    {
        for (auto* inst : registry.getInstances())
        {
            inst->setShiftMs (0.0);
            inst->setInverted (false);
            inst->setLastResult ({});
            inst->clearHits();
        }
    };
    addAndMakeVisible (resetButton);

    refreshSnapshot();
    startTimerHz (15);
}

ScribaAlignContent::~ScribaAlignContent()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

const ScribaAlignContent::TrackInfo* ScribaAlignContent::self() const
{
    for (auto& t : tracks) if (t.isSelf) return &t;
    return nullptr;
}

const ScribaAlignContent::TrackInfo* ScribaAlignContent::reference() const
{
    for (auto& t : tracks) if (t.isRef) return &t;
    return nullptr;
}

void ScribaAlignContent::refreshSnapshot()
{
    tracks.clear();
    for (auto* inst : registry.getInstances())
    {
        TrackInfo t;
        t.id        = inst->getInstanceId();
        t.name      = inst->getTrackName();
        t.shiftMs   = inst->getShiftMs();
        t.isRef     = registry.isReference (inst);
        t.isSelf    = inst == &proc;
        const auto& r = inst->getLastResult();
        t.hasResult = r.valid;
        t.agreement = r.agreement;
        t.hits      = r.hitsUsed;
        t.inverted  = inst->isInverted();
        t.follower  = registry.isFollower (inst);
        if (t.follower)
            if (auto* lead = registry.leaderOf (inst))
                t.pairName = lead->getTrackName();
        t.excluded  = r.excluded;
        t.message   = r.message;
        t.uid       = inst->getUid();
        if (! t.follower && ! r.valid && r.message.isNotEmpty())
            t.shortNote = r.hitsUsed < PhaseAnalyzer::minHits ? "pochi colpi"
                        : r.message.startsWith ("Risultato incerto") ? "incerto"
                        : r.message.upToFirstOccurrenceOf (":", false, false).toLowerCase();
        if (auto* tgt = registry.findByUid (inst->getAlignTo()); ! t.follower && tgt != nullptr && tgt != inst && ! registry.isReference (tgt))
            t.targetName = tgt->getTrackName();
        tracks.push_back (t);
    }

    capturing = registry.isCaptureRequested();
    auto* ref = registry.getReference();
    captureProgress = (capturing && ref != nullptr) ? ref->getCaptureProgress() : 0.0f;
    status = registry.getStatus();
    if (proc.getCurrentSampleRate() > 0.0) sampleRate = proc.getCurrentSampleRate();
    // pulsanti
    analyseButton.progress = captureProgress;
    analyseButton.label = ! capturing ? juce::String ("ANALIZZA")
                        : captureProgress > 0.0f ? "CATTURA " + juce::String (juce::roundToInt (captureProgress * 100.0f)) + "%  ·  STOP"
                                                 : juce::String ("PREMI PLAY  ·  STOP");
    analyseButton.repaint();

    if (! nameLabel.isBeingEdited())
    {
        const auto* s = self();
        const auto txt = s != nullptr ? s->name.toUpperCase() : juce::String();
        if (nameLabel.getText() != txt)
            nameLabel.setText (txt, juce::dontSendNotification);
    }

    rebuildTiles();
}

void ScribaAlignContent::rebuildTiles()
{
    bool same = (int) tracks.size() == tiles.size();
    for (int i = 0; same && i < tiles.size(); ++i)
        same = tiles[i]->instanceId == tracks[(size_t) i].id;

    if (! same)
    {
        tiles.clear();
        for (auto& t : tracks)
        {
            auto* b = tiles.add (new TileButton());
            b->instanceId = t.id;
            b->setInterceptsMouseClicks (false, false);   // la reference è sempre la cassa
            addAndMakeVisible (b);
        }
        resized();
    }

    for (int i = 0; i < tiles.size(); ++i)
    {
        auto& t = tracks[(size_t) i];
        auto* b = tiles[i];
        b->label = t.name.toUpperCase();
        b->icon  = iconForName (t.name);
        b->setToggleState (t.isRef, juce::dontSendNotification);
        b->repaint();
    }
}

void ScribaAlignContent::timerCallback()
{
    refreshSnapshot();
    repaint();
}

// ---------------------------------------------------------------------------
void ScribaAlignContent::paint (juce::Graphics& g)
{
    juce::ColourGradient bg (Col::bgTop, 0.0f, 0.0f, Col::bgBottom, 220.0f, (float) designH, false);
    bg.addColour (0.55, Col::bgMid);
    g.setGradientFill (bg);
    g.fillAll();

    paintHeader (g);
    paintWheel (g);
    paintRightPanel (g);
    paintTrackList (g);

    drawLabel (g, juce::String::fromUTF8 ("\xc2\xa9") + " 2026 Meteora Plugins - Scriba Align - v" + JucePlugin_VersionString,
               736.0f, 630.0f, font (11.0f, false), juce::Colour (0xff8fa3bd), juce::Justification::right);
    drawLabel (g, status, 22.0f, 630.0f, font (11.0f, false), Col::textSoft.withAlpha (0.8f));
}

void ScribaAlignContent::paintHeader (juce::Graphics& g)
{
    if (logo.isValid())
        g.drawImage (logo, juce::Rectangle<float> (20.0f, 16.0f, 62.0f, 62.0f), juce::RectanglePlacement::centred);

    const auto titleF = font (27.0f, true, 0.16f);
    drawLabel (g, "SCRIBA", 96.0f, 52.0f, titleF, Col::text);
    drawLabel (g, "ALIGN", 96.0f + textWidth (titleF, "SCRIBA") + 14.0f, 52.0f, titleF, Col::teal);
    drawLabel (g, juce::String::fromUTF8 ("PHASE \xc2\xb7 TIME \xc2\xb7 DRUMS \xc2\xb7 MULTITRACK"),
               97.0f, 72.0f, font (11.0f, false, 0.165f), Col::dimmer);

    // pillola di stato (come READY in Scriba)
    juce::String pill;
    if (capturing)                       pill = captureProgress > 0.0f ? "CATTURA" : "IN ATTESA";
    else if (reference() == nullptr)     pill = "MANCA KICK";
    else                                 pill = juce::String ((int) tracks.size()) + " TRACCE";

    const auto pf = font (13.0f, true, 0.08f);
    const float pw = 46.0f + textWidth (pf, pill);
    auto pr = juce::Rectangle<float> (738.0f - pw, 30.0f, pw, 34.0f);
    panel (g, pr, Col::pill, Col::panelStroke, 17.0f);
    g.setColour (capturing ? Col::amber : Col::teal);
    g.fillEllipse (pr.getX() + 16.0f, pr.getCentreY() - 4.5f, 9.0f, 9.0f);
    drawLabel (g, pill, pr.getX() + 32.0f, 52.0f, pf, Col::text);
}

void ScribaAlignContent::paintWheel (juce::Graphics& g)
{
    panel (g, { 20.0f, 96.0f, 320.0f, 346.0f }, Col::panel, Col::panelStroke, 14.0f);
    drawLabel (g, "QUESTA TRACCIA", 40.0f, 126.0f, font (11.5f, true, 0.14f), Col::dim);

    const float span = juce::degreesToRadians (wheelSpan);

    juce::Path ring;
    ring.addCentredArc (wheelCx, wheelCy, wheelR, wheelR, 0.0f, -span, span, true);
    g.setColour (Col::track);
    g.strokePath (ring, juce::PathStrokeType (16.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // tacche ogni 5 ms
    for (int ms = -20; ms <= 20; ms += 5)
    {
        auto a = wheelPoint (ms, wheelR + 10.0f), b = wheelPoint (ms, wheelR + (ms % 10 == 0 ? 15.0f : 13.0f));
        g.setColour (Col::trackLine);
        g.drawLine ({ a, b }, 1.2f);
    }

    g.setColour (juce::Colour (0xff182231));
    g.fillEllipse (wheelCx - 58.0f, wheelCy - 58.0f, 116.0f, 116.0f);
    g.setColour (Col::panelStroke);
    g.drawEllipse (wheelCx - 58.0f, wheelCy - 58.0f, 116.0f, 116.0f, 1.0f);

    const auto tickF = font (11.0f, false);
    for (int ms : { -20, -10, 0, 10, 20 })
    {
        auto pnt = wheelPoint (ms, wheelR + 27.0f);
        drawLabel (g, ms == 0 ? "0" : signedMs (ms, 0), pnt.x, pnt.y + 4.0f,
                   ms == 0 ? font (11.0f, true) : tickF, ms == 0 ? Col::text : Col::dim,
                   juce::Justification::horizontallyCentred);
    }

    const auto* me  = self();
    const auto* ref = reference();

    // arco turchese dallo zero alla posizione di questa traccia
    if (me != nullptr && ! me->isRef && std::abs (me->shiftMs) > 0.001)
    {
        const float a = juce::degreesToRadians ((float) (juce::jlimit (-20.0, 20.0, me->shiftMs) / 20.0) * wheelSpan);
        juce::Path arc;
        arc.addCentredArc (wheelCx, wheelCy, wheelR, wheelR, 0.0f, juce::jmin (0.0f, a), juce::jmax (0.0f, a), true);
        g.setColour (Col::teal);
        g.strokePath (arc, juce::PathStrokeType (16.0f, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
    }

    // altre tracce
    for (auto& t : tracks)
    {
        if (t.isSelf || t.isRef) continue;
        auto pnt = wheelPoint (t.shiftMs, wheelR);
        g.setColour (Col::dim);
        g.fillEllipse (pnt.x - 4.0f, pnt.y - 4.0f, 8.0f, 8.0f);
    }

    // reference in alto
    if (ref != nullptr)
    {
        auto pnt = wheelPoint (0.0, wheelR);
        g.setColour (Col::pill);
        g.fillEllipse (pnt.x - 5.5f, pnt.y - 5.5f, 11.0f, 11.0f);
        g.setColour (Col::text);
        g.drawEllipse (pnt.x - 5.5f, pnt.y - 5.5f, 11.0f, 11.0f, 2.0f);
    }

    // questa traccia
    if (me != nullptr && ! me->isRef)
    {
        auto pnt = wheelPoint (me->shiftMs, wheelR);
        g.setColour (Col::pill);
        g.fillEllipse (pnt.x - 9.0f, pnt.y - 9.0f, 18.0f, 18.0f);
        g.setColour (Col::teal);
        g.fillEllipse (pnt.x - 7.0f, pnt.y - 7.0f, 14.0f, 14.0f);
    }

    // centro
    if (me != nullptr && me->isRef)
    {
        drawLabel (g, "REFERENCE", wheelCx, 252.0f, font (17.0f, true, 0.08f), Col::text, juce::Justification::horizontallyCentred);
        drawLabel (g, "LE ALTRE SI ALLINEANO QUI", wheelCx, 270.0f, font (9.5f, false, 0.055f), Col::dim, juce::Justification::horizontallyCentred);
    }
    else if (me != nullptr && (me->hasResult || std::abs (me->shiftMs) > 0.0005))
    {
        drawLabel (g, signedMs (me->shiftMs, 2), wheelCx, 252.0f, font (27.0f, true), Col::text, juce::Justification::horizontallyCentred);
        drawLabel (g, juce::String::fromUTF8 ("MS \xc2\xb7 ") + juce::String (me->shiftMs * sampleRate / 1000.0, 1) + " CAMPIONI",
                   wheelCx, 270.0f, font (10.5f, false, 0.066f), Col::dim, juce::Justification::horizontallyCentred);
    }
    else
    {
        drawLabel (g, "--", wheelCx, 252.0f, font (27.0f, true), Col::dim, juce::Justification::horizontallyCentred);
        const bool uncertain = me != nullptr && me->hits > 0;
        drawLabel (g, uncertain ? "RISULTATO INCERTO" : "PREMI ANALIZZA", wheelCx, 270.0f, font (10.5f, false, 0.066f),
                   uncertain ? Col::amber : Col::dim, juce::Justification::horizontallyCentred);
    }

    // affidabilità: quota di colpi d'accordo sul valore trovato
    const bool showAg = me != nullptr && ! me->isRef && me->hits > 0;
    const float ag = showAg ? me->agreement : 0.0f;
    g.setColour (Col::track);
    g.fillRoundedRectangle (40.0f, 378.0f, 280.0f, 6.0f, 3.0f);
    if (showAg)
    {
        g.setColour (me->hasResult ? Col::teal : Col::amber);
        g.fillRoundedRectangle (40.0f, 378.0f, 280.0f * juce::jlimit (0.02f, 1.0f, ag), 6.0f, 3.0f);
    }
    drawLabel (g, showAg ? "Colpi d'accordo (" + juce::String (me->hits) + " colpi)" : juce::String ("Colpi d'accordo"),
               40.0f, 410.0f, font (13.0f, false), Col::dim);
    drawLabel (g, showAg ? juce::String (juce::roundToInt (ag * 100.0f)) + "%" : juce::String ("--"),
               320.0f, 410.0f, font (13.0f, true), showAg && ! me->hasResult ? Col::amber : Col::text,
               juce::Justification::right);
}

void ScribaAlignContent::paintRightPanel (juce::Graphics& g)
{
    drawLabel (g, "REFERENCE: KICK", 372.0f, 126.0f, font (11.5f, true, 0.14f), Col::dim);
    if (tracks.size() <= 1)
        drawLabel (g, "Inserisci Scriba Align anche sulle altre tracce", 736.0f, 126.0f,
                   font (11.0f, false), Col::amber, juce::Justification::right);
    else if (reference() == nullptr)
        drawLabel (g, "Nessuna traccia chiamata Kick o Cassa", 736.0f, 126.0f,
                   font (11.0f, false), Col::amber, juce::Justification::right);

    const auto* me = self();
    const double ms = me != nullptr ? me->shiftMs : 0.0;

    drawLabel (g, "CORREZIONE", 372.0f, 240.0f, font (11.5f, true, 0.14f), Col::text);
    drawLabel (g, signedMs (ms, 3) + " ms", 736.0f, 240.0f, font (14.0f, true), Col::text, juce::Justification::right);
    drawLabel (g, "ritarda", 372.0f, 284.0f, font (12.5f, false), Col::dimmer.brighter (0.2f));
    drawLabel (g, "anticipa", 736.0f, 284.0f, font (12.5f, false), Col::dimmer.brighter (0.2f), juce::Justification::right);

    // box informativo
    auto box = juce::Rectangle<float> (368.0f, 296.0f, 368.0f, 40.0f);
    panel (g, box, Col::panel, Col::panelStroke, 10.0f);
    g.setColour (Col::teal);
    g.drawRoundedRectangle (380.0f, 306.0f, 10.0f, 20.0f, 5.0f, 2.0f);
    g.drawRoundedRectangle (386.0f, 306.0f, 10.0f, 20.0f, 5.0f, 2.0f);

    juce::String info;
    if (me == nullptr)                 info = {};
    else if (me->isRef)                info = juce::String::fromUTF8 ("Questa \xc3\xa8 la cassa: tutte le altre tracce si allineano a lei.");
    else if (me->follower)             info = juce::String::fromUTF8 ("In coppia con ") + me->pairName
                                            + juce::String::fromUTF8 (": si sposta insieme a lei.");
    else if (me->excluded)             info = juce::String::fromUTF8 ("Room: resta ferma, il suo ritardo \xc3\xa8 il suono della stanza.");
    else if (me->message.isNotEmpty()) info = me->message;
    else if (! me->hasResult && std::abs (ms) < 0.0005)
                                       info = "Premi ANALIZZA e fai partire il play: fa tutto da solo.";
    else if (std::abs (ms) < 0.0005)   info = juce::String::fromUTF8 ("Gi\xc3\xa0 in fase.");
    else
        info = juce::String (ms > 0 ? "Anticipata" : "Ritardata") + " di " + juce::String (std::abs (ms), 3)
             + " ms (" + juce::String (std::abs (ms) * sampleRate / 1000.0, 1) + " campioni)";

    if (me != nullptr && ! me->isRef && ! me->excluded && me->inverted && me->message.isEmpty())
        info += juce::String::fromUTF8 (", polarit\xc3\xa0 invertita.");

    g.setColour (Col::text);
    g.setFont (font (12.5f, false));
    g.drawFittedText (info, juce::Rectangle<int> (406, 296, 322, 40), juce::Justification::centredLeft, 2, 0.85f);
}

void ScribaAlignContent::paintTrackList (juce::Graphics& g)
{
    panel (g, { 20.0f, 456.0f, 716.0f, 152.0f }, Col::panelLow, Col::panelLowLine, 14.0f);

    const auto* ref = reference();
    double maxMs = 0.0;
    for (auto& t : tracks) if (! t.isRef) maxMs = std::max (maxMs, std::abs (t.shiftMs));

    const auto hf = font (11.5f, true, 0.09f);
    auto pair = [&] (float x, const juce::String& a, const juce::String& b, juce::Colour bc)
    {
        drawLabel (g, a, x, 484.0f, hf, Col::textSoft);
        drawLabel (g, b, x + textWidth (hf, a) + 8.0f, 484.0f, hf, bc);
    };
    pair (40.0f, "REF", ref != nullptr ? ref->name.toUpperCase().substring (0, 12) : juce::String::fromUTF8 ("\xe2\x80\x94"), Col::teal);
    drawLabel (g, "ALLINEAMENTO AUTOMATICO", 716.0f, 484.0f, hf, Col::textSoft, juce::Justification::right);
    juce::ignoreUnused (maxMs);

    // righe
    const float top = 500.0f, avail = 104.0f, centreX = 320.0f;
    const int n = (int) tracks.size();
    if (n == 0) return;
    const float rowH = juce::jmin (20.0f, avail / (float) n);
    const float fs = juce::jmin (12.5f, rowH * 0.68f);

    for (float y = top; y < top + avail; y += 6.0f)
    {
        g.setColour (Col::dimmer);
        g.drawLine (centreX, y, centreX, y + 3.0f, 1.0f);
    }

    for (int i = 0; i < n; ++i)
    {
        const auto& t = tracks[(size_t) i];
        const float baseline = top + rowH * (float) i + rowH * 0.5f + fs * 0.35f + 2.0f;
        const float barY = top + rowH * (float) i + rowH * 0.5f + fs * 0.12f + 1.0f;

        drawLabel (g, t.name, 40.0f, baseline, font (fs, t.isSelf), t.isSelf ? Col::teal : Col::text);
        if (t.follower)
            drawLabel (g, juce::String::fromUTF8 ("\xe2\x86\x94 ") + t.pairName,
                       44.0f + textWidth (font (fs, t.isSelf), t.name), baseline, font (fs * 0.85f, false), Col::dim);
        else if (t.targetName.isNotEmpty())
            drawLabel (g, juce::String::fromUTF8 ("\xe2\x86\x92 ") + t.targetName,
                       44.0f + textWidth (font (fs, t.isSelf), t.name), baseline, font (fs * 0.85f, false), Col::dim);

        if (t.isRef)
        {
            drawLabel (g, "REFERENCE", centreX + 10.0f, baseline, font (fs * 0.88f, true, 0.07f), Col::teal);
            continue;
        }

        const float pxPerMs = 13.5f;
        const float w = juce::jlimit (-110.0f, 270.0f, (float) t.shiftMs * pxPerMs);
        g.setColour (t.isSelf ? Col::teal : Col::other);
        g.fillRoundedRectangle (juce::Rectangle<float>::leftTopRightBottom (juce::jmin (centreX, centreX + w), barY - 3.0f,
                                                                            juce::jmax (centreX + 2.0f, centreX + w), barY + 3.0f), 3.0f);

        juce::String note = t.shortNote;
        if (note.isEmpty() && t.inverted) note = juce::String::fromUTF8 ("\xc3\x98 polarit\xc3\xa0 invertita");
        if (note.isEmpty() && t.excluded) note = "resta ferma";
        if (note.isNotEmpty())
            drawLabel (g, note, juce::jmax (centreX, centreX + w) + 12.0f, baseline, font (fs * 0.92f, false), Col::amber);

        drawLabel (g, (t.hasResult || std::abs (t.shiftMs) > 0.0005) ? signedMs (t.shiftMs, 2) + " ms" : juce::String ("--"),
                   720.0f, baseline, font (fs, true), Col::text, juce::Justification::right);
    }
}

void ScribaAlignContent::resized()
{
    nameLabel.setBounds (110, 210, 140, 18);
    shiftSlider.setBounds (364, 246, 380, 24);
    analyseButton.setBounds (368, 346, 368, 50);
    resetButton.setBounds (368, 404, 368, 38);

    // tile della reference: fino a 6 per riga, poi su più righe
    const int n = tiles.size();
    if (n > 0)
    {
        const int cols = juce::jmin (n, 6);
        const int rows = (n + cols - 1) / cols;
        const float gap = 7.0f, areaX = 368.0f, areaY = 136.0f, areaW = 368.0f, areaH = 78.0f;
        const float tw = (areaW - gap * (float) (cols - 1)) / (float) cols;
        const float th = (areaH - gap * (float) (rows - 1)) / (float) rows;
        for (int i = 0; i < n; ++i)
            tiles[i]->setBounds (juce::Rectangle<float> (areaX + (float) (i % cols) * (tw + gap),
                                                         areaY + (float) (i / cols) * (th + gap), tw, th).toNearestInt());
    }
}

// ============================================================================
ScribaAlignEditor::ScribaAlignEditor (ScribaAlignProcessor& p)
    : AudioProcessorEditor (&p), content (p)
{
    addAndMakeVisible (content);
    setResizable (true, true);
    getConstrainer()->setFixedAspectRatio ((double) ScribaAlignContent::designW / ScribaAlignContent::designH);
    setResizeLimits (570, 480, 1520, 1280);
    setSize (ScribaAlignContent::designW, ScribaAlignContent::designH);
}

void ScribaAlignEditor::resized()
{
    const float scale = (float) getWidth() / (float) ScribaAlignContent::designW;
    content.setTransform (juce::AffineTransform::scale (scale));
}
