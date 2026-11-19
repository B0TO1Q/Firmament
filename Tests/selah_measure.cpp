// Bench measurements for Selah: prints the numbers a tech would check on real hardware.
#include "../Selah/Source/ChannelEngine.h"
#include <chrono>
#include <complex>
#include <cstdio>
#include <random>

using namespace sl;
static const double PI = 3.14159265358979323846;

// amplitude of one frequency in a buffer (Hann-windowed single-bin DFT)
static double toneAmp (const std::vector<float>& x, size_t from, double f, double sr)
{
    std::complex<double> acc = 0; double wsum = 0; const size_t n = x.size() - from;
    for (size_t i = 0; i < n; ++i)
    {
        const double w = 0.5 - 0.5 * std::cos (2 * PI * (double) i / (double) (n - 1));
        acc += w * (double) x[from + i] * std::exp (std::complex<double> (0, -2 * PI * f * (double) i / sr));
        wsum += w;
    }
    return 2.0 * std::abs (acc) / wsum;
}
static double dB (double v) { return 20.0 * std::log10 (std::max (v, 1e-12)); }

struct Run { std::vector<float> L, R; };
static Run runSine (Params p, double f, double ampDb, double seconds, double sr, double f2 = 0, double amp2Db = -200)
{
    ChannelEngine e; e.prepare (sr); e.setParams (p);
    const size_t n = (size_t) (seconds * sr);
    Run r; r.L.resize (n); r.R.resize (n);
    const double a = std::pow (10.0, ampDb / 20.0), a2 = std::pow (10.0, amp2Db / 20.0);
    for (size_t i = 0; i < n; ++i)
        r.L[i] = r.R[i] = (float) (a * std::sin (2 * PI * f * (double) i / sr) + a2 * std::sin (2 * PI * f2 * (double) i / sr));
    for (size_t pos = 0; pos < n; pos += 256) e.process (r.L.data() + pos, r.R.data() + pos, (int) std::min<size_t> (256, n - pos));
    return r;
}
static Params one (int source, int tool, float amt) { Params p; p.source = source; p.limiter = false; p.tool[source][tool] = amt; return p; }
static double gainAt (Params p, double f, double sr = 48000, double lvl = -30)
{
    const Run r = runSine (p, f, lvl, 0.8, sr);
    return dB (toneAmp (r.L, r.L.size() / 2, f, sr)) - lvl;
}

int main()
{
    int fails = 0;
    const double sr = 48000;

    std::printf ("== 1. Bell accuracy near the top of the range (error vs the analogue curve, dB) ==\n");
    for (double fs : { 44100.0, 48000.0 })
        for (double fc : { 6300.0, 10000.0, 16000.0, 20000.0 })
            for (double g : { 12.0, -12.0 })
            {
                const double Q = 2.0, A = std::pow (10.0, g / 40.0);
                Biquad m, b; m.peak ((float) fc, (float) Q, (float) g, (float) fs); b.peakBilinear ((float) fc, (float) Q, (float) g, (float) fs);
                double errM = 0, errB = 0;
                for (double f = 200; f < fs * 0.47; f *= 1.03)
                {
                    const std::complex<double> s (0, f / fc);
                    const double ref = std::abs ((s * s + s * (A / Q) + 1.0) / (s * s + s / (A * Q) + 1.0));
                    const std::complex<double> z = std::exp (std::complex<double> (0, -2 * PI * f / fs));
                    const auto H = [&z] (const Biquad& q) { return std::abs ((double) q.b0 + (double) q.b1 * z + (double) q.b2 * z * z) / std::abs (1.0 + (double) q.a1 * z + (double) q.a2 * z * z); };
                    errM = std::max (errM, std::abs (dB (H (m)) - dB (ref)));
                    errB = std::max (errB, std::abs (dB (H (b)) - dB (ref)));
                }
                const bool stable = std::abs (m.a2) < 1.0f && std::abs (m.a1) < 1.0f + m.a2;
                const bool ok = stable && errM < 1.25;
                fails += ! ok;
                std::printf ("  fs=%5.0f fc=%5.0f %+3.0f dB: matched %.2f   plain bilinear %.2f %s\n", fs, fc, g, errM, errB, ok ? "" : "FAIL");
            }

    std::printf ("\n== 2. What every tool does at full (per source) ==\n");
    std::mt19937 rng (7); std::uniform_real_distribution<float> u (-1.0f, 1.0f);
    for (int s = 0; s < kNumSources; ++s)
    {
        std::printf ("  %s\n", sourceName (s));
        for (int t = 0; t < kTools; ++t)
        {
            const auto& ts = toolSpec (s, t);
            char res[200] = "";
            switch (ts.kind)
            {
                case Kind::Eq:
                {
                    int len = 0;
                    for (const auto& b : ts.band)
                        if (b.kind != None)
                            len += std::snprintf (res + len, sizeof res - (size_t) len, "%s%+.1f dB @ %.0f Hz", len ? ", " : "",
                                                  gainAt (one (s, t, 1.0f), b.kind == HighShelf ? std::min (16000.0, b.hz * 1.5) : (b.kind == LowShelf ? b.hz * 0.6 : b.hz)), b.kind == HighShelf ? std::min (16000.0, b.hz * 1.5) : (b.kind == LowShelf ? b.hz * 0.6 : b.hz));
                    break;
                }
                case Kind::LowCut: case Kind::Gate: case Kind::DynLow:
                    std::snprintf (res, sizeof res, "40 Hz %+.1f dB, 1 kHz %+.1f dB", gainAt (one (s, t, 1.0f), 40, sr, -6), gainAt (one (s, t, 1.0f), 1000, sr, -6)); break;
                case Kind::Comp: case Kind::Parallel:
                    std::snprintf (res, sizeof res, "tone at -30: %+.1f dB, at -10: %+.1f dB, at 0: %+.1f dB", gainAt (one (s, t, 1.0f), 1000, sr, -30), gainAt (one (s, t, 1.0f), 1000, sr, -10), gainAt (one (s, t, 1.0f), 1000, sr, 0)); break;
                case Kind::DeEss:
                    std::snprintf (res, sizeof res, "loud S band %+.1f dB, 300 Hz %+.1f dB", gainAt (one (s, t, 1.0f), ts.p[0], sr, -14), gainAt (one (s, t, 1.0f), 300, sr, -14)); break;
                case Kind::DynHigh:
                    std::snprintf (res, sizeof res, "loud 9 kHz %+.1f dB, 1 kHz %+.1f dB", gainAt (one (s, t, 1.0f), 9000, sr, -8), gainAt (one (s, t, 1.0f), 1000, sr, -8)); break;
                case Kind::Saturate:
                {
                    const double f = ts.p[1] > 0 ? 300.0 : (ts.p[2] > 0 ? 60.0 : 220.0);
                    const Run r = runSine (one (s, t, 1.0f), f, -10, 1.0, sr);
                    const double h1 = toneAmp (r.L, r.L.size() / 2, f, sr), h2 = toneAmp (r.L, r.L.size() / 2, 2 * f, sr), h3 = toneAmp (r.L, r.L.size() / 2, 3 * f, sr);
                    std::snprintf (res, sizeof res, "%.0f Hz at -10: 2nd %.1f dB, 3rd %.1f dB", f, dB (h2 / h1), dB (h3 / h1)); break;
                }
                default:
                {
                    // level change on a burst of noise, plus what is left 300 ms after it stops
                    ChannelEngine e; e.prepare (sr); e.setParams (one (s, t, 1.0f));
                    std::vector<float> L (48000, 0.0f), R (48000, 0.0f), in (48000, 0.0f);
                    for (size_t i = 0; i < 24000; ++i) { in[i] = L[i] = R[i] = 0.2f * u (rng); }
                    for (size_t pos = 0; pos < L.size(); pos += 480) e.process (L.data() + pos, R.data() + pos, 480);
                    double di = 0, dout = 0, side = 0, tail = 0;
                    for (size_t i = 12000; i < 24000; ++i) { di += (double) in[i] * in[i]; dout += (double) L[i] * L[i]; side += 0.25 * (double) (L[i] - R[i]) * (L[i] - R[i]); }
                    for (size_t i = 38400; i < 48000; ++i) tail += (double) L[i] * L[i];
                    std::snprintf (res, sizeof res, "level %+.1f dB, side %.1f dB, tail after 300 ms %.1f dB", 10 * std::log10 (dout / di), 10 * std::log10 (side / di + 1e-12), 10 * std::log10 (tail / di * 1.25 + 1e-12));
                }
            }
            std::printf ("    %-10s %s\n", ts.name, res);
        }
    }

    std::printf ("\n== 9. Processing load (everything on, 21-band EQ, 48 kHz stereo) ==\n");
    {
        std::mt19937 rng (1); std::uniform_real_distribution<float> u (-0.3f, 0.3f);
        Params p; p.source = LeadVocal; for (int t = 0; t < kTools; ++t) p.tool[LeadVocal][t] = 1; p.eqMode = 2; for (auto& g : p.eqGain) g = 6;
        ChannelEngine e; e.prepare (sr); e.setParams (p);
        std::vector<float> L (480), R (480);
        const auto t0 = std::chrono::steady_clock::now();
        const int blocks = 2000;   // 20 seconds of audio
        for (int b = 0; b < blocks; ++b) { for (auto& v : L) v = u (rng); R = L; e.process (L.data(), R.data(), 480); }
        const double el = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        std::printf ("  %.1f%% of one core on this build machine (%.0fx faster than real time)\n", 100 * el / 20.0, 20.0 / el);
    }

    std::printf (fails ? "\nFAILURES: %d\n" : "\nALL CHECKS PASSED\n", fails);
    return fails ? 1 : 0;
}
