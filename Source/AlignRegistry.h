#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>

class ScribaAlignProcessor;

// Registro condiviso da tutte le istanze di Scriba Align caricate nella stessa DAW.
// Ogni istanza si registra qui; la reference e la cattura sono coordinate da qui.
// Si usa tramite juce::SharedResourcePointer<AlignRegistry>.
class AlignRegistry : private juce::Timer
{
public:
    AlignRegistry() = default;
    ~AlignRegistry() override { stopTimer(); }

    // --- istanze (solo message thread) ---
    int  add (ScribaAlignProcessor*);
    void remove (ScribaAlignProcessor*);
    juce::Array<ScribaAlignProcessor*> getInstances() const;
    ScribaAlignProcessor* findById (int id) const;
    ScribaAlignProcessor* findByUid (const juce::String& uid) const;

    // Destinazione effettiva di una traccia: la sua scelta, oppure la reference globale.
    // Se la destinazione è in coppia con un'altra traccia, si usa la traccia guida della coppia.
    ScribaAlignProcessor* resolveTarget (const ScribaAlignProcessor* p) const;

    // Traccia guida della coppia (segue i collegamenti "in coppia con").
    // Restituisce p stessa se p è indipendente o se è la reference.
    ScribaAlignProcessor* leaderOf (const ScribaAlignProcessor* p) const;
    bool isFollower (const ScribaAlignProcessor* p) const { return leaderOf (p) != p; }

    // Ruolo della traccia, dedotto dal nome
    enum class Role { kick, snareTop, snareBottom, overhead, room, other };
    static Role roleOf (const juce::String& trackName);

    // Configurazione automatica, nessuna scelta a mano:
    //  - reference = la cassa (se ce ne sono più di una, quella "in");
    //  - overhead legati in un blocco unico (la coppia stereo si sposta insieme), allineati alla cassa;
    //  - rullante, tom, hi-hat... allineati agli overhead (che a loro volta sono allineati alla cassa);
    //  - rullante sotto allineato al rullante sopra;
    //  - le room restano ferme.
    void autoConfigure();

    void setReference (ScribaAlignProcessor*);
    ScribaAlignProcessor* getReference() const;
    bool isReference (const ScribaAlignProcessor* p) const;

    // --- cattura ---
    void startCapture();
    void cancelCapture();
    bool isCaptureRequested() const noexcept { return captureRequested.load(); }
    int  getCaptureGeneration() const noexcept { return generation.load(); }

    juce::String getStatus() const;

    // Lanciata automaticamente a fine cattura; si può anche richiamare a mano.
    void analyse();

    static constexpr double maxShiftMs     = 20.0; // range di correzione ±20 ms (~6,8 m)
    static constexpr double captureSeconds = 8.0;  // durata della cattura

private:
    void timerCallback() override;
    void syncPairs();   // le tracce in coppia copiano spostamento e polarità della guida
    void setStatus (const juce::String&);

    mutable juce::CriticalSection lock;
    juce::Array<ScribaAlignProcessor*> instances;
    ScribaAlignProcessor* reference = nullptr;
    int nextId = 1;

    std::atomic<bool> captureRequested { false };
    std::atomic<int>  generation { 0 };
    juce::String status { "Scegli la reference e premi Analizza" };

    JUCE_DECLARE_NON_COPYABLE (AlignRegistry)
};
