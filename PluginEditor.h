#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"
#include "NHKnob.h"
#include "NineHandsWindow.h"
#include "Layout.h"

namespace vuGeo   // géométrie de l'aiguille du VU (unités de façade)
{
    inline const juce::Rectangle<float> face (250.6f, 397.4f, 190.8f, 92.8f);       // vitre (unités de façade)
    // pivot = centre des arcs de l'échelle, ajusté sur la photo (cercles des blocs et de l'arc fin, écart < 1 px)
    inline const juce::Point<float> pivot (346.8f, 573.3f);
    constexpr float tipR = 154.0f;                                     // pointe juste au-dessus des blocs, sous les chiffres
    // dB -> angle (degrés) mesurés sur la photo depuis ce pivot : séparations des blocs (-10 à -3),
    // limite vert / rouge (0), centre du chiffre -1, début et fin de l'échelle (-20, +3)
    inline float angleFor (float db)
    {
        static const float d[8] = { -20, -10, -7, -5, -3, -1, 0, 3 };
        static const float a[8] = { -26.4f, -18.85f, -12.25f, -5.6f, 1.05f, 8.9f, 13.5f, 26.7f };
        if (db <= d[0]) return a[0] - 2.0f * juce::jlimit (0.0f, 1.0f, (d[0] - db) / 20.0f);
        for (int i = 1; i < 8; ++i) if (db <= d[i]) return a[i - 1] + (a[i] - a[i - 1]) * (db - d[i - 1]) / (d[i] - d[i - 1]);
        return juce::jmin (29.0f, a[7] + (db - 3.0f) * 1.5f);
    }
}

class SpaceSonarEditor;

namespace ssui
{
    using namespace juce;

    // tailles de la fenêtre : repère 1600 x 1400 (100 %)
    constexpr int kW = 896, kH = 784;            // « 60 % » = taille minimale
    constexpr int kMaxW = 2400, kMaxH = 2100;    // 150 %
    constexpr int numSizes = 3;
    inline int sizeWidth (int i) { static const int w[numSizes] = { kW, 1200, 1600 }; return w[jlimit (0, numSizes - 1, i)]; }

    //------------------------------------------------------------------ inverseur POWER (photos du kit, 3 images)
    class Toggle : public Component, private Timer
    {
    public:
        Toggle (SpaceSonarEditor& e, RangedAudioParameter& p, Image strip);
        void paint (Graphics&) override;
        void mouseDown (const MouseEvent&) override;
        bool hitTest (int x, int y) override;
        void rescale (float phys);                 // images réduites à la taille d'affichage (timer de l'éditeur)
    private:
        void timerCallback() override;
        SpaceSonarEditor& ed;
        RangedAudioParameter& param;
        Image frames[3], shown[3];
        ParameterAttachment att;
        bool on = true; int mid = 0;
    };

    //------------------------------------------------------------------ glissière à 2 ou 3 positions
    class Slide : public Component
    {
    public:
        // xs : positions du bouton (unités du repère, centre), values : valeur du paramètre pour chaque position
        Slide (SpaceSonarEditor& e, RangedAudioParameter& p, Image knob, std::vector<float> xs, std::vector<float> values, float y);
        void paint (Graphics&) override;
        void mouseDown (const MouseEvent&) override;
        void mouseDrag (const MouseEvent&) override;
        int position() const;
        void place();
        void rescale (float phys);
        std::vector<float> xs, values;
        float y;
    private:
        SpaceSonarEditor& ed;
        RangedAudioParameter& param;
        Image knob, shown;
        ParameterAttachment att;
        Rectangle<float> knobRect (float x) const;
        float cur = 0.0f;
        void setPos (int i);
        int nearest (float unitX) const;
    };
}

//==============================================================================
class SpaceSonarEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit SpaceSonarEditor (SpaceSonarProcessor&);
    ~SpaceSonarEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void parentHierarchyChanged() override;
    void visibilityChanged() override;

    SpaceSonarProcessor& proc;
    NineHandsMenu::MenuLnf menuLnf;
    float k() const { return (float) getWidth() / ssl::kW; }
    juce::Rectangle<float> U (float x, float y, float w, float h) const { const float s = k(); return { x * s, y * s, w * s, h * s }; }
    juce::Font font (int weight, float unitsHeight) const;
    // façade agrandie : coordonnées d'origine de la façade -> repère de la fenêtre
    static juce::AffineTransform fac() { return juce::AffineTransform::scale (ssl::kFS, ssl::kFS, ssl::kFCX, ssl::kFCY); }
    static juce::Point<float> F (float x, float y) { return juce::Point<float> (x, y).transformedBy (fac()); }
    static juce::Point<float> unF (juce::Point<float> p) { return p.transformedBy (fac().inverted()); }
    void askRepaintFac (juce::Rectangle<float> r) { askRepaint (r.transformedBy (fac())); }     // 0 regular, 1 medium, 2 bold, 3 light (taille en unités du repère)

    void showStatus (const juce::String& s);
    void showParamMenu (const juce::String& paramId);
    juce::PopupMenu buildContextMenu (const juce::String& paramId = {});
    void showContextMenu();
    void showPresetMenu();
    void askRepaint (juce::Rectangle<float> unitsRect);
    void setParamUndoable (const char* id, float realValue);
    void beginUndo();
    void endUndo();

private:
    friend int main (int, char**);                       // outil de captures (tools/render)
    void timerCallback() override;
    void setWindowWidth (int w);
    void useSoftwareRenderer();
    void rebuildCache();
    void prevNextPreset (int dir);
    void askUserPresetName (int slot);
    juce::String statusFor (juce::RangedAudioParameter* p, float v01) const;
    void paintHeader (juce::Graphics&);
    void paintFacadeLive (juce::Graphics&);
    void paintDiagram (juce::Graphics&);
    void paintLower (juce::Graphics&);

    // images
    juce::Image background, ledOff, ledOn, ledOffS, ledOnS, cache;
    float cacheScale = 0.0f;
    juce::Typeface::Ptr reg, med, bold, light;
    std::unique_ptr<juce::Drawable> logo;

    // commandes
    std::vector<std::unique_ptr<nh::Knob>> knobs;
    struct KnobPos { const char* id; float x, y, r; bool black; };
    std::vector<KnobPos> knobPos;
    std::unique_ptr<ssui::Toggle> power;
    std::unique_ptr<ssui::Slide> slideEcho, slideInput, slideOut;

    NineHandsWindow::HostLock hostLock;
    void* lastPeer = nullptr;
    double lastRendererCheck = 0, repaintAskedMs = 0, lastPaintMs = 0;
    int resizeSettle = 2, scaleTick = 0;
    std::unique_ptr<juce::AlertWindow> nameDialog;

    // état affiché
    juce::String status;
    juce::String lastStatusDrawn;      // texte affiché au dernier dessin (redessin quand il change)
    double statusTime = -10.0;
    float statusAlpha = 0.0f;
    float glow = 1.0f;
    double glowCheck = 0;
    float vuAngle = -46.0f, peakLevel = 0.0f, powerLevel = 1.0f;
    float meterIn[2] { -90, -90 }, meterOut[2] { -90, -90 };
    int hoverZone = 0;
    std::vector<float> lastVals;
    std::vector<float> undoBefore;
    bool undoOpen = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpaceSonarEditor)
};
