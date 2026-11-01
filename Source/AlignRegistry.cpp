#include "AlignRegistry.h"
#include "PluginProcessor.h"
#include "PhaseAnalyzer.h"
#include <map>
#include <functional>

namespace
{
    // Segnale usato per l'analisi: la cattura di una traccia guida,
    // sommata alle catture delle tracce in coppia con lei.
    struct AnalysisSignal
    {
        std::vector<float> data;
        juce::int64 start = 0;
        int members = 0;
    };
}

int AlignRegistry::add (ScribaAlignProcessor* p)
{
    int id = 0;
    {
        const juce::ScopedLock sl (lock);
        instances.addIfNotAlreadyThere (p);
        id = nextId++;
    }
    // il timer gira sempre: tiene allineate le coppie e segue la cattura
    if (! isTimerRunning())
        startTimerHz (15);
    return id;
}

void AlignRegistry::remove (ScribaAlignProcessor* p)
{
    const juce::ScopedLock sl (lock);
    instances.removeAllInstancesOf (p);
    if (reference == p) reference = nullptr;
}

juce::Array<ScribaAlignProcessor*> AlignRegistry::getInstances() const
{
    const juce::ScopedLock sl (lock);
    return instances;
}

ScribaAlignProcessor* AlignRegistry::findById (int id) const
{
    const juce::ScopedLock sl (lock);
    for (auto* p : instances)
        if (p->getInstanceId() == id)
            return p;
    return nullptr;
}

ScribaAlignProcessor* AlignRegistry::findByUid (const juce::String& u) const
{
    if (u.isEmpty()) return nullptr;
    const juce::ScopedLock sl (lock);
    for (auto* p : instances)
        if (p->getUid() == u)
            return p;
    return nullptr;
}

ScribaAlignProcessor* AlignRegistry::leaderOf (const ScribaAlignProcessor* p) const
{
    auto* self = const_cast<ScribaAlignProcessor*> (p);
    auto* cur  = self;

    for (int depth = 0; depth < 16; ++depth)
    {
        if (isReference (cur)) return cur;   // la reference guida sempre se stessa
        auto* next = findByUid (cur->getLinkTo());
        if (next == nullptr || next == cur) return cur;
        cur = next;
        if (cur == self) return self;        // collegamenti in cerchio: traccia indipendente
    }
    return self;
}

ScribaAlignProcessor* AlignRegistry::resolveTarget (const ScribaAlignProcessor* p) const
{
    auto* t = findByUid (p->getAlignTo());
    if (t != nullptr)
        t = leaderOf (t);
    if (t == nullptr || t == p || t == leaderOf (p))
        return getReference();
    return t;
}

AlignRegistry::Role AlignRegistry::roleOf (const juce::String& trackName)
{
    const auto n = " " + trackName.toLowerCase().replaceCharacters ("_-.", "   ") + " ";
    auto has = [&] (const char* s) { return n.contains (s); };

    if (has ("kick") || has ("cassa") || has ("kik") || has (" bd"))                 return Role::kick;
    if (has ("snare") || has ("rullante") || has (" sn"))
        return (has ("bot") || has ("sotto") || has ("btm") || has ("under") || has (" b ")) ? Role::snareBottom
                                                                                            : Role::snareTop;
    if (has ("overhead") || has (" oh") || has ("over"))                              return Role::overhead;
    if (has ("room") || has ("amb") || has ("stanza"))                                return Role::room;
    return Role::other;
}

namespace
{
    juce::String paddedName (const ScribaAlignProcessor* p)
    {
        return " " + p->getTrackName().toLowerCase().replaceCharacters ("_-.", "   ") + " ";
    }

    // la traccia guida di un gruppo: preferibilmente quella di sinistra
    ScribaAlignProcessor* pickLeader (const juce::Array<ScribaAlignProcessor*>& group)
    {
        for (auto* p : group)
        {
            const auto n = paddedName (p);
            if (n.contains (" l ") || n.contains ("left") || n.contains (" sx ") || n.contains ("sinistr")
                || n.contains ("ohl ") || n.contains (" l1 "))
                return p;
        }
        return group.isEmpty() ? nullptr : group.getFirst();
    }

    // tra più casse si preferisce quella "in", poi una qualsiasi che non sia out/sub
    int kickScore (const ScribaAlignProcessor* p)
    {
        const auto n = paddedName (p);
        if (n.contains (" in ") || n.contains ("inside") || n.contains ("dentro")) return 3;
        if (n.contains (" out") || n.contains ("fuori") || n.contains ("sub"))     return 1;
        return 2;
    }
}

void AlignRegistry::autoConfigure()
{
    const auto all = getInstances();

    juce::Array<ScribaAlignProcessor*> kicks, ohs, rooms, snareTops;
    for (auto* p : all)
    {
        switch (roleOf (p->getTrackName()))
        {
            case Role::kick:     kicks.add (p);     break;
            case Role::overhead: ohs.add (p);       break;
            case Role::room:     rooms.add (p);     break;
            case Role::snareTop: snareTops.add (p); break;
            default: break;
        }
    }

    // 1) reference: la cassa
    ScribaAlignProcessor* kick = nullptr;
    for (auto* p : kicks)
        if (kick == nullptr || kickScore (p) > kickScore (kick))
            kick = p;

    if (kick != getReference())
    {
        {
            const juce::ScopedLock sl (lock);
            reference = kick;
        }
        if (kick != nullptr && ! captureRequested.load())
            setStatus ("Reference: " + kick->getTrackName() + ". Premi ANALIZZA e fai partire il play");
    }

    auto* ohLead   = pickLeader (ohs);
    auto* roomLead = pickLeader (rooms);

    // 2) coppie e destinazioni
    for (auto* p : all)
    {
        juce::String link, align;

        if (p != kick)
        {
            switch (roleOf (p->getTrackName()))
            {
                case Role::kick:
                    break;                                               // altre casse -> cassa reference
                case Role::overhead:
                    if (p != ohLead) link = ohLead->getUid();            // blocco unico, guida -> cassa
                    break;
                case Role::room:
                    if (p != roomLead) link = roomLead->getUid();        // restano ferme (vedi analyse)
                    break;
                case Role::snareBottom:
                    if (! snareTops.isEmpty())   align = snareTops.getFirst()->getUid();
                    else if (ohLead != nullptr)  align = ohLead->getUid();
                    break;
                case Role::snareTop:
                case Role::other:
                default:
                    if (ohLead != nullptr) align = ohLead->getUid();     // tramite gli overhead
                    break;
            }
        }

        if (p->getLinkTo() != link)   p->setLinkTo (link);
        if (p->getAlignTo() != align) p->setAlignTo (align);
    }

    // la reference non si sposta e non si inverte
    if (kick != nullptr)
    {
        if (std::abs (kick->getShiftMs()) > 1.0e-6) kick->setShiftMs (0.0);
        if (kick->isInverted())                     kick->setInverted (false);
    }
}

void AlignRegistry::setReference (ScribaAlignProcessor* p)
{
    const juce::ScopedLock sl (lock);
    reference = p;
}

ScribaAlignProcessor* AlignRegistry::getReference() const
{
    const juce::ScopedLock sl (lock);
    return reference;
}

bool AlignRegistry::isReference (const ScribaAlignProcessor* p) const
{
    const juce::ScopedLock sl (lock);
    return p != nullptr && p == reference;
}

void AlignRegistry::startCapture()
{
    autoConfigure();
    if (getReference() == nullptr)
    {
        setStatus ("Nessuna cassa trovata: chiama la traccia della cassa Kick o Cassa");
        return;
    }

    captureRequested = true;
    ++generation;
    setStatus ("In attesa del play...");
    if (! isTimerRunning())
        startTimerHz (15);
}

void AlignRegistry::cancelCapture()
{
    captureRequested = false;
    ++generation;
    setStatus ("Cattura annullata");
}

juce::String AlignRegistry::getStatus() const
{
    const juce::ScopedLock sl (lock);
    return status;
}

void AlignRegistry::setStatus (const juce::String& s)
{
    const juce::ScopedLock sl (lock);
    status = s;
}

void AlignRegistry::syncPairs()
{
    for (auto* p : getInstances())
    {
        auto* lead = leaderOf (p);
        if (lead == p) continue;

        if (std::abs (p->getShiftMs() - lead->getShiftMs()) > 1.0e-6)
            p->setShiftMs (lead->getShiftMs());
        if (p->isInverted() != lead->isInverted())
            p->setInverted (lead->isInverted());
    }
}

void AlignRegistry::timerCallback()
{
    if (! captureRequested.load())
        autoConfigure();
    syncPairs();

    if (! captureRequested.load())
        return;

    auto* ref = getReference();
    if (ref == nullptr) { cancelCapture(); return; }

    const float progress = ref->getCaptureProgress();

    if (ref->isCaptureComplete())
    {
        captureRequested = false;
        analyse();
        syncPairs();
    }
    else if (progress > 0.0f)
    {
        setStatus ("Cattura in corso... " + juce::String (juce::roundToInt (progress * 100.0f)) + "%");
    }
}

void AlignRegistry::analyse()
{
    autoConfigure();
    auto* ref = getReference();
    if (ref == nullptr || ! ref->isCaptureComplete())
    {
        setStatus ("Nessuna cattura della cassa disponibile");
        return;
    }

    const double sr     = ref->getCurrentSampleRate();
    const int    maxLag = juce::roundToInt (maxShiftMs * sr / 1000.0);
    const auto   all    = getInstances();

    // 0) segnali di analisi: ogni traccia guida + le tracce in coppia con lei, sommate
    std::map<ScribaAlignProcessor*, AnalysisSignal> signals;
    auto signalOf = [&] (ScribaAlignProcessor* lead) -> const AnalysisSignal&
    {
        if (auto it = signals.find (lead); it != signals.end())
            return it->second;

        AnalysisSignal s;
        s.start = lead->getCaptureStart();
        const int n = lead->getCapturedLength();
        s.data.assign (lead->getCaptureData(), lead->getCaptureData() + n);
        s.members = 1;

        for (auto* f : all)
        {
            if (f == lead || leaderOf (f) != lead || f->getCapturedLength() == 0)
                continue;

            const juce::int64 off  = f->getCaptureStart() - s.start;
            const juce::int64 from = juce::jmax ((juce::int64) 0, off);
            const juce::int64 to   = juce::jmin ((juce::int64) n, off + (juce::int64) f->getCapturedLength());
            const float* fd = f->getCaptureData();
            for (auto i = from; i < to; ++i)
                s.data[(size_t) i] += fd[i - off];
            ++s.members;
        }

        return signals.emplace (lead, std::move (s)).first->second;
    };

    // 1) spostamento relativo di ogni traccia guida verso la sua destinazione
    std::map<ScribaAlignProcessor*, AlignResult> rel;
    std::map<ScribaAlignProcessor*, ScribaAlignProcessor*> target;

    for (auto* p : all)
    {
        if (p == ref || isFollower (p)) continue;

        auto* t = resolveTarget (p);
        target[p] = t;
        AlignResult r;

        // le room restano ferme: il loro ritardo è il suono della stanza
        if (roleOf (p->getTrackName()) == Role::room)
        {
            p->setShiftMs (0.0);
            p->setInverted (false);
            r.valid = true;
            r.agreement = 1.0f;
            r.excluded = true;
            rel[p] = r;
            continue;
        }

        if (t == nullptr || p->getCapturedLength() == 0 || t->getCapturedLength() == 0)
        {
            r.message = "Nessun audio catturato: premi play dopo ANALIZZA";
            rel[p] = r;
            continue;
        }

        const auto& ps = signalOf (p);
        const auto& ts = signalOf (t);

        // sovrapposizione delle due catture sulla timeline
        const auto tn = (juce::int64) ts.data.size(), pn = (juce::int64) ps.data.size();
        const auto start = juce::jmax (ts.start, ps.start);
        const auto end   = juce::jmin (ts.start + tn, ps.start + pn);

        if (end - start < (juce::int64) (sr * 0.5))
        {
            r.message = "Catture non sovrapposte";
            rel[p] = r;
            continue;
        }

        const auto hits = PhaseAnalyzer::measureHits (ts.data.data() + (start - ts.start),
                                                      ps.data.data() + (start - ps.start),
                                                      (int) (end - start), maxLag, sr);

        // i colpi si accumulano finché reference, destinazione e coppie restano le stesse
        p->addHits (ref->getUid() + "|" + t->getUid() + "|" + juce::String (ps.members) + "|" + juce::String (ts.members), hits);
        rel[p] = PhaseAnalyzer::vote (p->getHitPool(), maxLag, sr);
    }

    // 2) spostamento e polarità totali: relativi + quelli della destinazione (catene)
    std::map<ScribaAlignProcessor*, int> state;   // 0 = da fare, 1 = in corso, 2 = fatto
    int done = 0, reliable = 0;

    std::function<void (ScribaAlignProcessor*)> resolve = [&] (ScribaAlignProcessor* p)
    {
        if (p == ref || isFollower (p) || state[p] == 2) return;
        state[p] = 1;

        auto* t = target[p];
        AlignResult r = rel[p];

        if (t != nullptr && t != ref)
        {
            if (state[t] == 1)
            {
                r.valid = false;
                r.message = "Destinazioni in cerchio: scegli un'altra traccia";
            }
            else
            {
                resolve (t);
            }
        }

        if (r.valid && ! r.excluded)
        {
            const double totalMs = r.lagMs + ((t != nullptr && t != ref) ? t->getShiftMs() : 0.0);
            if (std::abs (totalMs) >= maxShiftMs)
            {
                r.valid = false;
                r.message = "Fuori range: oltre 20 ms";
            }
            else
            {
                // la polarità si misura sulle catture originali: se la destinazione
                // è già stata invertita, questa traccia va invertita al contrario
                const bool totalInvert = r.polarityInverted != (t != nullptr && t->isInverted());

                r.lagMs      = totalMs;
                r.lagSamples = totalMs * sr / 1000.0;
                p->setShiftMs (totalMs);
                p->setInverted (totalInvert);
                r.polarityInverted = totalInvert;
                ++reliable;
            }
        }
        else if (r.valid)
        {
            ++reliable;   // room: resta ferma
        }

        p->setLastResult (r);
        state[p] = 2;
        ++done;
    };

    for (auto* p : all)
        resolve (p);

    // 3) le tracce in coppia prendono il risultato della loro guida
    for (auto* p : all)
    {
        auto* lead = leaderOf (p);
        if (lead == p) continue;

        AlignResult r;
        if (lead == ref)
        {
            r.valid = true;
            r.agreement = 1.0f;
        }
        else
        {
            r = lead->getLastResult();
        }
        r.lagMs            = lead->getShiftMs();
        r.lagSamples       = r.lagMs * sr / 1000.0;
        r.polarityInverted = lead->isInverted();
        r.pairedWith       = lead->getTrackName();
        p->setLastResult (r);

        ++done;
        if (r.valid) ++reliable;
    }

    if (done == 0)
        setStatus ("Nessuna altra istanza di Scriba Align trovata");
    else if (reliable == done)
        setStatus ("Analisi completata: " + juce::String (reliable) + " tracce allineate");
    else
        setStatus ("Allineate " + juce::String (reliable) + " tracce su " + juce::String (done)
                   + ". Per le altre premi di nuovo ANALIZZA su un altro tratto");
}
