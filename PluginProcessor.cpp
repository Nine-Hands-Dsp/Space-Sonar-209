#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

//==============================================================================
static NormalisableRange<float> logRange (float lo, float hi)
{
    return NormalisableRange<float> (lo, hi,
        [] (float a, float b, float v) { return a * std::pow (b / a, v); },
        [] (float a, float b, float x) { return std::log (x / a) / std::log (b / a); });
}

static String hzText (float v, int)
{
    if (v >= 1000.0f) return String (v / 1000.0f, v >= 10000.0f ? 1 : 2) + " kHz";
    return String ((int) std::round (v)) + " Hz";
}

AudioProcessorValueTreeState::ParameterLayout SpaceSonarProcessor::createLayout()
{
    AudioProcessorValueTreeState::ParameterLayout l;
    auto ten = [] (const char* id, const char* name, float def)
    {
        return std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (0.0f, 10.0f, 0.01f), def,
                                                      AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, 1); }));
    };
    StringArray modes; for (int i = 1; i <= 11; ++i) modes.add (String (i)); modes.add ("Reverb Only");

    l.add (std::make_unique<AudioParameterBool>   (ParameterID { "power", 1 }, "Power", true));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "inputSel", 1 }, "Input", StringArray { "Mic", "P.A.", "Inst" }, 2));
    l.add (ten ("micVol", "Mic Volume", 5.0f));
    l.add (ten ("paVol", "From P.A. Volume", 5.0f));
    l.add (ten ("instVol", "Instrument Volume", 5.0f));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "echoOn", 1 }, "Echo / Normal", StringArray { "Normal", "Echo" }, 1));
    l.add (std::make_unique<AudioParameterBool>   (ParameterID { "echoCancel", 1 }, "Echo Cancel", false));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "mode", 1 }, "Mode Selector", modes, 2));
    l.add (ten ("bass", "Bass", 5.0f));
    l.add (ten ("treble", "Treble", 5.0f));
    l.add (ten ("reverbVol", "Reverb Volume", 3.5f));
    l.add (ten ("repeatRate", "Repeat Rate", 5.0f));
    l.add (ten ("intensity", "Intensity", 4.5f));
    l.add (ten ("echoVol", "Echo Volume", 6.0f));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "outLevel", 1 }, "Output Level", StringArray { "H", "M", "L" }, 1));
    l.add (std::make_unique<AudioParameterBool>   (ParameterID { "sync", 1 }, "Tempo Sync", false));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "stereo", 1 }, "Stereo Mode", StringArray { "Single", "Dual" }, 1));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "rateRange", 1 }, "Rate Range", StringArray { "Normal", "Slow", "Fast" }, 0));
    l.add (std::make_unique<AudioParameterFloat>  (ParameterID { "speed", 1 }, "Speed", NormalisableRange<float> (-1.0f, 1.0f, 0.001f), 0.0f,
                                                   AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
                                                   { const int pc = (int) std::lround ((std::pow (2.0, 0.4 * v) - 1.0) * 100.0); return (pc > 0 ? "+" : "") + String (pc) + " %"; })));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "lfoWave", 1 }, "LFO Wave", StringArray { "Sine", "Triangle", "Ramp Down", "Ramp Up", "Square", "Random" }, 0));
    l.add (ten ("lfoRate", "LFO Rate", 4.0f));
    l.add (ten ("lfoShape", "LFO Waveform", 3.0f));
    l.add (ten ("lfoDepth", "LFO Depth", 0.0f));
    l.add (ten ("condition", "Condition", 3.0f));
    l.add (ten ("wf", "Wow & Flutter", 3.0f));
    l.add (ten ("motor", "Motor Torque", 5.0f));
    l.add (std::make_unique<AudioParameterFloat>  (ParameterID { "lowCut", 1 }, "Low Cut", logRange (20.0f, 1000.0f), 20.0f,
                                                   AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int i) { return v <= 20.5f ? String ("Off") : hzText (v, i); })));
    l.add (std::make_unique<AudioParameterFloat>  (ParameterID { "highCut", 1 }, "High Cut", logRange (1000.0f, 20000.0f), 20000.0f,
                                                   AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int i) { return v >= 19900.0f ? String ("Off") : hzText (v, i); })));
    l.add (std::make_unique<AudioParameterFloat>  (ParameterID { "master", 1 }, "Master", NormalisableRange<float> (-24.0f, 12.0f, 0.01f), 0.0f,
                                                   AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return (v > 0.05f ? "+" : "") + String (v, 1) + " dB"; })));
    return l;
}

//==============================================================================
SpaceSonarProcessor::SpaceSonarProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "SPACESONAR", createLayout())
{
    auto g = [this] (const char* id) { return apvts.getRawParameterValue (id); };
    prm = { g ("power"), g ("inputSel"), g ("micVol"), g ("paVol"), g ("instVol"), g ("echoOn"), g ("echoCancel"), g ("mode"), g ("bass"),
            g ("treble"), g ("reverbVol"), g ("repeatRate"), g ("intensity"), g ("echoVol"), g ("outLevel"), g ("sync"), g ("stereo"),
            g ("rateRange"), g ("speed"), g ("lfoWave"), g ("lfoRate"), g ("lfoShape"), g ("lfoDepth"), g ("condition"), g ("wf"), g ("motor"),
            g ("lowCut"), g ("highCut"), g ("master") };
    for (auto& v : inPeak) v = 0.0f;
    for (auto& v : outPeak) v = 0.0f;
    for (int i = 0; i < 128; ++i) { ccMap[i] = -1; ccMin[i] = 127; ccMax[i] = 0; }
    for (int i = 0; i < kMaxParams; ++i) { pendingValue[i] = 0.0f; pendingSet[i] = false; }
    jassert (getParameters().size() <= kMaxParams);
    startTimerHz (60);
}

SpaceSonarProcessor::~SpaceSonarProcessor() { stopTimer(); }

bool SpaceSonarProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto in = l.getMainInputChannelSet(), out = l.getMainOutputChannelSet();
    if (out != AudioChannelSet::mono() && out != AudioChannelSet::stereo()) return false;
    return in == AudioChannelSet::mono() || in == AudioChannelSet::stereo();
}

void SpaceSonarProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    maxBlock = jmax (64, samplesPerBlock);
    engine.prepare (sr);
    wet.setSize (2, maxBlock);
    powerSm = prm.power->load() > 0.5f ? 1.0 : 0.0;
    wasOn = powerSm > 0.5;
    setLatencySamples (0);
}

ss::Params SpaceSonarProcessor::currentParams() const
{
    ss::Params p;
    p.inputSel = (int) prm.inputSel->load();
    p.volMic = prm.micVol->load(); p.volPa = prm.paVol->load(); p.volInst = prm.instVol->load();
    p.echoOn = prm.echoOn->load() > 0.5f; p.echoCancel = prm.echoCancel->load() > 0.5f;
    p.mode = (int) prm.mode->load();
    p.bass = prm.bass->load(); p.treble = prm.treble->load(); p.reverbVol = prm.reverbVol->load();
    p.repeatRate = prm.repeatRate->load(); p.intensity = prm.intensity->load(); p.echoVol = prm.echoVol->load();
    p.outLevel = (int) prm.outLevel->load();
    p.sync = prm.sync->load() > 0.5f; p.bpm = hostBpm.load();
    p.dual = (int) prm.stereo->load() == 1;
    p.rateRange = (int) prm.rateRange->load(); p.speed = prm.speed->load();
    p.lfoWave = (int) prm.lfoWave->load(); p.lfoRate = prm.lfoRate->load(); p.lfoShape = prm.lfoShape->load(); p.lfoDepth = prm.lfoDepth->load();
    p.condition = prm.condition->load(); p.wf = prm.wf->load(); p.motor = prm.motor->load();
    p.lowCut = prm.lowCut->load(); p.highCut = prm.highCut->load(); p.masterDb = prm.master->load();
    return p;
}

//==============================================================================
void SpaceSonarProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midi)
{
    ScopedNoDenormals noDenormals;
    const int numIn = jmin (2, getTotalNumInputChannels()), numOut = jmin (2, buffer.getNumChannels());
    const int n = buffer.getNumSamples();
    for (int c = numOut; c < buffer.getNumChannels(); ++c) buffer.clear (c, 0, n);
    if (n == 0 || numOut == 0) return;
    if (n > maxBlock)                                           // bloc plus grand qu'annoncé : découpé, jamais d'allocation
    {
        for (int s0 = 0; s0 < n; s0 += maxBlock)
        {
            AudioBuffer<float> part (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), s0, jmin (maxBlock, n - s0));
            processBlock (part, midi);
        }
        return;
    }

    //-------------------------------------------------------------- MIDI : Learn + CC appris (file vers le thread de messages)
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (m.isProgramChange()) { pendingProgram = m.getProgramChangeNumber(); continue; }
        if (! m.isController()) continue;
        const int cc = m.getControllerNumber(), v = m.getControllerValue();
        const int lp = learnParam.load();
        if (lp >= 0)
        {
            for (auto& x : ccMap) if (x.load() == lp) x = -1;
            ccMap[cc] = lp; learnParam = -1; lastLearnedCc = cc;
            ccMin[cc] = v; ccMax[cc] = v;
        }
        const int pi = ccMap[cc].load();
        if (pi < 0 || pi >= kMaxParams) continue;
        if (v < ccMin[cc].load()) ccMin[cc] = v;
        if (v > ccMax[cc].load()) ccMax[cc] = v;
        const int lo = ccMin[cc].load(), hi = ccMax[cc].load();
        float x = (float) v / 127.0f;
        // course de la pédale apprise : 0 et 100 % exacts même si elle n'envoie que 2 à 116, zones mortes aux butées
        if (hi - lo >= 64) x = jlimit (0.0f, 1.0f, ((float) (v - lo) - 1.0f) / jmax (1.0f, (float) (hi - lo) - 2.0f));
        pendingValue[pi] = x;
        pendingSet[pi] = true;
    }
    midi.clear();

    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto bpm = pos->getBpm()) hostBpm = *bpm;

    const bool on = prm.power->load() > 0.5f;
    if (on && ! wasOn) engine.reset();                          // rallumé : bande effacée (comme l'appareil)
    wasOn = on;

    const float* inL = buffer.getReadPointer (0);
    const float* inR = numIn > 1 ? buffer.getReadPointer (1) : inL;
    float* wL = wet.getWritePointer (0); float* wR = wet.getWritePointer (1);
    const auto p = currentParams();
    engine.process (inL, inR, wL, wR, n, p);

    float inPk[2] = { 0, 0 }, outPk[2] = { 0, 0 };
    float* oL = buffer.getWritePointer (0);
    float* oR = numOut > 1 ? buffer.getWritePointer (1) : nullptr;
    const double aSm = 1.0 - std::exp (-1.0 / (0.02 * sr));
    const double powT = on ? 1.0 : 0.0;
    for (int i = 0; i < n; ++i)
    {
        powerSm += (powT - powerSm) * aSm;
        const float dl = inL[i], dr = inR[i];
        inPk[0] = jmax (inPk[0], std::abs (dl)); inPk[1] = jmax (inPk[1], std::abs (dr));
        const float yl = (float) (dl + powerSm * (wL[i] - dl));
        const float yr = (float) (dr + powerSm * (wR[i] - dr));
        if (oR != nullptr) { oL[i] = yl; oR[i] = yr; }
        else oL[i] = 0.5f * (yl + yr);
        outPk[0] = jmax (outPk[0], std::abs (yl)); outPk[1] = jmax (outPk[1], std::abs (yr));
    }

    auto maxAtomic = [] (std::atomic<float>& a, float v) { float o = a.load(); while (v > o && ! a.compare_exchange_weak (o, v)) {} };
    maxAtomic (inPeak[0], inPk[0]); maxAtomic (inPeak[1], numIn > 1 ? inPk[1] : inPk[0]);
    maxAtomic (outPeak[0], outPk[0]); maxAtomic (outPeak[1], outPk[1]);
    vu = on ? (float) engine.vuLevel : 0.0f;
    maxAtomic (peak, on ? (float) engine.peakHit : 0.0f);
    engine.peakHit = 0.0;
    head1Ms = (float) (engine.currentHead1() * 1000.0);
    lfoNow = (float) engine.lfoView;
}

//==============================================================================
void SpaceSonarProcessor::timerCallback()
{
    const int pc = pendingProgram.exchange (-1);
    if (pc >= 0 && pc < getNumPrograms())
    {
        loadFactoryPreset (pc);
        updateHostDisplay (ChangeDetails().withProgramChanged (true));
    }
    auto& params = getParameters();
    for (int i = 0; i < params.size() && i < kMaxParams; ++i)
        if (pendingSet[i].exchange (false))
        {
            auto* p = params[i];
            p->beginChangeGesture();
            p->setValueNotifyingHost (pendingValue[i].load());
            p->endChangeGesture();
        }
}

void SpaceSonarProcessor::setParam (const char* id, float realValue)
{
    if (auto* p = apvts.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (realValue));
        p->endChangeGesture();
    }
}

int SpaceSonarProcessor::paramIndexOf (const String& id) const
{
    auto& params = getParameters();
    for (int i = 0; i < params.size(); ++i)
        if (auto* wp = dynamic_cast<AudioProcessorParameterWithID*> (params[i]))
            if (wp->paramID == id) return i;
    return -1;
}

int SpaceSonarProcessor::ccForParam (int pi) const
{
    for (int c = 0; c < 128; ++c) if (ccMap[c].load() == pi) return c;
    return -1;
}

void SpaceSonarProcessor::clearMidiFor (int pi)
{
    for (auto& x : ccMap) if (x.load() == pi) x = -1;
    if (learnParam.load() == pi) learnParam = -1;
}

//==============================================================================
//  A / B et annuler / rétablir
//==============================================================================
std::vector<float> SpaceSonarProcessor::snapshot() const
{
    std::vector<float> v;
    for (auto* p : getParameters()) v.push_back (p->getValue());
    return v;
}

void SpaceSonarProcessor::restore (const std::vector<float>& v)
{
    auto& ps = getParameters();
    for (int i = 0; i < ps.size() && i < (int) v.size(); ++i)
        if (std::abs (ps[i]->getValue() - v[(size_t) i]) > 1.0e-6f)
        {
            ps[i]->beginChangeGesture(); ps[i]->setValueNotifyingHost (v[(size_t) i]); ps[i]->endChangeGesture();
        }
}

void SpaceSonarProcessor::pushUndo (const std::vector<float>& before)
{
    undoStack.push_back (before);
    if (undoStack.size() > 100) undoStack.erase (undoStack.begin());
    redoStack.clear();
}

bool SpaceSonarProcessor::undo()
{
    if (undoStack.empty()) return false;
    redoStack.push_back (snapshot());
    restore (undoStack.back()); undoStack.pop_back();
    presetDirty = true;
    return true;
}

bool SpaceSonarProcessor::redo()
{
    if (redoStack.empty()) return false;
    undoStack.push_back (snapshot());
    restore (redoStack.back()); redoStack.pop_back();
    presetDirty = true;
    return true;
}

void SpaceSonarProcessor::switchAB (int slot)
{
    slot = jlimit (0, 1, slot);
    if (slot == abSlot) return;
    abState[abSlot] = savePresetTree();
    abState[abSlot].setProperty ("name", presetName, nullptr);
    abSlot = slot;
    if (abState[slot].isValid()) loadPresetTree (abState[slot], abState[slot].getProperty ("name", presetName).toString());
}

//==============================================================================
const std::vector<SpaceSonarProcessor::FactoryPreset>& SpaceSonarProcessor::factoryPresets()
{
    // mode : index 0..11 (mode 1 = 0, REVERB ONLY = 11) ; repeatRate en synchro : 0..10 -> 14 valeurs de notes
    static const std::vector<FactoryPreset> p = {
        { "Default", {} },
        { "Classic Slapback", { { "mode", 0 }, { "repeatRate", 7.2f }, { "intensity", 1.2f }, { "echoVol", 6.5f } } },
        { "Rockabilly Slap", { { "mode", 0 }, { "repeatRate", 9.0f }, { "intensity", 0.6f }, { "echoVol", 7.2f }, { "treble", 6.0f } } },
        { "Dub Siren", { { "mode", 8 }, { "repeatRate", 3.2f }, { "intensity", 7.6f }, { "echoVol", 7.0f }, { "bass", 6.2f }, { "treble", 3.8f },
                         { "reverbVol", 3.0f }, { "condition", 5.0f } } },
        { "Triple Head Rhythm", { { "mode", 10 }, { "repeatRate", 5.0f }, { "intensity", 4.2f }, { "echoVol", 6.0f }, { "reverbVol", 3.0f } } },
        { "Spring Only", { { "mode", 11 }, { "reverbVol", 6.0f } } },
        { "Runaway Oscillation", { { "mode", 2 }, { "repeatRate", 4.0f }, { "intensity", 9.2f }, { "echoVol", 7.0f }, { "condition", 5.0f }, { "motor", 3.0f } } },
        { "Space Drift", { { "mode", 6 }, { "repeatRate", 4.0f }, { "intensity", 6.0f }, { "echoVol", 6.0f }, { "reverbVol", 4.0f }, { "lfoDepth", 3.0f },
                           { "lfoRate", 2.5f }, { "wf", 5.0f } } },
        { "Wobbly Old Tape", { { "mode", 2 }, { "intensity", 5.0f }, { "condition", 9.0f }, { "wf", 8.0f }, { "treble", 4.0f } } },
        { "Ambient Wash", { { "mode", 10 }, { "repeatRate", 2.0f }, { "rateRange", 1 }, { "intensity", 7.0f }, { "reverbVol", 6.0f }, { "treble", 3.5f },
                            { "lfoDepth", 1.5f }, { "lfoRate", 2.0f } } },
        { "Lead Echo 1/4 (Sync)", { { "sync", 1 }, { "mode", 4 }, { "repeatRate", 6.2f }, { "intensity", 4.0f }, { "echoVol", 5.5f }, { "reverbVol", 2.5f } } },
        { "Metal Lead 7-String", { { "sync", 1 }, { "mode", 4 }, { "repeatRate", 4.6f }, { "intensity", 3.5f }, { "echoVol", 4.5f }, { "reverbVol", 1.5f },
                                   { "lowCut", 250.0f }, { "highCut", 6000.0f } } },
        { "Mic Grit", { { "inputSel", 0 }, { "micVol", 7.0f }, { "mode", 2 }, { "intensity", 4.0f }, { "echoVol", 6.0f } } },
        { "Chopped Vibrato", { { "mode", 0 }, { "repeatRate", 8.0f }, { "intensity", 2.0f }, { "echoVol", 5.0f }, { "lfoWave", 4 }, { "lfoRate", 6.0f },
                               { "lfoShape", 6.0f }, { "lfoDepth", 4.0f } } },
    };
    return p;
}

int SpaceSonarProcessor::getNumPrograms() { return (int) factoryPresets().size(); }
const String SpaceSonarProcessor::getProgramName (int i) { return isPositiveAndBelow (i, (int) factoryPresets().size()) ? String (factoryPresets()[(size_t) i].name) : String(); }
void SpaceSonarProcessor::setCurrentProgram (int i) { if (isPositiveAndBelow (i, getNumPrograms())) loadFactoryPreset (i); }

// les presets ne touchent ni à POWER ni à ECHO CANCEL
static bool isPresetParam (const String& id) { return id != "power" && id != "echoCancel"; }

void SpaceSonarProcessor::loadFactoryPreset (int index)
{
    const auto& fp = factoryPresets()[(size_t) jlimit (0, (int) factoryPresets().size() - 1, index)];
    for (auto* p : getParameters())
        if (auto* wp = dynamic_cast<RangedAudioParameter*> (p))
            if (isPresetParam (wp->paramID))
                wp->setValueNotifyingHost (wp->getDefaultValue());
    for (const auto& kv : fp.values)
        if (auto* p = apvts.getParameter (kv.first))
            p->setValueNotifyingHost (p->convertTo0to1 (kv.second));
    currentPreset = index;
    presetName = fp.name;
    presetDirty = false;
}

ValueTree SpaceSonarProcessor::savePresetTree() const
{
    ValueTree t ("SPACESONARPRESET");
    for (auto* p : getParameters())
        if (auto* wp = dynamic_cast<AudioProcessorParameterWithID*> (p))
            if (isPresetParam (wp->paramID))
                t.setProperty (wp->paramID, p->getValue(), nullptr);
    return t;
}

void SpaceSonarProcessor::loadPresetTree (const ValueTree& t, const String& name)
{
    for (auto* p : getParameters())
        if (auto* wp = dynamic_cast<AudioProcessorParameterWithID*> (p))
            if (isPresetParam (wp->paramID) && t.hasProperty (wp->paramID))
                p->setValueNotifyingHost ((float) t[wp->paramID]);
    presetName = name;
    currentPreset = -1;
    presetDirty = false;
}

File SpaceSonarProcessor::userPresetFile (int slot)
{
    return File::getSpecialLocation (File::userApplicationDataDirectory).getChildFile (nhd::folderName())
               .getChildFile (JucePlugin_Name).getChildFile ("User Presets").getChildFile ("User " + String (slot) + ".xml");
}

String SpaceSonarProcessor::userPresetName (int slot) const
{
    const auto f = userPresetFile (slot);
    if (f.existsAsFile())
        if (auto x = XmlDocument::parse (f))
            return x->getStringAttribute ("presetName", "User " + String (slot));
    return {};
}

void SpaceSonarProcessor::saveUserPreset (int slot, const String& name)
{
    auto t = savePresetTree();
    t.setProperty ("presetName", name, nullptr);
    const auto f = userPresetFile (slot);
    f.getParentDirectory().createDirectory();
    if (auto x = t.createXml()) x->writeTo (f);
    presetName = name; currentPreset = -1; presetDirty = false;
}

void SpaceSonarProcessor::loadUserPreset (int slot)
{
    const auto f = userPresetFile (slot);
    if (auto x = XmlDocument::parse (f))
    {
        const auto t = ValueTree::fromXml (*x);
        loadPresetTree (t, t.getProperty ("presetName", "User " + String (slot)).toString());
    }
}

//==============================================================================
void SpaceSonarProcessor::getStateInformation (MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("uiSize", uiSizeIdx.load(), nullptr);
    state.setProperty ("glow", glowIdx.load(), nullptr);
    state.setProperty ("preset", currentPreset.load(), nullptr);
    state.setProperty ("presetName", presetName, nullptr);
    state.setProperty ("presetDirty", presetDirty.load(), nullptr);
    String map;
    for (int c = 0; c < 128; ++c)
        if (ccMap[c].load() >= 0)
            if (auto* wp = dynamic_cast<AudioProcessorParameterWithID*> (getParameters()[ccMap[c].load()]))
                map << c << "=" << wp->paramID << ";";
    state.setProperty ("midiMap", map, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void SpaceSonarProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
    {
        auto t = ValueTree::fromXml (*xml);
        if (! t.isValid()) return;
        apvts.replaceState (t);
        uiSizeIdx     = (int) t.getProperty ("uiSize", -1);
        glowIdx       = (int) t.getProperty ("glow", 2);
        currentPreset = (int) t.getProperty ("preset", 0);
        presetName    = t.getProperty ("presetName", "Default").toString();
        presetDirty   = (bool) t.getProperty ("presetDirty", false);
        for (auto& x : ccMap) x = -1;
        for (auto& e : StringArray::fromTokens (t.getProperty ("midiMap").toString(), ";", ""))
        {
            const int c = e.upToFirstOccurrenceOf ("=", false, false).getIntValue();
            const int pi = paramIndexOf (e.fromFirstOccurrenceOf ("=", false, false));
            if (isPositiveAndBelow (c, 128) && pi >= 0) ccMap[c] = pi;
        }
    }
}

AudioProcessorEditor* SpaceSonarProcessor::createEditor() { return new SpaceSonarEditor (*this); }
AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SpaceSonarProcessor(); }
