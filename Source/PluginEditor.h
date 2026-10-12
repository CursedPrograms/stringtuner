#pragma once
#include "PluginProcessor.h"
#include "Instruments.h"
#include <array>

class TunerLook : public juce::LookAndFeel_V4
{
public:
    TunerLook();
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    juce::Font getTextButtonFont (juce::TextButton&, int) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
};

// One open string: click to lock the tuner to it, click again for auto.
class StringPad : public juce::Component
{
public:
    std::function<void()> onClick;
    int number = 1, midi = 0;
    float hz = 0.0f;
    bool nearest = false, locked = false;
    juce::Colour accent;                        // same colour as the needle

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override { if (onClick) onClick(); }
};

class StringTunerEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit StringTunerEditor (StringTunerProcessor&);
    ~StringTunerEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void refresh();
    void layoutStrings();
    const Instrument& instrument() const;
    int lockedString() const;                   // -1 = auto
    void setLockedString (int);

    StringTunerProcessor& proc;
    TunerLook look;
    juce::ComboBox instrumentBox;
    juce::Slider reference;
    juce::TextButton muteButton { "MUTE" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> instrumentAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> referenceAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> muteAttachment;
    std::array<StringPad, maxStrings> pads;

    int shownInstrument = -1;
    bool hasNote = false;
    float hz = 0.0f, cents = 0.0f;
    int target = -1, nearest = -1, silentFrames = 1000, frame = 0;
    float recent[3] {};
    int recentCount = 0;
    double strobe = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StringTunerEditor)
};
