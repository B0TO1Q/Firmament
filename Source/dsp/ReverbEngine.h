// Firmament reverb engine. Portable C++17, no framework dependencies,
// so it can be tested on its own and reused by other plugins.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace fm
{

constexpr float kPi = 3.14159265358979323846f;

enum Mode { Quarters = 0, Sanctuary, Steel, Coil, Ladder, Vapor, Tide, kNumModes };

inline const char* modeName (int m)
{
    static const char* n[] = { "QUARTERS", "SANCTUARY", "STEEL", "COIL", "LADDER", "VAPOR", "TIDE" };
    return n[std::clamp (m, 0, kNumModes - 1)];
}

// What the CHARACTER knob does in each mode.
inline const char* characterName (int m)
{
    static const char* n[] = { "SIZE", "SIZE", "SHEEN", "DRIP", "CLIMB", "HAZE", "RISE" };
    return n[std::clamp (m, 0, kNumModes - 1)];
}

struct ModeCfg
{
    float scaleLo, scaleHi;   // tank size (CHARACTER sweeps it when lo != hi)
    float decayLo, decayHi;   // seconds at DECAY = 0 and 1
    float diffusion;
    float modMult;
    float dampLoHz, dampHiHz; // TONE range
    float wetGain;
};

inline const ModeCfg& modeCfg (int m)
{
    static const ModeCfg c[kNumModes] = {
        /* Quarters  */ { 0.16f, 0.50f, 0.15f,  3.0f, 0.55f, 0.5f, 2500.0f, 16000.0f, 1.00f },
        /* Sanctuary */ { 0.70f, 1.50f, 0.60f, 25.0f, 0.68f, 1.0f, 1800.0f, 14000.0f, 1.00f },
        /* Steel     */ { 0.50f, 0.50f, 0.30f, 10.0f, 0.74f, 0.6f, 3000.0f, 18000.0f, 1.00f },
        /* Coil      */ { 0.30f, 0.30f, 0.50f,  6.0f, 0.50f, 0.3f, 1500.0f,  6000.0f, 1.25f },
        /* Ladder    */ { 1.10f, 1.10f, 1.00f, 30.0f, 0.70f, 1.0f, 2000.0f, 12000.0f, 1.00f },
        /* Vapor     */ { 1.70f, 1.70f, 2.00f, 60.0f, 0.62f, 2.0f, 1200.0f,  9000.0f, 1.00f },
        /* Tide      */ { 1.00f, 1.00f, 1.00f, 25.0f, 0.70f, 1.2f, 1800.0f, 12000.0f, 1.10f },
    };
    return c[std::clamp (m, 0, kNumModes - 1)];
}

inline float decaySeconds (int mode, float decay01)
{
    const auto& c = modeCfg (mode);
    return c.decayLo * std::pow (c.decayHi / c.decayLo, std::clamp (decay01, 0.0f, 1.0f));
}

struct Params
{
    int   mode       = Sanctuary;
    float decay      = 0.5f;   // 0..1, mapped per mode
    float preDelayMs = 20.0f;  // 0..250
    float tone       = 0.6f;   // 0 dark .. 1 bright
    float mod        = 0.3f;   // 0..1
    float mix        = 0.35f;  // 0 dry .. 1 wet
    float character  = 0.5f;   // per-mode meaning, see characterName()
    bool  freeze     = false;
    // Ladder fine tuning
    float rise       = 0.7f;   // level of the octave-up voice
    float fall       = 0.0f;   // level of the octave-down voice
    float fineCents  = 0.0f;   // -50..+50, detunes both voices
};

//------------------------------------------------------------------------------
struct DelayLine
{
    std::vector<float> buf;
    int mask = 0, w = 0;

    void resize (int minSamples)
    {
        int n = 16;
        while (n < minSamples + 8) n <<= 1;
        buf.assign ((size_t) n, 0.0f);
        mask = n - 1;
        w = 0;
    }
    void clear() { std::fill (buf.begin(), buf.end(), 0.0f); }
    void push (float x) { buf[(size_t) w] = x; w = (w + 1) & mask; }
    float readInt (int d) const { return buf[(size_t) ((w - d) & mask)]; }
    float read (float d) const   // d >= 1, linear interpolation
    {
        const float rp = (float) w - d;
        const float fl = std::floor (rp);
        const int i = (int) fl;
        const float f = rp - fl;
        const float a = buf[(size_t) (i & mask)];
        const float b = buf[(size_t) ((i + 1) & mask)];
        return a + (b - a) * f;
    }
};

struct Allpass
{
    DelayLine d;
    float len = 1.0f;
    float process (float x, float g)
    {
        const float del = d.read (len);
        const float v = x - g * del;
        d.push (v);
        return del + g * v;
    }
};

// Dual-tap crossfading delay pitch shifter.
struct PitchShifter
{
    DelayLine d;
    float phase = 0.0f, win = 2880.0f;

    void prepare (float sr)
    {
        win = 0.06f * sr;
        d.resize ((int) win + 8);
        phase = 0.0f;
    }
    float process (float x, float ratio)
    {
        d.push (x);
        phase += (1.0f - ratio) / win;
        phase -= std::floor (phase);
        float p2 = phase + 0.5f;
        if (p2 >= 1.0f) p2 -= 1.0f;
        const float s = std::sin (kPi * phase);
        const float g1 = s * s;
        return g1 * d.read (1.0f + phase * win) + (1.0f - g1) * d.read (1.0f + p2 * win);
    }
};

struct OnePole
{
    float z = 0.0f;
    float lp (float x, float c) { z += c * (x - z); return z; }
    float hp (float x, float c) { z += c * (x - z); return x - z; }
};

inline float coefFor (float hz, float sr) { return 1.0f - std::exp (-2.0f * kPi * hz / sr); }

//------------------------------------------------------------------------------
class ReverbEngine
{
public:
    static constexpr int N = 8;

    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        const auto ms = [this] (float m) { return m * 0.001f * sr; };

        for (int i = 0; i < N; ++i)
        {
            lines[i].resize ((int) ms (baseMs[i] * 2.0f) + 512);
            damp[i].z = 0.0f;
            lfoPhase[i] = (float) i / (float) N;
        }
        for (int ch = 0; ch < 2; ++ch)
        {
            preDelay[ch].resize ((int) ms (260.0f));
            for (int k = 0; k < 4; ++k)
            {
                diff[ch][k].len = ms (diffMs[ch][k]);
                diff[ch][k].d.resize ((int) diff[ch][k].len + 4);
            }
            for (int k = 0; k < 2; ++k)
            {
                haze[ch][k].len = ms (hazeMs[ch][k]);
                haze[ch][k].d.resize ((int) haze[ch][k].len + 4);
            }
            up[ch].prepare (sr);
            down[ch].prepare (sr);
            if (ch == 1) { up[ch].phase = 0.25f; down[ch].phase = 0.25f; }
            for (auto& z : spring[ch]) z = 0.0f;
            dcBlock[ch] = {}; coilHp[ch] = {}; coilLp[ch] = {}; sheenLp[ch] = {};
            fbLp[ch] = {}; fbHp[ch] = {};
            fb[ch] = 0.0f;
        }
        envFast = envSlow = 0.0f;
        swell = 0.0f; gateOpen = false; holdDown = 0; closeCount = 0;
        first = true;
    }

    void setParams (const Params& np) { p = np; }

    // In-place stereo processing.
    void process (float* L, float* R, int n)
    {
        const auto& c = modeCfg (p.mode);
        const float ch01 = std::clamp (p.character, 0.0f, 1.0f);

        // ---- per-block targets -------------------------------------------
        const float tScale   = c.scaleLo + (c.scaleHi - c.scaleLo) * ch01;
        const float t60      = decaySeconds (p.mode, p.decay);
        const float tDamp    = coefFor (c.dampLoHz * std::pow (c.dampHiHz / c.dampLoHz, std::clamp (p.tone, 0.0f, 1.0f)), sr);
        const float tDiff    = p.mode == Vapor ? c.diffusion + 0.16f * ch01 : c.diffusion;
        const float tModDep  = std::clamp (p.mod, 0.0f, 1.0f) * c.modMult * (p.mode == Vapor ? 1.0f + 2.0f * ch01 : 1.0f)
                               * 14.0f * sr / 48000.0f;
        const float tPre     = 1.0f + std::clamp (p.preDelayMs, 0.0f, 250.0f) * 0.001f * sr;
        const float tFrz     = p.freeze ? 1.0f : 0.0f;
        const float tCoil    = p.mode == Coil   ? 1.0f : 0.0f;
        const float tLadder  = p.mode == Ladder ? 1.0f : 0.0f;
        const float tVapor   = p.mode == Vapor  ? 1.0f : 0.0f;
        const float tTide    = p.mode == Tide   ? 1.0f : 0.0f;
        const float tSheen   = p.mode == Steel  ? (-0.5f + 2.0f * ch01) : 0.0f;
        const float mixC     = std::clamp (p.mix, 0.0f, 1.0f);
        const float tDry     = std::cos (mixC * kPi * 0.5f);
        // Longer tails build more energy; pull the wet level back so DECAY isn't a volume knob.
        const float tWet     = std::sin (mixC * kPi * 0.5f) * c.wetGain * 1.5f / std::sqrt (0.4f + 0.6f * std::min (t60, 12.0f));

        const float climb    = (p.mode == Ladder ? ch01 : 0.0f) * 0.42f;
        const float tUp      = climb * std::clamp (p.rise, 0.0f, 1.0f);
        const float tDown    = climb * std::clamp (p.fall, 0.0f, 1.0f);
        const float fine     = std::pow (2.0f, std::clamp (p.fineCents, -50.0f, 50.0f) / 1200.0f);
        const float upRatio  = 2.0f * fine, downRatio = 0.5f * fine;
        const float springA  = 0.30f + 0.42f * ch01;
        const float riseSamp = (0.05f * std::pow (40.0f, ch01)) * sr;   // 50 ms .. 2 s

        if (first)
        {
            scale = tScale; dampC = tDamp; diffG = tDiff; modDep = tModDep; pre = tPre; frz = tFrz;
            coil = tCoil; ladder = tLadder; vapor = tVapor; tide = tTide; sheen = tSheen;
            dry = tDry; wet = tWet; upG = tUp; downG = tDown;
            first = false;
        }

        // Feedback gain per line for the requested RT60 (uses target size).
        float gT[N];
        for (int i = 0; i < N; ++i)
            gT[i] = std::pow (10.0f, -3.0f * (baseMs[i] * 0.001f * tScale) / t60);

        const float kFast = coefFor (12.0f, sr);    // ~13 ms
        const float kSlow = coefFor (1.6f, sr);     // ~100 ms (size / pre-delay glide)
        const float lfoInc = 1.0f / sr;
        const float cDc = coefFor (20.0f, sr), cCoilHp = coefFor (110.0f, sr), cCoilLp = coefFor (4800.0f, sr);
        const float cSheen = coefFor (3500.0f, sr), cFbLp = coefFor (7000.0f, sr), cFbHp = coefFor (70.0f, sr);
        const float envA = coefFor (300.0f, sr), envR = coefFor (8.0f, sr), envS = coefFor (3.0f, sr);

        for (int s = 0; s < n; ++s)
        {
            // ---- smoothing ------------------------------------------------
            scale  += kSlow * (tScale - scale);
            pre    += kSlow * (tPre - pre);
            dampC  += kFast * (tDamp - dampC);
            diffG  += kFast * (tDiff - diffG);
            modDep += kFast * (tModDep - modDep);
            frz    += kFast * (tFrz - frz);
            coil   += kFast * (tCoil - coil);
            ladder += kFast * (tLadder - ladder);
            vapor  += kFast * (tVapor - vapor);
            tide   += kFast * (tTide - tide);
            sheen  += kFast * (tSheen - sheen);
            dry    += kFast * (tDry - dry);
            wet    += kFast * (tWet - wet);
            upG    += kFast * (tUp - upG);
            downG  += kFast * (tDown - downG);

            const float inL = L[s], inR = R[s];
            float x[2] = { inL, inR };

            // ---- Tide: the signal entering the tank swells in after each attack
            if (tide > 0.001f)
            {
                const float a = std::max (std::abs (inL), std::abs (inR));
                envFast += (a > envFast ? envA : envR) * (a - envFast);
                envSlow += envS * (envFast - envSlow);

                if (! gateOpen && envFast > 0.004f) { gateOpen = true; closeCount = 0; }
                else if (gateOpen)
                {
                    if (envFast < 0.0012f) { if (++closeCount > (int) (0.08f * sr)) gateOpen = false; }
                    else closeCount = 0;
                    if (envFast > 3.0f * envSlow + 0.01f && swell > 0.35f && holdDown == 0)
                        holdDown = (int) (0.012f * sr);   // new attack: duck and swell again
                }
                if (holdDown > 0) { swell *= 1.0f - 6.0f / (0.012f * sr); --holdDown; }
                else if (gateOpen) swell = std::min (1.0f, swell + 1.0f / riseSamp);
                else swell *= 1.0f - 1.0f / (0.25f * sr);

                const float g = 1.0f + tide * (swell * swell - 1.0f);
                x[0] *= g; x[1] *= g;
            }

            // ---- pre-delay --------------------------------------------------
            for (int ch = 0; ch < 2; ++ch)
            {
                preDelay[ch].push (x[ch]);
                x[ch] = preDelay[ch].read (pre);
            }

            // ---- Coil: dispersion chain gives the spring "drip" -------------
            if (coil > 0.001f)
            {
                for (int ch = 0; ch < 2; ++ch)
                {
                    float y = x[ch];
                    for (auto& z : spring[ch])
                    {
                        const float v = y - springA * z;
                        y = z + springA * v;
                        z = v;
                    }
                    x[ch] += coil * (y - x[ch]);
                }
            }

            // ---- Ladder: pitch-shifted tail climbs back into the tank -------
            const float inGain = 1.0f - frz;
            x[0] = x[0] * inGain + fb[0] * inGain;
            x[1] = x[1] * inGain + fb[1] * inGain;

            // ---- input diffusion --------------------------------------------
            for (int ch = 0; ch < 2; ++ch)
            {
                for (int k = 0; k < 4; ++k)
                    x[ch] = diff[ch][k].process (x[ch], diffG);
                if (vapor > 0.001f)
                {
                    float y = x[ch];
                    for (int k = 0; k < 2; ++k)
                        y = haze[ch][k].process (y, 0.7f);
                    x[ch] += vapor * (y - x[ch]);
                }
            }

            // ---- feedback delay network -------------------------------------
            float v[N];
            float outL = 0.0f, outR = 0.0f;
            for (int i = 0; i < N; ++i)
            {
                lfoPhase[i] += lfoRate[i] * lfoInc;
                if (lfoPhase[i] >= 1.0f) lfoPhase[i] -= 1.0f;
                // cheap sine: parabola approximation
                const float ph = lfoPhase[i] * 2.0f - 1.0f;
                const float lfo = 4.0f * ph * (1.0f - std::abs (ph));
                const float len = baseMs[i] * 0.001f * sr * scale + modDep * (1.0f + lfo) + 2.0f;

                float y = lines[i].read (len);
                const float lp = damp[i].lp (y, dampC);
                y = lp + frz * (y - lp);                        // no damping while frozen
                gSm[i] += kFast * (gT[i] - gSm[i]);
                y *= gSm[i] + frz * (1.0f - gSm[i]);            // unity feedback while frozen
                v[i] = y;
                outL += y * outSignL[i];
                outR += y * outSignR[i];
            }

            // 8x8 Hadamard mix (orthogonal, energy preserving)
            for (int h = 1; h < N; h <<= 1)
                for (int i = 0; i < N; i += h << 1)
                    for (int j = i; j < i + h; ++j)
                    {
                        const float a = v[j], b = v[j + h];
                        v[j] = a + b; v[j + h] = a - b;
                    }
            const float norm = 0.35355339f; // 1/sqrt(8)
            for (int i = 0; i < N; ++i)
                lines[i].push (v[i] * norm + x[i & 1] * 0.5f);

            float wL = dcBlock[0].hp (outL * 0.5f, cDc);
            float wR = dcBlock[1].hp (outR * 0.5f, cDc);

            // ---- Ladder feedback voices -------------------------------------
            if (ladder > 0.001f || std::abs (fb[0]) + std::abs (fb[1]) > 1.0e-6f)
            {
                const float w2[2] = { wL, wR };
                for (int ch = 0; ch < 2; ++ch)
                {
                    float f = upG * up[ch].process (w2[ch], upRatio) + downG * down[ch].process (w2[ch], downRatio);
                    f = fbLp[ch].lp (f, cFbLp);
                    f = fbHp[ch].hp (f, cFbHp);
                    fb[ch] = std::tanh (f) * ladder;
                }
            }
            else { fb[0] = fb[1] = 0.0f; }

            // ---- Coil voicing: band-limit like a real tank -------------------
            if (coil > 0.001f)
            {
                const float bL = coilLp[0].lp (coilHp[0].hp (wL, cCoilHp), cCoilLp);
                const float bR = coilLp[1].lp (coilHp[1].hp (wR, cCoilHp), cCoilLp);
                wL += coil * (bL - wL); wR += coil * (bR - wR);
            }

            // ---- Steel sheen: tilt the top end -------------------------------
            if (std::abs (sheen) > 0.001f)
            {
                wL += sheen * (wL - sheenLp[0].lp (wL, cSheen));
                wR += sheen * (wR - sheenLp[1].lp (wR, cSheen));
            }

            L[s] = inL * dry + wL * wet;
            R[s] = inR * dry + wR * wet;
        }

        // flush denormals
        for (int i = 0; i < N; ++i)
            if (std::abs (damp[i].z) < 1.0e-20f) damp[i].z = 0.0f;
    }

private:
    static constexpr float baseMs[N]   = { 43.7f, 49.1f, 56.3f, 61.9f, 68.3f, 74.9f, 83.3f, 91.7f };
    static constexpr float lfoRate[N]  = { 0.31f, 0.47f, 0.59f, 0.71f, 0.83f, 0.97f, 1.09f, 1.21f };
    static constexpr float outSignL[N] = {  1, 0, -1, 0,  1, 0, -1, 0 };
    static constexpr float outSignR[N] = {  0, 1,  0, -1, 0, 1,  0, -1 };
    static constexpr float diffMs[2][4] = { { 4.71f, 7.33f, 11.13f, 13.69f }, { 5.03f, 7.79f, 10.61f, 14.23f } };
    static constexpr float hazeMs[2][2] = { { 25.3f, 37.1f }, { 27.7f, 35.3f } };

    Params p;
    float sr = 48000.0f;
    bool first = true;

    DelayLine lines[N];
    OnePole damp[N];
    float lfoPhase[N] = {};
    float gSm[N] = {};
    DelayLine preDelay[2];
    Allpass diff[2][4], haze[2][2];
    PitchShifter up[2], down[2];
    std::array<float, 40> spring[2] {};
    OnePole dcBlock[2], coilHp[2], coilLp[2], sheenLp[2], fbLp[2], fbHp[2];
    float fb[2] = { 0.0f, 0.0f };

    // smoothed values
    float scale = 1, dampC = 0.5f, diffG = 0.6f, modDep = 0, pre = 1, frz = 0;
    float coil = 0, ladder = 0, vapor = 0, tide = 0, sheen = 0, dry = 1, wet = 0, upG = 0, downG = 0;

    // Tide state
    float envFast = 0, envSlow = 0, swell = 0;
    bool gateOpen = false;
    int holdDown = 0, closeCount = 0;
};

} // namespace fm
