#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>
#include <vector>
#include "SpaceDsp.h"
#include "NineHandsFolder.h"

//==============================================================================
//  Space Sonar 209 (NINE HANDS | DSP) : écho à bande + réverbe à ressorts, d'après la Roland RE-201
//==============================================================================
class SpaceSonarProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    SpaceSonarProcessor();
    ~SpaceSonarProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 10.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return juce::jmax (0, currentPreset.load()); }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //------------------------------------------------------------------ paramètres
    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    struct P
    {
        std::atomic<float> *power, *inputSel, *micVol, *paVol, *instVol, *echoOn, *echoCancel, *mode, *bass, *treble, *reverbVol,
                           *repeatRate, *intensity, *echoVol, *outLevel, *sync, *stereo, *rateRange, *speed, *lfoWave, *lfoRate,
                           *lfoShape, *lfoDepth, *condition, *wf, *motor, *lowCut, *highCut, *master;
    } prm {};
    float paramValue (const char* id) const { return apvts.getRawParameterValue (id)->load(); }
    void setParam (const char* id, float realValue);         // thread de messages uniquement
    ss::Params currentParams() const;                        // lecture des atomiques (tout thread)

    std::atomic<double> hostBpm { 120.0 };

    //------------------------------------------------------------------ mesures (thread audio -> interface, sans verrou)
    std::atomic<float> inPeak[2], outPeak[2], vu { 0.0f }, peak { 0.0f }, head1Ms { 120.0f }, lfoNow { 0.0f };

    //------------------------------------------------------------------ réglages de l'interface (enregistrés avec le projet)
    std::atomic<int> uiSizeIdx { -1 };       // -1 = taille par défaut (60 %)
    std::atomic<int> glowIdx { 2 };          // 25 / 50 / 75 / 100 %
    std::atomic<int> currentPreset { 0 };
    std::atomic<bool> presetDirty { false };
    juce::String presetName { "Default" };

    //------------------------------------------------------------------ presets
    struct FactoryPreset { const char* name; std::vector<std::pair<const char*, float>> values; };
    static const std::vector<FactoryPreset>& factoryPresets();
    void loadFactoryPreset (int index);
    juce::ValueTree savePresetTree() const;
    void loadPresetTree (const juce::ValueTree& t, const juce::String& name);
    // presets utilisateur : 9 emplacements, dossier « Nine Hands DSP/Space Sonar 209/User Presets »
    static juce::File userPresetFile (int slot);
    juce::String userPresetName (int slot) const;
    void saveUserPreset (int slot, const juce::String& name);
    void loadUserPreset (int slot);

    //------------------------------------------------------------------ A / B, annuler / rétablir (thread de messages)
    int abSlot = 0;
    juce::ValueTree abState[2];
    void switchAB (int slot);
    std::vector<std::vector<float>> undoStack, redoStack;
    std::vector<float> snapshot() const;
    void restore (const std::vector<float>& v);
    void pushUndo (const std::vector<float>& before);
    bool undo();
    bool redo();

    //------------------------------------------------------------------ MIDI Learn (aucun CC par défaut)
    static constexpr int kMaxParams = 64;
    std::atomic<int> learnParam { -1 };
    std::atomic<int> ccMap[128];
    std::atomic<int> lastLearnedCc { -1 };
    int  paramIndexOf (const juce::String& id) const;
    int  ccForParam (int paramIndex) const;
    void clearMidiFor (int paramIndex);

private:
    void timerCallback() override;          // file MIDI -> setValueNotifyingHost (thread de messages)
    std::atomic<float> pendingValue[kMaxParams];
    std::atomic<bool>  pendingSet[kMaxParams];
    std::atomic<int>   ccMin[128], ccMax[128];
    std::atomic<int>   pendingProgram { -1 };      // Program Change -> preset usine (appliqué par le timer)

    ss::Engine engine;
    juce::AudioBuffer<float> wet;
    double sr = 48000.0;
    int maxBlock = 512;
    double powerSm = 1.0;
    bool wasOn = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpaceSonarProcessor)
};
