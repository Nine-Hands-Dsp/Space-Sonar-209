// Vérifications du moteur + captures de l'interface de Space Sonar 209 (hors plugin) : tools/render
#include "PluginEditor.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <random>
#include <iostream>
using namespace juce;

// guitare de test : cordes pincées (Karplus-Strong), notes détachées puis silences (on entend les répétitions)
struct TestGuitar
{
    double fs; std::mt19937 rng { 7 }; std::uniform_real_distribution<double> u { -1.0, 1.0 };
    std::vector<double> str; size_t pos = 0; double t = 0, next = 0; int note = 0;
    explicit TestGuitar (double f) : fs (f) {}
    void pluck (double hz) { str.assign ((size_t) std::max (8.0, fs / hz), 0.0); for (auto& v : str) v = u (rng) * 0.5; pos = 0; }
    float next1()
    {
        static const double notes[8] = { 98.0, 146.8, 196.0, 246.9, 329.6, 293.7, 246.9, 220.0 };
        if (t >= next) { pluck (notes[note++ % 8]); next += note % 4 == 0 ? 1.0 : 0.25; }
        double y = 0;
        if (! str.empty()) { const size_t i1 = (pos + 1) % str.size(); y = str[pos]; str[pos] = 0.996 * 0.5 * (str[pos] + str[i1]); pos = i1; }
        t += 1.0 / fs;
        return (float) (y * (t < 3.0 || std::fmod (t, 4.0) < 2.0 ? 1.0 : 0.0));
    }
};

static void setP (SpaceSonarProcessor& p, const char* id, float v) { auto* q = p.apvts.getParameter (id); q->setValueNotifyingHost (q->convertTo0to1 (v)); }

static void snap (Component& c, const String& name, float scale)
{
    auto img = c.createComponentSnapshot (c.getLocalBounds(), true, scale);
    File out = File::getCurrentWorkingDirectory().getChildFile (name);
    out.deleteFile();
    FileOutputStream os (out); PNGImageFormat png; png.writeImageToStream (img, os);
    std::cout << "ecrit " << out.getFullPathName() << " (" << img.getWidth() << "x" << img.getHeight() << ")\n";
}

// menu dessiné hors écran avec le LookAndFeel du plugin (en-tête + éléments)
static void snapMenu (SpaceSonarEditor& ed, const PopupMenu& m, const String& name)
{
    auto& lnf = ed.menuLnf;
    const int w = 420; int h = 14;
    PopupMenu::MenuItemIterator it (m);
    std::vector<std::pair<PopupMenu::Item, int>> items;
    while (it.next())
    {
        auto& item = it.getItem();
        int iw = 0, ih = 0;
        if (item.customComponent != nullptr) { item.customComponent->getIdealSize (iw, ih); }
        else lnf.getIdealPopupMenuItemSize (item.text, item.isSeparator, -1, iw, ih);
        items.push_back ({ item, ih }); h += ih;
    }
    Image img (Image::ARGB, w * 2, h * 2, true);
    Graphics g (img); g.addTransform (AffineTransform::scale (2.0f));
    lnf.drawPopupMenuBackground (g, w, h);
    int y = 7;
    for (auto& [item, ih] : items)
    {
        const Rectangle<int> r (7, y, w - 14, ih);
        if (item.customComponent != nullptr)
        {
            item.customComponent->setSize (r.getWidth(), r.getHeight());
            Graphics::ScopedSaveState ss (g); g.setOrigin (r.getPosition()); item.customComponent->paint (g);
        }
        else if (item.isSectionHeader) lnf.drawPopupMenuSectionHeader (g, r, item.text);
        else lnf.drawPopupMenuItem (g, r, item.isSeparator, item.isEnabled, false, item.isTicked, item.subMenu != nullptr, item.text, item.shortcutKeyDescription, item.image.get(), nullptr);
        y += ih;
    }
    File out = File::getCurrentWorkingDirectory().getChildFile (name);
    out.deleteFile(); FileOutputStream os (out); PNGImageFormat png; png.writeImageToStream (img, os);
    std::cout << "ecrit " << name << "\n";
}

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI gui;
    const double fs = 48000.0; const int B = 256;
    MidiBuffer midi;
    const bool analyse = argc > 1 && String (argv[1]) == "analyse";

    if (analyse)
    {
        //---------------------------------------------------------- 1. stabilité : intensité max, tous les modes, saut de vitesse
        std::cout << "== Stabilite (intensite 10, 20 s, saut de Repeat Rate) ==\n";
        for (int m = 0; m < 12; ++m)
        {
            SpaceSonarProcessor p; p.setPlayConfigDetails (2, 2, fs, B);
            setP (p, "mode", (float) m); setP (p, "intensity", 10.0f); setP (p, "echoVol", 10.0f); setP (p, "reverbVol", 10.0f);
            setP (p, "condition", 10.0f); setP (p, "wf", 10.0f); setP (p, "lfoDepth", 10.0f); setP (p, "bass", 10.0f);
            p.prepareToPlay (fs, B);
            AudioBuffer<float> buf (2, B); std::mt19937 r (3); std::normal_distribution<float> nd (0.0f, 0.3f);
            float worst = 0; bool nan = false;
            for (int k = 0; k < (int) (20 * fs / B); ++k)
            {
                for (int i = 0; i < B; ++i) { const float v = k < 40 ? nd (r) : 0.0f; buf.setSample (0, i, v); buf.setSample (1, i, v); }
                if (k == 600) setP (p, "repeatRate", 10.0f);
                if (k == 1200) setP (p, "repeatRate", 0.0f);
                p.processBlock (buf, midi);
                for (int c = 0; c < 2; ++c) for (int i = 0; i < B; ++i) { const float v = buf.getSample (c, i); if (! std::isfinite (v)) nan = true; worst = jmax (worst, std::abs (v)); }
            }
            std::cout << "  mode " << String (m + 1).paddedLeft (' ', 2) << "  crete max " << String (Decibels::gainToDecibels (worst), 1) << " dBFS" << (nan ? "  NaN !" : "") << "\n";
        }
        //---------------------------------------------------------- 2. temps des échos mesurés (impulsion, mode 1/2/3)
        std::cout << "== Temps de la tete 1 (Repeat Rate 0 / 5 / 10, NORMAL) ==\n";
        for (float rr : { 0.0f, 5.0f, 10.0f })
        {
            SpaceSonarProcessor p; p.setPlayConfigDetails (2, 2, fs, B);
            setP (p, "mode", 0.0f); setP (p, "repeatRate", rr); setP (p, "intensity", 0.0f); setP (p, "wf", 0.0f); setP (p, "condition", 0.0f);
            setP (p, "echoVol", 10.0f); setP (p, "stereo", 0.0f);
            p.prepareToPlay (fs, B);
            AudioBuffer<float> buf (2, B); int found = -1; long idx = 0; float best = 0;
            for (int k = 0; k < (int) (1.0 * fs / B); ++k)
            {
                buf.clear(); if (k == 0) { buf.setSample (0, 0, 0.5f); buf.setSample (1, 0, 0.5f); }
                p.processBlock (buf, midi);
                for (int i = 0; i < B; ++i, ++idx) if (idx > 100) { const float v = std::abs (buf.getSample (0, i)); if (v > best) { best = v; found = (int) idx; } }
            }
            std::cout << "  repeatRate " << rr << " -> " << String (found / fs * 1000.0, 1) << " ms\n";
        }
        //---------------------------------------------------------- 3. charge processeur
        std::cout << "== Charge processeur (48 kHz, Dual, mode 11, LFO, usure) ==\n";
        {
            SpaceSonarProcessor p; p.setPlayConfigDetails (2, 2, fs, B);
            setP (p, "mode", 10.0f); setP (p, "lfoDepth", 5.0f); setP (p, "condition", 8.0f); setP (p, "stereo", 1.0f);
            p.prepareToPlay (fs, B);
            TestGuitar gt (fs); AudioBuffer<float> buf (2, B);
            const double t0 = Time::getMillisecondCounterHiRes();
            for (int k = 0; k < (int) (10 * fs / B); ++k)
            {
                for (int i = 0; i < B; ++i) { const float v = gt.next1(); buf.setSample (0, i, v); buf.setSample (1, i, v); }
                p.processBlock (buf, midi);
            }
            std::cout << "  " << String ((Time::getMillisecondCounterHiRes() - t0) / 100.0, 2) << " % d'un coeur\n";
        }
        //---------------------------------------------------------- 4. bypass exact
        {
            SpaceSonarProcessor p; p.setPlayConfigDetails (2, 2, fs, B);
            setP (p, "power", 0.0f); p.prepareToPlay (fs, B);
            AudioBuffer<float> buf (2, B); double diff = 0;
            for (int k = 0; k < 400; ++k)
            {
                for (int i = 0; i < B; ++i) { const float v = (float) std::sin (0.05 * (k * B + i)); buf.setSample (0, i, v); buf.setSample (1, i, v); }
                AudioBuffer<float> ref; ref.makeCopyOf (buf);
                p.processBlock (buf, midi);
                if (k > 20) for (int i = 0; i < B; ++i) diff = jmax (diff, (double) std::abs (buf.getSample (0, i) - ref.getSample (0, i)));
            }
            std::cout << "Bypass : ecart max " << String (Decibels::gainToDecibels ((float) diff + 1e-12f), 1) << " dB\n";
        }
        //---------------------------------------------------------- 5. fichier audio de démonstration
        {
            SpaceSonarProcessor p; p.setPlayConfigDetails (2, 2, fs, B);
            p.loadFactoryPreset (3);
            p.prepareToPlay (fs, B);
            TestGuitar gt (fs); AudioBuffer<float> buf (2, B);
            AudioBuffer<float> all (2, (int) (12 * fs));
            for (int k = 0; k < (int) (12 * fs / B); ++k)
            {
                for (int i = 0; i < B; ++i) { const float v = gt.next1() * 0.5f; buf.setSample (0, i, v); buf.setSample (1, i, v); }
                if (k == (int) (6 * fs / B)) setP (p, "repeatRate", 8.0f);
                p.processBlock (buf, midi);
                for (int c = 0; c < 2; ++c) all.copyFrom (c, k * B, buf, c, 0, B);
            }
            File f = File::getCurrentWorkingDirectory().getChildFile ("demo_dub.wav"); f.deleteFile();
            WavAudioFormat wav;
            std::unique_ptr<AudioFormatWriter> w (wav.createWriterFor (new FileOutputStream (f), fs, 2, 24, {}, 0));
            if (w) w->writeFromAudioSampleBuffer (all, 0, all.getNumSamples());
            std::cout << "ecrit demo_dub.wav\n";
        }
        return 0;
    }

    //---------------------------------------------------------- controle de l'aiguille : une image par graduation
    if (argc > 1 && String (argv[1]) == "vucheck")
    {
        SpaceSonarProcessor p; p.setPlayConfigDetails (2, 2, fs, B); p.loadFactoryPreset (0); p.prepareToPlay (fs, B);
        SpaceSonarEditor ed (p); ed.setSize (1600, 1400);
        auto Fx = [] (float x) { return 800.0f + (x - 800.0f) * 1.12f; };
        auto Fy = [] (float y) { return 780.0f + (y - 780.0f) * 1.12f; };
        const Rectangle<int> area = Rectangle<float>::leftTopRightBottom (Fx (236), Fy (384), Fx (456), Fy (504)).toNearestInt();
        const float dbs[8] = { -20, -10, -7, -5, -3, -1, 0, 3 };
        for (int i = 0; i < 8; ++i)
        {
            ed.vuAngle = vuGeo::angleFor (dbs[i]); ed.repaint();
            auto img = ed.createComponentSnapshot (area, true, 3.0f);
            FileOutputStream os (File::getCurrentWorkingDirectory().getChildFile (String::formatted ("vuchk_%d.png", i))); PNGImageFormat png; png.writeImageToStream (img, os);
        }
        return 0;
    }

    //---------------------------------------------------------- GIF du VU-metre (images 30 i/s)
    if (argc > 1 && String (argv[1]) == "vugif")
    {
        SpaceSonarProcessor p; p.setPlayConfigDetails (2, 2, fs, B);
        p.loadFactoryPreset (0); p.prepareToPlay (fs, B);
        SpaceSonarEditor ed (p); ed.setSize (1600, 1400);
        for (int i = 0; i < 4; ++i) for (auto* c : ed.getChildren()) if (auto* kn = dynamic_cast<nh::Knob*> (c)) kn->forceTick();
        ed.statusTime = -1.0e9; ed.statusAlpha = 0;
        auto Fx = [] (float x) { return 800.0f + (x - 800.0f) * 1.12f; };
        auto Fy = [] (float y) { return 780.0f + (y - 780.0f) * 1.12f; };
        const Rectangle<int> area = Rectangle<float>::leftTopRightBottom (Fx (160), Fy (378), Fx (466), Fy (512)).toNearestInt();
        TestGuitar gt (fs); AudioBuffer<float> buf (2, 160);
        File dir = File::getCurrentWorkingDirectory().getChildFile ("vu_frames"); dir.deleteRecursively(); dir.createDirectory();
        const int N = 300;                                    // 10 s
        for (int f = 0; f < N; ++f)
        {
            if (f == 250) setP (p, "power", 0.0f);            // extinction a la fin : l'aiguille retombe
            const float gain = f < 120 ? 0.6f : (f < 160 ? 1.6f : (f < 200 ? 3.5f : 0.6f));   // passage fort -> pic rouge
            for (int b = 0; b < 10; ++b)                      // 1600 echantillons = 1/30 s
            {
                for (int i = 0; i < 160; ++i) { const float v = gt.next1() * gain; buf.setSample (0, i, v); buf.setSample (1, i, v); }
                p.processBlock (buf, midi);
            }
            ed.timerCallback();
            auto img = ed.createComponentSnapshot (area, true, 2.0f);
            FileOutputStream os (dir.getChildFile (String::formatted ("f%04d.png", f))); PNGImageFormat png; png.writeImageToStream (img, os);
        }
        std::cout << "ecrit " << N << " images\n";
        return 0;
    }

    //---------------------------------------------------------- captures
    auto shot = [&] (int preset, const String& name, std::function<void (SpaceSonarProcessor&)> tweak, int width)
    {
        SpaceSonarProcessor p; p.setPlayConfigDetails (2, 2, fs, B);
        p.loadFactoryPreset (preset);
        if (tweak) tweak (p);
        p.prepareToPlay (fs, B);
        SpaceSonarEditor ed (p); ed.setSize (width, width * 1400 / 1600);
        TestGuitar gt (fs); AudioBuffer<float> buf (2, B);
        for (int k = 0; k < (int) (3.0 * fs / B); ++k)
        {
            for (int i = 0; i < B; ++i) { const float v = gt.next1() * 0.6f; buf.setSample (0, i, v); buf.setSample (1, i, v); }
            p.processBlock (buf, midi);
            if (k % 6 == 0) ed.timerCallback();
        }
        for (int i = 0; i < 12; ++i) ed.timerCallback();
        for (int i = 0; i < 4; ++i) for (auto* c : ed.getChildren()) if (auto* kn = dynamic_cast<nh::Knob*> (c)) kn->forceTick();
        ed.statusTime = -1.0e9; ed.statusAlpha = 0;
        snap (ed, name, 2.0f);
        if (name == "ss_default.png")
        {
            ed.showStatus ("REPEAT RATE  5.0  \xc2\xb7  HEAD 3  372 ms"); ed.statusAlpha = 1.0f; snap (ed, "ss_status.png", 2.0f);
            snapMenu (ed, ed.buildContextMenu(), "menu_main.png");
            snapMenu (ed, ed.buildContextMenu ("repeatRate"), "menu_knob.png");
        }
    };
    shot (0, "ss_default.png", {}, 1600);
    shot (3, "ss_dub.png", [] (SpaceSonarProcessor& p) { setP (p, "inputSel", 0.0f); }, 1600);
    shot (5, "ss_single_reverb_only.png", [] (SpaceSonarProcessor& p) { setP (p, "stereo", 0.0f); setP (p, "echoCancel", 1.0f); }, 896);
    return 0;
}
