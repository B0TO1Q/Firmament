// Offline stability / sanity test for the reverb engine (no JUCE needed).
#include "../Source/dsp/ReverbEngine.h"
#include <cstdio>
#include <cstdlib>
#include <random>

using namespace fm;

static float lastRms = 0;
struct Result { float peak = 0, rmsDuring = 0, t60 = -1; bool bad = false; };

static Result run (Params p, float sr, float burstSec, float tailSec, bool freezeAfter = false)
{
    ReverbEngine e; e.prepare (sr); e.setParams (p);
    std::mt19937 rng (7); std::uniform_real_distribution<float> u (-0.5f, 0.5f);
    const int block = 256;
    const int nBurst = (int) (burstSec * sr), nTail = (int) (tailSec * sr);
    std::vector<float> L (block), R (block);
    Result r; double acc = 0; long cnt = 0;
    float refRms = 0; double win = 0; int wc = 0; const int winN = (int) (0.05f * sr);
    for (int pos = 0; pos < nBurst + nTail; pos += block)
    {
        if (freezeAfter && pos >= nBurst) { p.freeze = true; e.setParams (p); }
        for (int i = 0; i < block; ++i)
        {
            const bool on = pos + i < nBurst;
            // noise bursts with gaps so Tide retriggers
            const bool gate = on && (((pos + i) / (int) (0.5f * sr)) % 2 == 0 || p.mode != Tide);
            L[i] = gate ? u (rng) : 0.0f; R[i] = gate ? u (rng) : 0.0f;
        }
        e.process (L.data(), R.data(), block);
        for (int i = 0; i < block; ++i)
        {
            const float a = std::max (std::abs (L[i]), std::abs (R[i]));
            if (! std::isfinite (a)) r.bad = true;
            r.peak = std::max (r.peak, a);
            if (pos + i < nBurst) { acc += L[i] * L[i]; ++cnt; }
            else
            {
                win += L[i] * L[i];
                if (++wc == winN)
                {
                    const float rms = (float) std::sqrt (win / winN);
                    if (refRms == 0) refRms = rms;
                    else if (r.t60 < 0 && rms < refRms * 0.001f) r.t60 = (float) (pos + i - nBurst) / sr;
                    lastRms = rms; win = 0; wc = 0;
                }
            }
        }
    }
    r.rmsDuring = (float) std::sqrt (acc / std::max (1L, cnt));
    return r;
}

int main()
{
    int fails = 0;
    for (float sr : { 44100.0f, 96000.0f })
    for (int m = 0; m < kNumModes; ++m)
    for (float dec : { 0.0f, 0.5f, 1.0f })
    for (float ch : { 0.0f, 1.0f })
    {
        Params p; p.mode = m; p.decay = dec; p.character = ch; p.mix = 1.0f; p.mod = 1.0f; p.tone = 1.0f;
        p.rise = 1.0f; p.fall = 1.0f; p.fineCents = 50.0f;
        const float want = decaySeconds (m, dec);
        const float tail = std::min (want * 1.6f + 1.0f, 12.0f);
        Result r = run (p, sr, 3.0f, tail);
        const bool fail = r.bad || r.peak > 6.0f;
        fails += fail;
        if (sr == 44100.0f || fail)
            std::printf ("%-9s sr=%5.0f decay=%.1f char=%.0f  wetRMS=%.3f peak=%.2f  t60 want=%5.2f got=%5.2f %s\n",
                         modeName (m), sr, dec, ch, r.rmsDuring, r.peak, want, r.t60, fail ? "FAIL" : "");
    }
    // Freeze: tail must hold without growing.
    for (int m = 0; m < kNumModes; ++m)
    {
        Params p; p.mode = m; p.mix = 1.0f; p.mod = 1.0f; p.character = 1.0f; p.rise = 1; p.fall = 1;
        Result r = run (p, 48000.0f, 2.0f, 40.0f, true);
        const bool fail = r.bad || r.peak > 6.0f;
        fails += fail;
        std::printf ("freeze %-9s peak=%.2f rms@40s=%.4f %s\n", modeName (m), r.peak, lastRms, fail ? "FAIL" : "");
    }
    std::printf (fails ? "FAILURES: %d\n" : "ALL OK\n", fails);
    return fails ? 1 : 0;
}
