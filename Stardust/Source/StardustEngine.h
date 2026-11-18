// STARDUST engine. Portable C++17, no framework dependencies.
// Seven effects in series, all zero-latency:
//   DRIVE -> GLOW -> ORCHESTRA -> STUTTER -> SHIMMER + NEBULA (one reverb) -> PUMP -> FILTER
#pragma once

#include "../../Source/dsp/ReverbEngine.h"
#include <atomic>

namespace sd
{

constexpr float kPi = fm::kPi;

enum StutterRate { Quarter = 0, Eighth, Sixteenth, ThirtySecond, kNumRates };
enum ShimmerDir  { Up = 0, Down, Both };

inline float rateBeats (int r)
{
    static const float b[kNumRates] = { 1.0f, 0.5f, 0.25f, 0.125f };
    return b[std::clamp (r, 0, kNumRates - 1)];
}

struct Params
{
    float drive = 0, glow = 0, orchestra = 0, stutter = 0, shimmer = 0, nebula = 0, pump = 0;   // 0..1
    int   stutterRate = Sixteenth;
    int   shimmerDir  = Up;
    bool  freeze      = false;
    float filter   = 0.0f;   // -1 (low-pass) .. 0 (off) .. +1 (high-pass)
    float mix      = 1.0f;   // 0 dry .. 1 all STARDUST
    float outputDb = 0.0f;   // -24..+12
};

// Host transport for one block.
struct Transport
{
    double bpm = 120.0;
    double ppq = 0.0;        // position in quarter notes at the first sample of the block
    bool   playing = false;
};

//------------------------------------------------------------------------------
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1[2] = {}, z2[2] = {};
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
    void highShelf (float hz, float dB, float sr)
    {
        const float A = std::pow (10.0f, dB / 40.0f);
        const float w = 2.0f * kPi * std::min (hz, sr * 0.45f) / sr, cs = std::cos (w), sn = std::sin (w);
        const float al = sn * 0.5f * std::sqrt (2.0f), k = 2.0f * std::sqrt (A) * al;
        set (A * ((A + 1) + (A - 1) * cs + k), -2 * A * ((A - 1) + (A + 1) * cs), A * ((A + 1) + (A - 1) * cs - k),
             (A + 1) - (A - 1) * cs + k, 2 * ((A - 1) - (A + 1) * cs), (A + 1) - (A - 1) * cs - k);
    }
};

// Topology-preserving state-variable filter: stays stable while the cutoff sweeps fast.
struct Svf
{
    float ic1[2] = {}, ic2[2] = {};
    float g = 0.1f, k = 1.0f, a1 = 0, a2 = 0, a3 = 0;
    void set (float hz, float q, float sr)
    {
        g = std::tan (kPi * std::min (hz, sr * 0.45f) / sr);
        k = 1.0f / q;
        a1 = 1.0f / (1.0f + g * (g + k)); a2 = g * a1; a3 = g * a2;
    }
    void process (float x, int ch, float& lp, float& hp)
    {
        const float v3 = x - ic2[ch];
        const float v1 = a1 * ic1[ch] + a2 * v3;
        const float v2 = ic2[ch] + a2 * ic1[ch] + a3 * v3;
        ic1[ch] = 2.0f * v1 - ic1[ch];
        ic2[ch] = 2.0f * v2 - ic2[ch];
        lp = v2;
        hp = x - k * v1 - v2;
    }
    void reset() { ic1[0] = ic1[1] = ic2[0] = ic2[1] = 0; }
};

inline float onePole (float hz, float sr) { return 1.0f - std::exp (-2.0f * kPi * hz / sr); }
inline float fromDb (float dB) { return std::pow (10.0f, dB / 20.0f); }

//------------------------------------------------------------------------------
class Engine
{
public:
    static constexpr int kChunk = 32;

    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        reverb.prepare (sampleRate);
        for (int ch = 0; ch < 2; ++ch)
        {
            for (auto& v : ens[ch]) v.resize ((int) (0.06f * sr) + 8);
            octDown[ch].prepare (sr); octUp[ch].prepare (sr);
            if (ch == 1) { octDown[ch].phase = 0.37f; octUp[ch].phase = 0.21f; }
            ring[ch].assign ((size_t) (sr * 8.0f), 0.0f);
            driveLp[ch] = glowLp[ch] = dcD[ch] = dcG[ch] = downLp[ch] = upHp[ch] = upLp[ch] = 0.0f;
        }
        air.reset(); svf.reset();
        ensPhase = 0.0f;
        envFast = envSlow = 0.0f; bow = 1.0f;
        ringW = 0; sliceStart = 0; beatSample = 0; lastBeat = -1; freePpq = 0.0;
        beatActive = false; stutterMix = 0.0f;
        revTail = 0; first = true;
        meterOut = 0.0f;
    }

    void setParams (const Params& np) { p = np; }
    float outputPeak() const { return meterOut.load (std::memory_order_relaxed); }

    void process (float* L, float* R, int n, const Transport& tIn)
    {
        Transport t = tIn;
        t.bpm = std::clamp (t.bpm > 1.0 ? t.bpm : 120.0, 40.0, 300.0);
        if (! t.playing) t.ppq = freePpq;     // stopped: keep a steady free-running grid
        for (int pos = 0; pos < n; pos += kChunk)
        {
            const int m = std::min (kChunk, n - pos);
            processChunk (L + pos, R + pos, m, t);
            t.ppq += (double) m * t.bpm / 60.0 / (double) sr;
        }
        freePpq = t.ppq;
    }

private:
    void processChunk (float* L, float* R, int n, const Transport& t)
    {
        // ---- smoothing ------------------------------------------------------------
        const float tgt[10] = { clamp01 (p.drive), clamp01 (p.glow), clamp01 (p.orchestra), clamp01 (p.stutter),
                                clamp01 (p.shimmer), clamp01 (p.nebula), clamp01 (p.pump),
                                std::clamp (p.filter, -1.0f, 1.0f), clamp01 (p.mix), std::clamp (p.outputDb, -24.0f, 12.0f) };
        if (first) { for (int i = 0; i < 10; ++i) sm[i] = tgt[i]; first = false; }
        const float k = 1.0f - std::exp (-(float) n / (0.03f * sr));
        for (int i = 0; i < 10; ++i) sm[i] += k * (tgt[i] - sm[i]);
        const float drive = sm[0], glow = sm[1], orch = sm[2], stut = tgt[3], shim = sm[4], neb = sm[5],
                    pump = sm[6], filt = sm[7], mix = sm[8], outG = fromDb (sm[9]);

        float dryL[kChunk], dryR[kChunk];
        std::copy (L, L + n, dryL); std::copy (R, R + n, dryR);

        // ---- 1. DRIVE ---------------------------------------------------------------
        if (drive > 0.001f)
        {
            const float pre = 1.0f + 40.0f * drive * drive;
            const float bias = 0.2f * drive;
            const float tb = std::tanh (bias);
            const float comp = 1.0f / std::sqrt (pre);
            const float blend = std::min (1.0f, drive * 5.0f);
            const float lpC = onePole (14000.0f - 9000.0f * drive, sr);   // tames fizz as it gets dirtier
            const float dcC = onePole (8.0f, sr);
            float* io[2] = { L, R };
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < n; ++i)
                {
                    const float x = io[ch][i];
                    float y = (std::tanh (pre * x + bias) - tb) * comp * 1.6f;
                    driveLp[ch] += lpC * (y - driveLp[ch]); y = driveLp[ch];
                    dcD[ch] += dcC * (y - dcD[ch]); y -= dcD[ch];
                    io[ch][i] = x + blend * (y - x);
                }
        }

        // ---- 2. GLOW: harmonics generated from the top end only, plus air -----------
        if (glow > 0.001f)
        {
            air.highShelf (9000.0f, 5.0f * glow, sr);
            const float hpC = onePole (2500.0f, sr), dcC = onePole (8.0f, sr);
            float* io[2] = { L, R };
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < n; ++i)
                {
                    float x = io[ch][i];
                    glowLp[ch] += hpC * (x - glowLp[ch]);
                    const float hi = x - glowLp[ch];
                    const float h = std::tanh (hi * 6.0f) / 6.0f;          // new harmonics above 2.5 kHz
                    x += glow * 1.8f * (h - hi * 0.6f) + 0.25f * glow * x * x;
                    dcG[ch] += dcC * (x - dcG[ch]); x -= dcG[ch];
                    io[ch][i] = air.process (x, ch);
                }
        }

        // ---- 3. ORCHESTRA: unison section + octave below + octave above, bowed in ---
        if (orch > 0.001f)
        {
            const float lfoInc = 1.0f / sr;
            const float envA = onePole (200.0f, sr), envR = onePole (6.0f, sr), slowC = onePole (2.0f, sr);
            const float bowRise = 1.0f / (0.35f * sr);
            const float downLpC = onePole (1400.0f, sr), upHpC = onePole (700.0f, sr), upLpC = onePole (7000.0f, sr);
            const float ensG = 0.55f * orch, downG = 0.75f * orch, upG = 0.45f * std::max (0.0f, orch - 0.25f) / 0.75f;
            const float norm = 1.0f / (1.0f + 0.6f * orch);
            for (int i = 0; i < n; ++i)
            {
                const float in[2] = { L[i], R[i] };
                const float a = std::max (std::abs (in[0]), std::abs (in[1]));
                envFast += (a > envFast ? envA : envR) * (a - envFast);
                envSlow += slowC * (envFast - envSlow);
                if (envFast > 2.5f * envSlow + 0.004f) bow = std::min (bow, 0.15f);   // new note: re-bow
                bow = std::min (1.0f, bow + bowRise);
                const float bowG = bow * bow;

                ensPhase += lfoInc; if (ensPhase > 1000.0f) ensPhase -= 1000.0f;
                for (int ch = 0; ch < 2; ++ch)
                {
                    float sec = 0.0f;
                    for (int v = 0; v < 3; ++v)
                    {
                        ens[ch][v].push (in[ch]);
                        const float rate = 0.21f + 0.17f * (float) v + 0.05f * (float) ch;
                        const float d = (ensBaseMs[v] + (ch ? 3.1f : 0.0f) + 3.5f * std::sin (2.0f * kPi * rate * ensPhase + (float) v)) * 0.001f * sr;
                        sec += ens[ch][v].read (d);
                    }
                    float dn = octDown[ch].process (in[ch], 0.5f);
                    downLp[ch] += downLpC * (dn - downLp[ch]); dn = downLp[ch];
                    float up = octUp[ch].process (in[ch], 2.0f);
                    upHp[ch] += upHpC * (up - upHp[ch]); up -= upHp[ch];
                    upLp[ch] += upLpC * (up - upLp[ch]); up = upLp[ch];
                    const float layers = ensG * sec * 0.45f + bowG * (downG * dn + upG * up * (ch ? 1.0f : 0.8f));
                    (ch ? R : L)[i] = (in[ch] + layers) * norm;
                }
            }
        }

        // ---- 4. STUTTER: tempo-synced beat repeat ----------------------------------------
        {
            const double spb = 60.0 / t.bpm * (double) sr;        // samples per beat
            const float div = rateBeats (p.stutterRate);
            const int sliceLen = std::max (16, (int) (div * spb));
            const int fade = std::max (8, (int) (0.002f * sr));
            const int ringN = (int) ring[0].size();
            const float mixC = onePole (250.0f, sr);   // ~0.6 ms: crisp, but no click
            for (int i = 0; i < n; ++i)
            {
                const double ppq = t.ppq + (double) i / spb;
                const long beat = (long) std::floor (ppq);
                const float phase = (float) (ppq - (double) beat);
                const int beatInBar = (int) (((beat % 4) + 4) % 4);
                if (beat != lastBeat)
                {
                    lastBeat = beat;
                    sliceStart = ringW;
                    beatSample = (int) (phase * spb);   // usually 0; non-zero if the host jumped mid-beat
                    // density: a little = the last beat of each bar, more = beats 2 and 4, full = every beat
                    beatActive = stut > 0.001f && (stut >= 0.67f || (stut >= 0.34f ? (beatInBar % 2 == 1) : beatInBar == 3));
                }
                ring[0][(size_t) ringW] = L[i]; ring[1][(size_t) ringW] = R[i];
                ringW = (ringW + 1) % ringN;

                const bool repeating = beatActive && beatSample >= sliceLen;
                stutterMix += mixC * ((repeating ? 1.0f : 0.0f) - stutterMix);
                if (stutterMix > 0.0005f)
                {
                    const int off = beatSample % sliceLen;   // exact sample count, no rounding jitter
                    const int idx = (sliceStart + off) % ringN;
                    const float w = std::min (1.0f, std::min ((float) off / (float) fade, (float) (sliceLen - off) / (float) fade));
                    const float g = 0.6f + 0.4f * w;    // a soft chop at each repeat
                    L[i] += stutterMix * (ring[0][(size_t) idx] * g - L[i]);
                    R[i] += stutterMix * (ring[1][(size_t) idx] * g - R[i]);
                }
                ++beatSample;
            }
        }

        // ---- 5 + 6. SHIMMER and NEBULA: one big reverb ------------------------------------
        const bool revOn = neb > 0.002f || shim > 0.002f || p.freeze || revTail > 0;
        if (revOn)
        {
            float wL[kChunk], wR[kChunk];
            std::copy (L, L + n, wL); std::copy (R, R + n, wR);
            fm::Params rp;
            rp.mode = fm::Ladder;
            rp.decay = 0.30f + 0.65f * neb;                  // about 3 s up to about 24 s
            rp.character = shim;                             // how hard the tail climbs
            rp.rise = p.shimmerDir == Down ? 0.0f : (p.shimmerDir == Both ? 0.8f : 1.0f);
            rp.fall = p.shimmerDir == Up ? 0.0f : (p.shimmerDir == Both ? 0.7f : 1.0f);
            rp.preDelayMs = 30.0f; rp.tone = 0.55f + 0.2f * shim; rp.mod = 0.45f; rp.mix = 1.0f;
            rp.freeze = p.freeze;
            reverb.setParams (rp);
            reverb.process (wL, wR, n);
            const float wet = 1.3f * std::max (neb, 0.55f * shim) + (p.freeze ? 0.5f : 0.0f);
            float tailPk = 0.0f;
            for (int i = 0; i < n; ++i)
            {
                L[i] += wL[i] * wet; R[i] += wR[i] * wet;
                tailPk = std::max (tailPk, std::abs (wL[i]) + std::abs (wR[i]));
            }
            if (neb > 0.002f || shim > 0.002f || p.freeze) revTail = (int) (sr * 30.0f);
            else if (tailPk > 1.0e-5f) revTail = std::max (revTail, n + 1);
            revTail = std::max (0, revTail - n);
        }

        // ---- 7. PUMP: sidechain-style duck on every beat -------------------------------------
        if (pump > 0.001f)
        {
            const double spb = 60.0 / t.bpm * (double) sr;
            const float depth = 0.92f * pump;
            for (int i = 0; i < n; ++i)
            {
                const double ppq = t.ppq + (double) i / spb;
                const float ph = (float) (ppq - std::floor (ppq));
                float env;
                if (ph < 0.015f) env = ph / 0.015f;                                   // quick duck, no click
                else { const float r = std::min (1.0f, (ph - 0.015f) / 0.62f); env = (1.0f - r) * (1.0f - r); }
                const float g = 1.0f - depth * env;
                L[i] *= g; R[i] *= g;
            }
        }

        // ---- FILTER: one knob, left = low-pass, right = high-pass ----------------------------
        if (std::abs (filt) > 0.01f)
        {
            const float amt = std::min (1.0f, (std::abs (filt) - 0.01f) / 0.06f);   // eases in from the centre detent
            const float hz = filt < 0.0f ? 20000.0f * std::pow (60.0f / 20000.0f, -filt)
                                         : 20.0f * std::pow (9000.0f / 20.0f, filt);
            svf.set (hz, 1.4f, sr);
            for (int i = 0; i < n; ++i)
            {
                float lp, hp;
                svf.process (L[i], 0, lp, hp); L[i] += amt * ((filt < 0 ? lp : hp) - L[i]);
                svf.process (R[i], 1, lp, hp); R[i] += amt * ((filt < 0 ? lp : hp) - R[i]);
            }
        }
        else
            for (int i = 0; i < n; ++i) { float lp, hp; svf.process (L[i], 0, lp, hp); svf.process (R[i], 1, lp, hp); }

        // ---- MIX, OUTPUT, safety ------------------------------------------------------------
        float pk = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            float l = (dryL[i] + mix * (L[i] - dryL[i])) * outG;
            float r = (dryR[i] + mix * (R[i] - dryR[i])) * outG;
            l = softLimit (l); r = softLimit (r);
            L[i] = l; R[i] = r;
            pk = std::max (pk, std::max (std::abs (l), std::abs (r)));
        }
        const float fall = std::exp (-(float) n / (0.25f * sr));
        meterOut.store (std::max (pk, meterOut.load (std::memory_order_relaxed) * fall), std::memory_order_relaxed);
    }

    // Untouched below -1.5 dBFS, rounds off smoothly to a -0.3 dBFS ceiling above it.
    static float softLimit (float x)
    {
        const float a = std::abs (x), knee = 0.84f, top = 0.966f;
        if (a <= knee) return x;
        return std::copysign (knee + (top - knee) * std::tanh ((a - knee) / (top - knee)), x);
    }
    static float clamp01 (float v) { return std::clamp (v, 0.0f, 1.0f); }

    static constexpr float ensBaseMs[3] = { 11.0f, 17.0f, 24.0f };

    Params p;
    float sr = 48000.0f;
    bool first = true;
    float sm[10] = {};

    float driveLp[2] = {}, dcD[2] = {}, glowLp[2] = {}, dcG[2] = {};
    Biquad air;
    fm::DelayLine ens[2][3];
    fm::PitchShifter octDown[2], octUp[2];
    float downLp[2] = {}, upHp[2] = {}, upLp[2] = {};
    float ensPhase = 0, envFast = 0, envSlow = 0, bow = 1;

    std::vector<float> ring[2];
    int ringW = 0, sliceStart = 0, beatSample = 0;
    long lastBeat = -1;
    bool beatActive = false;
    float stutterMix = 0;
    double freePpq = 0.0;

    fm::ReverbEngine reverb;
    int revTail = 0;
    Svf svf;

    std::atomic<float> meterOut { 0.0f };
};

} // namespace sd
