#include "PhaseAnalyzer.h"
#include <vector>
#include <algorithm>
#include <cmath>
#include <numeric>

namespace
{
    // Inviluppo in dB: media mobile centrata di x² su 'win' campioni
    std::vector<float> envelopeDb (const float* x, int n, int win)
    {
        std::vector<double> pre ((size_t) n + 1, 0.0);
        for (int i = 0; i < n; ++i)
            pre[(size_t) i + 1] = pre[(size_t) i] + (double) x[i] * x[i];

        std::vector<float> db ((size_t) n);
        const int half = (win - 1) / 2;
        for (int i = 0; i < n; ++i)
        {
            const int a = std::max (0, i - half), b = std::min (n, i - half + win);
            const double m = (pre[(size_t) b] - pre[(size_t) a]) / (double) win;
            db[(size_t) i] = (float) (10.0 * std::log10 (m + 1.0e-14));
        }
        return db;
    }

    float maxIn (const std::vector<float>& v, int a, int b)
    {
        a = std::max (a, 0); b = std::min (b, (int) v.size());
        float m = -1.0e9f;
        for (int i = a; i < b; ++i) m = std::max (m, v[(size_t) i]);
        return m;
    }

    struct Hit { float score; int pos; };

    // Colpi della reference: salita forte, preceduta da un tratto più silenzioso
    std::vector<int> referenceHits (const std::vector<float>& db, int n, int maxLag, double sr, int maxHits = 40)
    {
        const int ms = (int) (0.001 * sr);

        std::vector<float> sorted (db);
        const size_t k = (size_t) std::clamp ((double) sorted.size() * 0.999, 0.0, (double) sorted.size() - 1.0);
        std::nth_element (sorted.begin(), sorted.begin() + (long) k, sorted.end());
        const float top = sorted[k];

        std::vector<Hit> hits;
        int i = maxLag + 12 * ms;
        const int end = n - maxLag - 12 * ms;

        while (i < end)
        {
            if (db[(size_t) i] > top - 20.0f && db[(size_t) i] - db[(size_t) (i - 3 * ms)] > 15.0f)
            {
                int j = i;
                while (j > i - 3 * ms && db[(size_t) (j - 1)] > db[(size_t) i] - 15.0f) --j;

                const float quiet = maxIn (db, j - 12 * ms, j - ms);
                const float peak  = maxIn (db, j, j + 5 * ms);
                if (peak - quiet > 12.0f)
                    hits.push_back ({ peak - quiet, j });

                i += 120 * ms;
            }
            else
            {
                ++i;
            }
        }

        std::sort (hits.begin(), hits.end(), [] (const Hit& a, const Hit& b) { return a.score > b.score; });
        if ((int) hits.size() > maxHits) hits.resize ((size_t) maxHits);

        std::vector<int> out;
        for (auto& h : hits) out.push_back (h.pos);
        std::sort (out.begin(), out.end());
        return out;
    }

    double normCorr (const float* a, const float* b, int len, double normA)
    {
        double num = 0.0, eb = 0.0;
        for (int i = 0; i < len; ++i) { num += (double) a[i] * b[i]; eb += (double) b[i] * b[i]; }
        return num / (normA * std::sqrt (eb) + 1.0e-12);
    }
}

std::vector<HitMeasure> PhaseAnalyzer::measureHits (const float* ref, const float* tgt, int n,
                                                     int maxLag, double sr)
{
    std::vector<HitMeasure> out;
    if (n < (int) (sr * 0.5) || sr <= 0.0)
        return out;

    const int ms   = (int) (0.001 * sr);
    const int A    = (int) (0.0005 * sr);   // prima dell'inizio del colpo
    const int B    = (int) (0.004 * sr);    // dopo l'inizio del colpo
    const int len  = A + B;
    const int envW = std::max (2, (int) (0.0005 * sr));

    const auto dbR = envelopeDb (ref, n, envW);
    const auto dbT = envelopeDb (tgt, n, envW);

    for (int o : referenceHits (dbR, n, maxLag, sr))
    {
        if (o - maxLag - 15 * ms <= 0 || o + maxLag + 10 * ms >= n)
            continue;

        const float* rs = ref + (o - A);
        double normR = 0.0;
        for (int i = 0; i < len; ++i) normR += (double) rs[i] * rs[i];
        normR = std::sqrt (normR);
        if (normR <= 0.0) continue;

        // candidati: salite nette del bersaglio entro ±maxLag
        std::vector<int> cands;
        for (int k = o - maxLag; k < o + maxLag; ++k)
            if (dbT[(size_t) k] - dbT[(size_t) (k - 2 * ms)] > 9.0f && dbT[(size_t) k] >= dbT[(size_t) (k - 1)])
                if (cands.empty() || k - cands.back() > ms)
                    cands.push_back (k);

        if (cands.empty()) continue;

        HitMeasure best;
        double bestAbs = -1.0;

        for (int c : cands)
        {
            const int base = c - o;
            const int lo = base - ms, hi = base + ms;
            std::vector<double> vals ((size_t) (hi - lo + 1));

            for (int lag = lo; lag <= hi; ++lag)
                vals[(size_t) (lag - lo)] = normCorr (rs, tgt + (o - A + lag), len, normR);

            int kBest = 0;
            for (int q = 1; q < (int) vals.size(); ++q)
                if (std::abs (vals[(size_t) q]) > std::abs (vals[(size_t) kBest])) kBest = q;

            const double v = std::abs (vals[(size_t) kBest]);
            if (v > bestAbs)
            {
                double frac = 0.0;
                if (kBest > 0 && kBest < (int) vals.size() - 1)
                {
                    const double y0 = std::abs (vals[(size_t) kBest - 1]), y2 = std::abs (vals[(size_t) kBest + 1]);
                    const double d = y0 - 2.0 * v + y2;
                    if (std::abs (d) > 1.0e-12) frac = std::clamp (0.5 * (y0 - y2) / d, -0.5, 0.5);
                }
                bestAbs    = v;
                best.lag   = (double) (lo + kBest) + frac;
                best.score = v;
                best.sign  = vals[(size_t) kBest] < 0.0 ? -1.0 : 1.0;
            }
        }

        out.push_back (best);
    }
    return out;
}

AlignResult PhaseAnalyzer::vote (const std::vector<HitMeasure>& hits, int maxLag, double sr)
{
    AlignResult res;
    res.hitsUsed = (int) hits.size();

    if (hits.empty())
    {
        res.message = "Nessun colpo chiaro: analizza un tratto con colpi della reference";
        return res;
    }

    // il gruppo con il punteggio più alto entro ±0,15 ms
    const double tol = 0.00015 * sr;
    size_t centre = 0;
    double bestScore = -1.0;
    for (size_t a = 0; a < hits.size(); ++a)
    {
        double s = 0.0;
        for (auto& h : hits)
            if (std::abs (h.lag - hits[a].lag) <= tol) s += h.score;
        if (s > bestScore) { bestScore = s; centre = a; }
    }

    std::vector<double> group;
    double posW = 0.0, negW = 0.0;
    for (auto& h : hits)
        if (std::abs (h.lag - hits[centre].lag) <= tol)
        {
            group.push_back (h.lag);
            (h.sign < 0.0 ? negW : posW) += h.score;   // voto pesato sulla somiglianza
        }

    std::sort (group.begin(), group.end());
    const size_t g = group.size();
    const double median = (g % 2 == 1) ? group[g / 2] : 0.5 * (group[g / 2 - 1] + group[g / 2]);

    res.lagSamples       = median;
    res.lagMs            = median * 1000.0 / sr;
    res.agreement        = (float) g / (float) hits.size();
    res.polarityInverted  = negW > posW;
    res.polarityAgreement = (float) (std::max (posW, negW) / std::max (1.0e-12, posW + negW));

    if (res.hitsUsed < minHits)
        res.message = juce::String::fromUTF8 ("Pochi colpi chiari: premi ANALIZZA su un altro tratto per aggiungerne");
    else if (res.agreement < (res.hitsUsed >= manyHits ? minAgreementManyHits : minAgreement))
        res.message = juce::String::fromUTF8 ("Risultato incerto: premi ANALIZZA su un altro tratto per aggiungere colpi");
    else if (res.polarityAgreement < minPolarityAgreement)
        res.message = juce::String::fromUTF8 ("Risultato incerto: polarit\xc3\xa0 non chiara, premi ANALIZZA su un altro tratto");
    else if (std::abs (median) >= (double) maxLag - 1.0)
        res.message = "Fuori range: oltre 20 ms";
    else
        res.valid = true;

    return res;
}
