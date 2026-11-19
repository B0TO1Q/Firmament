// Offline tests for the Selah channel engine (no JUCE needed).
#include "../Selah/Source/ChannelEngine.h"
#include <cstdio>
#include <random>

using namespace sl;

static void runBlocks (ChannelEngine& e, std::vector<float>& L, std::vector<float>& R, int block)
{
    for (size_t pos = 0; pos < L.size(); pos += (size_t) block)
        e.process (L.data() + pos, R.data() + pos, (int) std::min<size_t> ((size_t) block, L.size() - pos));
}

int main()
{
    int fails = 0;
    const float sr = 48000.0f;
    std::mt19937 rng (3); std::uniform_real_distribution<float> u (-1.0f, 1.0f);

    // 1. Every tool name is unique within its source, and every source's set is different.
    for (int s = 0; s < kNumSources; ++s)
        for (int a = 0; a < kTools; ++a)
            for (int b = a + 1; b < kTools; ++b)
                if (std::string (toolSpec (s, a).name) == toolSpec (s, b).name) { ++fails; std::printf ("duplicate tool %s in %s FAIL\n", toolSpec (s, a).name, sourceName (s)); }
    for (int s = 0; s < kNumSources; ++s)
        for (int s2 = s + 1; s2 < kNumSources; ++s2)
        {
            int same = 0;
            for (int a = 0; a < kTools; ++a) for (int b = 0; b < kTools; ++b) same += std::string (toolSpec (s, a).name) == toolSpec (s2, b).name;
            if (same == kTools) { ++fails; std::printf ("%s and %s have the same tools FAIL\n", sourceName (s), sourceName (s2)); }
        }
    std::printf ("tool sets: checked\n");

    // 2. All knobs down: output equals input.
    for (int s = 0; s < kNumSources; ++s)
    {
        ChannelEngine e; e.prepare (sr); Params p; p.source = s; p.limiter = true; e.setParams (p);
        std::vector<float> in (48000), L, R; for (auto& v : in) v = 0.25f * u (rng);
        L = in; R = in; runBlocks (e, L, R, 512);
        double err = 0, ref = 0; for (size_t i = 0; i < in.size(); ++i) { err += (double) (L[i] - in[i]) * (L[i] - in[i]); ref += (double) in[i] * in[i]; }
        const double d = 10 * std::log10 (std::max (err / ref, 1e-20));
        const bool ok = d < -90; fails += ! ok;
        if (! ok) std::printf ("%-10s all off: difference %.0f dB FAIL\n", sourceName (s), d);
    }
    std::printf ("all knobs down: transparent on every source\n");

    // 3. Zero latency with each source's default setup.
    for (int s = 0; s < kNumSources; ++s)
    {
        ChannelEngine e; e.prepare (sr); Params p; p.source = s;
        for (int t = 0; t < kTools; ++t) p.tool[s][t] = toolSpec (s, t).def;
        e.setParams (p);
        std::vector<float> L (512, 0.0f), R (512, 0.0f); L[0] = R[0] = 0.5f;
        e.process (L.data(), R.data(), 512);
        const bool ok = std::abs (L[0]) > 0.15f; fails += ! ok;
        std::printf ("latency %-10s first sample %.3f %s\n", sourceName (s), L[0], ok ? "(0 samples)" : "FAIL");
    }

    // 4. Every tool alone at full, and every source with all six at full: stable, under the ceiling.
    for (float rate : { 44100.0f, 96000.0f })
        for (int s = 0; s < kNumSources; ++s)
            for (int t = -1; t < kTools; ++t)
            {
                ChannelEngine e; e.prepare (rate); Params p; p.source = s; p.volumeDb = 12.0f; p.reverbLength = 1.0f;
                for (int j = 0; j < kTools; ++j) p.tool[s][j] = (t < 0 || t == j) ? 1.0f : 0.0f;
                e.setParams (p);
                std::vector<float> L ((size_t) (5 * rate)), R (L.size());
                for (size_t i = 0; i < L.size(); ++i) { const float a = (i / 9600) % 3 == 2 ? 0.02f : 0.5f; L[i] = a * u (rng); R[i] = a * u (rng); }
                runBlocks (e, L, R, 480);
                float pk = 0; bool bad = false;
                for (size_t i = 0; i < L.size(); ++i) { const float v = std::max (std::abs (L[i]), std::abs (R[i])); if (! std::isfinite (v)) bad = true; pk = std::max (pk, v); }
                const bool fail = bad || pk > 0.8915f; fails += fail;
                if (fail) std::printf ("%-10s %-10s %.0f Hz peak %.3f FAIL\n", sourceName (s), t < 0 ? "ALL SIX" : toolSpec (s, t).name, rate, pk);
            }
    std::printf ("every tool and every full setup: stable and under -1 dBFS\n");

    // 5. Each source keeps its own knobs: switching away and back restores the sound.
    {
        ChannelEngine e; e.prepare (sr); Params p;
        p.tool[Speaker][4] = 1.0f; p.tool[Drums][4] = 1.0f;
        p.source = Speaker; e.setParams (p);
        std::vector<float> L (24000), R (24000); for (size_t i = 0; i < L.size(); ++i) L[i] = R[i] = 0.4f * std::sin (6.283f * 440.0f * (float) i / sr);
        auto a = L; auto b = R; runBlocks (e, a, b, 480);
        p.source = Drums; e.setParams (p); auto c2 = L; auto d2 = R; runBlocks (e, c2, d2, 480);
        p.source = Speaker; e.setParams (p); auto x = L; auto y = R; runBlocks (e, x, y, 480);
        double diff = 0; for (size_t i = 12000; i < 24000; ++i) diff = std::max (diff, (double) std::abs (x[i] - a[i]));
        const bool ok = diff < 1e-3; fails += ! ok;
        std::printf ("source memory: Speaker sounds the same after visiting Drums (max diff %.1e) %s\n", diff, ok ? "" : "FAIL");
    }

    // 6. Mono switch: identical sides, even with stereo tools up.
    {
        ChannelEngine e; e.prepare (sr); Params p; p.source = Choir; p.tool[Choir][4] = 1.0f; p.tool[Choir][5] = 1.0f; p.mono = true; e.setParams (p);
        std::vector<float> L (48000), R (48000); for (size_t i = 0; i < L.size(); ++i) { L[i] = 0.3f * u (rng); R[i] = 0.3f * u (rng); }
        runBlocks (e, L, R, 480);
        float d = 0; for (size_t i = 0; i < L.size(); ++i) d = std::max (d, std::abs (L[i] - R[i]));
        const bool ok = d < 1e-6f; fails += ! ok;
        std::printf ("mono: max L-R difference %.1e %s\n", d, ok ? "" : "FAIL");
    }

    // 7. Graphic EQ in every band count, before and after, full boost and cut.
    for (int mode = 0; mode < 3; ++mode)
        for (int post = 0; post < 2; ++post)
            for (float g : { 12.0f, -12.0f })
            {
                ChannelEngine e; e.prepare (sr); Params p; p.source = LeadVocal; p.eqMode = mode; p.eqPost = post != 0;
                for (int b = 0; b < eqBandCount (mode); ++b) p.eqGain[b] = g;
                e.setParams (p);
                std::vector<float> L (96000), R (96000); for (size_t i = 0; i < L.size(); ++i) { L[i] = 0.1f * u (rng); R[i] = L[i]; }
                runBlocks (e, L, R, 480);
                float pk = 0; bool bad = false; for (float v : L) { if (! std::isfinite (v)) bad = true; pk = std::max (pk, std::abs (v)); }
                const bool fail = bad || pk > 0.8915f; fails += fail;
                if (fail) std::printf ("EQ %d bands %s %+.0f FAIL\n", eqBandCount (mode), post ? "after" : "before", g);
            }
    std::printf ("graphic EQ: stable in all band counts\n");

    std::printf (fails ? "FAILURES: %d\n" : "ALL OK\n", fails);
    return fails ? 1 : 0;
}
