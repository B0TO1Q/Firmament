// Selah channel engine. Portable C++17, no framework dependencies.
// Every source has its own six tools. Everything is zero-latency: minimum-phase filters, no look-ahead.
#pragma once

#include "../../Source/dsp/ReverbEngine.h"
#include <atomic>
#include <cstdio>
#include <memory>
#include <string>

namespace sl
{

enum Source { Speaker = 0, LeadVocal, Choir, Acoustic, Electric, Bass, Keys, Drums, FullMix, kNumSources };

inline const char* sourceName (int s)
{
    static const char* n[] = { "SPEAKER", "LEAD VOCAL", "CHOIR", "ACOUSTIC", "ELECTRIC", "BASS", "KEYS", "DRUMS", "FULL MIX" };
    return n[std::clamp (s, 0, kNumSources - 1)];
}

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
// Tool library. Each source gets six tools picked from these kinds. The kind
// order is also the signal order: clean-up first, tone, dynamics, then space.
enum class Kind { LowCut = 0, Gate, DynLow, Eq, DeEss, Saturate, DynHigh, Comp, Transient, Parallel, Double, Width, Echo, Reverb, Loudness };

enum BandKind { None = 0, Bell, LowShelf, HighShelf };
struct Band { int kind; float hz, db, q; };

struct ToolSpec
{
    const char* name;
    const char* hint;
    Kind kind;
    float p[6];     // meaning depends on the kind, see each tool below
    float x[6];     // extra settings (compressors and reverbs)
    Band band[3];   // EQ bands
    float def;      // default knob position
};

constexpr int kTools = 6;

inline const ToolSpec& toolSpec (int source, int t)
{
    using K = Kind;
    static const ToolSpec s[kNumSources][kTools] = {
        // ---- Speaker Mic: intelligibility first --------------------------------------------------------------------
        { { "ROOM NOISE", "Quiets the room\nbetween words", K::Gate,   { -52, 14, 140 }, {}, {}, 0.3f },
          { "BOOM",       "Tames boom when\nthey lean in", K::DynLow, { 220, 9, 0 }, {}, {}, 0.3f },
          { "CLARITY",    "Every word\nclear",             K::Eq, {}, {}, { { Bell, 320, -4.5f, 0.9f }, { Bell, 3200, 5.0f, 0.7f }, { HighShelf, 10000, 2.0f, 0.6f } }, 0.3f },
          { "SIBILANCE",  "Softens harsh\nS sounds",       K::DeEss, { 5800, 8 }, {}, {}, 0.3f },
          { "LEVELER",    "Evens soft and\nloud talking",  K::Comp, { 3.5f, 8, 120, 1000, -30, 12 }, { 0, 0.25f, 0, 1, 0.6f }, {}, 0.3f },
          { "AMBIENCE",   "A little room,\nducks under words", K::Reverb, { fm::Quarters, 0.6f, 6, 0.30f, 10, 0.55f }, { 250, 7000 }, {}, 0.0f } },

        // ---- Lead Vocal -----------------------------------------------------------------------------------------------
        { { "CLEAN",      "Low rumble and\nbreath noise",  K::Gate,   { -56, 10, 160 }, {}, {}, 0.3f },
          { "SIBILANCE",  "Softens harsh\nS sounds",       K::DeEss, { 6800, 8 }, {}, {}, 0.3f },
          { "SPARKLE",    "Presence and\nair on top",      K::Eq, {}, {}, { { Bell, 300, -2.5f, 0.9f }, { Bell, 4200, 4.0f, 0.6f }, { HighShelf, 12000, 5.0f, 0.6f } }, 0.3f },
          { "CONTROL",    "Keeps the voice\nup front",     K::Comp, { 4.0f, 4, 90, 600, -30, 10 }, { 0, 0.5f, 0, 0, 0.35f }, {}, 0.3f },
          { "DOUBLE",     "Thickens like a\nsecond take",  K::Double, { 1.0f }, {}, {}, 0.0f },
          { "PLATE",      "Smooth vocal\nplate",           K::Reverb, { fm::Steel, 1.8f, 3, 0.50f, 40, 0.45f }, { 300, 9000 }, {}, 0.2f } },

        // ---- Choir -----------------------------------------------------------------------------------------------------
        { { "RUMBLE",     "Stage and\nhandling rumble",    K::LowCut, { 160 }, {}, {}, 0.3f },
          { "MUD",        "Clears the\ncrowded low-mids",  K::Eq, {}, {}, { { Bell, 400, -5.0f, 0.9f } }, 0.3f },
          { "CLARITY",    "Lyrics you\ncan understand",    K::Eq, {}, {}, { { Bell, 2800, 4.0f, 0.6f }, { HighShelf, 10000, 3.0f, 0.6f } }, 0.3f },
          { "BLEND",      "Glues the\nvoices together",    K::Comp, { 2.5f, 20, 200, 1200, -24, 12 }, { 0, 0.1f, 0, 1, 0.5f }, {}, 0.3f },
          { "WIDTH",      "Spreads the\nchoir wide",       K::Width, { 1.0f }, {}, {}, 0.0f },
          { "HALL",       "A big, warm\nsanctuary",        K::Reverb, { fm::Sanctuary, 2.4f, 0, 0.55f, 25, 0.55f }, { 200, 10000 }, {}, 0.2f } },

        // ---- Acoustic Guitar ---------------------------------------------------------------------------------------------
        { { "BOOM",       "Tames body\nresonance",         K::DynLow, { 180, 9, 90 }, {}, {}, 0.3f },
          { "PIEZO FIX",  "Takes the plastic\nquack out", K::Eq, {}, {}, { { Bell, 1200, -5.0f, 1.0f }, { HighShelf, 7000, -3.0f, 0.6f }, { Bell, 200, 2.0f, 0.8f } }, 0.0f },
          { "PICK",       "Brings out the\npick attack",   K::Transient, { 7 }, {}, {}, 0.0f },
          { "SPARKLE",    "String shimmer\non top",        K::Eq, {}, {}, { { Bell, 5000, 2.0f, 0.8f }, { HighShelf, 10000, 5.0f, 0.6f } }, 0.3f },
          { "SQUASH",     "Even strumming",                K::Comp, { 3.0f, 12, 120, 700, -26, 10 }, { 0, 0.2f, 0, 0, 0.4f }, {}, 0.3f },
          { "ROOM",       "Natural room\naround it",       K::Reverb, { fm::Sanctuary, 1.4f, 0, 0.42f, 15, 0.25f }, { 250, 10000 }, {}, 0.15f } },

        // ---- Electric Guitar -----------------------------------------------------------------------------------------------
        { { "GATE",       "Kills amp hiss\nbetween parts", K::Gate,   { -58, 20, 100 }, {}, {}, 0.3f },
          { "FIZZ",       "Cuts harsh\ntop end",           K::Eq, {}, {}, { { Bell, 5500, -6.0f, 0.9f }, { HighShelf, 9000, -6.0f, 0.6f } }, 0.3f },
          { "MIDS",       "Cuts through\nthe band",        K::Eq, {}, {}, { { Bell, 1000, 5.0f, 0.7f } }, 0.0f },
          { "DRIVE",      "Amp-style\nwarmth to grit",     K::Saturate, { 12, 0, 0, 0.4f }, {}, {}, 0.0f },
          { "WIDE",       "Spreads it\nacross the stage",  K::Width, { 1.2f }, {}, {}, 0.0f },
          { "ECHO",       "Slap and\nrepeats", K::Echo, { 380, 0.38f }, {}, {}, 0.0f } },

        // ---- Bass -----------------------------------------------------------------------------------------------------------
        { { "SUB",        "Deep low end\nyou can feel",    K::Eq, {}, {}, { { LowShelf, 60, 5.0f, 0.7f } }, 0.2f },
          { "MUD",        "Clears the\nboxy low-mids",     K::Eq, {}, {}, { { Bell, 250, -5.0f, 0.9f } }, 0.3f },
          { "GROWL",      "Grit that reads\non small speakers", K::Saturate, { 8, 500, 1500, 0.2f }, {}, {}, 0.0f },
          { "STRING",     "Note definition",               K::Eq, {}, {}, { { Bell, 2500, 4.0f, 0.8f } }, 0.2f },
          { "SQUASH",     "Every note\nthe same size",     K::Comp, { 5.0f, 15, 120, 800, -28, 8 }, { 40, 0, 0.4f, 0, 0.45f }, {}, 0.3f },
          { "PUNCH",      "Front edge\nof each note",      K::Transient, { 5 }, {}, {}, 0.0f } },

        // ---- Keys -------------------------------------------------------------------------------------------------------------
        { { "MUD",        "Clears space for\nbass and voice", K::Eq, {}, {}, { { Bell, 300, -4.0f, 0.9f } }, 0.3f },
          { "WARMTH",     "Rich, round\nlow end",          K::Saturate, { 3, 0, 220, 0.4f }, {}, {}, 0.0f },
          { "SPARKLE",    "Air and\nshine",                K::Eq, {}, {}, { { HighShelf, 10000, 4.0f, 0.6f } }, 0.2f },
          { "WIDTH",      "Wide pads\nand piano",          K::Width, { 1.0f }, {}, {}, 0.0f },
          { "LEVEL",      "Smooth, steady\nvolume",        K::Comp, { 2.5f, 25, 200, 900, -24, 12 }, { 0, 0.1f, 0, 1, 0.5f }, {}, 0.2f },
          { "HALL",       "Big ambient\nspace",            K::Reverb, { fm::Sanctuary, 2.0f, 0, 0.45f, 20, 0.5f }, { 200, 10000 }, {}, 0.15f } },

        // ---- Drums --------------------------------------------------------------------------------------------------------------
        { { "THUMP",      "Kick drum\nweight",             K::Eq, {}, {}, { { Bell, 65, 6.0f, 0.9f } }, 0.2f },
          { "CRACK",      "Snare snap",                    K::Eq, {}, {}, { { Bell, 220, 3.0f, 1.2f }, { Bell, 5000, 3.0f, 0.8f } }, 0.2f },
          { "CYMBALS",    "Tames harsh\ncymbals",          K::DynHigh, { 6000, 12 }, {}, {}, 0.3f },
          { "PUNCH",      "Sharper hits",                  K::Transient, { 6 }, {}, {}, 0.0f },
          { "SMASH",      "Big parallel\ncompression",     K::Parallel, {}, {}, {}, 0.0f },
          { "ROOM",       "Short, big\nroom",              K::Reverb, { fm::Quarters, 0.7f, 0, 0.30f, 5, 0.9f }, { 150, 9000 }, {}, 0.0f } },

        // ---- Full Mix -------------------------------------------------------------------------------------------------------------
        { { "LOW END",    "Tight, full\nbottom",           K::Eq, {}, {}, { { LowShelf, 70, 3.0f, 0.7f }, { Bell, 250, -1.5f, 0.8f } }, 0.2f },
          { "WARMTH",     "Console-style\nwarmth",         K::Saturate, { 2, 0, 220, 0.3f }, {}, {}, 0.2f },
          { "CLARITY",    "Open, clear\ntop end",          K::Eq, {}, {}, { { Bell, 3000, 1.5f, 0.6f }, { HighShelf, 12000, 2.5f, 0.6f } }, 0.2f },
          { "GLUE",       "Holds the mix\ntogether",       K::Comp, { 4.0f, 30, 150, 1200, -16, 3 }, { 90, 0, 0, 0, 0.55f }, {}, 0.3f },
          { "WIDTH",      "A wider\nstereo image",         K::Width, { 0.35f }, {}, {}, 0.0f },
          { "LOUDNESS",   "Louder for the\nlivestream",    K::Loudness, { 9 }, {}, {}, 0.0f } },
    };
    return s[std::clamp (source, 0, kNumSources - 1)][std::clamp (t, 0, kTools - 1)];
}

struct Params
{
    int   source   = Speaker;
    float tool[kNumSources][kTools] = {};   // 0..1 per source, so each source keeps its own setup
    float volumeDb = 0.0f;                  // -24..+12
    // Advanced
    float reverbLength = 0.5f;   // 0.5 = the tool's default; 0 = half as long, 1 = twice as long
    bool  limiter      = true;
    bool  mono         = false;  // true = fold to mono in and out (same signal on both sides)
    // Graphic EQ
    int   eqMode = 0;            // 0 = 7 bands, 1 = 14, 2 = 21
    float eqGain[21] = {};       // dB, -12..+12, for the bands of the current mode
    bool  eqPost = false;        // false = before the channel processing, true = after it
};

inline float reverbSeconds (const ToolSpec& s, float reverbLength)
{
    return s.p[1] * std::pow (4.0f, std::clamp (reverbLength, 0.0f, 1.0f) - 0.5f);
}

inline float lowCutHz (float maxHz, float amt) { return 20.0f * std::pow (maxHz / 20.0f, std::pow (std::clamp (amt, 0.0f, 1.0f), 0.7f)); }

// One line of plain numbers for the Advanced read-out.
inline std::string describeTool (const ToolSpec& s, float a, float reverbLength)
{
    char b[160] = {};
    const auto hz = [] (float f, char* out)
    {
        if (f >= 1000.0f) std::snprintf (out, 16, "%.1fk", f / 1000.0f); else std::snprintf (out, 16, "%d", (int) f);
    };
    char f1[16], f2[16];
    switch (s.kind)
    {
        case Kind::LowCut:   std::snprintf (b, sizeof b, "low cut %d Hz", (int) lowCutHz (s.p[0], a)); break;
        case Kind::Gate:     std::snprintf (b, sizeof b, "gate %d dB, low cut %d Hz", (int) (-90.0f + (s.p[0] + 90.0f) * a), (int) lowCutHz (s.p[2], a)); break;
        case Kind::DynLow:   hz (s.p[0], f1); std::snprintf (b, sizeof b, "dynamic cut below %s, up to -%.0f dB", f1, s.p[1] * a); break;
        case Kind::DynHigh:  hz (s.p[0], f1); std::snprintf (b, sizeof b, "dynamic cut above %s, up to -%.0f dB", f1, s.p[1] * a); break;
        case Kind::DeEss:    hz (s.p[0], f1); std::snprintf (b, sizeof b, "de-ess %s, up to -%.1f dB", f1, s.p[1] * a); break;
        case Kind::Eq:
        {
            int len = 0;
            for (const auto& bd : s.band)
            {
                if (bd.kind == None) continue;
                hz (bd.hz, f1);
                len += std::snprintf (b + len, sizeof b - (size_t) len, "%s%+.1f dB @ %s", len ? " / " : "", bd.db * a, f1);
            }
            break;
        }
        case Kind::Saturate:
            if (s.p[1] > 0.0f) { hz (s.p[1], f1); hz (s.p[2], f2); std::snprintf (b, sizeof b, "saturation %s-%s, drive x%.1f", f1, f2, 1.0f + s.p[0] * a); }
            else if (s.p[2] > 0.0f) { hz (s.p[2], f1); std::snprintf (b, sizeof b, "transformer saturation below %s", f1); }
            else std::snprintf (b, sizeof b, "drive x%.1f", 1.0f + s.p[0] * a);
            break;
        case Kind::Comp:     std::snprintf (b, sizeof b, "%.1f:1, threshold %.0f dB, attack %.0f ms", 1.0f + (s.p[0] - 1.0f) * std::min (1.0f, a * 2.5f), s.p[4] * a, s.p[1]); break;
        case Kind::Transient:std::snprintf (b, sizeof b, "attack up to +%.1f dB", s.p[0] * a); break;
        case Kind::Parallel: std::snprintf (b, sizeof b, "10:1 crushed copy blended in, %d%%", (int) (a * 70.0f)); break;
        case Kind::Double:   std::snprintf (b, sizeof b, "two voices, 14 and 21 ms, %d%%", (int) (a * 100.0f)); break;
        case Kind::Width:    std::snprintf (b, sizeof b, "side +%d%%", (int) (a * s.p[0] * 100.0f)); break;
        case Kind::Echo:     std::snprintf (b, sizeof b, "%.0f ms, feedback %d%%, mix %d%%", s.p[0], (int) (s.p[1] * 100.0f), (int) (a * 50.0f)); break;
        case Kind::Reverb:   std::snprintf (b, sizeof b, "%s %.1f s", fm::modeName ((int) s.p[0]), reverbSeconds (s, reverbLength)); break;
        case Kind::Loudness: std::snprintf (b, sizeof b, "+%.1f dB into the limiter", s.p[0] * a); break;
    }
    return b;
}

//------------------------------------------------------------------------------
struct Ctx
{
    float sr = 48000.0f;
    float reverbLength = 0.5f;
    float pushDb = 0.0f;     // set by LOUDNESS
    float grMax = 0.0f;      // largest gain reduction this chunk, for the meter
};

class Tool
{
public:
    explicit Tool (const ToolSpec& s) : spec (s) {}
    virtual ~Tool() = default;
    virtual void prepare (float sampleRate) { sr = sampleRate; reset(); }
    virtual void reset() {}
    // a = smoothed knob 0..1. Called every chunk, even at 0, so tails can finish.
    virtual void process (float* L, float* R, int n, float a, Ctx& c) = 0;
    const ToolSpec& spec;

protected:
    float sr = 48000.0f;
};

// 18 dB/oct low cut: one pole + Butterworth biquad.
struct LowCutStage
{
    Biquad hp; float p1[2] = {};
    void reset() { hp.reset(); p1[0] = p1[1] = 0; }
    void run (float* L, float* R, int n, float hz, float sr)
    {
        hp.highpass (hz, 1.0f, sr);
        const float c = fm::coefFor (hz, sr);
        for (int i = 0; i < n; ++i)
        {
            p1[0] += c * (L[i] - p1[0]); L[i] = hp.process (L[i] - p1[0], 0);
            p1[1] += c * (R[i] - p1[1]); R[i] = hp.process (R[i] - p1[1], 1);
        }
    }
};

class LowCutTool final : public Tool
{
public:
    using Tool::Tool;
    void reset() override { lc.reset(); }
    void process (float* L, float* R, int n, float a, Ctx&) override
    {
        if (a > 0.003f) lc.run (L, R, n, lowCutHz (spec.p[0], a), sr); else lc.reset();
    }
private:
    LowCutStage lc;
};

class GateTool final : public Tool
{
public:
    using Tool::Tool;
    void reset() override { lc.reset(); env = 0; gain = 1; open = true; hold = 0; }
    void process (float* L, float* R, int n, float a, Ctx&) override
    {
        if (a <= 0.003f) { reset(); return; }
        if (spec.p[2] > 0.0f) lc.run (L, R, n, lowCutHz (spec.p[2], a), sr);
        const float thr = fromDb (-90.0f + (spec.p[0] + 90.0f) * a), floorG = fromDb (-spec.p[1] * a);
        const float eA = coefMs (0.3f, sr), eR = coefMs (25.0f, sr), gA = coefMs (1.0f, sr), gR = coefMs (150.0f, sr);
        const int holdN = (int) (0.08f * sr);
        for (int i = 0; i < n; ++i)
        {
            const float x = std::max (std::abs (L[i]), std::abs (R[i]));
            env += (x > env ? eA : eR) * (x - env);
            if (env >= thr) { open = true; hold = holdN; }
            else if (env < thr * 0.6f) { if (hold > 0) --hold; else open = false; }
            const float want = open ? 1.0f : floorG;
            gain += (want > gain ? gA : gR) * (want - gain);
            L[i] *= gain; R[i] *= gain;
        }
    }
private:
    LowCutStage lc;
    float env = 0, gain = 1; bool open = true; int hold = 0;
};

// Compresses only the band below (DynLow) or above (DynHigh) a split, so boom or harsh cymbals
// come down only when they jump out.
class DynBandTool final : public Tool
{
public:
    DynBandTool (const ToolSpec& s, bool lowBand) : Tool (s), low (lowBand) {}
    void reset() override { lc.reset(); sp[0] = sp[1] = 0; env = 0; red = 0; }
    void process (float* L, float* R, int n, float a, Ctx& c) override
    {
        if (a <= 0.003f) { reset(); return; }
        if (low && spec.p[2] > 0.0f) lc.run (L, R, n, lowCutHz (spec.p[2], a), sr);
        const float sc = fm::coefFor (spec.p[0], sr);
        const float thrDb = (low ? -14.0f : -20.0f) - 18.0f * a;
        const float eA = coefMs (2.0f, sr), eR = coefMs (60.0f, sr), rA = coefMs (5.0f, sr), rR = coefMs (120.0f, sr);
        const float maxRed = spec.p[1] * a;
        for (int i = 0; i < n; ++i)
        {
            sp[0] += sc * (L[i] - sp[0]); sp[1] += sc * (R[i] - sp[1]);
            const float b0 = low ? sp[0] : L[i] - sp[0], b1 = low ? sp[1] : R[i] - sp[1];
            const float x = std::max (std::abs (b0), std::abs (b1));
            env += (x > env ? eA : eR) * (x - env);
            const float want = std::clamp ((toDb (env) - thrDb) * 0.67f, 0.0f, maxRed);   // about 3:1
            red += (want > red ? rA : rR) * (want - red);
            const float k = 1.0f - fromDb (-red);
            L[i] -= k * b0; R[i] -= k * b1;
        }
        c.grMax = std::max (c.grMax, red);
    }
private:
    bool low;
    LowCutStage lc;
    float sp[2] = {}, env = 0, red = 0;
};

class EqTool final : public Tool
{
public:
    using Tool::Tool;
    void reset() override { for (auto& b : bq) b.reset(); }
    void process (float* L, float* R, int n, float a, Ctx&) override
    {
        if (a <= 0.003f) { reset(); return; }
        for (int k = 0; k < 3; ++k)
        {
            const Band& bd = spec.band[k];
            if (bd.kind == None) continue;
            const float g = bd.db * a;
            if (bd.kind == Bell)           bq[k].peak (bd.hz, bd.q * (0.8f + 0.4f * std::abs (g) / 12.0f), g, sr);
            else if (bd.kind == LowShelf)  bq[k].lowShelf (bd.hz, g, sr, 0.6f);
            else                           bq[k].highShelf (bd.hz, g, sr, 0.6f);
            for (int i = 0; i < n; ++i) { L[i] = bq[k].process (L[i], 0); R[i] = bq[k].process (R[i], 1); }
        }
    }
private:
    Biquad bq[3];
};

class DeEssTool final : public Tool
{
public:
    using Tool::Tool;
    void reset() override { bp.reset(); essEnv = fullEnv = red = 0; }
    void process (float* L, float* R, int n, float a, Ctx& c) override
    {
        if (a <= 0.003f) { reset(); return; }
        bp.bandpass (spec.p[0], 1.4f, sr);
        const float eA = coefMs (0.3f, sr), eR = coefMs (25.0f, sr), rA = coefMs (0.5f, sr), rR = coefMs (50.0f, sr);
        const float maxDb = spec.p[1] * a;
        for (int i = 0; i < n; ++i)
        {
            const float b0 = bp.process (L[i], 0), b1 = bp.process (R[i], 1);
            const float bA = std::max (std::abs (b0), std::abs (b1)), fA = std::max (std::abs (L[i]), std::abs (R[i]));
            essEnv += (bA > essEnv ? eA : eR) * (bA - essEnv);
            fullEnv += (fA > fullEnv ? eA : eR) * (fA - fullEnv);
            const float want = fullEnv > 0.002f ? std::clamp (toDb (essEnv / fullEnv) + 12.0f, 0.0f, maxDb) : 0.0f;
            red += (want > red ? rA : rR) * (want - red);
            const float k = 1.0f - fromDb (-red);
            L[i] -= k * b0; R[i] -= k * b1;
        }
        c.grMax = std::max (c.grMax, red);
    }
private:
    Biquad bp;
    float essEnv = 0, fullEnv = 0, red = 0;
};

class SaturateTool final : public Tool
{
public:
    using Tool::Tool;
    void reset() override { for (int ch = 0; ch < 2; ++ch) loS[ch] = hiS[ch] = dc[ch] = 0; }
    void process (float* L, float* R, int n, float a, Ctx&) override
    {
        if (a <= 0.003f) { reset(); return; }
        const bool band = spec.p[1] > 0.0f, lowOnly = ! band && spec.p[2] > 0.0f;
        const float cLo = band ? fm::coefFor (spec.p[1], sr) : 0.0f;
        const float cHi = (band || lowOnly) ? fm::coefFor (spec.p[2], sr) : 0.0f;
        const float drive = 1.0f + spec.p[0] * a;
        const float mix = std::min (1.0f, a * 4.0f), even = 0.08f * spec.p[3] * a, dcC = fm::coefFor (5.0f, sr);
        float* io[2] = { L, R };
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < n; ++i)
            {
                float x = io[ch][i], part = x;
                if (band || lowOnly) { hiS[ch] += cHi * (x - hiS[ch]); part = hiS[ch]; }
                if (band) { loS[ch] += cLo * (x - loS[ch]); part = hiS[ch] - loS[ch]; }
                float y;
                if (lowOnly)   y = x + mix * (softCube (part * drive) / drive - part);                    // iron on the lows
                else if (band) y = x + mix * (std::tanh (part * drive) / std::sqrt (drive) * 1.5f);      // grit added in a band
                else           y = x + mix * (std::tanh (x * drive) * (1.2f / std::sqrt (drive)) - x);    // whole-signal drive
                y += even * y * y;
                dc[ch] += dcC * (y - dc[ch]);
                io[ch][i] = y - dc[ch];
            }
    }
private:
    float loS[2] = {}, hiS[2] = {}, dc[2] = {};
};

// Shared compressor core: soft knee, two release times, optional side-chain low cut and colour.
struct CompCore
{
    float ms = 0, grFast = 0, grSlow = 0, scHp[2] = {};
    void reset() { ms = grFast = grSlow = 0; scHp[0] = scHp[1] = 0; }
    static float curve (float overDb, float slope, float knee)
    {
        if (overDb > knee * 0.5f) return overDb * slope;
        if (overDb > -knee * 0.5f) { const float u = overDb + knee * 0.5f; return slope * u * u / (2.0f * knee); }
        return 0.0f;
    }
};

class CompTool final : public Tool
{
public:
    using Tool::Tool;
    void reset() override { cc.reset(); dc[0] = dc[1] = 0; }
    void process (float* L, float* R, int n, float a, Ctx& c) override
    {
        if (a <= 0.003f) { reset(); return; }
        const float ratio = 1.0f + (spec.p[0] - 1.0f) * std::min (1.0f, a * 2.5f);
        const float slope = 1.0f - 1.0f / ratio, knee = spec.p[5], thr = spec.p[4] * a;
        const float cA = coefMs (spec.p[1], sr), cRF = coefMs (spec.p[2], sr);
        const float cAS = coefMs (spec.p[1] * 8.0f + 40.0f, sr), cRS = coefMs (spec.p[3], sr);
        const float msC = coefMs (8.0f, sr), scC = spec.x[0] > 0.0f ? fm::coefFor (spec.x[0], sr) : 0.0f;
        const bool rms = spec.x[3] > 0.5f;
        const float slowShare = spec.x[4], dcC = fm::coefFor (5.0f, sr);
        const float makeup = fromDb (0.75f * CompCore::curve (-14.0f - thr, slope, knee));
        float grMax = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            float s0 = L[i], s1 = R[i];
            if (scC > 0.0f) { cc.scHp[0] += scC * (s0 - cc.scHp[0]); s0 -= cc.scHp[0]; cc.scHp[1] += scC * (s1 - cc.scHp[1]); s1 -= cc.scHp[1]; }
            float lvl = std::max (std::abs (s0), std::abs (s1));
            if (rms) { cc.ms += msC * (lvl * lvl - cc.ms); lvl = std::sqrt (cc.ms) * 1.414f; }
            const float gr = CompCore::curve (toDb (lvl) - thr, slope, knee);
            cc.grFast += (gr > cc.grFast ? cA : cRF) * (gr - cc.grFast);
            cc.grSlow += (gr > cc.grSlow ? cAS : cRS) * (gr - cc.grSlow);
            const float red = std::max (cc.grFast, cc.grSlow * slowShare);
            const float g = fromDb (-red) * makeup;
            const float work = std::min (1.0f, red / 10.0f);
            const float c2 = 0.10f * spec.x[1] * work, c3 = 0.35f * spec.x[2] * work;
            float* io[2] = { L, R };
            for (int ch = 0; ch < 2; ++ch)
            {
                float y = io[ch][i] * g;
                if (c2 > 0.0f) y += c2 * y * y;
                if (c3 > 0.0f) { const float yc = std::clamp (y, -1.2f, 1.2f); y -= c3 * yc * yc * yc; }
                if (spec.x[1] > 0.0f) { dc[ch] += dcC * (y - dc[ch]); y -= dc[ch]; }
                io[ch][i] = y;
            }
            grMax = std::max (grMax, red);
        }
        c.grMax = std::max (c.grMax, grMax);
    }
private:
    CompCore cc;
    float dc[2] = {};
};

// Emphasises the start of each note: fast envelope minus slow envelope.
class TransientTool final : public Tool
{
public:
    using Tool::Tool;
    void reset() override { fast = slow = 0; gDb = 0; }
    void process (float* L, float* R, int n, float a, Ctx&) override
    {
        if (a <= 0.003f) { reset(); return; }
        const float fA = coefMs (0.5f, sr), fR = coefMs (40.0f, sr), sA = coefMs (25.0f, sr), sR = coefMs (40.0f, sr), gS = coefMs (1.0f, sr);
        const float maxDb = spec.p[0] * a;
        for (int i = 0; i < n; ++i)
        {
            const float x = std::max (std::abs (L[i]), std::abs (R[i]));
            fast += (x > fast ? fA : fR) * (x - fast);
            slow += (x > slow ? sA : sR) * (x - slow);
            const float diff = std::clamp (toDb (fast + 1e-6f) - toDb (slow + 1e-6f), 0.0f, 12.0f);
            gDb += gS * (diff / 12.0f * maxDb - gDb);
            const float g = fromDb (gDb);
            L[i] *= g; R[i] *= g;
        }
    }
private:
    float fast = 0, slow = 0, gDb = 0;
};

// New York style: a heavily crushed copy blended under the original.
class ParallelTool final : public Tool
{
public:
    using Tool::Tool;
    void reset() override { env = 0; }
    void process (float* L, float* R, int n, float a, Ctx& c) override
    {
        if (a <= 0.003f) { reset(); return; }
        const float cA = coefMs (1.0f, sr), cR = coefMs (60.0f, sr);
        const float mix = 0.7f * a, norm = 1.0f / (1.0f + 0.35f * a);
        float grMax = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            const float x = std::max (std::abs (L[i]), std::abs (R[i]));
            const float gr = CompCore::curve (toDb (x) + 32.0f, 0.9f, 4.0f);
            env += (gr > env ? cA : cR) * (gr - env);
            const float g = fromDb (-env + 14.0f);
            const float cl = std::tanh (L[i] * g * 1.2f) / 1.2f, cr = std::tanh (R[i] * g * 1.2f) / 1.2f;
            L[i] = (L[i] + mix * cl) * norm; R[i] = (R[i] + mix * cr) * norm;
            grMax = std::max (grMax, env);
        }
        c.grMax = std::max (c.grMax, grMax * a);
    }
private:
    float env = 0;
};

class DoubleTool final : public Tool
{
public:
    using Tool::Tool;
    void prepare (float s) override { sr = s; for (auto& d : dl) d.resize ((int) (0.05f * sr) + 8); reset(); }
    void reset() override { for (auto& d : dl) d.clear(); ph = 0; lp[0] = lp[1] = 0; }
    void process (float* L, float* R, int n, float a, Ctx&) override
    {
        if (a <= 0.003f) { if (ph > 0.0f) reset(); return; }
        const float lpC = fm::coefFor (7000.0f, sr), mix = 0.5f * a * spec.p[0];
        for (int i = 0; i < n; ++i)
        {
            const float m = 0.5f * (L[i] + R[i]);
            dl[0].push (m); dl[1].push (m);
            ph += 1.0f / sr; if (ph > 1000.0f) ph -= 1000.0f;
            const float d0 = (14.0f + 2.0f * std::sin (2.0f * fm::kPi * 0.31f * ph)) * 0.001f * sr;
            const float d1 = (21.0f + 2.5f * std::sin (2.0f * fm::kPi * 0.23f * ph + 1.3f)) * 0.001f * sr;
            lp[0] += lpC * (dl[0].read (d0) - lp[0]); lp[1] += lpC * (dl[1].read (d1) - lp[1]);
            L[i] += mix * lp[0]; R[i] += mix * lp[1];
        }
    }
private:
    fm::DelayLine dl[2];
    float ph = 0, lp[2] = {};
};

// Wider stereo without harming mono: side boost plus a decorrelated side signal made from the middle.
class WidthTool final : public Tool
{
public:
    using Tool::Tool;
    void prepare (float s) override { sr = s; dl.resize ((int) (0.02f * sr) + 8); reset(); }
    void reset() override { dl.clear(); }
    void process (float* L, float* R, int n, float a, Ctx&) override
    {
        if (a <= 0.003f) return;
        const float w = a * spec.p[0];
        const float d1 = 0.007f * sr, d2 = 0.011f * sr;
        for (int i = 0; i < n; ++i)
        {
            const float m = 0.5f * (L[i] + R[i]), s = 0.5f * (L[i] - R[i]);
            dl.push (m);
            const float s2 = s * (1.0f + w) + 0.3f * w * (dl.read (d1) - dl.read (d2));
            L[i] = m + s2; R[i] = m - s2;
        }
    }
private:
    fm::DelayLine dl;
};

// Ping-pong echo with darkening repeats.
class EchoTool final : public Tool
{
public:
    using Tool::Tool;
    void prepare (float s) override { sr = s; for (auto& d : dl) d.resize ((int) (1.0f * sr) + 8); reset(); }
    void reset() override { for (auto& d : dl) d.clear(); for (int c = 0; c < 2; ++c) lp[c] = hp[c] = 0; tail = 0; }
    void process (float* L, float* R, int n, float a, Ctx&) override
    {
        if (a <= 0.003f && tail <= 0) return;
        const float d = spec.p[0] * 0.001f * sr, fb = spec.p[1];
        const float lpC = fm::coefFor (4000.0f, sr), hpC = fm::coefFor (200.0f, sr);
        float pk = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            float e0 = dl[0].read (d), e1 = dl[1].read (d);
            lp[0] += lpC * (e0 - lp[0]); hp[0] += hpC * (lp[0] - hp[0]); e0 = lp[0] - hp[0];
            lp[1] += lpC * (e1 - lp[1]); hp[1] += hpC * (lp[1] - hp[1]); e1 = lp[1] - hp[1];
            const float in = a > 0.003f ? 0.5f * (L[i] + R[i]) : 0.0f;
            dl[0].push (in + fb * e1);      // left feeds right, right feeds left
            dl[1].push (fb * e0);
            const float m2 = 0.5f * std::max (a, lastA); L[i] += m2 * e0; R[i] += m2 * e1;
            pk = std::max (pk, std::abs (e0) + std::abs (e1));
        }
        if (a > 0.003f) { tail = (int) (sr * 6.0f); lastA = a; } else tail = pk > 1e-5f ? std::max (0, tail - n) : 0;
    }
private:
    fm::DelayLine dl[2];
    float lp[2] = {}, hp[2] = {};
    int tail = 0;
    float lastA = 0;
};

// Firmament with a filtered send, optionally ducked under the dry signal.
class ReverbTool final : public Tool
{
public:
    using Tool::Tool;
    void prepare (float s) override { sr = s; rev.prepare (s); reset(); }
    void reset() override { sh[0] = sh[1] = sl2[0] = sl2[1] = 0; duckEnv = 0; duckGain = 1; tail = 0; }
    void process (float* L, float* R, int n, float a, Ctx& c) override
    {
        if (a <= 0.002f && tail <= 0) return;
        float wL[64], wR[64];
        const float hpC = fm::coefFor (spec.x[0], sr), lpC = fm::coefFor (spec.x[1], sr);
        float dryPeak = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            dryPeak = std::max (dryPeak, std::max (std::abs (L[i]), std::abs (R[i])));
            sh[0] += hpC * (L[i] - sh[0]); sh[1] += hpC * (R[i] - sh[1]);
            sl2[0] += lpC * ((L[i] - sh[0]) - sl2[0]); sl2[1] += lpC * ((R[i] - sh[1]) - sl2[1]);
            wL[i] = sl2[0]; wR[i] = sl2[1];
        }
        const int mode = (int) spec.p[0];
        const auto& m = fm::modeCfg (mode);
        fm::Params rp;
        rp.mode = mode;
        rp.decay = std::clamp (std::log (reverbSeconds (spec, c.reverbLength) / m.decayLo) / std::log (m.decayHi / m.decayLo), 0.0f, 1.0f);
        rp.character = spec.p[5]; rp.preDelayMs = spec.p[4]; rp.tone = 0.6f; rp.mod = 0.25f; rp.mix = 1.0f;
        rev.setParams (rp);
        rev.process (wL, wR, n);
        const float dA = 1.0f - std::exp (-(float) n / (0.010f * sr)), dR = 1.0f - std::exp (-(float) n / (0.30f * sr));
        duckEnv += (dryPeak > duckEnv ? dA : dR) * (dryPeak - duckEnv);
        const float target = fromDb (-spec.p[2] * std::min (1.0f, duckEnv / 0.12f));
        const float wet = a * spec.p[3] * 1.6f, step = (target - duckGain) / (float) n;
        float pk = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            duckGain += step;
            L[i] += wL[i] * wet * duckGain; R[i] += wR[i] * wet * duckGain;
            pk = std::max (pk, std::abs (wL[i]) + std::abs (wR[i]));
        }
        if (a > 0.002f) tail = (int) (sr * 2.0f); else if (pk > 1e-5f) tail = std::max (tail, n + 1);
        tail = std::max (0, tail - n);
    }
private:
    fm::ReverbEngine rev;
    float sh[2] = {}, sl2[2] = {}, duckEnv = 0, duckGain = 1;
    int tail = 0;
};

class LoudnessTool final : public Tool
{
public:
    using Tool::Tool;
    void process (float*, float*, int, float a, Ctx& c) override { c.pushDb = spec.p[0] * a; }
};

inline std::unique_ptr<Tool> makeTool (const ToolSpec& s)
{
    switch (s.kind)
    {
        case Kind::LowCut:    return std::make_unique<LowCutTool> (s);
        case Kind::Gate:      return std::make_unique<GateTool> (s);
        case Kind::DynLow:    return std::make_unique<DynBandTool> (s, true);
        case Kind::DynHigh:   return std::make_unique<DynBandTool> (s, false);
        case Kind::Eq:        return std::make_unique<EqTool> (s);
        case Kind::DeEss:     return std::make_unique<DeEssTool> (s);
        case Kind::Saturate:  return std::make_unique<SaturateTool> (s);
        case Kind::Comp:      return std::make_unique<CompTool> (s);
        case Kind::Transient: return std::make_unique<TransientTool> (s);
        case Kind::Parallel:  return std::make_unique<ParallelTool> (s);
        case Kind::Double:    return std::make_unique<DoubleTool> (s);
        case Kind::Width:     return std::make_unique<WidthTool> (s);
        case Kind::Echo:      return std::make_unique<EchoTool> (s);
        case Kind::Reverb:    return std::make_unique<ReverbTool> (s);
        case Kind::Loudness:  return std::make_unique<LoudnessTool> (s);
    }
    return nullptr;
}

//------------------------------------------------------------------------------
class ChannelEngine
{
public:
    static constexpr int kChunk = 32;
    static constexpr float kCeiling = 0.891f;   // -1 dBFS

    ChannelEngine()
    {
        for (int s = 0; s < kNumSources; ++s)
        {
            for (int t = 0; t < kTools; ++t)
            {
                tools[s][t] = makeTool (toolSpec (s, t));
                order[s][t] = t;
            }
            // process in signal order (clean-up, tone, dynamics, space), whatever the knob order
            std::stable_sort (order[s], order[s] + kTools, [s] (int x, int y) { return (int) toolSpec (s, x).kind < (int) toolSpec (s, y).kind; });
        }
    }

    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        for (auto& row : tools) for (auto& t : row) t->prepare (sr);
        for (auto& row : sm) for (auto& v : row) v = 0.0f;
        volSm = 0.0f; first = true; lastSource = -1;
        eqLastMode = -1;
        limEnvFast = limEnvSlow = 0;
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
        const int src = std::clamp (p.source, 0, kNumSources - 1);
        const float k = 1.0f - std::exp (-(float) n / (0.03f * sr));   // 30 ms knob smoothing
        if (src != lastSource)
        {
            // a new source starts clean, with its knobs where they were left
            for (int t = 0; t < kTools; ++t) { tools[src][t]->reset(); sm[src][t] = std::clamp (p.tool[src][t], 0.0f, 1.0f); }
            lastSource = src;
        }
        for (int t = 0; t < kTools; ++t) sm[src][t] += k * (std::clamp (p.tool[src][t], 0.0f, 1.0f) - sm[src][t]);
        if (first) { volSm = std::clamp (p.volumeDb, -24.0f, 12.0f); first = false; }
        volSm += k * (std::clamp (p.volumeDb, -24.0f, 12.0f) - volSm);

        if (p.mono)
            for (int i = 0; i < n; ++i) { const float m = 0.5f * (L[i] + R[i]); L[i] = m; R[i] = m; }

        // ---- graphic EQ ---------------------------------------------------------------
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

        // ---- this source's six tools, in signal order ------------------------------------
        Ctx c; c.sr = sr; c.reverbLength = p.reverbLength;
        for (int j = 0; j < kTools; ++j)
        {
            const int t = order[src][j];
            tools[src][t]->process (L, R, n, sm[src][t], c);
        }

        if (p.eqPost) applyEq();

        if (p.mono)   // reverbs, doubler and width are stereo, so fold again
            for (int i = 0; i < n; ++i) { const float m = 0.5f * (L[i] + R[i]); L[i] = m; R[i] = m; }

        // ---- Volume + safety limiter (zero latency: smooth leveling plus soft clip) ------
        const float vol = fromDb (volSm + c.pushDb);
        const bool limOn = p.limiter || c.pushDb > 0.05f;
        const float lThr = 0.708f;
        const float lAtk = coefMs (0.4f, sr), lRelF = coefMs (60.0f, sr);
        const float lAtkS = coefMs (30.0f, sr), lRelS = coefMs (700.0f, sr);
        const float kneeStart = 0.70f, span = kCeiling - kneeStart;
        float peakOut = 0.0f, limMax = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            float l = L[i] * vol, r = R[i] * vol;
            if (limOn)
            {
                const float pk = std::max (std::abs (l), std::abs (r));
                const float over = pk > lThr ? pk / lThr - 1.0f : 0.0f;
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

        const float fall = std::exp (-(float) n / (0.25f * sr));
        meterOut.store   (std::max (peakOut, meterOut.load (std::memory_order_relaxed) * fall), std::memory_order_relaxed);
        meterGr.store    (std::max (c.grMax, meterGr.load (std::memory_order_relaxed) * fall), std::memory_order_relaxed);
        meterLimit.store (std::max (std::min (1.0f, limMax * 8.0f), meterLimit.load (std::memory_order_relaxed) * fall), std::memory_order_relaxed);
    }

    Params p;
    float sr = 48000.0f;
    bool first = true;
    int lastSource = -1;

    std::unique_ptr<Tool> tools[kNumSources][kTools];
    int order[kNumSources][kTools] = {};
    float sm[kNumSources][kTools] = {};
    float volSm = 0.0f;

    Biquad eq[kMaxEqBands];
    float eqSm[kMaxEqBands] = {}, eqCoef[kMaxEqBands] = {};
    int eqLastMode = -1;
    float limEnvFast = 0, limEnvSlow = 0;

    std::atomic<float> meterOut { 0.0f }, meterGr { 0.0f }, meterLimit { 0.0f };
};

} // namespace sl
