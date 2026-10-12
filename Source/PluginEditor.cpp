#include "PluginEditor.h"
#if JucePlugin_Build_Standalone
 #include <juce_audio_utils/juce_audio_utils.h>
 #include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>
#endif

using namespace juce;

namespace colours
{
const Colour bg { 0xff0f1113 }, panel { 0xff171b1f }, raised { 0xff1f2429 }, line { 0xff2c3238 },
             text { 0xffece9e2 }, dim { 0xff7f878e }, green { 0xff3ee08f }, amber { 0xffffb23e }, red { 0xffff5b4f };
}

namespace
{
constexpr float inTune = 2.0f;                       // cents

Font font (float height, bool bold = false)
{
    return Font (FontOptions ("Segoe UI", height, bold ? Font::bold : Font::plain));
}

Colour colourFor (bool hasNote, float cents)
{
    if (! hasNote)
        return colours::dim;
    const auto c = std::abs (cents);
    return c < inTune ? colours::green : c < 15.0f ? colours::amber : colours::red;
}
}

//==============================================================================
TunerLook::TunerLook()
{
    using namespace colours;
    setColour (ResizableWindow::backgroundColourId, bg);
    setColour (ComboBox::backgroundColourId, raised);
    setColour (ComboBox::outlineColourId, line);
    setColour (ComboBox::textColourId, text);
    setColour (ComboBox::arrowColourId, dim);
    setColour (PopupMenu::backgroundColourId, panel);
    setColour (PopupMenu::textColourId, text);
    setColour (PopupMenu::headerTextColourId, green);
    setColour (PopupMenu::highlightedBackgroundColourId, green.withAlpha (0.22f));
    setColour (PopupMenu::highlightedTextColourId, text);
    setColour (TextButton::buttonColourId, raised);
    setColour (TextButton::buttonOnColourId, red);
    setColour (TextButton::textColourOffId, text);
    setColour (TextButton::textColourOnId, bg);
    setColour (Slider::textBoxTextColourId, text);
    setColour (Slider::textBoxBackgroundColourId, raised);
    setColour (Slider::textBoxOutlineColourId, line);
    setColour (Label::textColourId, text);
}

Font TunerLook::getComboBoxFont (ComboBox&)                { return font (16.0f); }
Font TunerLook::getPopupMenuFont()                          { return font (16.0f); }
Font TunerLook::getTextButtonFont (TextButton&, int)        { return font (15.0f, true); }

Label* TunerLook::createSliderTextBox (Slider& slider)
{
    auto* label = LookAndFeel_V4::createSliderTextBox (slider);
    label->setFont (font (16.0f));
    return label;
}

//==============================================================================
void StringPad::paint (Graphics& g)
{
    const auto box = getLocalBounds().toFloat().reduced (3.0f);
    g.setColour (nearest ? accent.withAlpha (0.16f) : colours::raised);
    g.fillRoundedRectangle (box, 9.0f);
    g.setColour (nearest ? accent : locked ? colours::text : colours::line);
    g.drawRoundedRectangle (box, 9.0f, nearest || locked ? 2.5f : 1.2f);

    g.setColour (colours::dim);
    g.setFont (font (12.0f, true));
    g.drawText (String (number), box.reduced (9.0f, 6.0f), Justification::topLeft);
    if (locked)
        g.drawText ("LOCK", box.reduced (9.0f, 6.0f), Justification::topRight);

    g.setColour (nearest ? accent : colours::text);
    g.setFont (font (26.0f, true));
    g.drawText (noteName (midi), box.withTrimmedBottom (16.0f), Justification::centred);
    g.setColour (colours::dim);
    g.setFont (font (12.0f));
    g.drawText (String (hz, hz < 100.0f ? 2 : 1) + " Hz", box.withTrimmedTop (box.getHeight() - 26.0f), Justification::centred);
}

//==============================================================================
StringTunerEditor::StringTunerEditor (StringTunerProcessor& p)
    : AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&look);

    String group;
    int id = 1;
    for (const auto& i : instruments())
    {
        if (group != i.group)
        {
            group = i.group;
            instrumentBox.addSectionHeading (group);
        }
        instrumentBox.addItem (i.name, id++);
    }
    addAndMakeVisible (instrumentBox);
    instrumentAttachment = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (p.params, "instrument", instrumentBox);

    reference.setSliderStyle (Slider::IncDecButtons);
    reference.setTextBoxStyle (Slider::TextBoxLeft, false, 84, 34);
    reference.setIncDecButtonsMode (Slider::incDecButtonsDraggable_Vertical);
    reference.setTooltip ("Reference pitch for A4");
    addAndMakeVisible (reference);
    referenceAttachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (p.params, "reference", reference);

    muteButton.setClickingTogglesState (true);
    muteButton.setTooltip ("Silence the output while you tune");
    addAndMakeVisible (muteButton);
    muteAttachment = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (p.params, "mute", muteButton);

    for (int s = 0; s < maxStrings; ++s)
    {
        pads[(size_t) s].number = s + 1;
        pads[(size_t) s].onClick = [this, s] { setLockedString (lockedString() == s ? -1 : s); };
        addChildComponent (pads[(size_t) s]);
    }

   #if JucePlugin_Build_Standalone
    // The standalone app starts with the input muted "to avoid a feedback loop" and a bar across
    // the top; a tuner has to hear you, so open with the input live (MUTE is right here).
    if (p.wrapperType == AudioProcessor::wrapperType_Standalone)
        if (auto* holder = StandalonePluginHolder::getInstance())
            holder->getMuteInputValue().setValue (false);
   #endif

    setSize (780, 500);
    refresh();
    layoutStrings();
    startTimerHz (60);
}

StringTunerEditor::~StringTunerEditor()
{
    setLookAndFeel (nullptr);
}

const Instrument& StringTunerEditor::instrument() const
{
    const auto index = (int) proc.params.getRawParameterValue ("instrument")->load();
    return instruments()[(size_t) jlimit (0, (int) instruments().size() - 1, index)];
}

int StringTunerEditor::lockedString() const
{
    const auto s = (int) proc.params.getRawParameterValue ("string")->load() - 1;
    return s < (int) instrument().strings.size() ? s : -1;
}

void StringTunerEditor::setLockedString (int s)
{
    auto* param = proc.params.getParameter ("string");
    param->beginChangeGesture();
    param->setValueNotifyingHost (param->convertTo0to1 ((float) (s + 1)));
    param->endChangeGesture();
}

void StringTunerEditor::timerCallback()
{
    if ((++frame & 1) == 0)
        refresh();
    if (hasNote)
        strobe += 0.45 * jlimit (-60.0f, 60.0f, cents);        // stands still when in tune
    repaint();
}

void StringTunerEditor::refresh()
{
    const auto index = (int) proc.params.getRawParameterValue ("instrument")->load();
    if (index != shownInstrument)
    {
        if (shownInstrument >= 0 && lockedString() >= 0)     // a new instrument starts on auto
            setLockedString (-1);
        shownInstrument = index;
        hasNote = false;
        layoutStrings();
    }

    const auto& inst = instrument();
    const auto ref = proc.params.getRawParameterValue ("reference")->load();
    auto minHz = 15.0f, maxHz = 4600.0f;               // chromatic: C0 .. just above C8
    if (! inst.strings.empty())
    {
        const auto [lo, hi] = std::minmax_element (inst.strings.begin(), inst.strings.end());
        minHz = hzFor ((float) *lo - 7.0f, ref);
        maxHz = hzFor ((float) *hi + 7.0f, ref);
    }

    const auto r = proc.tuner.update (minHz, maxHz);
    if (! r.valid)
    {
        if (++silentFrames > 25)                       // hold the last note a moment after you stop
            hasNote = false;
    }
    else
    {
        // median of the last three readings against the odd glitch; a new note starts fresh
        if (recentCount > 0 && std::abs (std::log2 (r.hz / recent[(recentCount - 1) % 3])) > 0.04f)
            recentCount = 0;
        recent[recentCount % 3] = r.hz;
        ++recentCount;
        auto h = r.hz;
        if (recentCount >= 3)
        {
            float v[3] = { recent[0], recent[1], recent[2] };
            std::sort (v, v + 3);
            h = v[1];
        }

        const auto midi = midiFor (h, ref);
        int newNearest = -1, newTarget;
        if (inst.strings.empty())
            newTarget = roundToInt (midi);
        else
        {
            newNearest = lockedString() >= 0 ? lockedString() : nearestString (inst, midi);
            newTarget = inst.strings[(size_t) newNearest];
        }
        const auto c = (midi - (float) newTarget) * 100.0f;
        // the needle glides instead of jittering; jumps straight to a new note
        cents = (hasNote && newTarget == target && silentFrames < 4) ? cents + 0.4f * (c - cents) : c;
        hz = h;
        target = newTarget;
        nearest = newNearest;
        hasNote = true;
        silentFrames = 0;
    }

    const auto locked = lockedString();
    for (int s = 0; s < (int) inst.strings.size(); ++s)
    {
        auto& pad = pads[(size_t) s];
        pad.midi = inst.strings[(size_t) s];
        pad.hz = hzFor ((float) pad.midi, ref);
        pad.nearest = hasNote && s == nearest;
        pad.accent = colourFor (true, cents);
        pad.locked = s == locked;
        pad.repaint();
    }
}

void StringTunerEditor::layoutStrings()
{
    const auto& inst = instrument();
    const auto count = (int) inst.strings.size();
    const auto area = Rectangle<int> (24, 358, 732, 92);
    const auto w = count > 0 ? jmin (120, area.getWidth() / count) : 0;
    auto x = area.getCentreX() - w * count / 2;
    for (int s = 0; s < maxStrings; ++s)
    {
        pads[(size_t) s].setVisible (s < count);
        if (s < count)
        {
            pads[(size_t) s].setBounds (x, area.getY(), w, area.getHeight());
            x += w;
        }
    }
}

void StringTunerEditor::resized()
{
    instrumentBox.setBounds (296, 22, 250, 34);
    reference.setBounds (556, 22, 128, 34);
    muteButton.setBounds (692, 22, 64, 34);
    layoutStrings();
}

void StringTunerEditor::paint (Graphics& g)
{
    using namespace colours;
    g.fillAll (bg);

    AttributedString word;
    word.append ("string", font (32.0f, true), text);
    word.append ("tuner", font (32.0f, true), green);
    word.draw (g, { 24.0f, 14.0f, 260.0f, 48.0f });

    const auto card = Rectangle<float> (24.0f, 74.0f, 732.0f, 268.0f);
    g.setColour (panel);
    g.fillRoundedRectangle (card, 14.0f);
    g.setColour (line);
    g.drawRoundedRectangle (card, 14.0f, 1.2f);

    const auto& inst = instrument();
    const auto chromatic = inst.strings.empty();
    const auto tuned = hasNote && std::abs (cents) < inTune;
    const auto colour = colourFor (hasNote, cents);

    // the note
    auto inner = card.reduced (24.0f, 20.0f);
    const auto noteArea = inner.removeFromLeft (210.0f);
    g.setColour (dim);
    g.setFont (font (14.0f, true));
    g.drawText (chromatic ? String ("NOTE") : nearest >= 0 && hasNote ? "STRING " + String (nearest + 1) : String ("STRING"),
                noteArea.withHeight (20.0f), Justification::topLeft);
    if (hasNote)
    {
        const auto name = noteName (target);
        const auto letters = name.initialSectionNotContaining ("-0123456789");
        const auto octave = name.substring (letters.length());
        g.setColour (colour);
        // letters and the small octave number, centred together
        const auto big = font (letters.length() > 1 ? 104.0f : 128.0f, true), small = font (40.0f, true);
        const auto w = GlyphArrangement::getStringWidth (big, letters), wo = GlyphArrangement::getStringWidth (small, octave);
        const auto x = noteArea.getCentreX() - (w + 4.0f + wo) / 2.0f;
        g.setFont (big);
        g.drawText (letters, Rectangle<float> (x, noteArea.getY() + 16.0f, w + 2.0f, 150.0f), Justification::centredLeft, false);
        g.setFont (small);
        g.drawText (octave, Rectangle<float> (x + w + 4.0f, noteArea.getY() + 92.0f, wo + 2.0f, 60.0f), Justification::centredLeft, false);
        g.setColour (text);
        g.setFont (font (24.0f));
        g.drawText (String (hz, hz < 100.0f ? 2 : 1) + " Hz", noteArea.withTrimmedTop (176.0f), Justification::centredLeft);
    }
    else
    {
        g.setColour (line);
        g.setFont (font (110.0f, true));
        g.drawText ("-", noteArea.withTrimmedTop (16.0f).withHeight (150.0f), Justification::centred);
        g.setColour (dim);
        g.setFont (font (18.0f));
        g.drawText ("play a string", noteArea.withTrimmedTop (176.0f), Justification::centredLeft);
    }

    // the meter: -50 .. +50 cents
    inner.removeFromLeft (16.0f);
    auto meter = inner.removeFromTop (106.0f);
    const auto scaleY = meter.getY() + 76.0f;
    const auto xFor = [&] (float c) { return meter.getX() + (c + 50.0f) / 100.0f * meter.getWidth(); };
    g.setColour (green.withAlpha (0.12f));
    g.fillRect (Rectangle<float>::leftTopRightBottom (xFor (-inTune), meter.getY() + 6.0f, xFor (inTune), scaleY));
    g.setColour (line);
    g.drawLine (meter.getX(), scaleY, meter.getRight(), scaleY, 2.0f);
    g.setFont (font (12.0f, true));
    for (int c = -50; c <= 50; c += 5)
    {
        const auto big = c % 25 == 0;
        g.setColour (c == 0 ? text : big ? dim : line);
        g.drawLine (xFor ((float) c), scaleY - (c == 0 ? 70.0f : big ? 18.0f : 9.0f), xFor ((float) c), scaleY, c == 0 ? 2.0f : 1.5f);
        if (big)
            g.drawText ((c > 0 ? "+" : "") + String (c), Rectangle<float> (xFor ((float) c) - 20.0f, scaleY + 4.0f, 40.0f, 16.0f), Justification::centred);
    }
    g.setColour (dim);
    g.drawText ("flat", meter.withHeight (16.0f), Justification::topLeft);
    g.drawText ("sharp", meter.withHeight (16.0f), Justification::topRight);
    if (hasNote)
    {
        const auto x = xFor (jlimit (-50.0f, 50.0f, cents));
        g.setColour (colour);
        g.fillRoundedRectangle (x - 2.5f, meter.getY() + 10.0f, 5.0f, scaleY - meter.getY() - 10.0f, 2.5f);
        g.fillEllipse (x - 7.0f, scaleY - 7.0f, 14.0f, 14.0f);
    }

    // the strobe: stripes drift right when sharp, left when flat, stand still when in tune
    inner.removeFromTop (14.0f);
    const auto band = inner.removeFromTop (42.0f);
    {
        Graphics::ScopedSaveState save (g);
        Path clip;
        clip.addRoundedRectangle (band, 8.0f);
        g.reduceClipRegion (clip);
        g.setColour (raised);
        g.fillRect (band);
        constexpr float period = 28.0f;
        const auto offset = (float) std::fmod (strobe, (double) period);
        g.setColour (hasNote ? colour.withAlpha (tuned ? 0.95f : 0.75f) : line);
        for (auto x = band.getX() - period * 2.0f + offset; x < band.getRight() + period; x += period)
        {
            Path stripe;
            stripe.addQuadrilateral (x, band.getBottom(), x + period * 0.5f, band.getBottom(),
                                     x + period * 0.5f + 14.0f, band.getY(), x + 14.0f, band.getY());
            g.fillPath (stripe);
        }
    }

    // what to do
    inner.removeFromTop (12.0f);
    if (hasNote)
    {
        const auto direction = tuned ? String ("in tune") : cents < 0.0f ? String ("tune up") : String ("tune down");
        g.setColour (colour);
        g.setFont (font (28.0f, true));
        g.drawText (direction, inner.withHeight (36.0f), Justification::centredLeft);
        g.setFont (font (28.0f));
        g.drawText ((cents >= 0.0f ? "+" : "") + String (cents, 1) + " c", inner.withHeight (36.0f), Justification::centredRight);
        g.setColour (dim);
        g.setFont (font (14.0f));
        g.drawText ("target " + noteName (target) + "  " + String (hzFor ((float) target, proc.params.getRawParameterValue ("reference")->load()), 2) + " Hz",
                    inner.withTrimmedTop (38.0f).withHeight (18.0f), Justification::centredLeft);
    }

    if (chromatic)
    {
        g.setColour (dim);
        g.setFont (font (16.0f));
        g.drawText ("Chromatic: any note from C0 (16 Hz, octobass) to C8 (4186 Hz, top of the piano)",
                    Rectangle<float> (24.0f, 358.0f, 732.0f, 92.0f), Justification::centred);
    }

    g.setColour (dim);
    g.setFont (font (13.0f));
    const auto footer = Rectangle<float> (24.0f, 462.0f, 732.0f, 24.0f);
    g.drawText (String ("stringtuner v" STRINGTUNER_VERSION "      ")
                    + String (CharPointer_UTF8 ("Farica\xe2\x98\x86Kimora \xc2\xa9 Cursed Entertainment")),
                footer, Justification::centredLeft);
    if (! chromatic)
        g.drawText ("click a string to lock it, again for auto", footer, Justification::centredRight);
}
