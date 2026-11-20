#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

//==============================================================================
// NINE HANDS | DSP : texte de la ligne d'etat sans tremblement.
// Chiffres a chasse fixe (tous de la largeur du plus large) : le texte ne bouge plus quand une valeur change ;
// texte trop long : la police est reduite (jusqu'a 60 %) au lieu d'ecraser les lettres a chaque image.
// Police : celle du Graphics (g.setFont avant l'appel).
//==============================================================================
namespace nhstatus
{
    inline float glyphW (const juce::Font& f, juce::juce_wchar c)
    {
        return juce::GlyphArrangement::getStringWidth (f, juce::String::charToString (c));
    }

    inline void draw (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> r,
                      juce::Justification just = juce::Justification::centred)
    {
        auto f = g.getCurrentFont();
        float digitW = 0.0f;
        for (juce::juce_wchar c = '0'; c <= '9'; ++c) digitW = juce::jmax (digitW, glyphW (f, c));
        auto widthOf = [&] (const juce::Font& ff, float dw)
        {
            float w = 0.0f;
            for (auto c : text) w += juce::CharacterFunctions::isDigit (c) ? dw : glyphW (ff, c);
            return w;
        };
        float w = widthOf (f, digitW);
        if (w > r.getWidth() && w > 0.0f)
        {
            const float k = juce::jmax (0.6f, r.getWidth() / w);
            f = f.withHeight (f.getHeight() * k);
            digitW *= k;
            w = widthOf (f, digitW);
        }
        g.setFont (f);
        float x = just.testFlags (juce::Justification::left) ? r.getX()
                : just.testFlags (juce::Justification::right) ? r.getRight() - w
                : r.getCentreX() - w * 0.5f;
        for (auto c : text)
        {
            const float cw = juce::CharacterFunctions::isDigit (c) ? digitW : glyphW (f, c);
            g.drawText (juce::String::charToString (c), juce::Rectangle<float> (x, r.getY(), cw, r.getHeight()),
                        juce::Justification::centred, false);
            x += cw;
        }
    }
}
