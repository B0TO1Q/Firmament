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
static Params flat (int source)
{
    Params p; p.source = source; p.cleanUp = p.warmth = p.clarity = p.evenOut = p.space = 0; p.limiter = false; return p;
}
static double gainAt (Params p, double f, double sr = 48000, double lvl = -30)
{
    const Run r = runSine (p, f, lvl, 0.6, sr);
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

    std::printf ("\n== 2. Tone-shaping per source (dB change at the knob's full setting) ==\n");
    std::printf ("  %-10s | low cut: -3dB near | warmth @shelf | mud cut | presence | air\n", "");
    for (int s = 0; s < kNumSources; ++s)
    {
        const auto& c = sourceCfg (s);
        Params p = flat (s); p.cleanUp = 1;
        double f3 = 0; for (double f = 400; f > 15; f /= 1.04) if (gainAt (p, f, 48000, -6) < -3.0) { f3 = f; break; }
        Params w = flat (s); w.warmth = 1;
        Params cl = flat (s); cl.clarity = 1;
        std::printf ("  %-10s | %5.0f Hz (%2.0f dB/oct) | %+5.1f @%4.0f  | %+5.1f   | %+5.1f    | %+5.1f\n", sourceName (s), f3,
                     gainAt (p, f3 / 2, 48000, -6) - gainAt (p, f3 / 4, 48000, -6), gainAt (w, c.warmHz * 0.5), c.warmHz,
                     gainAt (cl, c.mudHz), gainAt (cl, c.presHz), gainAt (cl, std::min (16000.0f, c.airHz * 1.4f)));
    }

    std::printf ("\n== 3. Warmth harmonics (knob full, tone at -10 dBFS; harmonics relative to the tone) ==\n");
    for (int s : { (int) LeadVocal, (int) Bass, (int) Drums, (int) FullMix })
        for (double f : { 60.0, 1000.0 })
        {
            Params p = flat (s); p.warmth = 1;
            const Run r = runSine (p, f, -10, 1.0, sr);
            const double h1 = toneAmp (r.L, r.L.size() / 2, f, sr), h2 = toneAmp (r.L, r.L.size() / 2, 2 * f, sr), h3 = toneAmp (r.L, r.L.size() / 2, 3 * f, sr);
            std::printf ("  %-10s %5.0f Hz: 2nd %6.1f dB (%.2f%%)   3rd %6.1f dB (%.2f%%)\n", sourceName (s), f, dB (h2 / h1), 100 * h2 / h1, dB (h3 / h1), 100 * h3 / h1);
        }

    std::printf ("\n== 4. Even Out: steady gain reduction for a 1 kHz tone (knob at 5 and at 10) and timing ==\n");
    for (int s = 0; s < kNumSources; ++s)
    {
        std::printf ("  %-10s", sourceName (s));
        for (float kn : { 0.5f, 1.0f })
        {
            Params p = flat (s); p.evenOut = kn;
            std::printf ("  knob %2.0f:", kn * 10);
            for (double in : { -30.0, -20.0, -10.0, 0.0 })
            {
                const Run r = runSine (p, 1000, in, 4.0, sr);
                std::printf (" %+5.1f", dB (toneAmp (r.L, r.L.size() * 3 / 4, 1000, sr)) - in);
            }
        }
        // attack / release: step from -40 to -8 dBFS and back
        Params p = flat (s); p.evenOut = 1;
        ChannelEngine e; e.prepare (sr); e.setParams (p);
        const size_t n = (size_t) (6 * sr); std::vector<float> L (n), R (n);
        for (size_t i = 0; i < n; ++i) { const double a = (i > (size_t) sr && i < (size_t) (3 * sr)) ? 0.4 : 0.01; L[i] = R[i] = (float) (a * std::sin (2 * PI * 1000 * (double) i / sr)); }
        for (size_t pos = 0; pos < n; pos += 256) e.process (L.data() + pos, R.data() + pos, 256 <= n - pos ? 256 : (int) (n - pos));
        const auto envAt = [&] (double t) { float m = 0; for (size_t i = (size_t) (t * sr); i < (size_t) (t * sr) + 96; ++i) m = std::max (m, std::abs (L[i])); return (double) m; };
        const double first = envAt (1.0005), settled = envAt (2.9), quiet0 = envAt (0.9), quietEnd = envAt (5.9);
        double ta = 0, tr = 0;
        for (double t = 1.0005; t < 2.9; t += 0.0005) if (envAt (t) < settled + (first - settled) * 0.37) { ta = (t - 1.0) * 1000; break; }
        (void) quietEnd;
        for (double t = 3.002; t < 5.9; t += 0.002) if (envAt (t) > quiet0 * 0.794) { tr = (t - 3.0) * 1000; break; }   // back within 2 dB
        std::printf ("   attack ~%3.0f ms, recovered in ~%4.0f ms\n", ta, tr);
    }
    std::printf ("  (columns per knob setting: change in level for inputs of -30, -20, -10, 0 dBFS)\n");

    std::printf ("\n== 5. De-esser (Lead Vocal, Clean Up full) ==\n");
    {
        Params p = flat (LeadVocal); p.cleanUp = 1;
        std::printf ("  harsh 6.8 kHz tone alone:           %+5.1f dB\n", gainAt (p, 6800, sr, -20) );
        std::printf ("  300 Hz tone alone (body):           %+5.1f dB\n", gainAt (p, 300, sr, -20));
        Run r = runSine (p, 300, -12, 0.6, sr, 6800, -30);
        std::printf ("  vowel-like (300 Hz loud + 6.8k low): 6.8k changes by %+5.1f dB\n", dB (toneAmp (r.L, r.L.size() / 2, 6800, sr)) + 30);
        r = runSine (p, 300, -30, 0.6, sr, 6800, -14);
        std::printf ("  S-like (6.8k loud + 300 Hz low):     6.8k changes by %+5.1f dB, 300 Hz by %+5.1f dB\n",
                     dB (toneAmp (r.L, r.L.size() / 2, 6800, sr)) + 14, dB (toneAmp (r.L, r.L.size() / 2, 300, sr)) + 30);
    }

    std::printf ("\n== 6. Safety limiter (1 kHz tone pushed past full scale) ==\n");
    for (double in : { -6.0, 0.0, 6.0, 12.0 })
    {
        Params p = flat (FullMix); p.limiter = true;
        const Run r = runSine (p, 1000, in, 2.0, sr);
        float pk = 0; for (float v : r.L) pk = std::max (pk, std::abs (v));
        const double h1 = toneAmp (r.L, r.L.size() / 2, 1000, sr), h3 = toneAmp (r.L, r.L.size() / 2, 3000, sr);
        const bool ok = pk <= 0.8911f; fails += ! ok;
        std::printf ("  in %+5.1f dBFS -> peak %+5.2f dBFS, steady %+5.2f dBFS, 3rd harmonic %5.1f dB %s\n", in, dB (pk), dB (h1), dB (h3 / h1), ok ? "" : "FAIL");
    }

    std::printf ("\n== 7. All knobs down: is it out of circuit? ==\n");
    {
        std::mt19937 rng (5); std::uniform_real_distribution<float> u (-0.25f, 0.25f);
        ChannelEngine e; e.prepare (sr); Params p = flat (Pastor); p.limiter = true; e.setParams (p);
        std::vector<float> in (48000), L, R; for (auto& v : in) v = u (rng);
        L = in; R = in;
        for (size_t pos = 0; pos < L.size(); pos += 256) e.process (L.data() + pos, R.data() + pos, (int) std::min<size_t> (256, L.size() - pos));
        double err = 0, ref = 0; for (size_t i = 0; i < L.size(); ++i) { err += (double) (L[i] - in[i]) * (L[i] - in[i]); ref += (double) in[i] * in[i]; }
        const double d = 10 * std::log10 (std::max (err / ref, 1e-20));
        const bool ok = d < -90; fails += ! ok;
        std::printf ("  difference between output and input: %.0f dB %s\n", d, ok ? "(bit-transparent to well below audibility)" : "FAIL");
    }

    std::printf ("\n== 8. Space: reverb level under the dry signal at knob 5, and ducking ==\n");
    for (int s = 0; s < kNumSources; ++s)
    {
        std::mt19937 rng (9); std::uniform_real_distribution<float> u (-0.2f, 0.2f);
        Params p = flat (s); p.space = 0.5f;
        ChannelEngine e; e.prepare (sr); e.setParams (p);
        const size_t n = (size_t) (4 * sr), burst = (size_t) (2 * sr); std::vector<float> L (n, 0.0f), R (n, 0.0f), dry (n, 0.0f);
        for (size_t i = 0; i < burst; ++i) { dry[i] = L[i] = u (rng); R[i] = u (rng); }
        for (size_t pos = 0; pos < n; pos += 256) e.process (L.data() + pos, R.data() + pos, (int) std::min<size_t> (256, n - pos));
        double d = 0, w = 0, tail = 0;
        for (size_t i = burst / 2; i < burst; ++i) { d += (double) dry[i] * dry[i]; w += (double) (L[i] - dry[i]) * (L[i] - dry[i]); }
        for (size_t i = burst + 2400; i < burst + 12000; ++i) tail += (double) L[i] * L[i];
        std::printf ("  %-10s %-9s %.1f s  wet %+5.1f dB under dry while playing, tail right after: %+5.1f dB\n", sourceName (s),
                     fm::modeName (sourceCfg (s).reverbMode), describe (p).revSeconds, 10 * std::log10 (w / d), 10 * std::log10 (tail / 9600 / (d / (burst / 2))));
    }

    std::printf ("\n== 9. Processing load (everything on, 21-band EQ, 48 kHz stereo) ==\n");
    {
        std::mt19937 rng (1); std::uniform_real_distribution<float> u (-0.3f, 0.3f);
        Params p; p.source = LeadVocal; p.cleanUp = p.warmth = p.clarity = p.evenOut = p.space = 1; p.eqMode = 2; for (auto& g : p.eqGain) g = 6;
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
