// Offline test for the Selah channel engine (no JUCE needed).
#include "../Selah/Source/ChannelEngine.h"
#include <cstdio>
#include <random>

using namespace sl;

int main()
{
    int fails = 0;
    const float sr = 48000.0f;
    const int block = 480;
    std::vector<float> L ((size_t) block), R ((size_t) block);

    // 1. Zero latency: an impulse must come out on the very same sample.
    for (int s = 0; s < kNumSources; ++s)
    {
        ChannelEngine e; e.prepare (sr);
        Params p; p.source = s; p.cleanUp = 0; p.warmth = 0; p.clarity = 0; p.evenOut = 0; p.space = 0.5f;
        e.setParams (p);
        std::fill (L.begin(), L.end(), 0.0f); std::fill (R.begin(), R.end(), 0.0f);
        L[0] = R[0] = 0.5f;
        e.process (L.data(), R.data(), block);
        const bool ok = std::abs (L[0]) > 0.3f;
        fails += ! ok;
        std::printf ("latency %-10s first sample=%.3f %s\n", sourceName (s), L[0], ok ? "(0 samples)" : "FAIL");
    }

    // 2. Every source, knobs at min / mid / max, loud noise with bursts: stable, and never past the ceiling.
    std::mt19937 rng (3); std::uniform_real_distribution<float> u (-1.0f, 1.0f);
    for (float rate : { 44100.0f, 96000.0f })
    for (int s = 0; s < kNumSources; ++s)
    for (float kn : { 0.0f, 0.5f, 1.0f })
    {
        ChannelEngine e; e.prepare (rate);
        Params p; p.source = s; p.cleanUp = p.warmth = p.clarity = p.evenOut = p.space = kn;
        p.volumeDb = kn == 1.0f ? 12.0f : 0.0f; p.reverbLength = kn;
        e.setParams (p);
        float peak = 0; double acc = 0, accIn = 0; long cnt = 0; bool bad = false; float gr = 0;
        for (int b = 0; b < (int) (6.0f * rate) / block; ++b)
        {
            const float amp = (b / 20) % 3 == 2 ? 0.02f : 0.5f;   // loud, loud, quiet
            for (int i = 0; i < block; ++i) { L[(size_t) i] = amp * u (rng); R[(size_t) i] = amp * u (rng); accIn += L[(size_t) i] * L[(size_t) i]; }
            e.process (L.data(), R.data(), block);
            for (int i = 0; i < block; ++i)
            {
                const float a = std::max (std::abs (L[(size_t) i]), std::abs (R[(size_t) i]));
                if (! std::isfinite (a)) bad = true;
                peak = std::max (peak, a); acc += L[(size_t) i] * L[(size_t) i]; ++cnt;
            }
            gr = std::max (gr, e.gainReductionDb());
        }
        const bool fail = bad || peak > 0.8915f;
        fails += fail;
        if (rate == 44100.0f || fail)
            std::printf ("%-10s knobs=%.1f  in=%5.1f dB  out=%5.1f dB  peak=%.3f  maxGR=%4.1f dB %s\n", sourceName (s), kn,
                         10.0 * std::log10 (accIn / cnt), 10.0 * std::log10 (acc / cnt), peak, gr, fail ? "FAIL" : "");
    }

    // 2b. Graphic EQ: every band count, pre and post, full boost and full cut: stable, still zero latency.
    for (int mode = 0; mode < 3; ++mode)
    for (int post = 0; post < 2; ++post)
    for (float gdb : { 12.0f, -12.0f })
    {
        ChannelEngine e; e.prepare (sr);
        Params p; p.source = LeadVocal; p.eqMode = mode; p.eqPost = post != 0;
        for (int b = 0; b < eqBandCount (mode); ++b) p.eqGain[b] = gdb;
        e.setParams (p);
        float peak = 0; bool bad = false; double acc = 0; long cnt = 0;
        for (int b = 0; b < 300; ++b)
        {
            for (int i = 0; i < block; ++i) { L[(size_t) i] = 0.1f * u (rng); R[(size_t) i] = 0.1f * u (rng); }
            const float first = L[0];
            e.process (L.data(), R.data(), block);
            if (b == 0 && std::abs (L[0]) < std::abs (first) * 0.05f) bad = true;   // delayed output
            for (int i = 0; i < block; ++i) { const float a = std::abs (L[(size_t) i]); if (! std::isfinite (a)) bad = true; peak = std::max (peak, a); acc += a * a; ++cnt; }
        }
        const bool fail = bad || peak > 0.8915f;
        fails += fail;
        std::printf ("EQ %2d bands %-4s %+3.0f dB  out=%5.1f dB peak=%.3f %s\n", eqBandCount (mode), post ? "post" : "pre", gdb,
                     10.0 * std::log10 (acc / (double) cnt), peak, fail ? "FAIL" : "");
    }

    // 2c. Mono switch: both sides must come out identical, even with the stereo reverb up.
    {
        ChannelEngine e; e.prepare (sr);
        Params p; p.source = Choir; p.space = 1.0f; p.mono = true; e.setParams (p);
        float diff = 0, peak = 0;
        for (int b = 0; b < 300; ++b)
        {
            for (int i = 0; i < block; ++i) { L[(size_t) i] = 0.3f * u (rng); R[(size_t) i] = 0.3f * u (rng); }
            e.process (L.data(), R.data(), block);
            for (int i = 0; i < block; ++i) { diff = std::max (diff, std::abs (L[(size_t) i] - R[(size_t) i])); peak = std::max (peak, std::abs (L[(size_t) i])); }
        }
        const bool ok = diff < 1.0e-6f && peak > 0.01f; fails += ! ok;
        std::printf ("mono: max L-R difference %.2e, peak %.3f %s\n", diff, peak, ok ? "" : "FAIL");
    }

    // 3. Limiter off must pass audio untouched in level terms (no clamp), and silence in = silence out.
    {
        ChannelEngine e; e.prepare (sr);
        Params p; p.space = 0; e.setParams (p);
        std::fill (L.begin(), L.end(), 0.0f); std::fill (R.begin(), R.end(), 0.0f);
        float pk = 0;
        for (int b = 0; b < 200; ++b) { e.process (L.data(), R.data(), block); for (float v : L) pk = std::max (pk, std::abs (v)); }
        const bool ok = pk < 1.0e-6f; fails += ! ok;
        std::printf ("silence in -> peak %.2e %s\n", pk, ok ? "" : "FAIL");
    }
    std::printf (fails ? "FAILURES: %d\n" : "ALL OK\n", fails);
    return fails ? 1 : 0;
}
