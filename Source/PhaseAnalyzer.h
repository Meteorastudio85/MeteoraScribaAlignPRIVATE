#pragma once
#include <juce_core/juce_core.h>
#include <vector>

// Misura su un singolo colpo della reference
struct HitMeasure
{
    double lag   = 0.0;   // campioni, > 0 = la traccia arriva dopo
    double score = 0.0;   // somiglianza 0..1 con il colpo della reference
    double sign  = 1.0;   // -1 se la forma d'onda è invertita
};

// Risultato del confronto tra una traccia e la sua destinazione.
struct AlignResult
{
    bool   valid = false;            // true solo se il risultato è affidabile
    double lagSamples = 0.0;         // > 0: la traccia arriva IN RITARDO
    double lagMs = 0.0;
    float  agreement = 0.0f;         // 0..1, quota di colpi d'accordo sul valore trovato
    int    hitsUsed = 0;             // colpi analizzati (anche accumulati da più catture)
    bool   polarityInverted = false; // true: la traccia va anche invertita di polarità
    float  polarityAgreement = 1.0f; // 0..1, quota di colpi d'accordo sulla polarità
    juce::String pairedWith;         // nome della traccia guida, se questa è in coppia
    bool   excluded = false;         // true: traccia che resta ferma di proposito (room)
    juce::String message;            // vuoto se tutto ok
};

namespace PhaseAnalyzer
{
    constexpr int   minHits      = 4;
    constexpr float minAgreement = 0.7f;    // con pochi colpi
    constexpr int   manyHits     = 16;      // con tanti colpi accumulati...
    constexpr float minAgreementManyHits = 0.6f;   // ...basta un po' meno
    constexpr float minPolarityAgreement = 0.75f;  // colpi d'accordo sulla polarità
    constexpr int   maxPool      = 200;   // colpi accumulati al massimo

    // 1) trova i colpi netti e isolati della reference;
    // 2) per ogni colpo cerca nella traccia le salite nette entro ±maxLag
    //    e sceglie quella che somiglia di più al colpo;
    // 3) rifinisce con correlazione normalizzata e interpolazione sub-campione.
    std::vector<HitMeasure> measureHits (const float* ref, const float* tgt, int numSamples,
                                         int maxLag, double sampleRate);

    // 4) i colpi votano: vince il gruppo più forte entro ±0,15 ms;
    //    dentro il gruppo votano anche la polarità (va applicata insieme allo spostamento,
    //    altrimenti si allineano le forme d'onda rovesciate e si ottiene la massima cancellazione).
    AlignResult vote (const std::vector<HitMeasure>& hits, int maxLag, double sampleRate);

    inline AlignResult computeAlignment (const float* ref, const float* tgt, int numSamples,
                                         int maxLag, double sampleRate)
    {
        return vote (measureHits (ref, tgt, numSamples, maxLag, sampleRate), maxLag, sampleRate);
    }
}
