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
    static const char* n[] = { "PASTOR", "LEAD VOCAL", "CHOIR", "ACOUSTIC", "ELECTRIC", "BASS", "KEYS", "DRUMS", "FULL MIX" };
    return n[std::clamp (s, 0, kNumSources - 1)];
}

struct SourceCfg
{
    // Clean Up
    float hpMaxHz, deEssMaxDb, gateThrDb;          // gateThrDb <= -80 disables the gate
    // Warmth (transformer-console style low end + saturation)
    float warmHz, warmMaxDb, driveMax;
    // Clarity (console EQ style)
    float mudHz, mudMaxCutDb, presHz, presMaxDb, airHz, airMaxDb;
    // Even Out
    float ratio, attackMs, releaseMs, thrAtMaxDb, colour;
    // Space (Firmament)
    int reverbMode; float revDecay, revCharacter, revPreMs, revTone, wetMax;
};

inline const SourceCfg& sourceCfg (int s)
{
    static const SourceCfg c[kNumSources] = {
        /* Pastor    */ { 140, 8, -50,   160, 4.0f, 0.6f,   350, 5, 3500, 5.0f, 10000, 3,   4.0f,  5, 120, -28, 0.3f,  fm::Quarters,  0.45f, 0.6f, 10, 0.5f, 0.30f },
        /* LeadVocal */ { 120, 8, -55,   200, 4.0f, 0.8f,   300, 4, 4000, 5.0f, 12000, 5,   4.0f,  3, 100, -28, 0.6f,  fm::Steel,     0.45f, 0.5f, 30, 0.6f, 0.50f },
        /* Choir     */ { 110, 5, -90,   220, 3.0f, 0.5f,   400, 4, 3000, 4.0f, 11000, 4,   3.0f, 15, 200, -24, 0.2f,  fm::Sanctuary, 0.45f, 0.5f, 20, 0.6f, 0.55f },
        /* Acoustic  */ { 100, 0, -90,   120, 4.0f, 0.5f,   250, 5, 5000, 4.0f, 12000, 5,   3.0f, 15, 150, -24, 0.2f,  fm::Sanctuary, 0.30f, 0.2f, 15, 0.6f, 0.45f },
        /* Electric  */ {  90, 0, -55,   150, 3.0f, 0.7f,   400, 4, 2800, 4.0f,  8000, 3,   3.0f, 15, 150, -22, 0.3f,  fm::Steel,     0.35f, 0.5f, 15, 0.6f, 0.40f },
        /* Bass      */ {  35, 0, -90,    80, 5.0f, 0.9f,   250, 5,  900, 4.0f,  3500, 3,   5.0f, 20, 150, -26, 0.5f,  fm::Quarters,  0.20f, 0.5f,  0, 0.5f, 0.12f },
        /* Keys      */ {  50, 0, -90,   100, 3.0f, 0.5f,   300, 3, 3000, 3.0f, 10000, 4,   2.5f, 20, 200, -22, 0.1f,  fm::Sanctuary, 0.40f, 0.5f, 20, 0.6f, 0.45f },
        /* Drums     */ {  40, 0, -90,    80, 5.0f, 1.0f,   400, 6, 4500, 5.0f, 10000, 4,   6.0f, 10,  80, -26, 0.8f,  fm::Quarters,  0.50f, 0.9f,  5, 0.6f, 0.30f },
        /* FullMix   */ {  35, 0, -90,    90, 2.5f, 0.4f,   300, 2, 3500, 2.0f, 12000, 3,   3.0f, 30, 250, -18, 0.0f,  fm::Sanctuary, 0.30f, 0.4f, 20, 0.6f, 0.18f },
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
    float reverbLength = 0.5f;   // 0.5 = the source's default
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
inline float eqQ (int mode) { return mode <= 0 ? 1.05f : (mode == 1 ? 2.15f : 2.87f); }

// What the plain-English knobs are doing, for the Advanced read-out.
struct Tech { float hpHz, deEssDb, gateThrDb, warmDb, mudDb, presDb, airDb, thrDb, ratio, revSeconds; };

inline float revDecay01 (const Params& p)
{
    return std::clamp (sourceCfg (p.source).revDecay + (p.reverbLength - 0.5f) * 0.6f, 0.0f, 1.0f);
}

inline Tech describe (const Params& p)
{
    const auto& c = sourceCfg (p.source);
    Tech t;
    t.hpHz      = 20.0f * std::pow (c.hpMaxHz / 20.0f, p.cleanUp);
    t.deEssDb   = c.deEssMaxDb * p.cleanUp;
    t.gateThrDb = c.gateThrDb <= -80.0f ? -120.0f : -90.0f + (c.gateThrDb + 90.0f) * p.cleanUp;
    t.warmDb    = c.warmMaxDb * p.warmth;
    t.mudDb     = -c.mudMaxCutDb * p.clarity;
    t.presDb    = c.presMaxDb * p.clarity;
    t.airDb     = c.airMaxDb * p.clarity;
    t.thrDb     = c.thrAtMaxDb * p.evenOut;
    t.ratio     = 1.0f + (c.ratio - 1.0f) * std::min (1.0f, p.evenOut * 2.0f);
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
    void peak (float hz, float q, float dB, float sr)
    {
        const float A = std::pow (10.0f, dB / 40.0f);
        const float w = 2.0f * fm::kPi * std::min (hz, sr * 0.45f) / sr, cs = std::cos (w), al = std::sin (w) / (2.0f * q);
        set (1 + al * A, -2 * cs, 1 - al * A, 1 + al / A, -2 * cs, 1 - al / A);
    }
    void lowShelf (float hz, float dB, float sr)
    {
        const float A = std::pow (10.0f, dB / 40.0f);
        const float w = 2.0f * fm::kPi * std::min (hz, sr * 0.45f) / sr, cs = std::cos (w), sn = std::sin (w);
        const float al = sn * 0.5f * std::sqrt (2.0f), k = 2.0f * std::sqrt (A) * al;
        set (A * ((A + 1) - (A - 1) * cs + k), 2 * A * ((A - 1) - (A + 1) * cs), A * ((A + 1) - (A - 1) * cs - k),
             (A + 1) + (A - 1) * cs + k, -2 * ((A - 1) + (A + 1) * cs), (A + 1) + (A - 1) * cs - k);
    }
    void highShelf (float hz, float dB, float sr)
    {
        const float A = std::pow (10.0f, dB / 40.0f);
        const float w = 2.0f * fm::kPi * std::min (hz, sr * 0.45f) / sr, cs = std::cos (w), sn = std::sin (w);
        const float al = sn * 0.5f * std::sqrt (2.0f), k = 2.0f * std::sqrt (A) * al;
        set (A * ((A + 1) + (A - 1) * cs + k), -2 * A * ((A - 1) + (A + 1) * cs), A * ((A + 1) + (A - 1) * cs - k),
             (A + 1) - (A - 1) * cs + k, 2 * ((A - 1) - (A + 1) * cs), (A + 1) - (A - 1) * cs - k);
    }
};

inline float toDb (float lin) { return 20.0f * std::log10 (std::max (lin, 1.0e-6f)); }
inline float fromDb (float dB) { return std::pow (10.0f, dB / 20.0f); }

//------------------------------------------------------------------------------
class ChannelEngine
{
public:
    static constexpr int kChunk = 32;

    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        reverb.prepare (sampleRate);
        for (auto* b : { &hp, &warm, &mud, &pres, &air }) b->reset();
        essLp[0] = essLp[1] = 0; essEnv = fullEnv = 0; essGain = 1;
        gateEnv = 0; gateGain = 1;
        compGr = 0; limGain = 1;
        first = true;
        eqLastMode = -1;
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
                eq[b].peak (eqFreq (eqMode, b), eqQ (eqMode), eqSm[b], sr);
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
        if (p.mono)
            for (int i = 0; i < n; ++i) { const float m = 0.5f * (L[i] + R[i]); L[i] = m; R[i] = m; }

        if (! p.eqPost) applyEq();

        Params q = p;
        q.cleanUp = sm[0]; q.warmth = sm[1]; q.clarity = sm[2]; q.evenOut = sm[3]; q.space = sm[4];
        const Tech t = describe (q);

        hp.highpass (t.hpHz, 0.707f, sr);
        warm.lowShelf (c.warmHz, t.warmDb, sr);
        mud.peak (c.mudHz, 1.0f, t.mudDb, sr);
        pres.peak (c.presHz, 0.8f, t.presDb, sr);
        air.highShelf (c.airHz, t.airDb, sr);

        const float essC   = fm::coefFor (5500.0f, sr);
        const float envAtk = fm::coefFor (400.0f, sr), envRel = fm::coefFor (12.0f, sr);
        const float gAtk   = fm::coefFor (300.0f, sr), gRel = fm::coefFor (4.0f, sr);
        const float gateThr = fromDb (t.gateThrDb);
        const float gateFloor = fromDb (-18.0f * sm[0]);
        const bool  gateOn = c.gateThrDb > -80.0f && sm[0] > 0.01f;
        const float essMax = t.deEssDb;

        const float drive = 1.0f + 2.5f * c.driveMax * sm[1];
        const float satNorm = 1.0f / std::pow (drive, 0.8f);
        const float bias = 0.08f;
        const float tanhBias = std::tanh (bias);
        const float satMix = sm[1];

        const float cAtk = fm::coefFor (1000.0f / (2.0f * fm::kPi * c.attackMs), sr);
        const float cRel = fm::coefFor (1000.0f / (2.0f * fm::kPi * c.releaseMs), sr);
        const float slope = 1.0f - 1.0f / t.ratio;
        const float knee = 6.0f;
        const float makeup = fromDb (-t.thrDb * slope * 0.45f);
        const float colour = c.colour;

        const float vol = fromDb (sm[5]);
        const float ceiling = 0.891f;     // -1 dBFS
        const float lRel = fm::coefFor (2.5f, sr);

        // Space: run Firmament fully wet on a copy, add it back by the knob.
        float wL[kChunk], wR[kChunk];
        const float wetAmt = sm[4] * c.wetMax * 1.6f;
        const bool spaceOn = sm[4] > 0.002f || spaceTail > 0;

        float peakOut = 0.0f, grMax = 0.0f, limMax = 0.0f;

        for (int i = 0; i < n; ++i)
        {
            float x[2] = { L[i], R[i] };

            // ---- Clean Up: low cut -------------------------------------------
            x[0] = hp.process (x[0], 0); x[1] = hp.process (x[1], 1);

            // ---- Clean Up: gentle gate (downward expander) --------------------
            const float a = std::max (std::abs (x[0]), std::abs (x[1]));
            if (gateOn)
            {
                gateEnv += (a > gateEnv ? envAtk : envRel) * (a - gateEnv);
                const float want = gateEnv >= gateThr ? 1.0f : std::max (gateFloor, gateEnv / gateThr);
                gateGain += (want > gateGain ? gAtk : gRel) * (want - gateGain);
                x[0] *= gateGain; x[1] *= gateGain;
            }
            else gateGain += gAtk * (1.0f - gateGain);

            // ---- Clean Up: de-esser (split band, top is ducked on harsh S) ----
            if (essMax > 0.05f)
            {
                essLp[0] += essC * (x[0] - essLp[0]); essLp[1] += essC * (x[1] - essLp[1]);
                const float hi0 = x[0] - essLp[0], hi1 = x[1] - essLp[1];
                const float hiA = std::max (std::abs (hi0), std::abs (hi1));
                essEnv  += (hiA > essEnv ? envAtk : envRel) * (hiA - essEnv);
                fullEnv += (a > fullEnv ? envAtk : envRel) * (a - fullEnv);
                float red = 0.0f;
                if (fullEnv > 0.003f)
                    red = std::clamp (toDb (essEnv / fullEnv) + 8.0f, 0.0f, essMax);
                const float want = fromDb (-red);
                essGain += (want < essGain ? gAtk : envRel) * (want - essGain);
                x[0] = essLp[0] + hi0 * essGain; x[1] = essLp[1] + hi1 * essGain;
            }

            // ---- Warmth: low shelf + transformer-style saturation -------------
            for (int ch = 0; ch < 2; ++ch)
            {
                float y = warm.process (x[ch], ch);
                const float s = (std::tanh (drive * y + bias) - tanhBias) * satNorm;
                x[ch] = y + satMix * (s - y);
            }

            // ---- Clarity: mud cut, presence, air -------------------------------
            for (int ch = 0; ch < 2; ++ch)
                x[ch] = air.process (pres.process (mud.process (x[ch], ch), ch), ch);

            // ---- Even Out: stereo-linked compressor ----------------------------
            {
                const float lvl = toDb (std::max (std::abs (x[0]), std::abs (x[1])));
                const float over = lvl - t.thrDb;
                float gr = 0.0f;
                if (t.thrDb < -0.1f)
                {
                    if (over > knee * 0.5f) gr = over * slope;
                    else if (over > -knee * 0.5f) { const float u = over + knee * 0.5f; gr = slope * u * u / (2.0f * knee); }
                }
                compGr += (gr > compGr ? cAtk : cRel) * (gr - compGr);
                const float g = fromDb (-compGr) * makeup;
                const float col = colour * std::min (1.0f, compGr / 12.0f);   // more grit as it works harder
                for (int ch = 0; ch < 2; ++ch)
                {
                    float y = x[ch] * g;
                    if (col > 0.001f) y += col * (std::tanh (1.5f * y) / 1.5f - y);
                    x[ch] = y;
                }
                grMax = std::max (grMax, compGr);
            }

            wL[i] = x[0]; wR[i] = x[1];
            L[i] = x[0]; R[i] = x[1];
        }

        if (spaceOn)
        {
            fm::Params rp;
            rp.mode = c.reverbMode; rp.decay = revDecay01 (p); rp.character = c.revCharacter;
            rp.preDelayMs = c.revPreMs; rp.tone = c.revTone; rp.mod = 0.25f; rp.mix = 1.0f;
            reverb.setParams (rp);
            reverb.process (wL, wR, n);
            float wetPeak = 0.0f;
            for (int i = 0; i < n; ++i)
            {
                L[i] += wL[i] * wetAmt; R[i] += wR[i] * wetAmt;
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

        // ---- Volume + safety limiter (instant attack, no look-ahead) ----------
        for (int i = 0; i < n; ++i)
        {
            float l = L[i] * vol, r = R[i] * vol;
            if (p.limiter)
            {
                const float pk = std::max (std::abs (l), std::abs (r));
                limGain += lRel * (1.0f - limGain);
                if (pk * limGain > ceiling) limGain = ceiling / pk;
                l *= limGain; r *= limGain;
                limMax = std::max (limMax, 1.0f - limGain);
            }
            else limGain = 1.0f;
            L[i] = l; R[i] = r;
            peakOut = std::max (peakOut, std::max (std::abs (l), std::abs (r)));
        }

        // meters: fast attack, slow fall
        const float fall = std::exp (-(float) n / (0.25f * sr));
        meterOut.store   (std::max (peakOut, meterOut.load (std::memory_order_relaxed) * fall), std::memory_order_relaxed);
        meterGr.store    (std::max (grMax, meterGr.load (std::memory_order_relaxed) * fall), std::memory_order_relaxed);
        meterLimit.store (std::max (std::min (1.0f, limMax * 6.0f), meterLimit.load (std::memory_order_relaxed) * fall), std::memory_order_relaxed);
    }

    Params p;
    float sr = 48000.0f;
    bool first = true;
    float sm[6] = { 0, 0, 0, 0, 0, 0 };

    Biquad hp, warm, mud, pres, air;
    Biquad eq[kMaxEqBands];
    float eqSm[kMaxEqBands] = {}, eqCoef[kMaxEqBands] = {};
    int eqLastMode = -1;
    float essLp[2] = { 0, 0 }, essEnv = 0, fullEnv = 0, essGain = 1;
    float gateEnv = 0, gateGain = 1;
    float compGr = 0, limGain = 1;
    fm::ReverbEngine reverb;
    int spaceTail = 0;

    std::atomic<float> meterOut { 0.0f }, meterGr { 0.0f }, meterLimit { 0.0f };
};

} // namespace sl
