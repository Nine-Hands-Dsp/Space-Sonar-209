#include "PluginEditor.h"
#include "BinaryData.h"

using namespace juce;
using namespace ssui;

static double nowMs() { return Time::getMillisecondCounterHiRes(); }
static Image png (const void* d, int n) { return ImageCache::getFromMemory (d, n); }
static const String dot = String::fromUTF8 ("  \xc2\xb7  ");

namespace col
{
    const Colour green (0xff46c860), greenDim (0xff2c8a3f), text (0xffeef1f3), dim (0xff8f9693), faint (0xff4e5352),
                 boxOn (0xff565a5a), boxOff (0xff1e1f20), edgeOn (0xffd6dad6), edgeOff (0xff4e5050), wireOn (0xffd6dad6), wireOff (0xff4a4c4c),
                 pill (0xff323536), pillOn (0xff38a050), header (0xff161718), btn (0xff2e3032), btnHi (0xff3a3d40), amber (0xffffb347),
                 blue (0xff46aae6), orange (0xffe6783c), red (0xffe0463c), vuGlow (0xff9cf0b0);
    const Colour accent (0xff46c860);        // couleur de la pédale (pastille des menus)
}

//==============================================================================
//  Inverseur POWER
//==============================================================================
Toggle::Toggle (SpaceSonarEditor& e, RangedAudioParameter& p, Image strip)
    : ed (e), param (p), att (p, [this] (float v) { const bool o = v > 0.5f; if (o != on) { on = o; mid = 2; repaint(); } }, nullptr)
{
    const int fh = strip.getHeight() / 3;
    for (int i = 0; i < 3; ++i) frames[i] = strip.getClippedImage ({ 0, i * fh, strip.getWidth(), fh });
    att.sendInitialUpdate();
    mid = 0;
    startTimerHz (30);
}

bool Toggle::hitTest (int x, int y) { return getLocalBounds().toFloat().reduced (getWidth() * 0.25f, getHeight() * 0.1f).contains ((float) x, (float) y); }

void Toggle::paint (Graphics& g)
{
    g.setImageResamplingQuality (Graphics::highResamplingQuality);
    g.drawImage (frames[mid > 0 ? 1 : on ? 0 : 2], getLocalBounds().toFloat());
}

void Toggle::mouseDown (const MouseEvent& e)
{
    if (e.mods.isPopupMenu()) { ed.showParamMenu ("power"); return; }
    att.setValueAsCompleteGesture (on ? 0.0f : 1.0f);
}

void Toggle::timerCallback() { if (mid > 0 && --mid == 0) repaint(); }       // position du milieu ~55 ms pendant la bascule

//==============================================================================
//  Glissière
//==============================================================================
Slide::Slide (SpaceSonarEditor& e, RangedAudioParameter& p, Image k, std::vector<float> x, std::vector<float> v, float yy)
    : xs (std::move (x)), values (std::move (v)), y (yy), ed (e), param (p), knob (std::move (k)),
      att (p, [this] (float val) { cur = val; repaint(); }, nullptr)
{
    att.sendInitialUpdate();
}

int Slide::position() const
{
    int best = 0;
    for (int i = 1; i < (int) values.size(); ++i) if (std::abs (values[(size_t) i] - cur) < std::abs (values[(size_t) best] - cur)) best = i;
    return best;
}

void Slide::place()
{
    const float s = ed.k();
    setBounds (Rectangle<float> ((xs.front() - 22) * s, (y - 22) * s, (xs.back() - xs.front() + 44) * s, 44 * s).toNearestInt());
}

int Slide::nearest (float unitX) const
{
    int best = 0;
    for (int i = 1; i < (int) xs.size(); ++i) if (std::abs (xs[(size_t) i] - unitX) < std::abs (xs[(size_t) best] - unitX)) best = i;
    return best;
}

void Slide::setPos (int i) { if (i != position()) att.setValueAsCompleteGesture (values[(size_t) i]); }

void Slide::paint (Graphics& g)
{
    const float s = ed.k();
    const float x = xs[(size_t) position()];
    const auto r = Rectangle<float> (40 * s, 40 * s).withCentre ({ (x - xs.front() + 22) * s, 22 * s });
    g.setImageResamplingQuality (Graphics::highResamplingQuality);
    g.drawImage (knob, r);
}

void Slide::mouseDown (const MouseEvent& e)
{
    if (e.mods.isPopupMenu()) { ed.showParamMenu (param.paramID); return; }
    setPos (nearest (e.position.x / ed.k() + xs.front() - 22));
}

void Slide::mouseDrag (const MouseEvent& e) { if (! e.mods.isPopupMenu()) setPos (nearest (e.position.x / ed.k() + xs.front() - 22)); }

//==============================================================================
//  Fenêtre
//==============================================================================
SpaceSonarEditor::SpaceSonarEditor (SpaceSonarProcessor& p)
    : AudioProcessorEditor (p), proc (p)
{
    reg   = Typeface::createSystemTypefaceFor (BinaryData::PoppinsRegular_ttf, BinaryData::PoppinsRegular_ttfSize);
    med   = Typeface::createSystemTypefaceFor (BinaryData::PoppinsMedium_ttf, BinaryData::PoppinsMedium_ttfSize);
    bold  = Typeface::createSystemTypefaceFor (BinaryData::PoppinsBold_ttf, BinaryData::PoppinsBold_ttfSize);
    light = Typeface::createSystemTypefaceFor (BinaryData::PoppinsLight_ttf, BinaryData::PoppinsLight_ttfSize);
    menuLnf.f = { reg, med, bold };
    if (auto xml = XmlDocument::parse (String::fromUTF8 (BinaryData::NineHandsLogoBlack_svg, BinaryData::NineHandsLogoBlack_svgSize)))
        logo = Drawable::createFromSVG (*xml);

    background = png (BinaryData::background_jpg, BinaryData::background_jpgSize);
    ledOff = png (BinaryData::led_red_off_png, BinaryData::led_red_off_pngSize);
    ledOn  = png (BinaryData::led_red_on_png, BinaryData::led_red_on_pngSize);

    nh::Knob::Layers chrome { png (BinaryData::knob_chrome_shadow_png, BinaryData::knob_chrome_shadow_pngSize),
                              png (BinaryData::knob_chrome_crown_png, BinaryData::knob_chrome_crown_pngSize),
                              png (BinaryData::knob_chrome_cap_png, BinaryData::knob_chrome_cap_pngSize), {},
                              png (BinaryData::knob_chrome_marker_png, BinaryData::knob_chrome_marker_pngSize),
                              png (BinaryData::knob_chrome_highlight_png, BinaryData::knob_chrome_highlight_pngSize) };
    nh::Knob::Layers modeL { chrome.shadow,
                             png (BinaryData::knob_mode_crown_png, BinaryData::knob_mode_crown_pngSize),
                             png (BinaryData::knob_mode_cap_png, BinaryData::knob_mode_cap_pngSize), {},
                             png (BinaryData::knob_mode_marker_png, BinaryData::knob_mode_marker_pngSize),
                             png (BinaryData::knob_mode_highlight_png, BinaryData::knob_mode_highlight_pngSize) };
    nh::Knob::Layers black { png (BinaryData::knob_black_shadow_png, BinaryData::knob_black_shadow_pngSize), {},
                             png (BinaryData::knob_black_cap_png, BinaryData::knob_black_cap_pngSize), {},
                             png (BinaryData::knob_black_marker_png, BinaryData::knob_black_marker_pngSize),
                             png (BinaryData::knob_black_highlight_png, BinaryData::knob_black_highlight_pngSize) };

    knobPos = {
        { "micVol", 190, 619, 31, false }, { "paVol", 345, 619, 31, false }, { "instVol", 500, 619, 31, false },
        { "mode", 689, 528, 44, false },
        { "bass", 882, 478, 30, false }, { "treble", 1020, 478, 30, false }, { "reverbVol", 1158, 478, 30, false },
        { "repeatRate", 882, 612, 30, false }, { "intensity", 1020, 612, 30, false }, { "echoVol", 1158, 612, 30, false },
        { "speed", 1120, 1170, 24, true }, { "lfoRate", 1310, 1170, 24, true }, { "lfoShape", 1410, 1170, 24, true }, { "lfoDepth", 1510, 1170, 24, true },
        { "condition", 905, 1330, 24, true }, { "wf", 1010, 1330, 24, true }, { "motor", 1120, 1330, 24, true },
        { "lowCut", 1265, 1330, 24, true }, { "highCut", 1385, 1330, 24, true }, { "master", 1510, 1330, 24, true } };
    for (auto& kp : knobPos)
    {
        nh::Knob::Style st;
        if (String (kp.id) == "mode")
        {
            // 12 crans tous les 30° : 1 à 11 de -150° à +150°, REVERB ONLY en bas (180°)
            st.startAngle = degreesToRadians (-150.0f); st.endAngle = degreesToRadians (180.0f);
            st.dragPixels = 330.0f;
        }
        auto* prm = proc.apvts.getParameter (kp.id);
        auto kn = std::make_unique<nh::Knob> (*prm, kp.black ? black : String (kp.id) == "mode" ? modeL : chrome, st);
        const String id = kp.id;
        kn->onRightClick = [this, id] (const MouseEvent&) { showParamMenu (id); };
        addAndMakeVisible (*kn);
        knobs.push_back (std::move (kn));
    }

    power = std::make_unique<Toggle> (*this, *proc.apvts.getParameter ("power"), png (BinaryData::toggle_strip_png, BinaryData::toggle_strip_pngSize));
    addAndMakeVisible (*power);
    const auto sk = png (BinaryData::slide_knob_png, BinaryData::slide_knob_pngSize);
    slideEcho  = std::make_unique<Slide> (*this, *proc.apvts.getParameter ("echoOn"), sk, std::vector<float> { 604, 670 }, std::vector<float> { 1, 0 }, 744.0f);
    slideInput = std::make_unique<Slide> (*this, *proc.apvts.getParameter ("inputSel"), sk, std::vector<float> { 776, 821, 866 }, std::vector<float> { 0, 1, 2 }, 744.0f);
    slideOut   = std::make_unique<Slide> (*this, *proc.apvts.getParameter ("outLevel"), sk, std::vector<float> { 1040, 1066, 1092 }, std::vector<float> { 0, 1, 2 }, 750.0f);
    for (auto* s : { slideEcho.get(), slideInput.get(), slideOut.get() }) addAndMakeVisible (s);

    glow = (float) NineHandsGlow::percent (JucePlugin_Name) / 100.0f;
    addMouseListener (this, true);              // annuler / rétablir : chaque geste sur une commande
    setOpaque (true);
    setResizable (true, true);
    setResizeLimits (kW, kH, kMaxW, kMaxH);
    getConstrainer()->setFixedAspectRatio ((double) kW / (double) kH);
    hostLock.setLimits (kW, kH, kMaxW, kMaxH);
    setSize (kW, kH);
    setWindowWidth (sizeWidth (NineHandsWindow::openIndex (JucePlugin_Name, proc.uiSizeIdx.load(), numSizes)));

    for (auto* prm : proc.getParameters()) lastVals.push_back (prm->getValue());
    powerLevel = proc.prm.power->load() > 0.5f ? 1.0f : 0.0f;
    startTimerHz (30);
}

SpaceSonarEditor::~SpaceSonarEditor()
{
    PopupMenu::dismissAllActiveMenus();
    stopTimer();
}

Font SpaceSonarEditor::font (int w, float h) const
{
    const auto& tf = w == 0 ? reg : w == 1 ? med : w == 2 ? bold : light;
    return Font (FontOptions (tf).withPointHeight (h));
}

void SpaceSonarEditor::setWindowWidth (int w)
{
    w = jlimit (kW, kMaxW, w);
    setSize (w, roundToInt ((double) w * (double) kH / (double) kW));
    hostLock.enforce();
}

void SpaceSonarEditor::resized()
{
    // jamais de setSize ici (mise à l'échelle Windows -> boucle avec l'hôte)
    const float s = k();
    for (size_t i = 0; i < knobs.size(); ++i)
    {
        const auto& kp = knobPos[i];
        const float side = 2.0f * kp.r * 1.30f;
        knobs[i]->setBounds (Rectangle<float> (side * s, side * s).withCentre ({ kp.x * s, kp.y * s }).toNearestInt());
    }
    power->setBounds (Rectangle<float> (80 * s, 80 * s).withCentre ({ 1412 * s, 622 * s }).toNearestInt());
    for (auto* sl : { slideEcho.get(), slideInput.get(), slideOut.get() }) sl->place();
    resizeSettle = 4;
}

//==============================================================================
//  Anti-gel v2
//==============================================================================
void SpaceSonarEditor::useSoftwareRenderer()
{
    auto* peer = getPeer();
    if ((void*) peer != lastPeer) { lastPeer = (void*) peer; repaint(); }
    lastRendererCheck = nowMs();
   #if JUCE_WINDOWS
    if (peer != nullptr)
    {
        int sw = peer->getAvailableRenderingEngines().indexOf ("Software Renderer");
        if (sw < 0) sw = 0;
        if (peer->getCurrentRenderingEngine() != sw) peer->setCurrentRenderingEngine (sw);
    }
   #endif
}

void SpaceSonarEditor::parentHierarchyChanged() { useSoftwareRenderer(); hostLock.track (*this); }
void SpaceSonarEditor::visibilityChanged()      { useSoftwareRenderer(); if (isShowing()) repaint(); }

void SpaceSonarEditor::askRepaint (Rectangle<float> u)
{
    if (repaintAskedMs <= 0.0) repaintAskedMs = nowMs();
    repaint (u.transformedBy (AffineTransform::scale (k())).getSmallestIntegerContainer().expanded (2));
}

// fond + sérigraphies + logo, à la résolution physique de l'écran (timer uniquement, jamais dans paint)
void SpaceSonarEditor::rebuildCache()
{
    const float phys = jlimit (1.0f, 4.0f, (float) Component::getApproximateScaleFactorForComponent (this));
    const int w = roundToInt ((float) getWidth() * phys), h = roundToInt ((float) getHeight() * phys);
    if (w <= 0 || h <= 0) return;
    Image img (Image::RGB, w, h, false);
    {
        Graphics g (img);
        g.setImageResamplingQuality (Graphics::highResamplingQuality);
        g.drawImage (background, Rectangle<float> (0, 0, (float) w, (float) h));
        g.addTransform (AffineTransform::scale ((float) w / ssl::kW));
        int n = 0;
        const auto* t = ssl::texts (n);
        for (int i = 0; i < n; ++i)
        {
            const auto& x = t[i];
            auto f = font (x.weight, x.size);
            if (x.track > 0.0f) f = f.withExtraKerningFactor (x.track / x.size);
            g.setFont (f);
            g.setColour (Colour (x.argb));
            const float wd = GlyphArrangement::getStringWidth (f, String::fromUTF8 (x.s)) + 4.0f;
            const auto r = x.left ? Rectangle<float> (x.x, x.y - x.size, wd, x.size * 2.0f)
                                  : Rectangle<float> (wd, x.size * 2.0f).withCentre ({ x.x, x.y });
            Graphics::ScopedSaveState rs (g);
            if (x.rot != 0.0f) g.addTransform (AffineTransform::rotation (degreesToRadians (x.rot), x.x, x.y));   // textes en arc (MODE SELECTOR)
            g.drawText (String::fromUTF8 (x.s), r, x.left ? Justification::centredLeft : Justification::centred, false);
        }
        if (logo != nullptr)
            logo->drawWithin (g, Rectangle<float> (1236, 310, 220, 34), RectanglePlacement::xRight | RectanglePlacement::yMid, 0.92f);
    }
    cache = img;
    cacheScale = phys;
    repaint();
}

//==============================================================================
//  Dessin
//==============================================================================
void SpaceSonarEditor::paint (Graphics& g)
{
    lastPaintMs = nowMs(); repaintAskedMs = 0;
    g.fillAll (Colour (0xff141516));
    g.setImageResamplingQuality (Graphics::mediumResamplingQuality);
    if (cache.isValid()) g.drawImage (cache, getLocalBounds().toFloat());
    else g.drawImage (background, getLocalBounds().toFloat());
    g.addTransform (AffineTransform::scale (k()));
    paintHeader (g);
    paintFacadeLive (g);
    paintDiagram (g);
    paintLower (g);
}

static void pillBox (Graphics& g, Rectangle<float> r, Colour c) { g.setColour (c); g.fillRoundedRectangle (r, r.getHeight() * 0.5f); }

void SpaceSonarEditor::paintHeader (Graphics& g)
{
    // pastille presets / ligne d'état (taille de référence 400 x 38, Poppins Medium)
    const Rectangle<float> p (20, 29, 400, 38);
    g.setColour (Colour (0xff464a4a)); g.fillRoundedRectangle (p.expanded (1), 20);
    g.setColour (Colour (0xff0a0b0b)); g.fillRoundedRectangle (p, 19);
    const float a = 1.0f - statusAlpha;
    if (a > 0.01f)
    {
        g.setFont (font (1, 21));
        g.setColour (col::text.withAlpha (a));
        g.drawFittedText (proc.presetName + (proc.presetDirty.load() ? " *" : ""), p.reduced (36, 0).toNearestInt(), Justification::centred, 1, 0.75f);
        for (int side = 0; side < 2; ++side)
        {
            const float cx = side == 0 ? p.getX() + 17 : p.getRight() - 17, cy = p.getCentreY();
            Path t;
            if (side == 0) t.addTriangle (cx + 4, cy - 5.5f, cx + 4, cy + 5.5f, cx - 5.5f, cy);
            else           t.addTriangle (cx - 4, cy - 5.5f, cx - 4, cy + 5.5f, cx + 5.5f, cy);
            g.setColour ((hoverZone == side + 1 ? Colours::white : col::text.withAlpha (0.6f)).withMultipliedAlpha (a));
            g.fillPath (t);
        }
    }
    if (statusAlpha > 0.01f)
    {
        g.setFont (font (1, 21));
        g.setColour (col::text.withAlpha (statusAlpha));
        g.drawFittedText (status, p.reduced (19, 0).toNearestInt(), Justification::centred, 1, 0.6f);
    }

    // vumètres IN / OUT
    auto bar = [&] (float x, float db)
    {
        const Rectangle<float> r (x, 18, 7, 44);
        g.setColour (Colour (0xff080808)); g.fillRoundedRectangle (r, 1.5f);
        const float n = jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);
        if (n > 0.0f)
        {
            const auto lv = r.withTop (r.getBottom() - r.getHeight() * n);
            g.setColour (db > -1.0f ? col::red : db > -9.0f ? col::amber : col::green);
            g.fillRoundedRectangle (lv, 1.5f);
        }
    };
    bar (1076, meterIn[0]); bar (1087, meterIn[1]); bar (1136, meterOut[0]); bar (1147, meterOut[1]);
    g.setFont (font (1, 9.5f)); g.setColour (col::dim);
    g.drawText ("IN", Rectangle<float> (1066, 72, 40, 16), Justification::centred);
    g.drawText ("OUT", Rectangle<float> (1126, 72, 40, 16), Justification::centred);

    // BYPASS, A / B, annuler / rétablir, menu
    const bool bypassed = proc.prm.power->load() < 0.5f;
    g.setColour (bypassed ? Colour (0xff4a3a1e) : col::btn); g.fillRoundedRectangle (Rectangle<float> (1190, 26, 110, 44), 6);
    g.setFont (font (1, 11).withExtraKerningFactor (0.09f));
    g.setColour (bypassed ? col::amber : col::text.withAlpha (0.85f));
    g.drawText ("BYPASS", Rectangle<float> (1190, 26, 110, 44), Justification::centred);
    g.setColour (Colour (0xff242628)); g.fillRoundedRectangle (Rectangle<float> (1314, 26, 104, 44), 6);
    for (int i = 0; i < 2; ++i)
    {
        const bool act = proc.abSlot == i;
        const Rectangle<float> r (1314.0f + 52.0f * (float) i, 26, 52, 44);
        g.setColour (act ? col::green : col::dim);
        g.setFont (font (1, 12));
        g.drawText (i == 0 ? "A" : "B", r.withTrimmedTop (10), Justification::centred);
        if (act) g.fillRect (Rectangle<float> (r.getCentreX() - 14, 37, 28, 2));
    }
    g.setColour (Colour (0xff242628)); g.fillRoundedRectangle (Rectangle<float> (1432, 26, 58, 44), 6);
    for (int i = 0; i < 2; ++i)
    {
        const bool can = i == 0 ? ! proc.undoStack.empty() : ! proc.redoStack.empty();
        const float cx = i == 0 ? 1450.0f : 1472.0f;
        Path arc; arc.addCentredArc (cx, 51, 7, 7, 0, -MathConstants<float>::halfPi, MathConstants<float>::halfPi, true);
        g.setColour (can ? col::text.withAlpha (0.85f) : col::faint);
        g.strokePath (arc, PathStrokeType (1.8f));
        const float ax = i == 0 ? cx - 7.0f : cx + 7.0f;
        Path tri; tri.addTriangle (ax - 3.6f, 50, ax + 3.6f, 50, ax, 55.5f);
        g.fillPath (tri);
    }
    g.setColour (Colour (0xff242628)); g.fillRoundedRectangle (Rectangle<float> (1500, 26, 80, 44), 6);
    g.setColour (col::text.withAlpha (0.8f));
    for (int i = 0; i < 3; ++i) g.fillRoundedRectangle (Rectangle<float> (1526, 37.0f + 9.0f * (float) i, 28, 2), 1);
}

void SpaceSonarEditor::paintFacadeLive (Graphics& g)
{
    // VU : rétroéclairage + aiguille
    const Rectangle<float> face (252, 394, 188, 100);
    {
        Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (face.toNearestInt());
        if (powerLevel > 0.01f)
        {
            ColourGradient gl (col::vuGlow.withAlpha (0.20f * powerLevel), face.getCentreX(), face.getBottom(), col::vuGlow.withAlpha (0.0f),
                               face.getCentreX(), face.getY() - 30, true);
            g.setGradientFill (gl); g.fillRect (face);
        }
        const Point<float> pv (346, 520);
        const float a = degreesToRadians (vuAngle);
        const Point<float> tip = pv + Point<float> (std::sin (a), -std::cos (a)) * 99.0f;
        g.setColour (Colours::black.withAlpha (0.35f));
        g.drawLine ({ pv.translated (1.5f, 1.5f), tip.translated (1.5f, 1.5f) }, 1.6f);
        g.setColour (Colour (0xffe6ebe4));
        g.drawLine ({ pv, tip }, 1.4f);
    }
    // LED PEAK LEVEL et témoin secteur
    auto led = [&] (Point<float> c, float level)
    {
        const float side = 16.25f;
        const auto r = Rectangle<float> (side, side).withCentre (c);
        g.drawImage (ledOff, r);
        if (level > 0.01f)
        {
            // halo (dosé par Glow), puis la lentille allumée (toujours à fond)
            ColourGradient h (col::red.withAlpha (0.42f * level * glow), c.x, c.y, col::red.withAlpha (0.0f), c.x + 26.0f, c.y, true);
            g.setGradientFill (h); g.fillEllipse (r.expanded (18));
            g.setOpacity (level);
            g.drawImage (ledOn, r);
            g.setOpacity (1.0f);
        }
    };
    led ({ 200, 436 }, peakLevel);
    led ({ 1318, 622 }, powerLevel);

    // prises : entrée choisie (bleu), sortie (orange), ECHO CANCEL actif (rouge)
    auto ringAt = [&] (Point<float> c, Colour cl)
    {
        g.setColour (cl.withAlpha (0.25f * glow)); g.drawEllipse (Rectangle<float> (36, 36).withCentre (c), 3.0f);
        g.setColour (cl); g.drawEllipse (Rectangle<float> (28.5f, 28.5f).withCentre (c), 2.4f);
    };
    static const Point<float> ins[3] = { { 198, 748 }, { 353, 748 }, { 478, 748 } };
    ringAt (ins[jlimit (0, 2, (int) proc.prm.inputSel->load())], col::blue);
    ringAt ({ 1200, 748 }, col::orange);
    if (proc.prm.echoCancel->load() > 0.5f) ringAt ({ 1296, 748 }, col::red);
}

void SpaceSonarEditor::paintDiagram (Graphics& g)
{
    const int md = (int) proc.prm.mode->load();
    const auto& M = ss::mode (md);
    const int sel = (int) proc.prm.inputSel->load();
    const bool dual = (int) proc.prm.stereo->load() == 1;
    const bool on = proc.prm.power->load() > 0.5f;
    const bool echo = on && M.echo && proc.prm.echoCancel->load() < 0.5f;
    const bool send = echo && proc.prm.echoOn->load() > 0.5f;
    const bool rev = on && M.reverb;

    auto wire = [&] (std::initializer_list<Point<float>> pts, bool act)
    {
        Path pth; bool first = true;
        for (auto pt : pts) { if (first) pth.startNewSubPath (pt); else pth.lineTo (pt); first = false; }
        g.setColour (act ? col::wireOn : col::wireOff);
        g.strokePath (pth, PathStrokeType (1.6f, PathStrokeType::mitered, PathStrokeType::square));
    };
    auto box = [&] (Rectangle<float> r, const String& lab, bool act, const String& sub = {})
    {
        g.setColour (act ? col::boxOn : col::boxOff); g.fillRoundedRectangle (r, 3);
        g.setColour (act ? col::edgeOn : col::edgeOff); g.drawRoundedRectangle (r.reduced (0.8f), 3, 1.6f);
        g.setColour (act ? Colours::white : col::faint.brighter (0.4f));
        g.setFont (font (1, 10));
        if (sub.isEmpty()) g.drawText (lab, r, Justification::centred);
        else
        {
            g.drawText (lab, r.withTrimmedBottom (r.getHeight() * 0.4f), Justification::centredBottom);
            g.setFont (font (1, 7.5f));
            g.drawText (sub, r.withTrimmedTop (r.getHeight() * 0.62f), Justification::centredTop);
        }
    };

    // entrées
    struct In { const char* n; float y; int type; bool right; };
    static const In ins[6] = { { "MIC L", 1012, 0, false }, { "MIC R", 1040, 0, true }, { "P.A. L", 1100, 1, false }, { "P.A. R", 1128, 1, true },
                               { "INST L", 1316, 2, false }, { "INST R", 1344, 2, true } };
    const float busL = 268, busR = 280;
    for (const auto& i : ins)
    {
        const bool act = on && i.type == sel && (! i.right || dual);
        const float xs = i.type == 0 ? 236.0f : 126.0f;
        wire ({ { xs, i.y }, { i.right ? busR : busL, i.y } }, act);
    }
    // préamplis micro
    wire ({ { 126, 1012 }, { 146, 1012 } }, on && sel == 0);
    wire ({ { 126, 1040 }, { 146, 1040 } }, on && sel == 0 && dual);
    // bus gauche et droit
    const float yIn = sel == 0 ? 1012.0f : sel == 1 ? 1100.0f : 1316.0f;
    wire ({ { busL, std::min (1014.0f, yIn) }, { busL, 1300.0f } }, false);
    wire ({ { busL, std::min (1014.0f, yIn) }, { busL, std::max (1150.0f, yIn) } }, on);
    wire ({ { busR, std::min (1040.0f, yIn + 28) }, { busR, 1344.0f } }, false);
    wire ({ { busR, std::min (1040.0f, yIn + 28) }, { busR, std::max (1316.0f, yIn + 28) } }, on && dual);
    // canal gauche : VU, écho, réverbe, son sec
    wire ({ { busL, 1014 }, { 300, 1014 } }, on);
    wire ({ { busL, 1080 }, { 300, 1080 } }, send);
    wire ({ { busL, 1150 }, { 300, 1150 } }, rev);
    wire ({ { busL, 1048 }, { 560, 1048 }, { 560, 1066 } }, on);
    wire ({ { 372, 1080 }, { 420, 1080 } }, echo);
    wire ({ { 500, 1080 }, { 540, 1080 } }, echo);
    wire ({ { 372, 1150 }, { 420, 1150 } }, rev);
    wire ({ { 500, 1150 }, { 580, 1150 }, { 580, 1094 } }, rev);
    wire ({ { 620, 1080 }, { 680, 1080 } }, on);
    // canal droit
    const bool d = on && dual;
    wire ({ { busR, 1236 }, { 300, 1236 } }, d && send);
    wire ({ { busR, 1300 }, { 300, 1300 } }, d && rev);
    wire ({ { busR, 1208 }, { 560, 1208 }, { 560, 1222 } }, d);
    wire ({ { 372, 1236 }, { 420, 1236 } }, d && echo);
    wire ({ { 500, 1236 }, { 540, 1236 } }, d && echo);
    wire ({ { 372, 1300 }, { 420, 1300 } }, d && rev);
    wire ({ { 500, 1300 }, { 580, 1300 }, { 580, 1250 } }, d && rev);
    wire ({ { 620, 1236 }, { 680, 1236 } }, d);
    if (! dual) wire ({ { 650, 1080 }, { 650, 1236 }, { 680, 1236 } }, on);          // SINGLE : même signal à droite

    for (const auto& i : ins)
        box ({ 70, i.y - 11, 56, 22 }, i.n, on && i.type == sel && (! i.right || dual));
    box ({ 146, 1001, 90, 22 }, "MIC AMP", on && sel == 0);
    box ({ 146, 1029, 90, 22 }, "MIC AMP", on && sel == 0 && dual);
    box ({ 300, 1001, 72, 26 }, "VU", on);
    box ({ 300, 1064, 72, 32 }, "ECHO", echo, "TAPE " + String (M.h[0] ? "1" : "") + (M.h[1] ? "2" : "") + (M.h[2] ? "3" : ""));
    box ({ 420, 1066, 80, 28 }, "TONE", echo);
    box ({ 540, 1064, 80, 32 }, "AMP", on, "H/M/L");
    box ({ 300, 1136, 72, 28 }, "REVERB", rev);
    box ({ 420, 1136, 80, 28 }, "MIC AMP", rev);
    box ({ 680, 1066, 80, 28 }, "OUT L", on);
    box ({ 300, 1220, 72, 32 }, "ECHO", d && echo, "TAPE");
    box ({ 420, 1222, 80, 28 }, "TONE", d && echo);
    box ({ 540, 1220, 80, 32 }, "AMP", d, "H/M/L");
    box ({ 300, 1286, 72, 28 }, "REVERB", d && rev);
    box ({ 420, 1286, 80, 28 }, "MIC AMP", d && rev);
    box ({ 680, 1222, 80, 28 }, "OUT R", on);
    g.setFont (font (1, 8)); g.setColour (col::dim);
    g.drawText ("BASS / TREBLE", Rectangle<float> (410, 1098, 100, 14), Justification::centred);
    // temps des têtes
    const auto pr = proc.currentParams();
    const double d1 = ss::head1Seconds (pr) * 1000.0;
    String t = "MODE " + String (md < 11 ? String (md + 1) : String ("REVERB ONLY"));
    if (M.echo)
    {
        t << dot;
        bool first = true;
        for (int h = 0; h < 3; ++h) if (M.h[h]) { t << (first ? "" : "  ") << "H" << (h + 1) << " " << (int) std::lround (d1 * (h + 1)) << " ms"; first = false; }
        if (pr.sync) t << dot << "SYNC " << ss::noteName (ss::noteIndexFromKnob (pr.repeatRate));
    }
    g.setFont (font (1, 10).withExtraKerningFactor (0.06f)); g.setColour (col::green.withAlpha (on ? 0.95f : 0.4f));
    g.drawText (t, Rectangle<float> (300, 1356, 480, 18), Justification::centredLeft);
}

void SpaceSonarEditor::paintLower (Graphics& g)
{
    auto pill = [&] (Rectangle<float> r, const String& lab, bool on)
    {
        pillBox (g, r, on ? col::pillOn : col::pill);
        g.setColour (on ? Colours::white : col::text.withAlpha (0.78f));
        g.setFont (font (1, 9));
        g.drawText (lab, r, Justification::centred);
    };
    pill (Rectangle<float> (110, 26).withCentre ({ 905, 1046 }), "TEMPO SYNC", proc.prm.sync->load() > 0.5f);
    const int st = (int) proc.prm.stereo->load();
    pill (Rectangle<float> (80, 26).withCentre ({ 905, 1146 }), "SINGLE", st == 0);
    pill (Rectangle<float> (80, 26).withCentre ({ 905, 1178 }), "DUAL", st == 1);
    const int rr = (int) proc.prm.rateRange->load();
    pill (Rectangle<float> (80, 26).withCentre ({ 1120, 1046 }), "NORMAL", rr == 0);
    pill (Rectangle<float> (66, 26).withCentre ({ 1080, 1078 }), "SLOW", rr == 1);
    pill (Rectangle<float> (66, 26).withCentre ({ 1160, 1078 }), "FAST", rr == 2);
    // formes d'onde du LFO
    const int wv = (int) proc.prm.lfoWave->load();
    for (int kk = 0; kk < 6; ++kk)
    {
        const auto r = Rectangle<float> (36, 24).withCentre ({ 1300.0f + 44.0f * (float) kk, 1046 });
        g.setColour (kk == wv ? col::pillOn : col::pill); g.fillRoundedRectangle (r, 5);
        Path pth;
        for (int j = 0; j <= 24; ++j)
        {
            const float u = (float) j / 24.0f;
            float v;
            switch (kk)
            {
                case 0: v = std::sin (u * MathConstants<float>::twoPi); break;
                case 1: v = 1.0f - 4.0f * std::abs (u - 0.5f); break;
                case 2: v = 1.0f - 2.0f * u; break;
                case 3: v = 2.0f * u - 1.0f; break;
                case 4: v = u < 0.5f ? 1.0f : -1.0f; break;
                default: { static const float rv[5] = { 0.2f, -0.7f, 0.9f, -0.3f, 0.5f }; v = rv[jmin (4, (int) (u * 5.0f))]; break; }
            }
            const Point<float> pt (r.getCentreX() - 12.0f + 24.0f * u, r.getCentreY() - 6.0f * v);
            if (j == 0) pth.startNewSubPath (pt); else pth.lineTo (pt);
        }
        g.setColour (Colours::white.withAlpha (kk == wv ? 1.0f : 0.75f));
        g.strokePath (pth, PathStrokeType (1.4f));
    }
    // arcs de valeur des potards noirs
    for (size_t i = 0; i < knobs.size(); ++i)
    {
        const auto& kp = knobPos[i];
        if (! kp.black) continue;
        const float v = knobs[i]->value01();
        const float rad = kp.r + 7.0f;
        Path bg; bg.addCentredArc (kp.x, kp.y, rad, rad, 0, degreesToRadians (-135.0f), degreesToRadians (135.0f), true);
        g.setColour (Colour (0xff404444)); g.strokePath (bg, PathStrokeType (3.0f, PathStrokeType::curved, PathStrokeType::rounded));
        const bool bip = String (kp.id) == "speed" || String (kp.id) == "master";
        const float a0 = bip ? (String (kp.id) == "master" ? -135.0f + 270.0f * (24.0f / 36.0f) : 0.0f) : -135.0f;
        const float a1 = -135.0f + 270.0f * v;
        if (std::abs (a1 - a0) > 0.5f)
        {
            Path fg; fg.addCentredArc (kp.x, kp.y, rad, rad, 0, degreesToRadians (jmin (a0, a1)), degreesToRadians (jmax (a0, a1)), true);
            g.setColour (col::green); g.strokePath (fg, PathStrokeType (3.0f, PathStrokeType::curved, PathStrokeType::rounded));
        }
    }
}

//==============================================================================
//  Souris
//==============================================================================
void SpaceSonarEditor::beginUndo() { undoBefore = proc.snapshot(); undoOpen = true; }
void SpaceSonarEditor::endUndo()
{
    if (! undoOpen) return;
    undoOpen = false;
    if (proc.snapshot() != undoBefore) { proc.pushUndo (undoBefore); askRepaint ({ 1432, 26, 58, 44 }); }
}

void SpaceSonarEditor::setParamUndoable (const char* id, float realValue)
{
    const auto before = proc.snapshot();
    proc.setParam (id, realValue);
    if (proc.snapshot() != before) proc.pushUndo (before);
    repaint();
}

void SpaceSonarEditor::mouseMove (const MouseEvent& e)
{
    if (e.eventComponent != this) return;
    const auto p = e.position / k();
    int z = 0;
    if (Rectangle<float> (20, 29, 36, 38).contains (p)) z = 1;
    else if (Rectangle<float> (384, 29, 36, 38).contains (p)) z = 2;
    if (z != hoverZone) { hoverZone = z; askRepaint ({ 14, 22, 412, 52 }); }
}

void SpaceSonarEditor::mouseUp (const MouseEvent&) { endUndo(); }

void SpaceSonarEditor::mouseDown (const MouseEvent& e)
{
    if (e.eventComponent != this)                      // geste sur une commande : point d'annulation
    {
        if (! e.mods.isPopupMenu()) beginUndo();
        return;
    }
    const auto p = e.position / k();
    auto in = [&] (float x, float y, float w, float h) { return Rectangle<float> (x, y, w, h).contains (p); };
    auto inC = [&] (float cx, float cy, float w, float h) { return Rectangle<float> (w, h).withCentre ({ cx, cy }).contains (p); };
    const bool rc = e.mods.isPopupMenu();

    // zones liées à un paramètre : clic droit = MIDI Learn
    struct Zone { Rectangle<float> r; const char* id; };
    const Zone zones[] = {
        { { 1190, 26, 110, 44 }, "power" }, { Rectangle<float> (110, 26).withCentre ({ 905, 1046 }), "sync" },
        { Rectangle<float> (80, 60).withCentre ({ 905, 1162 }), "stereo" }, { Rectangle<float> (170, 64).withCentre ({ 1120, 1062 }), "rateRange" },
        { { 1278, 1032, 266, 28 }, "lfoWave" }, { { 1252, 696, 220, 84 }, "echoCancel" },
        { { 128, 696, 140, 84 }, "inputSel" }, { { 276, 696, 154, 84 }, "inputSel" }, { { 438, 696, 140, 84 }, "inputSel" } };
    if (rc)
    {
        for (const auto& z : zones) if (z.r.contains (p)) { showParamMenu (z.id); return; }
        if (! in (0, 0, 1600, 96)) showContextMenu();
        return;
    }

    // en-tête
    if (in (20, 29, 400, 38))
    {
        if (statusAlpha > 0.05f) { statusTime = -10.0; return; }
        beginUndo();
        if (in (20, 29, 36, 38)) prevNextPreset (-1);
        else if (in (384, 29, 36, 38)) prevNextPreset (1);
        else { undoOpen = false; showPresetMenu(); }
        return;
    }
    if (in (1190, 26, 110, 44)) { setParamUndoable ("power", proc.prm.power->load() > 0.5f ? 0.0f : 1.0f); return; }
    if (in (1314, 26, 52, 44) || in (1366, 26, 52, 44))
    {
        proc.switchAB (in (1314, 26, 52, 44) ? 0 : 1);
        showStatus (proc.abSlot == 0 ? "SETTINGS A" : "SETTINGS B");
        repaint();
        return;
    }
    if (in (1432, 26, 29, 44)) { if (proc.undo()) showStatus ("UNDO"); repaint(); return; }
    if (in (1461, 26, 29, 44)) { if (proc.redo()) showStatus ("REDO"); repaint(); return; }
    if (in (1500, 26, 80, 44)) { showContextMenu(); return; }

    beginUndo();
    // bandeau des prises : choix de l'entrée, ECHO CANCEL
    if (in (128, 696, 140, 84)) { proc.setParam ("inputSel", 0); return; }
    if (in (276, 696, 154, 84)) { proc.setParam ("inputSel", 1); return; }
    if (in (438, 696, 140, 84)) { proc.setParam ("inputSel", 2); return; }
    if (in (1252, 696, 220, 84)) { proc.setParam ("echoCancel", proc.prm.echoCancel->load() > 0.5f ? 0.0f : 1.0f); return; }
    // panneau étendu
    if (inC (905, 1046, 110, 26)) { proc.setParam ("sync", proc.prm.sync->load() > 0.5f ? 0.0f : 1.0f); return; }
    if (inC (905, 1146, 80, 26)) { proc.setParam ("stereo", 0); return; }
    if (inC (905, 1178, 80, 26)) { proc.setParam ("stereo", 1); return; }
    if (inC (1120, 1046, 80, 26)) { proc.setParam ("rateRange", 0); return; }
    if (inC (1080, 1078, 66, 26)) { proc.setParam ("rateRange", 1); return; }
    if (inC (1160, 1078, 66, 26)) { proc.setParam ("rateRange", 2); return; }
    for (int kk = 0; kk < 6; ++kk)
        if (inC (1300.0f + 44.0f * (float) kk, 1046, 38, 26)) { proc.setParam ("lfoWave", (float) kk); return; }
}

//==============================================================================
//  Presets
//==============================================================================
void SpaceSonarEditor::prevNextPreset (int dir)
{
    const int n = (int) SpaceSonarProcessor::factoryPresets().size();
    const int cur = proc.currentPreset.load();
    proc.loadFactoryPreset (((cur < 0 ? 0 : cur) + dir + n) % n);
    proc.updateHostDisplay (AudioProcessor::ChangeDetails().withProgramChanged (true));
    repaint();
}

void SpaceSonarEditor::askUserPresetName (int slot)
{
    nameDialog = std::make_unique<AlertWindow> ("Save user preset " + String (slot), "Preset name:", MessageBoxIconType::NoIcon, this);
    nameDialog->addTextEditor ("name", proc.presetName.isNotEmpty() ? proc.presetName : "User " + String (slot));
    nameDialog->addButton ("Save", 1, KeyPress (KeyPress::returnKey));
    nameDialog->addButton ("Cancel", 0, KeyPress (KeyPress::escapeKey));
    nameDialog->enterModalState (true, ModalCallbackFunction::create ([this, slot] (int r)
    {
        if (r == 1 && nameDialog != nullptr)
        {
            auto n = nameDialog->getTextEditorContents ("name").trim();
            if (n.isEmpty()) n = "User " + String (slot);
            proc.saveUserPreset (slot, n);
            showStatus ("SAVED  " + n);
            repaint();
        }
        nameDialog.reset();
    }), false);
}

void SpaceSonarEditor::showPresetMenu()
{
    PopupMenu m;
    m.setLookAndFeel (&menuLnf);
    NineHandsMenu::addHeader (m, menuLnf.f, NineHandsMenu::icoPreset, col::accent, "Presets", proc.presetName);
    m.addSectionHeader ("Factory presets");
    const auto& fp = SpaceSonarProcessor::factoryPresets();
    for (int i = 0; i < (int) fp.size(); ++i)
        m.addItem (100 + i, String (i + 1) + ". " + fp[(size_t) i].name, true, proc.currentPreset.load() == i && ! proc.presetDirty.load());
    m.addSeparator();
    PopupMenu load, save;
    for (int s = 1; s <= 9; ++s)
    {
        const auto n = proc.userPresetName (s);
        load.addItem (200 + s, "User " + String (s) + (n.isNotEmpty() ? " : " + n : String ("  (empty)")), n.isNotEmpty());
        save.addItem (300 + s, "User " + String (s) + (n.isNotEmpty() ? " : " + n + "  (overwrite)" : String()));
    }
    m.addSubMenu ("Load user preset", load);
    m.addSubMenu ("Save user preset", save);
    const auto area = Rectangle<float> (20, 29, 400, 38).transformedBy (AffineTransform::scale (k())).getSmallestIntegerContainer();
    m.showMenuAsync (PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (area)),
        [safe = Component::SafePointer<SpaceSonarEditor> (this)] (int r)
        {
            if (safe == nullptr || r == 0) return;
            auto& pr = safe->proc;
            const auto before = pr.snapshot();
            if (r >= 100 && r < 200) { pr.loadFactoryPreset (r - 100); pr.updateHostDisplay (AudioProcessor::ChangeDetails().withProgramChanged (true)); }
            else if (r > 200 && r < 300) pr.loadUserPreset (r - 200);
            else if (r > 300 && r < 400) { safe->askUserPresetName (r - 300); return; }
            if (pr.snapshot() != before) pr.pushUndo (before);
            safe->repaint();
        });
}

//==============================================================================
//  Menus clic droit (design d'Octopus), toujours ouverts à l'endroit du clic
//==============================================================================
static String learnName (RangedAudioParameter* p) { return p != nullptr ? p->getName (40) : String(); }

PopupMenu SpaceSonarEditor::buildContextMenu (const String& paramId)
{
    using namespace NineHandsMenu;
    PopupMenu menu;
    menu.setLookAndFeel (&menuLnf);
    auto ccText = [this] (int pi) { const int cc = proc.ccForParam (pi); return cc >= 0 ? "CC " + String (cc) : String ("not assigned"); };
    const int lt = proc.learnParam.load();
    auto& params = proc.getParameters();

    if (paramId.isNotEmpty())
    {
        // menu d'une commande (tronc commun) : en-tête, MIDI Learn, Clear MIDI, Cancel MIDI learn
        const int pi = proc.paramIndexOf (paramId);
        auto* rp = dynamic_cast<RangedAudioParameter*> (params[pi]);
        const String name = learnName (rp);
        const bool sw = dynamic_cast<AudioParameterBool*> (rp) != nullptr || dynamic_cast<AudioParameterChoice*> (rp) != nullptr;
        addHeader (menu, menuLnf.f, sw ? icoSwitch : icoKnob, col::accent, name, "MIDI: " + ccText (pi));
        menu.addItem (1000 + pi, "MIDI Learn: " + name, true, lt == pi);
        menu.addItem (1100 + pi, "Clear MIDI: " + name, proc.ccForParam (pi) >= 0);
        menu.addItem (1200, "Cancel MIDI learn", lt >= 0);
        return menu;
    }

    addHeader (menu, menuLnf.f, icoPedal, col::accent, "Space Sonar 209", "Tape echo & spring reverb  v" + String (JucePlugin_VersionString));
    PopupMenu factory;
    const auto& fp = SpaceSonarProcessor::factoryPresets();
    for (int i = 0; i < (int) fp.size(); ++i)
        addItem (factory, 100 + i, String (i + 1) + ". " + fp[(size_t) i].name, icoDot, true, proc.currentPreset.load() == i && ! proc.presetDirty.load());
    menu.addSubMenu ("Factory presets", factory);
    PopupMenu load, save;
    for (int s = 1; s <= 9; ++s)
    {
        const auto n = proc.userPresetName (s);
        load.addItem (200 + s, "User " + String (s) + (n.isNotEmpty() ? " : " + n : String ("  (empty)")), n.isNotEmpty());
        save.addItem (300 + s, "User " + String (s) + (n.isNotEmpty() ? " : " + n + "  (overwrite)" : String()));
    }
    menu.addSubMenu ("Load user preset", load);
    menu.addSubMenu ("Save user preset", save);
    menu.addSeparator();

    PopupMenu settings;
    settings.addSectionHeader ("Input");
    static const char* inN[3] = { "Mic", "P.A.", "Instrument" };
    for (int i = 0; i < 3; ++i) settings.addItem (510 + i, String ("Input: ") + inN[i], true, (int) proc.prm.inputSel->load() == i);
    settings.addSectionHeader ("Tape");
    settings.addItem (501, "Tempo sync (Repeat Rate in note values)", true, proc.prm.sync->load() > 0.5f);
    static const char* rrN[3] = { "Normal", "Slow (x 2)", "Fast (x 0.5)" };
    for (int i = 0; i < 3; ++i) settings.addItem (520 + i, String ("Rate range: ") + rrN[i], true, (int) proc.prm.rateRange->load() == i);
    settings.addSectionHeader ("Output");
    settings.addItem (530, "Stereo mode: Single (mono, original)", true, (int) proc.prm.stereo->load() == 0);
    settings.addItem (531, "Stereo mode: Dual (left / right)", true, (int) proc.prm.stereo->load() == 1);
    settings.addItem (502, "Echo cancel", true, proc.prm.echoCancel->load() > 0.5f);
    menu.addSubMenu ("Settings", settings);
    menu.addSeparator();

    PopupMenu midi;
    midi.addSectionHeader ("MIDI Learn");
    for (int pi = 0; pi < params.size(); ++pi)
    {
        auto* rp = dynamic_cast<RangedAudioParameter*> (params[pi]);
        const bool sw = dynamic_cast<AudioParameterBool*> (rp) != nullptr || dynamic_cast<AudioParameterChoice*> (rp) != nullptr;
        addItem (midi, 1000 + pi, "Learn: " + learnName (rp) + " (" + ccText (pi) + ")", sw ? icoSwitch : icoMidi, true, lt == pi);
    }
    midi.addSeparator();
    midi.addItem (1200, "Cancel MIDI learn", lt >= 0);
    midi.addItem (1201, "Clear all MIDI assignments");
    midi.addSeparator();
    NineHandsWindow::addProgramChangeInfo (midi);
    menu.addSubMenu ("MIDI", midi);
    menu.addSeparator();

    const char* names[numSizes] = { "60 %", "75 %", "100 %" };
    const int heights[numSizes] = { kH, 1050, 1400 };
    menu.addSubMenu ("Plugin size", NineHandsWindow::sizeMenu (JucePlugin_Name, numSizes, names, heights, getHeight()));
    menu.addSubMenu ("Glow intensity", NineHandsWindow::glowMenu (roundToInt (glow * 100.0f)));
    return menu;
}

void SpaceSonarEditor::showParamMenu (const String& paramId) { auto m = buildContextMenu (paramId); m.showMenuAsync (PopupMenu::Options().withMousePosition(),
    [safe = Component::SafePointer<SpaceSonarEditor> (this)] (int r)
    {
        if (safe == nullptr || r == 0) return;
        auto& pr = safe->proc;
        if (r >= 1000 && r < 1100) pr.learnParam = (pr.learnParam.load() == r - 1000) ? -1 : r - 1000;
        else if (r >= 1100 && r < 1200) { pr.clearMidiFor (r - 1100); safe->showStatus (learnName (dynamic_cast<RangedAudioParameter*> (pr.getParameters()[r - 1100])).toUpperCase() + "  MIDI CLEARED"); }
        else if (r == 1200) pr.learnParam = -1;
    }); }

void SpaceSonarEditor::showContextMenu()
{
    auto menu = buildContextMenu();
    menu.showMenuAsync (PopupMenu::Options().withMousePosition(),
        [safe = Component::SafePointer<SpaceSonarEditor> (this)] (int r)
        {
            if (safe == nullptr || r == 0) return;
            auto& ed = *safe;
            auto& pr = ed.proc;
            int inst = pr.uiSizeIdx.load();
            const int sz = NineHandsWindow::handleSize (r, JucePlugin_Name, numSizes, inst);
            if (sz >= 0) { pr.uiSizeIdx = inst; ed.setWindowWidth (sizeWidth (sz)); return; }
            if (const int pc = NineHandsWindow::handleGlow (r, JucePlugin_Name)) { ed.glow = (float) pc / 100.0f; ed.showStatus ("GLOW  " + String (pc) + " %"); ed.repaint(); return; }
            const auto before = pr.snapshot();
            if (r >= 100 && r < 200) { pr.loadFactoryPreset (r - 100); pr.updateHostDisplay (AudioProcessor::ChangeDetails().withProgramChanged (true)); }
            else if (r > 200 && r < 300) pr.loadUserPreset (r - 200);
            else if (r > 300 && r < 400) { ed.askUserPresetName (r - 300); return; }
            else if (r == 501) pr.setParam ("sync", pr.prm.sync->load() > 0.5f ? 0.0f : 1.0f);
            else if (r == 502) pr.setParam ("echoCancel", pr.prm.echoCancel->load() > 0.5f ? 0.0f : 1.0f);
            else if (r >= 510 && r < 513) pr.setParam ("inputSel", (float) (r - 510));
            else if (r >= 520 && r < 523) pr.setParam ("rateRange", (float) (r - 520));
            else if (r == 530 || r == 531) pr.setParam ("stereo", (float) (r - 530));
            else if (r >= 1000 && r < 1100) pr.learnParam = (pr.learnParam.load() == r - 1000) ? -1 : r - 1000;
            else if (r == 1200) pr.learnParam = -1;
            else if (r == 1201) { pr.learnParam = -1; for (auto& x : pr.ccMap) x = -1; ed.showStatus ("ALL MIDI CLEARED"); }
            if (pr.snapshot() != before) pr.pushUndo (before);
            ed.repaint();
        });
}

void SpaceSonarEditor::showStatus (const String& s) { status = s; statusTime = nowMs(); }

//==============================================================================
//  Ligne d'état
//==============================================================================
String SpaceSonarEditor::statusFor (RangedAudioParameter* rp, float v01) const
{
    const String id = rp->paramID;
    const float v = rp->convertFrom0to1 (v01);
    String txt = rp->getName (40).toUpperCase() + "  ";
    if (id == "power") return v > 0.5f ? "SPACE SONAR 209 ON" : "BYPASSED";
    if (dynamic_cast<AudioParameterBool*> (rp) != nullptr) return txt + (v > 0.5f ? "ON" : "OFF");
    if (id == "mode")
    {
        const int m = (int) std::lround (v);
        const auto& M = ss::mode (m);
        if (! M.echo) return "MODE  REVERB ONLY";
        String h; for (int i = 0; i < 3; ++i) if (M.h[i]) h << (h.isEmpty() ? "" : "+") << (i + 1);
        return "MODE " + String (m + 1) + dot + "HEAD " + h + (M.reverb ? " + REVERB" : "");
    }
    if (id == "repeatRate" || id == "rateRange" || id == "speed" || id == "sync")
    {
        auto pr = proc.currentParams();
        const double d1 = ss::head1Seconds (pr) * 1000.0;
        const int lead = ss::leadingHead (pr.mode);
        String t = id == "rateRange" ? "RATE RANGE  " + rp->getCurrentValueAsText().toUpperCase() : id == "speed" ? "SPEED  " + rp->getCurrentValueAsText()
                 : id == "sync" ? String ("TEMPO SYNC  ") + (pr.sync ? "ON" : "OFF") : "REPEAT RATE  " + String (v, 1);
        if (pr.sync) return t + dot + ss::noteName (ss::noteIndexFromKnob (pr.repeatRate)) + "  " + String ((int) std::lround (d1 * lead)) + " ms";
        return t + dot + "HEAD " + String (lead) + "  " + String ((int) std::lround (d1 * lead)) + " ms";
    }
    return txt + rp->getCurrentValueAsText().toUpperCase();
}

//==============================================================================
//  30 Hz
//==============================================================================
void SpaceSonarEditor::timerCallback()
{
    const double t = nowMs();
    if ((void*) getPeer() != lastPeer || t - lastRendererCheck > 1000.0) { useSoftwareRenderer(); hostLock.track (*this); }
    // chien de garde : une demande de redessin non servie depuis 1,5 s -> moteur logiciel réappliqué, tout redessiné
    if (repaintAskedMs > 0.0 && t - repaintAskedMs > 1500.0 && isShowing()) { lastPeer = nullptr; useSoftwareRenderer(); repaint(); repaintAskedMs = 0.0; }

    if (++scaleTick >= 8)
    {
        scaleTick = 0;
        const float phys = jlimit (1.0f, 4.0f, (float) Component::getApproximateScaleFactorForComponent (this));
        if (std::abs (phys - cacheScale) > 0.01f && resizeSettle == 0) resizeSettle = 1;
    }
    if (resizeSettle > 0 && --resizeSettle == 0) rebuildCache();

    if (t - glowCheck > 3000.0)
    {
        glowCheck = t;
        const float gl = (float) NineHandsGlow::percent (JucePlugin_Name) / 100.0f;
        if (std::abs (gl - glow) > 0.001f) { glow = gl; repaint(); }
    }

    // réglages qui changent (souris, automation, MIDI) -> ligne d'état + redessin
    auto& params = proc.getParameters();
    bool full = false;
    for (int i = 0; i < params.size() && i < (int) lastVals.size(); ++i)
    {
        const float v = params[i]->getValue();
        if (std::abs (v - lastVals[(size_t) i]) <= 1.0e-5f) continue;
        lastVals[(size_t) i] = v;
        auto* rp = dynamic_cast<RangedAudioParameter*> (params[i]);
        if (rp == nullptr) continue;
        showStatus (statusFor (rp, v));
        if (rp->paramID != "power" && rp->paramID != "echoCancel") proc.presetDirty = true;
        bool isKnob = false;
        for (size_t kk = 0; kk < knobPos.size(); ++kk)
            if (rp->paramID == knobPos[kk].id)
            {
                isKnob = true;
                if (knobPos[kk].black) askRepaint (Rectangle<float> (knobPos[kk].r * 2 + 22, knobPos[kk].r * 2 + 22).withCentre ({ knobPos[kk].x, knobPos[kk].y }));
                if (String (knobPos[kk].id) == "mode" || String (knobPos[kk].id) == "repeatRate" || String (knobPos[kk].id) == "speed")
                    askRepaint ({ 50, 990, 750, 390 });
            }
        if (! isKnob) full = true;
    }
    if (full) { if (repaintAskedMs <= 0.0) repaintAskedMs = t; repaint(); }

    // MIDI Learn
    const int lp = proc.learnParam.load();
    if (lp >= 0) showStatus ("MIDI LEARN" + dot + learnName (dynamic_cast<RangedAudioParameter*> (params[lp])));
    const int ll = proc.lastLearnedCc.exchange (-1);
    if (ll >= 0)
    {
        const int pi = proc.ccMap[ll].load();
        if (pi >= 0) showStatus ("CC " + String (ll) + dot + learnName (dynamic_cast<RangedAudioParameter*> (params[pi])));
    }

    // ligne d'état : ~2,5 s après le dernier mouvement, puis fondu vers la barre de presets
    const float target = (t - statusTime < 2500.0) ? 1.0f : 0.0f;
    const float na = target > statusAlpha ? jmin (1.0f, statusAlpha + 0.28f) : jmax (0.0f, statusAlpha - 0.11f);
    if (std::abs (na - statusAlpha) > 0.001f) { statusAlpha = na; askRepaint ({ 14, 22, 412, 52 }); }

    // VU (aiguille), LED, vumètres
    const bool on = proc.prm.power->load() > 0.5f;
    const float pl = on ? jmin (1.0f, powerLevel + 0.45f) : jmax (0.0f, powerLevel - 0.28f);
    if (std::abs (pl - powerLevel) > 0.001f) { powerLevel = pl; askRepaint ({ 1290, 594, 56, 56 }); askRepaint ({ 245, 387, 202, 114 }); }
    {
        const float lvl = proc.vu.load();
        const float db = 20.0f * std::log10 (jmax (1.0e-5f, lvl / 0.9f)) + 18.0f;         // 0 VU = -18 dBFS
        const float volts = jlimit (0.0f, 1.0f, (std::pow (10.0f, jlimit (-20.0f, 3.5f, db) / 20.0f) - 0.1f) / (1.413f - 0.1f));
        const float ang = -46.0f + 92.0f * volts;
        const float shown = vuAngle + (ang - vuAngle) * 0.35f;
        if (std::abs (shown - vuAngle) > 0.04f) { vuAngle = shown; askRepaint ({ 252, 394, 188, 100 }); }
    }
    {
        const float pk = proc.peak.exchange (0.0f);
        const float tgt = pk > 0.89f ? 1.0f : 0.0f;
        const float np = tgt > peakLevel ? 1.0f : jmax (0.0f, peakLevel - 0.12f);
        if (std::abs (np - peakLevel) > 0.001f) { peakLevel = np; askRepaint ({ 172, 408, 56, 56 }); }
    }
    bool meters = false;
    for (int c = 0; c < 2; ++c)
    {
        const float di = 20.0f * std::log10 (jmax (1.0e-6f, proc.inPeak[c].exchange (0.0f)));
        const float dO = 20.0f * std::log10 (jmax (1.0e-6f, proc.outPeak[c].exchange (0.0f)));
        const float ni = jmax (di, meterIn[c] - 1.6f), no = jmax (dO, meterOut[c] - 1.6f);
        if (std::abs (ni - meterIn[c]) > 0.05f || std::abs (no - meterOut[c]) > 0.05f) meters = true;
        meterIn[c] = jmax (-90.0f, ni); meterOut[c] = jmax (-90.0f, no);
    }
    if (meters) askRepaint ({ 1070, 14, 90, 52 });
}
