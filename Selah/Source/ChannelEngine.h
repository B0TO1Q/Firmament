// Selah channel engine. Portable C++17, no framework dependencies.
// Everything here is zero-latency: minimum-phase filters, no look-ahead.
#pragma once

#include "../../Source/dsp/ReverbEngine.h"
#include <atomic>

namespace sl
{

enum Source { Pastor = 0, LeadVocal, Choir, Acoustic, Electric, Bass, Keys, Drums, FullMix, kNumSources };

inline const char* sourceName (int s)
{
    static const char* n[] = { "SPEAKER", "LEAD VOCAL", "CHOIR", "ACOUSTIC", "ELECTRIC", "BASS", "KEYS", "DRUMS", "FULL MIX" };
    return n[std::clamp (s, 0, kNumSources - 1)];
}

// Every number below is a voicing decision for that kind of source.
struct SourceCfg
{
    // Clean Up: 18 dB/oct low cut, band de-esser, soft gate
    float hpMaxHz, deEssHz, deEssMaxDb, gateThrDb, gateRangeDb;   // gateThrDb <= -80 disables the gate
    // Warmth: broad low shelf, transformer-style low-end saturation, a little even-harmonic amp colour
    float warmHz, warmMaxDb, xfmr, even;
    // Clarity: proportional-Q console EQ (mud cut, presence, air)
    float mudHz, mudMaxCutDb, presHz, presMaxDb, airHz, airMaxDb;
    // Even Out: compressor voicing
    float ratio, attackMs, relFastMs, relSlowMs, kneeDb, thrAtMaxDb, scHpHz, colour2, colour3;
    bool  rmsDetect;
    float slowShare;   // how much of the gain reduction lets go slowly after sustained loudness (leveling feel)
    // Space: Firmament, with a filtered send and optional ducking under the dry signal
    int   reverbMode;
    float revSeconds, revCharacter, revPreMs, revTone, wetMax, sendHpHz, sendLpHz, duckDb;
};

inline const SourceCfg& sourceCfg (int s)
{
    static const SourceCfg c[kNumSources] = {
        // Speaker: smooth leveling, intelligibility first, a short dark room that ducks under the words
        { 180, 5800, 8, -52, 12,    200, 3.5f, 0.45f, 0.5f,   320, 4.5f, 3200, 4.5f, 10000, 2.5f,
          3.5f,  8, 120, 1000, 12, -30,   0, 0.25f, 0.0f, true, 0.60f,
          fm::Quarters, 0.55f, 0.55f, 10, 0.45f, 0.28f, 250, 7000, 6 },
        // Lead vocal: fast soft-knee compression with a touch of second harmonic, plate with pre-delay
        { 160, 6800, 8, -56, 10,    220, 3.5f, 0.50f, 0.7f,   300, 3.5f, 4200, 4.5f, 12000, 5.0f,
          4.0f,  4,  90,  600, 10, -30,   0, 0.50f, 0.0f, false, 0.35f,
          fm::Steel, 1.8f, 0.45f, 40, 0.55f, 0.50f, 300, 9000, 3 },
        // Choir: gentle, wide, long hall
        { 150, 7200, 5, -90,  0,    220, 3.0f, 0.35f, 0.4f,   350, 4.0f, 2800, 3.0f, 10000, 4.0f,
          2.5f, 20, 200, 1200, 12, -26,   0, 0.10f, 0.0f, true, 0.50f,
          fm::Sanctuary, 2.4f, 0.55f, 25, 0.60f, 0.55f, 200, 10000, 0 },
        // Acoustic guitar: tame the boom, keep the pick, airy top
        { 140, 0, 0, -90,  0,       110, 3.5f, 0.40f, 0.5f,   200, 5.0f, 5000, 3.5f, 12000, 5.0f,
          3.0f, 12, 120,  700, 10, -26,   0, 0.20f, 0.0f, false, 0.40f,
          fm::Sanctuary, 1.4f, 0.25f, 15, 0.60f, 0.42f, 250, 10000, 0 },
        // Electric guitar: amp hiss gate, no fizz above the speaker's range, plate
        { 130, 0, 0, -58, 18,       110, 3.0f, 0.55f, 0.6f,   350, 3.0f, 3000, 3.5f,  6500, 2.5f,
          3.0f, 20, 150,  600,  8, -24,   0, 0.30f, 0.0f, false, 0.40f,
          fm::Steel, 1.5f, 0.50f, 20, 0.55f, 0.40f, 300, 7000, 0 },
        // Bass: big iron on the lows, growl at 800, string definition, firm compression that ignores the sub
        {  45, 0, 0, -90,  0,        60, 4.5f, 0.90f, 0.5f,   250, 5.0f,  800, 4.0f,  2500, 3.0f,
          5.0f, 15, 120,  800,  8, -28,  40, 0.00f, 0.40f, false, 0.45f,
          fm::Quarters, 0.4f, 0.50f,  0, 0.50f, 0.10f, 300, 5000, 0 },
        // Keys: transparent, hall
        {  70, 0, 0, -90,  0,       110, 3.0f, 0.35f, 0.4f,   300, 3.0f, 2500, 3.0f, 10000, 4.0f,
          2.5f, 25, 200,  900, 12, -24,   0, 0.10f, 0.0f, true, 0.50f,
          fm::Sanctuary, 2.0f, 0.50f, 20, 0.60f, 0.45f, 200, 10000, 0 },
        // Drums: punchy 6:1 that lets the hit through, third-harmonic grit, big short room
        {  50, 0, 0, -90,  0,        60, 5.0f, 0.80f, 0.5f,   400, 6.0f, 4000, 4.0f, 10000, 4.0f,
          6.0f, 12,  70,  300,  6, -26,  80, 0.00f, 0.70f, false, 0.25f,
          fm::Quarters, 0.7f, 0.90f,  5, 0.60f, 0.30f, 150, 9000, 0 },
        // Full mix: bus glue (4:1, slow attack, auto release, low end kept out of the detector), mastering-size EQ moves
        {  40, 0, 0, -90,  0,        60, 2.0f, 0.30f, 0.3f,   250, 1.5f, 3000, 1.5f, 12000, 2.5f,
          4.0f, 30, 150, 1200,  3, -16,  90, 0.00f, 0.0f, false, 0.55f,
          fm::Sanctuary, 1.2f, 0.40f, 20, 0.60f, 0.16f, 250, 10000, 0 },
    };
    return c[std::clamp (s, 0, kNumSources - 1)];
}

struct Params
{
    int   source   = Pastor;
    float cleanUp  = 0.5f;   // all 0..1
    float warmth   = 0.3f;
    float clarity  = 0.4f;
    float evenOut  = 0.4f;
    float space    = 0.2f;
    float volumeDb = 0.0f;   // -24..+12
    // Advanced
    float reverbLength = 0.5f;   // 0.5 = the source's default; 0 = half as long, 1 = twice as long
    bool  limiter      = true;
    bool  mono         = false;  // true = fold to mono in and out (same signal on both sides)
    // Graphic EQ
    int   eqMode = 0;            // 0 = 7 bands, 1 = 14, 2 = 21
    float eqGain[21] = {};       // dB, -12..+12, for the bands of the current mode
    bool  eqPost = false;        // false = before the channel processing, true = after it
};

constexpr int kMaxEqBands = 21;
inline int eqBandCount (int mode) { return mode <= 0 ? 7 : (mode == 1 ? 14 : 21); }
inline float eqFreq (int mode, int band)
{
    static const float f7[7]   = { 63, 160, 400, 1000, 2500, 6300, 16000 };
    static const float f14[14] = { 40, 63, 100, 160, 250, 400, 630, 1000, 1600, 2500, 4000, 6300, 10000, 16000 };
    static const float f21[21] = { 20, 28, 40, 56, 80, 112, 160, 224, 315, 450, 630, 900, 1250, 1800, 2500, 3500, 5000, 7000, 10000, 14000, 20000 };
    band = std::clamp (band, 0, eqBandCount (mode) - 1);
    return mode <= 0 ? f7[band] : (mode == 1 ? f14[band] : f21[band]);
}
// Proportional Q: broad for small moves, tighter as a fader is pushed.
inline float eqQ (int mode, float gainDb)
{
    const float base = mode <= 0 ? 1.05f : (mode == 1 ? 2.15f : 2.87f);
    return base * (0.7f + 0.3f * std::min (1.0f, std::abs (gainDb) / 12.0f));
}

// What the plain-English knobs are doing, for the Advanced read-out.
struct Tech { float hpHz, deEssDb, gateThrDb, warmDb, mudDb, presDb, airDb, thrDb, ratio, revSeconds; };

inline float revSeconds (const Params& p)
{
    return sourceCfg (p.source).revSeconds * std::pow (4.0f, std::clamp (p.reverbLength, 0.0f, 1.0f) - 0.5f);
}

inline float revDecay01 (const Params& p)
{
    const auto& m = fm::modeCfg (sourceCfg (p.source).reverbMode);
    return std::clamp (std::log (revSeconds (p) / m.decayLo) / std::log (m.decayHi / m.decayLo), 0.0f, 1.0f);
}

inline Tech describe (const Params& p)
{
    const auto& c = sourceCfg (p.source);
    Tech t;
    t.hpHz      = 20.0f * std::pow (c.hpMaxHz / 20.0f, std::pow (std::clamp (p.cleanUp, 0.0f, 1.0f), 0.7f));
    t.deEssDb   = c.deEssMaxDb * p.cleanUp;
    t.gateThrDb = c.gateThrDb <= -80.0f ? -120.0f : -90.0f + (c.gateThrDb + 90.0f) * p.cleanUp;
    t.warmDb    = c.warmMaxDb * p.warmth;
    t.mudDb     = -c.mudMaxCutDb * p.clarity;
    t.presDb    = c.presMaxDb * p.clarity;
    t.airDb     = c.airMaxDb * p.clarity;
    t.thrDb     = c.thrAtMaxDb * p.evenOut;
    t.ratio     = 1.0f + (c.ratio - 1.0f) * std::min (1.0f, p.evenOut * 2.5f);
    t.revSeconds = fm::decaySeconds (c.reverbMode, revDecay01 (p));
    return t;
}

//------------------------------------------------------------------------------
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    float z1[2] = { 0, 0 }, z2[2] = { 0, 0 };

    float process (float x, int ch)
    {
        const float y = b0 * x + z1[ch];
        z1[ch] = b1 * x - a1 * y + z2[ch];
        z2[ch] = b2 * x - a2 * y;
        return y;
    }
    void reset() { z1[0] = z1[1] = z2[0] = z2[1] = 0; }
    void identity() { b0 = 1; b1 = b2 = a1 = a2 = 0; }

    void set (float nb0, float nb1, float nb2, float a0, float na1, float na2)
    {
        const float inv = 1.0f / a0;
        b0 = nb0 * inv; b1 = nb1 * inv; b2 = nb2 * inv; a1 = na1 * inv; a2 = na2 * inv;
    }
    void highpass (float hz, float q, float sr)
    {
        const float w = 2.0f * fm::kPi * std::min (hz, sr * 0.45f) / sr, cs = std::cos (w), al = std::sin (w) / (2.0f * q);
        set ((1 + cs) * 0.5f, -(1 + cs), (1 + cs) * 0.5f, 1 + al, -2 * cs, 1 - al);
    }
    void bandpass (float hz, float q, float sr)   // 0 dB at the centre
    {
        const float w = 2.0f * fm::kPi * std::min (hz, sr * 0.45f) / sr, cs = std::cos (w), al = std::sin (w) / (2.0f * q);
        set (al, 0.0f, -al, 1 + al, -2 * cs, 1 - al);
    }
    // Symmetric boost/cut bell. In the top octaves the classic bilinear bell pinches as it nears
    // Nyquist, so there we use a design matched to the analogue curve (after M. Vicanek, "Matched
    // Second Order Digital Filters"): high bands keep their true shape at 44.1 and 48 kHz.
    void peak (float hz, float q, float dB, float sr)
    {
        if (std::abs (dB) < 0.005f) { identity(); return; }
        if (hz < sr * 0.04f) { peakBilinear (hz, q, dB, sr); return; }
        if (dB < 0.0f)   // a cut is the exact mirror image of the same boost
        {
            peak (hz, q, -dB, sr);
            const float inv = 1.0f / b0, nb1 = a1 * inv, nb2 = a2 * inv, na1 = b1 * inv, na2 = b2 * inv;
            if (std::abs (na2) < 1.0f && std::abs (na1) < 1.0f + na2) { b0 = inv; b1 = nb1; b2 = nb2; a1 = na1; a2 = na2; }
            else peakBilinear (hz, q, dB, sr);
            return;
        }

        const double G  = std::pow (10.0, (double) dB / 20.0);
        const double qp = (double) q * std::sqrt (G);             // pole Q of the symmetric prototype
        const double w0 = 2.0 * 3.14159265358979323846 * (double) std::min (hz, sr * 0.47f) / (double) sr;
        const double d  = 1.0 / (2.0 * qp);
        const double e  = std::exp (-d * w0);
        const double A1c = -2.0 * e * (d <= 1.0 ? std::cos (std::sqrt (1.0 - d * d) * w0) : std::cosh (std::sqrt (d * d - 1.0) * w0));
        const double A2c = e * e;
        const double A0 = (1.0 + A1c + A2c) * (1.0 + A1c + A2c), A1 = (1.0 - A1c + A2c) * (1.0 - A1c + A2c), A2 = -4.0 * A2c;
        const double p1 = std::sin (w0 * 0.5) * std::sin (w0 * 0.5), p0 = 1.0 - p1, p2 = 4.0 * p0 * p1;
        const double R1 = (A0 * p0 + A1 * p1 + A2 * p2) * G * G;
        const double R2 = (-A0 + A1 + 4.0 * (p0 - p1) * A2) * G * G;
        const double B0 = A0;
        const double B2 = (R1 - R2 * p1 - B0) / (4.0 * p1 * p1);
        const double B1 = R2 + B0 + 4.0 * (p1 - p0) * B2;
        if (B1 < 0.0) { peakBilinear (hz, q, dB, sr); return; }
        const double W  = 0.5 * (std::sqrt (B0) + std::sqrt (B1));
        const double disc = W * W + B2;
        if (disc < 0.0) { peakBilinear (hz, q, dB, sr); return; }
        const double nb0 = 0.5 * (W + std::sqrt (disc));
        const double nb1 = 0.5 * (std::sqrt (B0) - std::sqrt (B1));
        const double nb2 = -B2 / (4.0 * nb0);
        if (! std::isfinite (nb0) || ! std::isfinite (nb2) || std::abs (nb0) < 1.0e-12) { peakBilinear (hz, q, dB, sr); return; }
        b0 = (float) nb0; b1 = (float) nb1; b2 = (float) nb2; a1 = (float) A1c; a2 = (float) A2c;
    }
    void peakBilinear (float hz, float q, float dB, float sr)
    {
        const float A = std::pow (10.0f, dB / 40.0f);
        const float w = 2.0f * fm::kPi * std::min (hz, sr * 0.45f) / sr, cs = std::cos (w), al = std::sin (w) / (2.0f * q);
        set (1 + al * A, -2 * cs, 1 - al * A, 1 + al / A, -2 * cs, 1 - al / A);
    }
    void lowShelf (float hz, float dB, float sr, float slope = 0.707f)
    {
        const float A = std::pow (10.0f, dB / 40.0f);
        const float w = 2.0f * fm::kPi * std::min (hz, sr * 0.45f) / sr, cs = std::cos (w), sn = std::sin (w);
        const float al = sn * 0.5f * std::sqrt ((A + 1.0f / A) * (1.0f / slope - 1.0f) + 2.0f), k = 2.0f * std::sqrt (A) * al;
        set (A * ((A + 1) - (A - 1) * cs + k), 2 * A * ((A - 1) - (A + 1) * cs), A * ((A + 1) - (A - 1) * cs - k),
             (A + 1) + (A - 1) * cs + k, -2 * ((A - 1) + (A + 1) * cs), (A + 1) + (A - 1) * cs - k);
    }
    void highShelf (float hz, float dB, float sr, float slope = 0.707f)
    {
        const float A = std::pow (10.0f, dB / 40.0f);
        const float w = 2.0f * fm::kPi * std::min (hz, sr * 0.45f) / sr, cs = std::cos (w), sn = std::sin (w);
        const float al = sn * 0.5f * std::sqrt ((A + 1.0f / A) * (1.0f / slope - 1.0f) + 2.0f), k = 2.0f * std::sqrt (A) * al;
        set (A * ((A + 1) + (A - 1) * cs + k), -2 * A * ((A - 1) + (A + 1) * cs), A * ((A + 1) + (A - 1) * cs - k),
             (A + 1) - (A - 1) * cs + k, 2 * ((A - 1) - (A + 1) * cs), (A + 1) - (A - 1) * cs - k);
    }
};

inline float toDb (float lin) { return 20.0f * std::log10 (std::max (lin, 1.0e-6f)); }
inline float fromDb (float dB) { return std::pow (10.0f, dB / 20.0f); }
inline float coefMs (float ms, float sr) { return 1.0f - std::exp (-1.0f / (std::max (0.01f, ms) * 0.001f * sr)); }

// Transformer-style curve: clean when small, rounds off smoothly, odd harmonics (mostly third).
inline float softCube (float x)
{
    x = std::clamp (x, -1.5f, 1.5f);
    return x - (4.0f / 27.0f) * x * x * x;
}

//------------------------------------------------------------------------------
class ChannelEngine
{
public:
    static constexpr int kChunk = 32;
    static constexpr float kCeiling = 0.891f;   // -1 dBFS

    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        reverb.prepare (sampleRate);
        for (auto* b : { &hp, &warm, &mud, &pres, &air, &essBp }) b->reset();
        for (int ch = 0; ch < 2; ++ch)
            hp1[ch] = xfLp[ch] = dc[ch] = scHp[ch] = sendHp[ch] = sendLp[ch] = 0.0f;
        essEnv = fullEnv = 0; essRed = 0;
        gateEnv = 0; gateGain = 1; gateOpen = true; gateHold = 0;
        ms = 0; grFast = grSlow = 0;
        duckEnv = 0; duckGain = 1;
        limEnvFast = limEnvSlow = 0;
        spaceTail = 0;
        first = true;
        eqLastMode = -1;
        lastSource = -1;
        meterOut = 0.0f; meterGr = 0.0f; meterLimit = 0.0f;
    }

    void setParams (const Params& np) { p = np; }

    // Safe to read from the UI thread.
    float outputPeak() const { return meterOut.load (std::memory_order_relaxed); }      // linear
    float gainReductionDb() const { return meterGr.load (std::memory_order_relaxed); }  // positive dB
    float limiting() const { return meterLimit.load (std::memory_order_relaxed); }      // 0..1

    void process (float* L, float* R, int n)
    {
        for (int pos = 0; pos < n; pos += kChunk)
            processChunk (L + pos, R + pos, std::min (kChunk, n - pos));
    }

private:
    void processChunk (float* L, float* R, int n)
    {
        const auto& c = sourceCfg (p.source);
        const float tgt[6] = { std::clamp (p.cleanUp, 0.0f, 1.0f), std::clamp (p.warmth, 0.0f, 1.0f),
                               std::clamp (p.clarity, 0.0f, 1.0f), std::clamp (p.evenOut, 0.0f, 1.0f),
                               std::clamp (p.space, 0.0f, 1.0f),   std::clamp (p.volumeDb, -24.0f, 12.0f) };
        if (first) { for (int i = 0; i < 6; ++i) sm[i] = tgt[i]; first = false; }
        const float k = 1.0f - std::exp (-(float) n / (0.03f * sr));   // 30 ms knob smoothing
        for (int i = 0; i < 6; ++i) sm[i] += k * (tgt[i] - sm[i]);

        if (p.source != lastSource)     // new source: start its detectors clean
        {
            essBp.reset(); essEnv = fullEnv = 0; essRed = 0;
            lastSource = p.source;
        }

        if (p.mono)
            for (int i = 0; i < n; ++i) { const float m = 0.5f * (L[i] + R[i]); L[i] = m; R[i] = m; }

        // ---- graphic EQ set-up ------------------------------------------------
        const int eqMode = std::clamp (p.eqMode, 0, 2);
        const int nb = eqBandCount (eqMode);
        if (eqMode != eqLastMode)
        {
            for (int b = 0; b < kMaxEqBands; ++b) { eq[b].reset(); eqSm[b] = b < nb ? std::clamp (p.eqGain[b], -12.0f, 12.0f) : 0.0f; eqCoef[b] = 999.0f; }
            eqLastMode = eqMode;
        }
        for (int b = 0; b < nb; ++b)
        {
            eqSm[b] += k * (std::clamp (p.eqGain[b], -12.0f, 12.0f) - eqSm[b]);
            if (std::abs (eqSm[b] - eqCoef[b]) > 0.01f)
            {
                eq[b].peak (eqFreq (eqMode, b), eqQ (eqMode, eqSm[b]), eqSm[b], sr);
                eqCoef[b] = eqSm[b];
            }
        }
        const auto applyEq = [this, nb, L, R, n]
        {
            for (int b = 0; b < nb; ++b)
            {
                if (std::abs (eqSm[b]) < 0.02f) continue;
                for (int i = 0; i < n; ++i) { L[i] = eq[b].process (L[i], 0); R[i] = eq[b].process (R[i], 1); }
            }
        };
        if (! p.eqPost) applyEq();

        Params q = p;
        q.cleanUp = sm[0]; q.warmth = sm[1]; q.clarity = sm[2]; q.evenOut = sm[3]; q.space = sm[4];
        const Tech t = describe (q);

        // ---- coefficients for this chunk ----------------------------------------
        const bool hpOn = sm[0] > 0.003f;                       // knob at zero = truly out of circuit
        hp.highpass (t.hpHz, 1.0f, sr);                         // + one pole below = 18 dB/oct Butterworth
        const float hp1C = fm::coefFor (t.hpHz, sr);
        warm.lowShelf (c.warmHz, t.warmDb, sr, 0.5f);           // broad, gentle shelf
        mud.peak (c.mudHz, 0.9f * (1.0f + std::abs (t.mudDb) / 12.0f), t.mudDb, sr);
        pres.peak (c.presHz, 0.6f * (1.0f + std::abs (t.presDb) / 12.0f), t.presDb, sr);
        if (c.airHz < 5000.0f) air.peak (c.airHz, 0.8f, t.airDb, sr);
        else                   air.highShelf (c.airHz, t.airDb, sr, 0.6f);

        const bool  essOn  = c.deEssMaxDb > 0.0f;
        const float essMax = t.deEssDb;
        if (essOn) essBp.bandpass (c.deEssHz, 1.4f, sr);
        const float envAtk = coefMs (0.3f, sr), envRel = coefMs (25.0f, sr);
        const float essAtk = coefMs (0.5f, sr), essRel = coefMs (50.0f, sr);

        const bool  gateOn    = c.gateThrDb > -80.0f && sm[0] > 0.01f;
        const float gateThr   = fromDb (t.gateThrDb);
        const float gateFloor = fromDb (-c.gateRangeDb * sm[0]);
        const float gAtk = coefMs (1.0f, sr), gRel = coefMs (150.0f, sr);
        const int   holdSamples = (int) (0.08f * sr);

        const float xfLpC   = fm::coefFor (220.0f, sr);
        const float xfDrive = 0.7f * (1.0f + 3.0f * c.xfmr * sm[1]);
        const float xfInv   = 1.0f / xfDrive;
        const float xfMix   = std::min (1.0f, sm[1] * 5.0f);
        const float evenAmt = 0.07f * c.even * sm[1];

        const float cAtk   = coefMs (c.attackMs, sr);
        const float cRelF  = coefMs (c.relFastMs, sr);
        const float cAtkS  = coefMs (c.attackMs * 8.0f + 40.0f, sr);
        const float cRelS  = coefMs (c.relSlowMs, sr);
        const float msC    = coefMs (8.0f, sr);
        const float scC    = c.scHpHz > 0.0f ? fm::coefFor (c.scHpHz, sr) : 0.0f;
        const float slope  = 1.0f - 1.0f / t.ratio;
        const float knee   = c.kneeDb;
        const bool  compOn = t.thrDb < -0.1f;
        // make-up: give back most of what a normal-level signal (-14 dBFS) loses, so the knob evens
        // things out instead of just turning them up
        float nomGr = 0.0f;
        {
            const float over = -14.0f - t.thrDb;
            if (over > knee * 0.5f) nomGr = over * slope;
            else if (over > -knee * 0.5f) { const float u = over + knee * 0.5f; nomGr = slope * u * u / (2.0f * knee); }
        }
        const float makeup = fromDb (0.75f * nomGr);
        const float dcC    = fm::coefFor (5.0f, sr);
        const bool  dcOn   = (xfMix > 0.0f && c.even > 0.0f) || (compOn && c.colour2 > 0.0f);

        float grMax = 0.0f;

        for (int i = 0; i < n; ++i)
        {
            float x[2] = { L[i], R[i] };

            // ---- Clean Up: 18 dB/oct low cut ---------------------------------
            if (hpOn)
                for (int ch = 0; ch < 2; ++ch)
                {
                    hp1[ch] += hp1C * (x[ch] - hp1[ch]);
                    x[ch] = hp.process (x[ch] - hp1[ch], ch);
                }

            // ---- Clean Up: soft gate with hold, so it never chatters ----------
            const float a = std::max (std::abs (x[0]), std::abs (x[1]));
            if (gateOn)
            {
                gateEnv += (a > gateEnv ? envAtk : envRel) * (a - gateEnv);
                if (gateEnv >= gateThr) { gateOpen = true; gateHold = holdSamples; }
                else if (gateEnv < gateThr * 0.6f) { if (gateHold > 0) --gateHold; else gateOpen = false; }
                const float want = gateOpen ? 1.0f : gateFloor;
                gateGain += (want > gateGain ? gAtk : gRel) * (want - gateGain);
                x[0] *= gateGain; x[1] *= gateGain;
            }
            else { gateGain += gAtk * (1.0f - gateGain); gateOpen = true; }

            // ---- Clean Up: de-esser. Only the harsh band is turned down, only while it sticks out
            if (essOn)
            {
                const float bp0 = essBp.process (x[0], 0), bp1 = essBp.process (x[1], 1);
                const float bpA = std::max (std::abs (bp0), std::abs (bp1));
                essEnv  += (bpA > essEnv ? envAtk : envRel) * (bpA - essEnv);
                fullEnv += (a > fullEnv ? envAtk : envRel) * (a - fullEnv);
                float want = 0.0f;
                if (fullEnv > 0.002f && essMax > 0.05f)
                    want = std::clamp (toDb (essEnv / fullEnv) + 12.0f, 0.0f, essMax);   // level-independent
                essRed += (want > essRed ? essAtk : essRel) * (want - essRed);
                if (essRed > 0.01f)
                {
                    const float kk = 1.0f - fromDb (-essRed);
                    x[0] -= kk * bp0; x[1] -= kk * bp1;
                }
            }

            // ---- Warmth -------------------------------------------------------
            for (int ch = 0; ch < 2; ++ch)
            {
                float y = warm.process (x[ch], ch);
                xfLp[ch] += xfLpC * (y - xfLp[ch]);
                if (xfMix > 0.0f)
                {
                    // the iron saturates on low frequencies first: third harmonic on the bottom end only
                    const float lo = xfLp[ch];
                    y += xfMix * (softCube (lo * xfDrive) * xfInv - lo);
                    // class-A amp stage: a little second harmonic across the band
                    y += evenAmt * y * y;
                }
                x[ch] = y;
            }

            // ---- Clarity ------------------------------------------------------
            for (int ch = 0; ch < 2; ++ch)
                x[ch] = air.process (pres.process (mud.process (x[ch], ch), ch), ch);

            // ---- Even Out -----------------------------------------------------
            if (compOn)
            {
                float s0 = x[0], s1 = x[1];
                if (scC > 0.0f)   // keep the low end from pumping the whole signal
                {
                    scHp[0] += scC * (s0 - scHp[0]); s0 -= scHp[0];
                    scHp[1] += scC * (s1 - scHp[1]); s1 -= scHp[1];
                }
                float lvl = std::max (std::abs (s0), std::abs (s1));
                if (c.rmsDetect) { ms += msC * (lvl * lvl - ms); lvl = std::sqrt (ms) * 1.414f; }
                const float over = toDb (lvl) - t.thrDb;
                float gr = 0.0f;
                if (over > knee * 0.5f) gr = over * slope;
                else if (over > -knee * 0.5f) { const float u = over + knee * 0.5f; gr = slope * u * u / (2.0f * knee); }

                // two release times: quick recovery after a transient, slow after sustained loudness
                grFast += (gr > grFast ? cAtk : cRelF) * (gr - grFast);
                grSlow += (gr > grSlow ? cAtkS : cRelS) * (gr - grSlow);
                const float red = std::max (grFast, grSlow * c.slowShare);
                const float g = fromDb (-red) * makeup;
                const float work = std::min (1.0f, red / 10.0f);       // more colour as it works harder
                const float c2 = 0.10f * c.colour2 * work, c3 = 0.35f * c.colour3 * work;
                for (int ch = 0; ch < 2; ++ch)
                {
                    float y = x[ch] * g;
                    if (c2 > 0.0f) y += c2 * y * y;
                    if (c3 > 0.0f) { const float yc = std::clamp (y, -1.2f, 1.2f); y -= c3 * yc * yc * yc; }
                    x[ch] = y;
                }
                grMax = std::max (grMax, red);
            }
            else { grFast *= 1.0f - cRelF; grSlow *= 1.0f - cRelS; }

            // even harmonics leave a DC offset; take it out
            for (int ch = 0; ch < 2; ++ch) { dc[ch] += dcC * (x[ch] - dc[ch]); if (dcOn) x[ch] -= dc[ch]; }

            L[i] = x[0]; R[i] = x[1];
        }

        // ---- Space: filtered send into Firmament, ducked under the dry signal ----
        const bool spaceOn = sm[4] > 0.002f || spaceTail > 0;
        if (spaceOn)
        {
            float wL[kChunk], wR[kChunk];
            const float hpC = fm::coefFor (c.sendHpHz, sr), lpC = fm::coefFor (c.sendLpHz, sr);
            float dryPeak = 0.0f;
            for (int i = 0; i < n; ++i)
            {
                dryPeak = std::max (dryPeak, std::max (std::abs (L[i]), std::abs (R[i])));
                sendHp[0] += hpC * (L[i] - sendHp[0]); sendHp[1] += hpC * (R[i] - sendHp[1]);
                sendLp[0] += lpC * ((L[i] - sendHp[0]) - sendLp[0]); sendLp[1] += lpC * ((R[i] - sendHp[1]) - sendLp[1]);
                wL[i] = sendLp[0]; wR[i] = sendLp[1];
            }
            fm::Params rp;
            rp.mode = c.reverbMode; rp.decay = revDecay01 (p); rp.character = c.revCharacter;
            rp.preDelayMs = c.revPreMs; rp.tone = c.revTone; rp.mod = 0.25f; rp.mix = 1.0f;
            reverb.setParams (rp);
            reverb.process (wL, wR, n);

            // duck: while the dry signal is present the reverb sits back, then blooms in the gaps
            const float dAtk = 1.0f - std::exp (-(float) n / (0.010f * sr)), dRel = 1.0f - std::exp (-(float) n / (0.30f * sr));
            duckEnv += (dryPeak > duckEnv ? dAtk : dRel) * (dryPeak - duckEnv);
            const float duckTarget = fromDb (-c.duckDb * std::min (1.0f, duckEnv / 0.12f));
            const float wetAmt = sm[4] * c.wetMax * 1.6f;
            const float gStep = (duckTarget - duckGain) / (float) n;
            float wetPeak = 0.0f;
            for (int i = 0; i < n; ++i)
            {
                duckGain += gStep;
                L[i] += wL[i] * wetAmt * duckGain; R[i] += wR[i] * wetAmt * duckGain;
                wetPeak = std::max (wetPeak, std::abs (wL[i]) + std::abs (wR[i]));
            }
            // keep the reverb running until its tail has died away, then let it rest
            if (sm[4] > 0.002f) spaceTail = (int) (sr * 2.0f);
            else if (wetPeak > 1.0e-5f) spaceTail = std::max (spaceTail, n + 1);
            spaceTail = std::max (0, spaceTail - n);
        }

        if (p.eqPost) applyEq();

        if (p.mono)   // the reverb is stereo, so fold again after it
            for (int i = 0; i < n; ++i) { const float m = 0.5f * (L[i] + R[i]); L[i] = m; R[i] = m; }

        // ---- Volume + safety limiter ---------------------------------------------
        // Stage 1 rides the level down smoothly (fast for peaks, slow for sustained loudness).
        // Stage 2 is a soft clipper that rounds off whatever is faster than stage 1 can catch.
        // Together they hold the ceiling with no look-ahead, so no delay.
        const float vol = fromDb (sm[5]);
        const float lThr = 0.708f;                       // -3 dBFS: where limiting starts
        const float lAtk = coefMs (0.4f, sr), lRelF = coefMs (60.0f, sr);
        const float lAtkS = coefMs (30.0f, sr), lRelS = coefMs (700.0f, sr);
        const float kneeStart = 0.70f, span = kCeiling - kneeStart;
        float peakOut = 0.0f, limMax = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            float l = L[i] * vol, r = R[i] * vol;
            if (p.limiter)
            {
                const float pk = std::max (std::abs (l), std::abs (r));
                const float over = pk > lThr ? pk / lThr - 1.0f : 0.0f;          // how far past the threshold
                limEnvFast += (over > limEnvFast ? lAtk : lRelF) * (over - limEnvFast);
                limEnvSlow += (over > limEnvSlow ? lAtkS : lRelS) * (over - limEnvSlow);
                const float g = 1.0f / (1.0f + std::max (limEnvFast, limEnvSlow));
                l *= g; r *= g;
                const float al = std::abs (l), ar = std::abs (r);
                if (al > kneeStart) l = std::copysign (kneeStart + span * std::tanh ((al - kneeStart) / span), l);
                if (ar > kneeStart) r = std::copysign (kneeStart + span * std::tanh ((ar - kneeStart) / span), r);
                limMax = std::max (limMax, 1.0f - g);
            }
            L[i] = l; R[i] = r;
            peakOut = std::max (peakOut, std::max (std::abs (l), std::abs (r)));
        }

        // meters: fast attack, slow fall
        const float fall = std::exp (-(float) n / (0.25f * sr));
        meterOut.store   (std::max (peakOut, meterOut.load (std::memory_order_relaxed) * fall), std::memory_order_relaxed);
        meterGr.store    (std::max (grMax, meterGr.load (std::memory_order_relaxed) * fall), std::memory_order_relaxed);
        meterLimit.store (std::max (std::min (1.0f, limMax * 8.0f), meterLimit.load (std::memory_order_relaxed) * fall), std::memory_order_relaxed);
    }

    Params p;
    float sr = 48000.0f;
    bool first = true;
    int lastSource = -1;
    float sm[6] = { 0, 0, 0, 0, 0, 0 };

    Biquad hp, warm, mud, pres, air, essBp;
    Biquad eq[kMaxEqBands];
    float eqSm[kMaxEqBands] = {}, eqCoef[kMaxEqBands] = {};
    int eqLastMode = -1;

    float hp1[2] = {}, xfLp[2] = {}, dc[2] = {}, scHp[2] = {}, sendHp[2] = {}, sendLp[2] = {};
    float essEnv = 0, fullEnv = 0, essRed = 0;
    float gateEnv = 0, gateGain = 1; bool gateOpen = true; int gateHold = 0;
    float ms = 0, grFast = 0, grSlow = 0;
    float duckEnv = 0, duckGain = 1;
    float limEnvFast = 0, limEnvSlow = 0;
    fm::ReverbEngine reverb;
    int spaceTail = 0;

    std::atomic<float> meterOut { 0.0f }, meterGr { 0.0f }, meterLimit { 0.0f };
};

} // namespace sl
