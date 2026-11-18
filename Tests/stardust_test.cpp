// Offline tests for the STARDUST engine (no JUCE needed).
#include "../Stardust/Source/StardustEngine.h"
#include <cstdio>
#include <random>

using namespace sd;

static double dB (double v) { return 20.0 * std::log10 (std::max (v, 1e-12)); }

int main()
{
    int fails = 0;
    const float sr = 48000.0f;
    const int block = 512;
    std::mt19937 rng (4); std::uniform_real_distribution<float> u (-1.0f, 1.0f);

    // 1. Everything off: output must equal input (transparent), even at full MIX.
    {
        Engine e; e.prepare (sr); Params p; e.setParams (p);
        double err = 0, ref = 0;
        std::vector<float> L (block), R (block), in (block);
        Transport t; t.playing = true;
        for (int b = 0; b < 200; ++b)
        {
            for (int i = 0; i < block; ++i) { in[(size_t) i] = 0.3f * u (rng); L[(size_t) i] = R[(size_t) i] = in[(size_t) i]; }
            e.process (L.data(), R.data(), block, t);
            t.ppq += block * t.bpm / 60.0 / sr;
            for (int i = 0; i < block; ++i) { err += (double) (L[(size_t) i] - in[(size_t) i]) * (L[(size_t) i] - in[(size_t) i]); ref += (double) in[(size_t) i] * in[(size_t) i]; }
        }
        const double d = 10 * std::log10 (std::max (err / ref, 1e-20));
        const bool ok = d < -100; fails += ! ok;
        std::printf ("all off: difference %.0f dB %s\n", d, ok ? "(transparent)" : "FAIL");
    }

    // 2. Zero latency: an impulse comes out on the same sample, with every effect up.
    {
        Engine e; e.prepare (sr);
        Params p; p.drive = p.glow = p.orchestra = 0.5f; p.nebula = p.shimmer = 0.5f; e.setParams (p);
        std::vector<float> L (block, 0.0f), R (block, 0.0f); L[0] = R[0] = 0.5f;
        Transport t; t.playing = true; t.ppq = 0.5;   // mid-beat: pump and stutter idle
        e.process (L.data(), R.data(), block, t);
        const bool ok = std::abs (L[0]) > 0.1f; fails += ! ok;
        std::printf ("latency: first sample %.3f %s\n", L[0], ok ? "(0 samples)" : "FAIL");
    }

    // 3. Every effect alone and all together, min/mid/max, loud noise, 44.1/96 kHz: stable and under the ceiling.
    const char* names[7] = { "drive", "glow", "orchestra", "stutter", "shimmer", "nebula", "pump" };
    for (float rate : { 44100.0f, 96000.0f })
        for (int fx = 0; fx <= 7; ++fx)
            for (float amt : { 0.5f, 1.0f })
            {
                Engine e; e.prepare (rate);
                Params p;
                float* knobs[7] = { &p.drive, &p.glow, &p.orchestra, &p.stutter, &p.shimmer, &p.nebula, &p.pump };
                for (int j = 0; j < 7; ++j) if (fx == 7 || fx == j) *knobs[j] = amt;
                p.shimmerDir = Both; p.filter = fx == 7 ? -0.5f : 0.0f; p.outputDb = 12.0f;
                e.setParams (p);
                std::vector<float> L (block), R (block);
                Transport t; t.playing = true; t.bpm = 124;
                float peak = 0; bool bad = false; double acc = 0; long cnt = 0;
                for (int b = 0; b < (int) (8.0f * rate) / block; ++b)
                {
                    const float a = (b / 30) % 2 ? 0.02f : 0.6f;
                    for (int i = 0; i < block; ++i) { L[(size_t) i] = a * u (rng); R[(size_t) i] = a * u (rng); }
                    e.process (L.data(), R.data(), block, t);
                    t.ppq += block * t.bpm / 60.0 / rate;
                    for (int i = 0; i < block; ++i)
                    {
                        const float v = std::max (std::abs (L[(size_t) i]), std::abs (R[(size_t) i]));
                        if (! std::isfinite (v)) bad = true;
                        peak = std::max (peak, v); acc += (double) L[(size_t) i] * L[(size_t) i]; ++cnt;
                    }
                }
                const bool fail = bad || peak > 0.97f;
                fails += fail;
                if (rate == 44100.0f || fail)
                    std::printf ("%-9s %.1f  rms %6.1f dB  peak %.3f %s\n", fx == 7 ? "ALL" : names[fx], amt, 10 * std::log10 (acc / (double) cnt), peak, fail ? "FAIL" : "");
            }

    // 4. Freeze holds a tail for a long time without growing.
    {
        Engine e; e.prepare (sr); Params p; p.nebula = 1; p.shimmer = 1; e.setParams (p);
        std::vector<float> L (block), R (block); Transport t;
        float early = 0, late = 0, peak = 0;
        for (int b = 0; b < (int) (60 * sr) / block; ++b)
        {
            const bool feed = b < (int) (2 * sr) / block;
            if (! feed && ! p.freeze) { p.freeze = true; e.setParams (p); }
            for (int i = 0; i < block; ++i) { L[(size_t) i] = feed ? 0.3f * u (rng) : 0.0f; R[(size_t) i] = feed ? 0.3f * u (rng) : 0.0f; }
            e.process (L.data(), R.data(), block, t);
            for (int i = 0; i < block; ++i)
            {
                const float v = std::abs (L[(size_t) i]); peak = std::max (peak, v);
                if (b * block > 5 * sr && b * block < 6 * sr) early = std::max (early, v);
                if (b * block > 58 * sr) late = std::max (late, v);
            }
        }
        const bool ok = late > early * 0.2f && peak < 0.97f; fails += ! ok;
        std::printf ("freeze: level at 5 s %.3f, at 58 s %.3f, peak %.3f %s\n", early, late, peak, ok ? "(holds)" : "FAIL");
    }

    // 5. Stutter really repeats: with full density at 1/16, the second 16th of each beat equals the first.
    {
        Engine e; e.prepare (sr); Params p; p.stutter = 1.0f; p.stutterRate = Sixteenth; e.setParams (p);
        const double bpm = 120; const int spb = (int) (60.0 / bpm * sr);
        std::vector<float> in ((size_t) spb * 4), L, R;
        for (auto& v : in) v = 0.3f * u (rng);
        L = in; R = in;
        Transport t; t.playing = true; t.bpm = bpm; t.ppq = 0;
        for (size_t pos = 0; pos < L.size(); pos += (size_t) block)
        {
            const int m = (int) std::min<size_t> ((size_t) block, L.size() - pos);
            e.process (L.data() + pos, R.data() + pos, m, t);
            t.ppq += m * bpm / 60.0 / sr;
        }
        const int s16 = spb / 4;
        double diff = 0, ref = 0;
        for (int i = 200; i < s16 - 200; ++i)   // compare inside the slice, away from the chop fades
        {
            const double a = L[(size_t) (spb + i)], b = L[(size_t) (spb + s16 + i)];
            diff += (a - b) * (a - b); ref += a * a;
        }
        const double d = 10 * std::log10 (std::max (diff / ref, 1e-20));
        const bool ok = d < -20; fails += ! ok;
        std::printf ("stutter: repeat matches first slice to %.0f dB %s\n", d, ok ? "(locked to the grid)" : "FAIL");
    }

    // 6. Pump: level is lowest right after the beat and recovers before the next.
    {
        Engine e; e.prepare (sr); Params p; p.pump = 1.0f; e.setParams (p);
        const int spb = (int) (60.0 / 120.0 * sr);
        std::vector<float> L ((size_t) spb * 2, 0.5f), R ((size_t) spb * 2, 0.5f);
        Transport t; t.playing = true; t.bpm = 120; t.ppq = 0;
        for (size_t pos = 0; pos < L.size(); pos += (size_t) block)
        {
            const int m = (int) std::min<size_t> ((size_t) block, L.size() - pos);
            e.process (L.data() + pos, R.data() + pos, m, t);
            t.ppq += m * 120.0 / 60.0 / sr;
        }
        const double atBeat = L[(size_t) (spb + spb / 50)], late = L[(size_t) (spb + spb * 9 / 10)];
        const bool ok = dB (atBeat / late) < -12; fails += ! ok;
        std::printf ("pump: just after the beat %.1f dB below the end of the beat %s\n", -dB (atBeat / late), ok ? "" : "FAIL");
    }

    std::printf (fails ? "FAILURES: %d\n" : "ALL OK\n", fails);
    return fails ? 1 : 0;
}
