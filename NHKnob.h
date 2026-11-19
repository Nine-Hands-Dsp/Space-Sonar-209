#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

//==============================================================================
//  nh::Knob (NINE HANDS | DSP) : potard en calques (shadow, crown, cap, wear, marker, highlight).
//  Marqueur, chapeau, couronne et ombre tournent ensemble, au même angle, avec flou de mouvement ;
//  reflets fixes (lumière en haut à gauche à 45°), ombre décalée à 45° vers le bas à droite.
//  Space Sonar 209 : angle libre (angleFn) pour le sélecteur de mode à 12 crans, glisser cumulé
//  (un paramètre à crans suit la souris sans rester bloqué sur le cran).
//==============================================================================
namespace nh
{
// Réduction d'image sans crénelage : moyennes exactes 2 x 2 (filtre boîte) tant que l'image fait plus du double
// de la cible, puis dernière étape lissée (<= 2x). Le rééchantillonnage de JUCE ne filtre pas les fortes réductions
// (moletage et contours en escalier, aspect « basse résolution ») : toutes les pièces passent par ici.
inline juce::Image halve (const juce::Image& src)
{
    const int w = juce::jmax (1, src.getWidth() / 2), h = juce::jmax (1, src.getHeight() / 2);
    juce::Image out (src.getFormat(), w, h, false);
    const juce::Image::BitmapData s (src, juce::Image::BitmapData::readOnly);
    juce::Image::BitmapData d (out, juce::Image::BitmapData::writeOnly);
    const int ps = s.pixelStride;
    for (int y = 0; y < h; ++y)
    {
        const juce::uint8* r0 = s.getLinePointer (juce::jmin (2 * y, src.getHeight() - 1));
        const juce::uint8* r1 = s.getLinePointer (juce::jmin (2 * y + 1, src.getHeight() - 1));
        juce::uint8* o = d.getLinePointer (y);
        for (int x = 0; x < w; ++x)
        {
            const int x0 = 2 * x * ps, x1 = juce::jmin (2 * x + 1, src.getWidth() - 1) * ps;
            for (int c = 0; c < ps; ++c)
                o[x * d.pixelStride + c] = (juce::uint8) ((r0[x0 + c] + r0[x1 + c] + r1[x0 + c] + r1[x1 + c] + 2) >> 2);
        }
    }
    return out;
}

inline juce::Image downscale (juce::Image src, int w, int h, juce::Image::PixelFormat fmt = juce::Image::ARGB)
{
    if (src.isNull() || w <= 0 || h <= 0) return {};
    if (src.getFormat() != fmt) src = src.convertedToFormat (fmt);
    while (src.getWidth() >= 2 * w && src.getHeight() >= 2 * h) src = halve (src);
    juce::Image out (fmt, w, h, true);
    juce::Graphics g (out);
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    g.drawImage (src, juce::Rectangle<float> (0.0f, 0.0f, (float) w, (float) h));
    return out;
}

struct KnobStyle
{
    float startAngle = juce::degreesToRadians (-135.0f);
    float endAngle   = juce::degreesToRadians ( 135.0f);
    juce::Point<float> shadowOffset { 0.025f, 0.025f };   // lumiere en haut a gauche a 45 deg -> ombre a 45 deg vers le bas a droite
    float blurMax    = juce::degreesToRadians (40.0f);
    float blurStep   = juce::degreesToRadians (3.0f);
    float blurDecay  = 0.6f;
    float dragPixels = 250.0f;
    std::function<float (float)> angleFn;                 // valeur 0..1 -> angle (radians), sinon course lineaire
};

class Knob : public juce::Component, private juce::Timer
{
public:
    struct Layers { juce::Image shadow, crown, cap, wear, marker, highlight; };

    using Style = KnobStyle;

    Knob (juce::RangedAudioParameter& p, Layers l, Style s = {})
        : param (p), layers (std::move (l)), style (std::move (s)),
          attachment (p, [this] (float v) { target = param.convertTo0to1 (v); }, nullptr)
    {
        attachment.sendInitialUpdate();
        shown = target;
        lastAngle = angleFor (shown);
        startTimerHz (30);
    }

    std::function<void (const juce::MouseEvent&)> onRightClick;
    juce::RangedAudioParameter& parameter() { return param; }
    void forceTick() { timerCallback(); }                   // outil de captures (tools/render)
    float value01() const { return target; }

    void resized() override { resizeSettle = frame.isValid() ? 3 : 1; }

    void paint (juce::Graphics& g) override
    {
        if (frame.isValid()) g.drawImage (frame, getLocalBounds().toFloat());
    }

    // zone cliquable : le disque du potard seulement
    bool hitTest (int x, int y) override
    {
        const auto c = getLocalBounds().toFloat().getCentre();
        return c.getDistanceFrom ({ (float) x, (float) y }) <= (float) juce::jmin (getWidth(), getHeight()) * 0.42f;
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) { if (onRightClick) onRightClick (e); return; }
        dragging = true; dragStartY = e.position.y; dragStartV = target; fine = e.mods.isShiftDown(); attachment.beginGesture();
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! dragging) return;
        if (e.mods.isShiftDown() != fine) { fine = e.mods.isShiftDown(); dragStartY = e.position.y; dragStartV = target; }   // Maj : reglage fin x5
        const auto range = style.dragPixels * (fine ? 5.0f : 1.0f);
        setNormalised (dragStartV - (e.position.y - dragStartY) / range, true);
    }
    void mouseUp (const juce::MouseEvent&) override { if (dragging) attachment.endGesture(); dragging = false; }
    void mouseDoubleClick (const juce::MouseEvent& e) override { if (! e.mods.isPopupMenu()) setNormalised (param.getDefaultValue(), false); }
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
    {
        const int steps = param.getNumSteps();
        if (steps > 1 && steps < 50)
            setNormalised (target + (w.deltaY > 0 ? 1.0f : w.deltaY < 0 ? -1.0f : 0.0f) / (float) (steps - 1), false);
        else
            setNormalised (target + w.deltaY * 0.25f, false);
    }

private:
    float angleFor (float v) const
    {
        if (style.angleFn) return style.angleFn (v);
        return style.startAngle + v * (style.endAngle - style.startAngle);
    }

    void setNormalised (float v, bool inGesture)
    {
        v = juce::jlimit (0.0f, 1.0f, v);
        const auto plain = param.convertFrom0to1 (v);
        if (inGesture) attachment.setValueAsPartOfGesture (plain);
        else           attachment.setValueAsCompleteGesture (plain);
    }

    juce::AffineTransform place (const juce::Image& img, juce::Point<float> c, float a) const
    {
        return juce::AffineTransform::translation (-img.getWidth() * 0.5f, -img.getHeight() * 0.5f)
                   .scaled (1.0f / scale).rotated (a).translated (c.x, c.y);
    }

    void timerCallback() override
    {
        if (++scaleTick >= 8)
        {
            scaleTick = 0;
            const auto s = juce::jmax (1.0f, juce::Component::getApproximateScaleFactorForComponent (this));
            if (std::abs (s - scale) > 0.01f && resizeSettle == 0) resizeSettle = 1;
        }
        if (resizeSettle > 0 && --resizeSettle == 0) { rebuildCache(); dirty = true; }

        shown = target;
        const auto a = angleFor (shown);
        const auto v = a - lastAngle;
        lastAngle = a;
        if (std::abs (v) > 1.0e-5f) blurDir = v > 0.0f ? 1.0f : -1.0f;
        blurSpan = juce::jmax (juce::jmin (std::abs (v), style.blurMax), blurSpan * style.blurDecay);
        if (blurSpan < 0.002f) blurSpan = 0.0f;

        const bool changed = dirty || std::abs (a - drawnAngle) > 1.0e-4f || blurSpan != drawnSpan;
        if (changed && stack.isValid())
        {
            renderFrame (a);
            drawnAngle = a; drawnSpan = blurSpan; dirty = false;
            repaint();
        }
    }

    void renderFrame (float a)
    {
        const int w = juce::roundToInt (getWidth() * scale), h = juce::roundToInt (getHeight() * scale);
        if (w <= 0 || h <= 0) return;
        if (frame.getWidth() != w || frame.getHeight() != h)
        {
            frame   = juce::Image (juce::Image::ARGB, w, h, true);
            scratch = juce::Image (juce::Image::ARGB, w, h, true);
        }
        frame.clear (frame.getBounds());
        juce::Graphics g (frame);
        g.addTransform (juce::AffineTransform::scale (scale));
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        const auto b = getLocalBounds().toFloat();
        const auto d = juce::jmin (b.getWidth(), b.getHeight());
        const auto c = b.getCentre();
        if (shadowCache.isValid()) drawBlurred (g, shadowCache, c + style.shadowOffset * d, a);
        drawBlurred (g, stack, c, a);
        if (highlightCache.isValid()) g.drawImage (highlightCache, b.withSizeKeepingCentre (d, d));
    }

    void drawBlurred (juce::Graphics& g, const juce::Image& img, juce::Point<float> c, float a)
    {
        if (blurSpan <= 0.0f) { g.drawImageTransformed (img, place (img, c, a), false); return; }
        const int n = juce::jlimit (4, 16, (int) std::ceil (blurSpan / style.blurStep) + 1);
        scratch.clear (scratch.getBounds());
        {
            juce::Graphics gs (scratch);
            gs.addTransform (juce::AffineTransform::scale (scale));
            gs.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
            for (int k = 0; k < n; ++k)
            {
                const auto t = (float) k / (float) (n - 1);
                gs.setOpacity (1.0f / (float) (k + 1)); // moyenne progressive exacte
                gs.drawImageTransformed (img, place (img, c, a - blurDir * blurSpan * t), false);
            }
        }
        g.drawImage (scratch, getLocalBounds().toFloat());
    }

    void rebuildCache()
    {
        scale = juce::jmax (1.0f, juce::Component::getApproximateScaleFactorForComponent (this));
        const int px = juce::roundToInt (juce::jmin (getWidth(), getHeight()) * scale);
        if (px <= 0) return;
        auto fit = [px] (const juce::Image& src)
        {
            return downscale (src, px, px);                  // calques carrés : même cadrage
        };
        shadowCache = fit (layers.shadow);
        highlightCache = fit (layers.highlight);
        stack = juce::Image (juce::Image::ARGB, px, px, true);
        juce::Graphics g (stack);
        for (auto* l : { &layers.crown, &layers.cap, &layers.wear, &layers.marker })
            if (l->isValid()) g.drawImageAt (fit (*l), 0, 0);
    }

    juce::RangedAudioParameter& param;
    Layers layers;
    Style style;
    juce::ParameterAttachment attachment;
    juce::Image stack, shadowCache, highlightCache, frame, scratch;
    float scale = 1.0f, target = 0.0f, shown = 0.0f, lastAngle = 0.0f;
    float blurSpan = 0.0f, blurDir = 1.0f, dragStartY = 0.0f, dragStartV = 0.0f;
    float drawnAngle = 1.0e9f, drawnSpan = -1.0f;
    int resizeSettle = 0, scaleTick = 0;
    bool dragging = false, dirty = true, fine = false;
};
} // namespace nh
