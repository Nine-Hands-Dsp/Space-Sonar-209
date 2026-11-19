#pragma once
#include <cmath>
#include <cstdint>
#include <vector>
#include <algorithm>

//==============================================================================
//  Space Sonar 209 (NINE HANDS | DSP) : moteur d'écho à bande + réverbe à ressorts
//  d'après la Roland RE-201 Space Echo (1974)
//
//  - préamplis d'entrée MIC / P.A. / INST (le son sec passe aussi par le préampli, comme sur l'appareil)
//  - bande sans fin : 1 tête d'enregistrement, 3 têtes de lecture espacées régulièrement (1 x, 2 x, 3 x)
//  - vitesse de bande = REPEAT RATE (69 à 177 ms sur la tête 1 en NORMAL), inertie du moteur (MOTOR TORQUE) :
//    tourner REPEAT RATE fait glisser la hauteur des répétitions
//  - boucle : saturation de la bande, perte d'aigus liée à la vitesse et à l'usure (CONDITION), bosse de tête,
//    BASS / TREBLE dans la boucle (les répétitions s'assombrissent), auto-oscillation avec INTENSITY
//  - wow & flutter, LFO de vitesse de bande (6 formes), souffle, pertes de niveau d'une bande usée
//  - réverbe à ressorts (3 ressorts dispersifs), en parallèle sur le signal d'entrée (modes 5 à 12)
//==============================================================================
namespace ss
{
    constexpr double kPi = 3.14159265358979323846;
    inline double dbToGain (double d) { return std::pow (10.0, d / 20.0); }

    //------------------------------------------------------------------ filtre à variables d'état (TPT) : stable sous modulation
    struct Svf
    {
        double g = 0, k = 1.414, a1 = 0, a2 = 0, a3 = 0, m0 = 0, m1 = 0, m2 = 1, ic1 = 0, ic2 = 0;
        void reset() { ic1 = ic2 = 0; }
        void coef (double fs, double f, double q, double gScale = 1.0)
        {
            f = std::clamp (f, 5.0, 0.49 * fs);
            g = std::tan (kPi * f / fs) * gScale; k = 1.0 / q;
            a1 = 1.0 / (1.0 + g * (g + k)); a2 = g * a1; a3 = g * a2;
        }
        void lp (double fs, double f, double q) { coef (fs, f, q); m0 = 0; m1 = 0; m2 = 1; }
        void hp (double fs, double f, double q) { coef (fs, f, q); m0 = 1; m1 = -k; m2 = -1; }
        void bell (double fs, double f, double q, double db)
        {
            const double A = std::pow (10.0, db / 40.0);
            f = std::clamp (f, 5.0, 0.49 * fs);
            g = std::tan (kPi * f / fs); k = 1.0 / (q * A);
            a1 = 1.0 / (1.0 + g * (g + k)); a2 = g * a1; a3 = g * a2;
            m0 = 1; m1 = k * (A * A - 1.0); m2 = 0;
        }
        void lowShelf (double fs, double f, double db)
        {
            const double A = std::pow (10.0, db / 40.0);
            coef (fs, f, 0.707, 1.0 / std::sqrt (A));
            m0 = 1; m1 = k * (A - 1.0); m2 = A * A - 1.0;
        }
        void highShelf (double fs, double f, double db)
        {
            const double A = std::pow (10.0, db / 40.0);
            coef (fs, f, 0.707, std::sqrt (A));
            m0 = A * A; m1 = k * (1.0 - A) * A; m2 = 1.0 - A * A;
        }
        inline double process (double v0)
        {
            const double v3 = v0 - ic2;
            const double v1 = a1 * ic1 + a2 * v3;
            const double v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2.0 * v1 - ic1; ic2 = 2.0 * v2 - ic2;
            return m0 * v0 + m1 * v1 + m2 * v2;
        }
    };

    //------------------------------------------------------------------ tanh avec anti-repliement (ADAA d'ordre 1)
    struct AdaaTanh
    {
        double x1 = 0, F1 = 0;
        static double logcosh (double x) { const double a = std::abs (x); return a + std::log1p (std::exp (-2.0 * a)) - 0.69314718055994531; }
        inline double process (double x)
        {
            const double F = logcosh (x), d = x - x1;
            const double y = std::abs (d) < 1.0e-6 ? std::tanh (0.5 * (x + x1)) : (F - F1) / d;
            x1 = x; F1 = F;
            return y;
        }
    };

    // saturation : y = sat (drive * (x + biais)) / drive, centrée (biais = harmoniques paires)
    struct Saturator
    {
        AdaaTanh t; double drive = 1, bias = 0, off = 0;
        void set (double d, double asym)
        {
            drive = std::max (0.05, d);
            if (asym != bias) { bias = asym; off = std::tanh (bias); reset(); }
        }
        void reset() { t.x1 = bias; t.F1 = AdaaTanh::logcosh (bias); }
        inline double process (double x) { return (t.process (drive * x + bias) - off) / drive; }
    };

    struct Noise
    {
        uint32_t s = 22222u;
        inline double white() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (double) s * (2.0 / 4294967296.0) - 1.0; }
    };

    // fluctuations lentes et aléatoires (wow, dérive) : bruit filtré deux fois
    struct Drift
    {
        Noise n; double a = 0, y1 = 0, y2 = 0;
        void set (double fs, double hz, uint32_t seed) { a = 1.0 - std::exp (-2.0 * kPi * hz / fs); n.s = seed; y1 = y2 = 0; }
        inline double next() { y1 += a * (n.white() * 1.8 - y1); y2 += a * (y1 - y2); return y2 * 3.0; }
    };

    //------------------------------------------------------------------ ligne de retard (interpolation d'Hermite)
    struct Line
    {
        std::vector<float> b; int size = 0, w = 0;
        void prepare (int n) { size = 1; while (size < n) size <<= 1; b.assign ((size_t) size, 0.0f); w = 0; }
        void clear() { std::fill (b.begin(), b.end(), 0.0f); w = 0; }
        inline void push (double x) { b[(size_t) w] = (float) x; w = (w + 1) & (size - 1); }
        inline double read (double d) const
        {
            d = std::clamp (d, 2.0, (double) size - 4.0);
            const double rp = (double) w - d;
            const double fl = std::floor (rp);
            const double t = rp - fl;
            const int i = (int) fl, M = size - 1;
            const double xm1 = b[(size_t) ((i - 1) & M)], x0 = b[(size_t) (i & M)], x1 = b[(size_t) ((i + 1) & M)], x2 = b[(size_t) ((i + 2) & M)];
            const double c1 = 0.5 * (x1 - xm1), c2 = xm1 - 2.5 * x0 + 2.0 * x1 - 0.5 * x2, c3 = 0.5 * (x2 - xm1) + 1.5 * (x0 - x1);
            return ((c3 * t + c2) * t + c1) * t + x0;
        }
    };

    //------------------------------------------------------------------ table des modes (têtes 1, 2, 3 ; réverbe)
    struct Mode { bool h[3]; bool reverb; bool echo; };
    inline const Mode& mode (int i)
    {
        static const Mode m[12] = {
            { { true,  false, false }, false, true },   // 1
            { { false, true,  false }, false, true },   // 2
            { { false, false, true  }, false, true },   // 3
            { { false, true,  true  }, false, true },   // 4
            { { true,  false, false }, true,  true },   // 5
            { { false, true,  false }, true,  true },   // 6
            { { false, false, true  }, true,  true },   // 7
            { { true,  true,  false }, true,  true },   // 8
            { { false, true,  true  }, true,  true },   // 9
            { { true,  false, true  }, true,  true },   // 10
            { { true,  true,  true  }, true,  true },   // 11
            { { false, false, false }, true,  false },  // 12 : REVERB ONLY
        };
        return m[std::clamp (i, 0, 11)];
    }

    // valeurs de notes de la synchro (en noires)
    inline int numNotes() { return 14; }
    inline const char* noteName (int i)
    {
        static const char* n[14] = { "1/32", "1/16T", "1/16", "1/16D", "1/8T", "1/8", "1/8D", "1/4T", "1/4", "1/4D", "1/2T", "1/2", "1/2D", "1/1" };
        return n[std::clamp (i, 0, 13)];
    }
    inline double noteBeats (int i)
    {
        static const double b[14] = { 0.125, 1.0 / 6.0, 0.25, 0.375, 1.0 / 3.0, 0.5, 0.75, 2.0 / 3.0, 1.0, 1.5, 4.0 / 3.0, 2.0, 3.0, 4.0 };
        return b[std::clamp (i, 0, 13)];
    }
    inline int noteIndexFromKnob (double k) { return (int) std::lround (std::clamp (k, 0.0, 10.0) / 10.0 * 13.0); }

    //------------------------------------------------------------------ réglages d'un bloc
    struct Params
    {
        int inputSel = 2;                    // 0 MIC, 1 P.A., 2 INST
        double volMic = 5, volPa = 5, volInst = 5;
        bool echoOn = true, echoCancel = false;
        int mode = 2;
        double bass = 5, treble = 5, reverbVol = 3.5, repeatRate = 5, intensity = 4.5, echoVol = 6;
        int outLevel = 1;                    // 0 H, 1 M, 2 L
        bool sync = false; double bpm = 120;
        bool dual = true;
        int rateRange = 0;                   // 0 NORMAL, 1 SLOW, 2 FAST
        double speed = 0;                    // -1 .. 1
        int lfoWave = 0; double lfoRate = 4, lfoShape = 3, lfoDepth = 0;
        double condition = 3, wf = 3, motor = 5;
        double lowCut = 20, highCut = 20000, masterDb = 0;
    };

    // temps de la tête 1 (secondes) d'après les réglages
    inline int leadingHead (int m)
    {
        const auto& M = mode (m);
        for (int h = 0; h < 3; ++h) if (M.h[h]) return h + 1;
        return 1;
    }
    inline double head1Seconds (const Params& p)
    {
        if (p.sync)
        {
            const double bpm = std::clamp (p.bpm, 20.0, 400.0);
            const double sec = 60.0 / bpm * noteBeats (noteIndexFromKnob (p.repeatRate));
            return std::clamp (sec / (double) leadingHead (p.mode), 0.012, 0.95);
        }
        double d = 0.177 * std::pow (69.0 / 177.0, std::clamp (p.repeatRate, 0.0, 10.0) / 10.0);   // 177 -> 69 ms
        d *= p.rateRange == 1 ? 2.0 : p.rateRange == 2 ? 0.5 : 1.0;
        d *= std::pow (2.0, -0.4 * std::clamp (p.speed, -1.0, 1.0));
        return std::clamp (d, 0.012, 0.95);
    }

    //==========================================================================
    //  Réverbe à ressorts : 3 ressorts dispersifs (chaînes de passe-tout), boucle sombre, ~2,2 s
    //==========================================================================
    struct Spring
    {
        struct Ap { double a = 0, z = 0; inline double process (double x) { const double y = a * x + z; z = x - a * y; return y; } };
        static constexpr int kIn = 8, kLoop = 10;
        Ap in[kIn], loop[kLoop];
        Line line; Svf lp, hp; Drift wob;
        double D = 0.035, g = 0.85, fbState = 0, fs = 48000;
        void prepare (double sr, double delaySec, double t60, uint32_t seed, double apCoef)
        {
            fs = sr; D = delaySec;
            line.prepare ((int) (fs * (D * 1.1 + 0.02)) + 16);
            g = std::pow (10.0, -3.0 * D / t60);
            for (auto& a : in) { a.a = apCoef; a.z = 0; }
            for (auto& a : loop) { a.a = apCoef * 0.9; a.z = 0; }
            lp.lp (fs, 3600.0, 0.6); hp.hp (fs, 140.0, 0.7);
            wob.set (fs, 0.7, seed);
            fbState = 0;
        }
        void reset() { line.clear(); lp.reset(); hp.reset(); fbState = 0; for (auto& a : in) a.z = 0; for (auto& a : loop) a.z = 0; }
        inline double process (double x)
        {
            for (auto& a : in) x = a.process (x);
            line.push (x + g * fbState);
            double y = line.read (D * fs * (1.0 + 0.0012 * wob.next()));
            double f = y;
            for (auto& a : loop) f = a.process (f);
            fbState = hp.process (lp.process (f));
            return y;
        }
    };

    struct SpringTank
    {
        Spring s[3]; Svf inLp, outHp, outLp, presence; Saturator drive;
        void prepare (double fs, uint32_t seed, double spread)
        {
            s[0].prepare (fs, 0.0331 * spread, 2.3, seed + 1u, 0.62);
            s[1].prepare (fs, 0.0397 * spread, 2.1, seed + 2u, 0.58);
            s[2].prepare (fs, 0.0463 * spread, 2.4, seed + 3u, 0.66);
            inLp.lp (fs, 4800.0, 0.707); outHp.hp (fs, 260.0, 0.7); outLp.lp (fs, 4600.0, 0.707); presence.bell (fs, 1900.0, 0.9, 3.0);
            drive.set (1.6, 0.0);
        }
        void reset() { for (auto& x : s) x.reset(); inLp.reset(); outHp.reset(); outLp.reset(); presence.reset(); drive.reset(); }
        inline double process (double x)
        {
            x = drive.process (inLp.process (x) * 1.2);                       // pilote du réservoir
            const double y = 0.55 * s[0].process (x) - 0.5 * s[1].process (x) + 0.5 * s[2].process (x);
            return presence.process (outLp.process (outHp.process (y))) * 0.9;
        }
    };

    // sortie : rigoureusement linéaire jusqu'à 0,85, puis genou doux jusqu'au plafond de 1,0 (0 dBFS)
    inline double outClip (double x)
    {
        const double a = std::abs (x);
        if (a <= 0.85) return x;
        const double y = 0.85 + 0.15 * std::tanh ((a - 0.85) / 0.15);
        return x < 0 ? -y : y;
    }

    //==========================================================================
    //  Un canal : préampli, bande, boucle, tonalité, réverbe, sorties
    //==========================================================================
    struct Channel
    {
        Line tape;
        Saturator pre, rec, echoAmp;
        Svf preHp, prePres, pbHp, pbLp1, pbLp2, bump, bass, treble, wetLc, wetHc;
        SpringTank spring;
        Noise noise; Drift wow, jitter, drop;
        double fb = 0, dropGain = 1, dcX = 0, dcY = 0;
        void reset()
        {
            tape.clear(); pre.reset(); rec.reset(); echoAmp.reset();
            for (auto* f : { &preHp, &prePres, &pbHp, &pbLp1, &pbLp2, &bump, &bass, &treble, &wetLc, &wetHc }) f->reset();
            spring.reset(); fb = 0; dropGain = 1; dcX = dcY = 0;
        }
    };

    class Engine
    {
    public:
        void prepare (double sampleRate)
        {
            fs = sampleRate;
            for (int c = 0; c < 2; ++c)
            {
                auto& C = ch[c];
                C.tape.prepare ((int) std::ceil (fs * 3.3) + 64);              // 3 x 0,95 s + modulation
                C.noise.s = 1234567u + 977u * (uint32_t) c;
                C.wow.set (fs, 0.9, 11u + 13u * (uint32_t) c);
                C.jitter.set (fs, 22.0, 37u + 7u * (uint32_t) c);
                C.drop.set (fs, 3.0, 91u + 5u * (uint32_t) c);
                C.spring.prepare (fs, 100u + 50u * (uint32_t) c, c == 0 ? 1.0 : 1.083);
                C.echoAmp.set (0.8, 0.02);
            }
            lfoNoise.s = 4242u;
            reset();
        }

        void reset()
        {
            for (auto& c : ch) c.reset();
            first = true; phWow = phFl = phLfo = 0; lfoHeld = 0; lfoSm = 0; lfoPrevPh = 0;
            echoGate = 1; revGate = 1;
        }

        // vitesse de bande (tête 1) actuellement lue, en secondes, pour l'affichage
        double currentHead1() const { return d1Cur; }
        double vuLevel = 0.0, peakHit = 0.0, lfoView = 0.0;

        void process (const float* inL, const float* inR, float* outL, float* outR, int n, const Params& p)
        {
            for (int s0 = 0; s0 < n; s0 += kSub)
            {
                const int m = std::min (kSub, n - s0);
                processChunk (inL + s0, inR + s0, outL + s0, outR + s0, m, p);
            }
        }

    private:
        static constexpr int kSub = 32;
        double fs = 48000.0;
        Channel ch[2];
        bool first = true;
        double spd1 = 1, spd2 = 1, d1Cur = 0.12;             // vitesse lissée (inertie du moteur), en 1 / s
        double phWow = 0, phFl = 0, phLfo = 0, lfoPrevPh = 0, lfoHeld = 0, lfoSm = 0;
        Noise lfoNoise;
        double echoGate = 1, revGate = 1;
        double gInSm = 1, gEchoSm = 0.5, gRevSm = 0.3, fbSm = 0.4, outSm = 1;
        int lastInput = -1;
        double lcSm = 20, hcSm = 20000;

        static double knob2 (double k, double top) { const double x = std::clamp (k, 0.0, 10.0) / 10.0; return top * x * x; }

        void setupChunk (const Params& p, double speedRel, double cond)
        {
            // préampli selon l'entrée choisie
            const int sel = std::clamp (p.inputSel, 0, 2);
            for (auto& C : ch)
            {
                if (sel != lastInput)
                {
                    if (sel == 0) C.pre.set (2.6, 0.16); else if (sel == 1) C.pre.set (1.1, 0.03); else C.pre.set (1.7, 0.08);
                }
                C.preHp.hp (fs, sel == 0 ? 70.0 : 35.0, 0.7);
                C.prePres.bell (fs, sel == 0 ? 2600.0 : 3200.0, 0.7, sel == 0 ? 2.0 : sel == 1 ? 0.3 : 1.0);
                // lecture : la bande plus lente perd des aigus, la bande usée aussi
                const double fc = std::clamp (5600.0 * std::pow (speedRel, 0.55) * (1.0 - 0.45 * cond), 1400.0, 13000.0);
                C.pbLp1.lp (fs, fc, 0.5412); C.pbLp2.lp (fs, fc * 1.1, 1.3066);
                C.pbHp.hp (fs, 85.0, 0.707);
                C.bump.bell (fs, std::clamp (115.0 * std::sqrt (speedRel), 60.0, 220.0), 0.9, 2.2);
                // BASS / TREBLE : pente continue des deux côtés de midi (aucune cassure)
                const double xb = (p.bass - 5.0) / 5.0, xt = (p.treble - 5.0) / 5.0;
                C.bass.lowShelf (fs, 160.0, 12.0 * xb);
                C.treble.highShelf (fs, 2800.0, 12.0 * xt);
                C.rec.set (1.25 + 0.9 * cond, 0.10);
                C.wetLc.hp (fs, lcSm, 0.707);
                C.wetHc.lp (fs, std::min (hcSm >= 19900.0 ? 0.45 * fs : hcSm, 0.45 * fs), 0.707);
            }
            lastInput = sel;
        }

        double lfoValue (const Params& p, double dt)
        {
            double hz;
            if (p.sync)
            {
                const double bpm = std::clamp (p.bpm, 20.0, 400.0);
                // RATE en synchro : de 4 rondes à la double-croche
                static const double beats[11] = { 16.0, 8.0, 6.0, 4.0, 3.0, 2.0, 1.5, 1.0, 0.75, 0.5, 0.25 };
                hz = bpm / 60.0 / beats[(int) std::lround (std::clamp (p.lfoRate, 0.0, 10.0))];
            }
            else hz = 0.05 * std::pow (200.0, std::clamp (p.lfoRate, 0.0, 10.0) / 10.0);   // 0,05 à 10 Hz
            phLfo += hz * dt;
            if (phLfo >= 1.0) { phLfo -= std::floor (phLfo); lfoHeld = lfoNoise.white(); }
            const double u = phLfo;
            double v;
            switch (std::clamp (p.lfoWave, 0, 5))
            {
                case 0:  v = std::sin (2.0 * kPi * u); break;
                case 1:  v = u < 0.5 ? 4.0 * u - 1.0 : 3.0 - 4.0 * u; break;
                case 2:  v = 1.0 - 2.0 * u; break;
                case 3:  v = 2.0 * u - 1.0; break;
                case 4:  v = u < 0.5 ? 1.0 : -1.0; break;
                default: v = lfoHeld; break;
            }
            // WAVEFORM : adoucit la forme (0 = arêtes vives, 10 = très arrondie)
            const double fc = hz * (1.0 + 40.0 * (1.0 - std::clamp (p.lfoShape, 0.0, 10.0) / 10.0));
            lfoSm += (v - lfoSm) * (1.0 - std::exp (-2.0 * kPi * fc * dt));
            return lfoSm;
        }

        void processChunk (const float* inL, const float* inR, float* outL, float* outR, int n, const Params& p)
        {
            const Mode& M = mode (p.mode);
            const double cond = std::clamp (p.condition, 0.0, 10.0) / 10.0;
            const double wf = std::clamp (p.wf, 0.0, 10.0) / 10.0;
            const double d1T = head1Seconds (p);
            const double spT = 1.0 / d1T;
            if (first) { spd1 = spd2 = spT; d1Cur = d1T; lcSm = p.lowCut; hcSm = p.highCut; first = false; }

            const double kS = 1.0 - std::exp (-(double) n / (0.025 * fs));
            lcSm = std::exp (std::log (lcSm) + (std::log (std::max (10.0, p.lowCut)) - std::log (lcSm)) * kS);
            hcSm = std::exp (std::log (hcSm) + (std::log (std::max (200.0, p.highCut)) - std::log (hcSm)) * kS);
            setupChunk (p, std::clamp (0.12 / d1Cur, 0.1, 10.0), cond);

            // inertie du moteur : 0,9 s (couple faible) à 40 ms (couple fort), deux pôles (démarrage en douceur)
            const double tau = 0.9 * std::pow (0.04 / 0.9, std::clamp (p.motor, 0.0, 10.0) / 10.0);
            const double aM = 1.0 - std::exp (-1.0 / (tau * 0.5 * fs));

            const double wowD = (0.0005 + 0.0032 * wf) * (1.0 + 1.3 * cond);
            const double flD  = (0.00015 + 0.0011 * wf) * (1.0 + 1.6 * cond);
            const double lfoD = 0.06 * std::clamp (p.lfoDepth, 0.0, 10.0) / 10.0;
            const double hiss = dbToGain (-80.0 + 16.0 * cond);
            const double dropAmt = cond > 0.5 ? (cond - 0.5) * 2.0 : 0.0;

            // gains (lissés)
            const double vol = p.inputSel == 0 ? p.volMic : p.inputSel == 1 ? p.volPa : p.volInst;
            const double gInT = std::pow (std::clamp (vol, 0.0, 10.0) / 5.0, 2.0) * (p.inputSel == 0 ? 1.6 : 1.0);
            const double gEchoT = knob2 (p.echoVol, 1.5);
            const double gRevT = knob2 (p.reverbVol, 1.6) * (M.reverb ? 1.0 : 0.0);
            const double fbT = 0.93 * std::pow (std::clamp (p.intensity, 0.0, 10.0) / 10.0, 1.15);
            const double outT = dbToGain (p.masterDb + (p.outLevel == 0 ? 6.0 : p.outLevel == 2 ? -6.0 : 0.0));
            const double echoT = (M.echo && ! p.echoCancel) ? 1.0 : 0.0;
            const double sendT = p.echoOn ? 1.0 : 0.0;
            const double smA = 1.0 - std::exp (-1.0 / (0.02 * fs));

            int nh = 0; double hr[3];
            for (int h = 0; h < 3; ++h) if (M.h[h]) hr[nh++] = (double) (h + 1);
            const double outNorm = nh > 0 ? 1.0 / std::sqrt ((double) nh) : 0.0;
            const double fbNorm = nh == 1 ? 1.0 : nh == 2 ? 0.55 : 0.39;      // auto-oscillation vers 8,5 (1 tete), 7,8 (2), 7,5 (3)
            const bool dual = p.dual;
            const double dt = 1.0 / fs;
            const double dcR = 1.0 - 2.0 * kPi * 5.0 / fs;
            double vuAcc = vuLevel, pk = peakHit;
            const double vuA = 1.0 - std::exp (-1.0 / (0.3 * fs));
            double sendSm = sendGate;

            for (int i = 0; i < n; ++i)
            {
                gInSm += (gInT - gInSm) * smA; gEchoSm += (gEchoT - gEchoSm) * smA; gRevSm += (gRevT - gRevSm) * smA;
                fbSm += (fbT - fbSm) * smA; outSm += (outT - outSm) * smA;
                echoGate += (echoT - echoGate) * smA; sendSm += (sendT - sendSm) * smA;

                // moteur
                spd1 += (spT - spd1) * aM; spd2 += (spd1 - spd2) * aM;
                d1Cur = 1.0 / spd2;
                // wow & flutter + LFO
                phWow += 0.63 * dt; if (phWow >= 1) phWow -= 1;
                phFl += 7.4 * dt; if (phFl >= 1) phFl -= 1;
                const double lfo = lfoD > 0.0 ? lfoValue (p, dt) : 0.0;
                lfoView = lfo * (lfoD > 0.0 ? 1.0 : 0.0);

                const double xl = inL[i], xr = inR[i];
                const int nc = dual ? 2 : 1;
                double y[2] = { 0, 0 };
                for (int c = 0; c < nc; ++c)
                {
                    auto& C = ch[c];
                    const double x = dual ? (c == 0 ? xl : xr) : 0.5 * (xl + xr);
                    // entrée + préampli
                    const double xin = C.preHp.process (x * gInSm);
                    vuAcc += (std::abs (xin) - vuAcc) * vuA;
                    pk = std::max (pk, std::abs (xin));
                    const double dry = C.prePres.process (C.pre.process (xin));

                    // modulation de la vitesse (fraction du temps)
                    const double off = c == 0 ? 0.0 : 0.21;
                    const double w = 0.6 * std::sin (2 * kPi * (phWow + off)) + 0.3 * std::sin (2 * kPi * (phWow * 1.63 + 0.31 + off)) + 0.5 * C.wow.next();
                    const double f = 0.7 * std::sin (2 * kPi * (phFl + off)) + 0.3 * std::sin (2 * kPi * (phFl * 1.77 + 0.2)) + 0.3 * C.jitter.next();
                    const double frac = wowD * w + flD * f - lfoD * lfo;
                    const double dS = d1Cur * fs * (1.0 + frac);

                    // lecture des têtes
                    double r = 0;
                    for (int h = 0; h < nh; ++h) r += C.tape.read (dS * hr[h]);
                    // une bande usée perd ponctuellement du niveau
                    if (dropAmt > 0.0)
                    {
                        const double dv = C.drop.next();
                        const double tgt = 1.0 - dropAmt * 0.35 * std::clamp (dv * 2.0 - 0.9, 0.0, 1.0);
                        C.dropGain += (tgt - C.dropGain) * 0.002;
                        r *= C.dropGain;
                    }
                    double e = C.pbHp.process (r);
                    e = C.bump.process (e);
                    e = C.pbLp2.process (C.pbLp1.process (e));
                    e = C.treble.process (C.bass.process (e));
                    // écriture : entrée + réinjection, saturation de la bande, souffle
                    double wr = dry * sendSm + e * fbNorm * fbSm;
                    wr = C.rec.process (wr) + C.noise.white() * hiss;
                    if (! std::isfinite (wr)) wr = 0.0;
                    C.tape.push (wr);

                    double wet = C.echoAmp.process (e * outNorm * gEchoSm) * echoGate;      // ampli d'écho : sature doucement en auto-oscillation
                    wet += C.spring.process (dry) * gRevSm;
                    // anti-continu, LOW CUT / HIGH CUT sur l'écho et la réverbe
                    const double dc = wet - C.dcX + dcR * C.dcY; C.dcX = wet; C.dcY = dc;
                    wet = C.wetHc.process (C.wetLc.process (dc));
                    y[c] = outClip ((dry + wet) * outSm);
                }
                if (! dual) y[1] = y[0];
                outL[i] = (float) y[0];
                outR[i] = (float) y[1];
            }
            sendGate = sendSm;
            vuLevel = vuAcc;
            peakHit = pk;
        }
        double sendGate = 1;
    };
}
