#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
constexpr int editorMinWidth = 960;
constexpr int editorMinHeight = 690;
constexpr int editorDefaultWidth = 1240;
constexpr int editorDefaultHeight = 820;

juce::Rectangle<int> getInitialEditorBounds()
{
    auto bounds = juce::Rectangle<int>(editorDefaultWidth, editorDefaultHeight);

    if (auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
    {
        const auto availableArea = display->userArea.reduced(32, 32);
        bounds.setSize(juce::jmin(editorDefaultWidth, juce::jmax(editorMinWidth, availableArea.getWidth())),
                       juce::jmin(editorDefaultHeight, juce::jmax(editorMinHeight, availableArea.getHeight())));
    }

    return bounds;
}

int getHeaderHeightForSize(int editorHeight)
{
    return juce::jlimit(72, 80, juce::roundToInt((float) editorHeight * 0.095f));
}

int getKeyboardHeightForSize(int editorHeight)
{
    return juce::jlimit(92, 118, juce::roundToInt((float) editorHeight * 0.13f));
}

float getRainDelayTimeSeconds(float amount) noexcept
{
    return (85.0f + std::pow(juce::jlimit(0.0f, 1.0f, amount), 0.84f) * 390.0f) / 1000.0f;
}

juce::Colour getStatusReadyColour() noexcept { return RootFlowLookAndFeel::getAccentColor(); }
juce::Colour getStatusMapColour() noexcept   { return juce::Colours::orange; }

void styleKeyboardDrawer(juce::MidiKeyboardComponent& keyboardDrawer)
{
    keyboardDrawer.setColour(juce::MidiKeyboardComponent::whiteNoteColourId, juce::Colour(0xff1a2620));
    keyboardDrawer.setColour(juce::MidiKeyboardComponent::blackNoteColourId, juce::Colour(0xff050a08));
    keyboardDrawer.setColour(juce::MidiKeyboardComponent::keyDownOverlayColourId, RootFlow::accent.withAlpha(0.28f));
    keyboardDrawer.setColour(juce::MidiKeyboardComponent::keySeparatorLineColourId, RootFlow::accent.withAlpha(0.04f));
}

juce::Path makeKeyboardCradle(juce::Rectangle<float> r)
{
    juce::Path p;
    p.startNewSubPath(r.getX() + r.getWidth() * 0.06f, r.getY() + r.getHeight() * 0.18f);
    p.cubicTo(r.getX() + r.getWidth() * 0.20f, r.getY() - r.getHeight() * 0.02f,
              r.getCentreX() - r.getWidth() * 0.10f, r.getY() - r.getHeight() * 0.08f,
              r.getCentreX(), r.getY() + r.getHeight() * 0.04f);
    p.cubicTo(r.getCentreX() + r.getWidth() * 0.10f, r.getY() - r.getHeight() * 0.08f,
              r.getRight() - r.getWidth() * 0.20f, r.getY() - r.getHeight() * 0.02f,
              r.getRight() - r.getWidth() * 0.06f, r.getY() + r.getHeight() * 0.18f);
    p.lineTo(r.getRight() - r.getWidth() * 0.03f, r.getBottom() - r.getHeight() * 0.10f);
    p.cubicTo(r.getRight() - r.getWidth() * 0.18f, r.getBottom() + r.getHeight() * 0.02f,
              r.getX() + r.getWidth() * 0.18f, r.getBottom() + r.getHeight() * 0.02f,
              r.getX() + r.getWidth() * 0.03f, r.getBottom() - r.getHeight() * 0.10f);
    p.closeSubPath();
    return p;
}

void drawHeaderChip(juce::Graphics& g,
                    juce::Rectangle<float> area,
                    const juce::String& label,
                    const juce::String& value,
                    juce::Colour tint,
                    bool emphasise = false)
{
    RootFlow::drawGlassPanel(g, area, area.getHeight() * 0.5f, emphasise ? 0.58f : 0.44f);
    
    g.setColour(tint.withAlpha(emphasise ? 0.038f : 0.020f));
    g.fillRoundedRectangle(area.reduced(2.0f), area.getHeight() * 0.44f);

    auto inner = area.reduced(12.0f, 5.0f);
    auto labelArea = inner.removeFromTop(area.getHeight() * 0.34f);

    g.setColour(RootFlow::textMuted.withAlpha(emphasise ? 0.70f : 0.62f));
    g.setFont(RootFlow::getFont(9.1f));
    g.drawText(label.toUpperCase(), labelArea, juce::Justification::centredLeft, false);

    g.setColour((emphasise ? tint.brighter(0.08f) : RootFlow::text).withAlpha(emphasise ? 0.90f : 0.84f));
    g.setFont(RootFlow::getFont(12.2f).boldened());
    g.drawFittedText(value, inner.toNearestInt(), juce::Justification::centredLeft, 1);

    RootFlow::drawGlowOrb(g, { area.getRight() - 14.0f, area.getCentreY() }, 2.7f, tint, emphasise ? 0.28f : 0.18f);
}

void drawHeaderFocusBubble(juce::Graphics& g,
                           juce::Rectangle<float> area,
                           const juce::String& section,
                           const juce::String& value,
                           juce::Colour tint,
                           bool emphasise = false)
{
    RootFlow::drawGlassPanel(g, area, area.getHeight() * 0.5f, emphasise ? 0.64f : 0.54f);

    g.setColour(tint.withAlpha(emphasise ? 0.06f : 0.035f));
    g.fillRoundedRectangle(area.reduced(2.0f), area.getHeight() * 0.44f);

    auto inner = area.reduced(10.0f, 4.0f);
    auto sectionArea = inner.removeFromTop(8.0f);

    g.setColour(RootFlow::textMuted.withAlpha(0.86f));
    g.setFont(RootFlow::getFont(7.8f).boldened());
    g.drawText(section.toUpperCase(), sectionArea, juce::Justification::centredLeft, false);

    g.setColour((emphasise ? tint.brighter(0.12f) : RootFlow::text).withAlpha(0.90f));
    g.setFont(RootFlow::getFont(10.2f).boldened());
    g.drawFittedText(value.toUpperCase(), inner.toNearestInt(), juce::Justification::centredLeft, 1);

    RootFlow::drawGlowOrb(g, { area.getRight() - 10.0f, area.getCentreY() }, 2.5f, tint, emphasise ? 0.34f : 0.22f);
}

void drawToolbarCaption(juce::Graphics& g,
                        juce::Rectangle<float> area,
                        const juce::String& text,
                        juce::Colour tint,
                        juce::Justification justification = juce::Justification::centred)
{
    RootFlow::drawGlassPanel(g, area, area.getHeight() * 0.5f, 0.34f);
    g.setColour(tint.withAlpha(0.05f));
    g.fillRoundedRectangle(area.reduced(2.0f), area.getHeight() * 0.42f);

    auto textArea = area.toNearestInt().reduced(8, 0);
    g.setFont(RootFlow::getFont(8.8f).boldened());
    g.setColour(juce::Colours::black.withAlpha(0.22f));
    g.drawFittedText(text, textArea.translated(0, 1), justification, 1);

    g.setColour(RootFlow::text.interpolatedWith(tint, 0.22f).withAlpha(0.88f));
    g.drawFittedText(text, textArea, justification, 1);
}

void drawClusterDivider(juce::Graphics& g, float x, juce::Rectangle<float> cluster, juce::Colour tint)
{
    const float top = cluster.getY() + 7.0f;
    const float bottom = cluster.getBottom() - 7.0f;
    g.setColour(tint.withAlpha(0.05f));
    g.drawLine(x, top, x, bottom, 1.0f);
    g.setColour(juce::Colours::white.withAlpha(0.02f));
    g.drawLine(x + 1.0f, top + 2.0f, x + 1.0f, bottom - 2.0f, 1.0f);
}

juce::Path makeWavePreviewPath(juce::Rectangle<float> area, int waveSelection, float phase)
{
    juce::Path p;
    constexpr int numPoints = 48;

    for (int i = 0; i < numPoints; ++i)
    {
        const float t = (float) i / (float) (numPoints - 1);
        const float samplePos = std::fmod(t + phase, 1.0f);
        float sample = 0.0f;

        if (waveSelection == 2)
            sample = samplePos * 2.0f - 1.0f;
        else if (waveSelection == 3)
            sample = samplePos < 0.5f ? 0.82f : -0.82f;
        else
            sample = std::sin((t + phase) * juce::MathConstants<float>::twoPi);

        const float x = area.getX() + t * area.getWidth();
        const float y = area.getCentreY() - sample * area.getHeight() * 0.34f;

        if (i == 0)
            p.startNewSubPath(x, y);
        else
            p.lineTo(x, y);
    }

    return p;
}

int countHeldNotes(juce::MidiKeyboardState& keyboardState)
{
    int count = 0;
    for (int note = 21; note < 109; ++note)
        if (keyboardState.isNoteOnForChannels(0xffff, note))
            ++count;

    return count;
}

juce::String getMidiActivitySection(const RootFlowAudioProcessor::MidiActivitySnapshot& snapshot)
{
    return snapshot.wasMapped ? juce::String("Mapped") : juce::String("Last Midi");
}

juce::String getMidiActivityValue(const RootFlowAudioProcessor::MidiActivitySnapshot& snapshot)
{
    using Type = RootFlowAudioProcessor::MidiActivitySnapshot::Type;

    switch (snapshot.type)
    {
        case Type::note:
        {
            if (! juce::isPositiveAndBelow(snapshot.noteNumber, 128))
                return "Note Event";

            auto noteName = juce::MidiMessage::getMidiNoteName(snapshot.noteNumber, true, true, 3);
            return snapshot.value > 0 ? noteName + " V" + juce::String(snapshot.value)
                                      : noteName + " Off";
        }

        case Type::controller:
            return snapshot.controllerNumber >= 0 ? "CC " + juce::String(snapshot.controllerNumber) + " " + juce::String(snapshot.value)
                                                  : juce::String("Controller");

        case Type::timbre:
            return "Timbre " + juce::String(snapshot.value);

        case Type::pitchBend:
            return (snapshot.bendCents >= 0 ? juce::String("+") : juce::String()) + juce::String(snapshot.bendCents) + "c";

        case Type::pressure:
            if (juce::isPositiveAndBelow(snapshot.noteNumber, 128))
                return juce::MidiMessage::getMidiNoteName(snapshot.noteNumber, true, true, 3) + " " + juce::String(snapshot.value);

            return "Pressure " + juce::String(snapshot.value);

        case Type::none:
        default:
            break;
    }

    return "Awaiting Input";
}

juce::Colour getMidiActivityTint(const RootFlowAudioProcessor::MidiActivitySnapshot& snapshot)
{
    using Type = RootFlowAudioProcessor::MidiActivitySnapshot::Type;

    switch (snapshot.type)
    {
        case Type::note:       return RootFlow::accentSoft;
        case Type::controller: return RootFlow::accent;
        case Type::timbre:     return RootFlow::amber;
        case Type::pitchBend:  return RootFlow::violet;
        case Type::pressure:   return RootFlow::amber.interpolatedWith(RootFlow::accent, 0.35f);
        case Type::none:
        default:               return RootFlow::textMuted;
    }
}

juce::String makeSeedMemoryButtonLabel(const juce::String& prompt)
{
    auto cleaned = prompt.trim().replaceCharacters("\r\n\t", "   ");
    cleaned = cleaned.replaceCharacters("|", "/").trim();

    if (cleaned.isEmpty())
        return {};

    constexpr int maxLabelLength = 18;
    if (cleaned.length() <= maxLabelLength)
        return cleaned;

    return cleaned.substring(0, maxLabelLength - 3).trimEnd() + "...";
}

juce::String makeUtilitySummaryText(bool hoverEffectsEnabled,
                                    bool idleEffectsEnabled,
                                    bool popupOverlaysEnabled,
                                    bool testToneEnabled,
                                    const RootFlowAudioProcessor& audioProcessor)
{
    juce::StringArray parts;
    juce::StringArray locks;
    if (audioProcessor.isGrowLockEnabled(RootFlowAudioProcessor::GrowLockGroup::root))       locks.add("Root");
    if (audioProcessor.isGrowLockEnabled(RootFlowAudioProcessor::GrowLockGroup::motion))     locks.add("Move");
    if (audioProcessor.isGrowLockEnabled(RootFlowAudioProcessor::GrowLockGroup::air))        locks.add("Air");
    if (audioProcessor.isGrowLockEnabled(RootFlowAudioProcessor::GrowLockGroup::fx))         locks.add("FX");
    if (audioProcessor.isGrowLockEnabled(RootFlowAudioProcessor::GrowLockGroup::sequencer))  locks.add("Seq");

    parts.add(locks.isEmpty() ? juce::String("Locks Off")
                              : juce::String("Locks ") + locks.joinIntoString("/"));
    parts.add("Midi " + juce::String(audioProcessor.isMidiLearnArmed() ? "Armed" : "Off"));
    parts.add("Hover " + juce::String(hoverEffectsEnabled ? "On" : "Off"));
    parts.add("Idle " + juce::String(idleEffectsEnabled ? "On" : "Off"));
    parts.add("Popups " + juce::String(popupOverlaysEnabled ? "On" : "Off"));
    parts.add("Tone " + juce::String(testToneEnabled ? "On" : "Off"));
    return parts.joinIntoString(" / ");
}

struct StylePromptRecipe
{
    const char* menuLabel;
    const char* seedA;
    const char* seedB;
    float morphAmount;
    const char* tooltip;
};

constexpr std::array<StylePromptRecipe, 5> stylePromptRecipes {{
    { "Techno Bass",
      "warehouse techno bass, dark mono sub, punchy 4-on-the-floor, tight machine groove",
      "acid pulse, hypnotic motion, industrial edge, straight grid",
      0.32f,
      "Dark club low-end with tight mono focus and machine drive" },
    { "Neo-Soul Keys",
      "neo-soul keys, warm electric piano pad, soft jazzy chords, humanized swing",
      "velvet soul texture, airy shimmer, intimate stereo, mellow groove",
      0.46f,
      "Warm, musical, humanized keys/pad territory" },
    { "Afro Pulse",
      "afrobeat groove, percussive pluck, warm organic tone, polyrhythmic motion",
      "amapiano log drum bounce, human timing, sunny air, dancefloor pocket",
      0.54f,
      "Polyrhythmic, warm pulse with organic motion and bounce" },
    { "Trap Sub",
      "trap sub bass, dark 808, halftime bounce, mono center, punchy transient",
      "drill tension, ominous texture, sliding low-end, sparse pocket",
      0.42f,
      "Heavy halftime sub with dark urban tension" },
    { "DnB Motion",
      "drum and bass motion, broken beat energy, tight low-end, fast nervous groove",
      "neurofunk texture, glitch detail, bright attack, restless movement",
      0.58f,
      "Fast broken rhythm with animated bass motion" }
}};
}

RootFlowAudioProcessorEditor::RootFlowAudioProcessorEditor(RootFlowAudioProcessor& p)
    : juce::AudioProcessorEditor(p),
      audioProcessor(p),
      keyboardDrawer(p.getKeyboardState(), juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel(&look);
    setOpaque(true);
    setResizable(true, true);
    setResizeLimits(editorMinWidth, editorMinHeight, 1800, 1300);

    styleKeyboardDrawer(keyboardDrawer);

    // Anchoring Glow (Soft Upward Radiance)
    auto* kbdGlow = new juce::GlowEffect();
    kbdGlow->setGlowProperties(2.4f, RootFlow::accent.withAlpha(0.08f));
    keyboardDrawer.setComponentEffect(kbdGlow);

    // Main Layout components are added and initialized
    mainLayout = std::make_unique<MainLayoutComponent>(p);
    mainLayout->setOpaque(false); // Let the Editor's backdrop shine through
    addAndMakeVisible(*mainLayout);

    startTimerHz(30); // Global UI Animation Pulse

    // --- ACTUAL ATTACHMENTS FOR SUB-PANELS ---
    auto& rp = mainLayout->getRootPanel();
    rootDepthAtt  = std::make_unique<Attachment>(p.tree, "rootDepth", rp.getDepthSlider());
    rootSoilAtt   = std::make_unique<Attachment>(p.tree, "rootSoil",  rp.getSoilSlider());
    rootAnchorAtt = std::make_unique<Attachment>(p.tree, "rootAnchor", rp.getAnchorSlider());

    auto& pp = mainLayout->getPulsePanel();
    pulseRateAtt   = std::make_unique<Attachment>(p.tree, "pulseRate",   pp.getRateSlider());
    pulseBreathAtt = std::make_unique<Attachment>(p.tree, "pulseBreath", pp.getBreathSlider());
    pulseGrowthAtt = std::make_unique<Attachment>(p.tree, "pulseGrowth", pp.getGrowthSlider());

    auto& cc = mainLayout->getCenterComponent();
    sapFlowAtt      = std::make_unique<Attachment>(p.tree, "sapFlow",      cc.getFlowSlider());
    sapVitalityAtt  = std::make_unique<Attachment>(p.tree, "sapVitality",  cc.getVitalitySlider());
    sapTextureAtt   = std::make_unique<Attachment>(p.tree, "sapTexture",   cc.getTextureSlider());
    canopyAtt       = std::make_unique<Attachment>(p.tree, "canopy",       cc.getCanopySlider());
    instabilityAtt  = std::make_unique<Attachment>(p.tree, "instability",  cc.getInstabilitySlider());
    atmosAtt        = std::make_unique<Attachment>(p.tree, "atmosphere",   cc.getAtmosSlider());
    seasonsAtt      = std::make_unique<Attachment>(p.tree, "ecoSystem",    cc.getSeasonsSlider());

    // Connect the Visualizer to the Processor
    cc.getEnergyDisplay().setProcessor(&audioProcessor);

    auto& bp = mainLayout->getBottomPanel();
    bloomAtt = std::make_unique<Attachment>(audioProcessor.tree, "bloom", bp.getBloomSlider());
    rainAtt = std::make_unique<Attachment>(audioProcessor.tree, "rain", bp.getRainSlider());
    sunAtt = std::make_unique<Attachment>(audioProcessor.tree, "sun", bp.getSunSlider());
    evolutionAtt = std::make_unique<Attachment>(audioProcessor.tree, "evolution", cc.getEvolution());

    // --- GLOBAL HEADER UI ---
    addAndMakeVisible(presetBox);
    addAndMakeVisible(patchMenuButton);
    addAndMakeVisible(presetSaveButton);
    addAndMakeVisible(presetDeleteButton);
    addAndMakeVisible(midiLearnButton);
    addAndMakeVisible(utilityMenuButton);
    addAndMakeVisible(testToneButton);
    addAndMakeVisible(hoverToggleButton);
    addAndMakeVisible(idleToggleButton);
    addAndMakeVisible(popupToggleButton);
    addAndMakeVisible(mutateModeButton);
    addAndMakeVisible(mutateButton);
    addAndMakeVisible(growLockRootButton);
    addAndMakeVisible(growLockMotionButton);
    addAndMakeVisible(growLockAirButton);
    addAndMakeVisible(growLockFxButton);
    addAndMakeVisible(growLockSeqButton);
    addAndMakeVisible(promptEditor);
    addAndMakeVisible(morphPromptEditor);
    addAndMakeVisible(promptMorphSlider);
    addAndMakeVisible(promptApplyButton);
    for (auto& seedMemoryButton : seedMemoryButtons)
        addAndMakeVisible(seedMemoryButton);
    addAndMakeVisible(waveSelector);
    addAndMakeVisible(keyboardDrawer);

    presetBox.setText("FACTORY SEED");
    patchMenuButton.setButtonText("EDIT");
    presetSaveButton.setButtonText("SAVE");
    presetDeleteButton.setButtonText("DEL");
    midiLearnButton.setButtonText("MAP");
    utilityMenuButton.setButtonText("TOOLS");
    testToneButton.setButtonText("TONE");
    hoverToggleButton.setButtonText("HOVER");
    idleToggleButton.setButtonText("IDLE");
    popupToggleButton.setButtonText("POPUP");
    mutateModeButton.setButtonText(audioProcessor.getMutationModeShortLabel());
    mutateButton.setButtonText("MUTATE");
    growLockRootButton.setButtonText("ROOT");
    growLockMotionButton.setButtonText("MOVE");
    growLockAirButton.setButtonText("AIR");
    growLockFxButton.setButtonText("FX");
    growLockSeqButton.setButtonText("SEQ");
    promptApplyButton.setButtonText("GROW");
    presetBox.setTextWhenNothingSelected("PRESET");
    midiLearnButton.setClickingTogglesState(true);
    testToneButton.setClickingTogglesState(true);
    hoverToggleButton.setClickingTogglesState(true);
    idleToggleButton.setClickingTogglesState(true);
    popupToggleButton.setClickingTogglesState(true);
    growLockRootButton.setClickingTogglesState(true);
    growLockMotionButton.setClickingTogglesState(true);
    growLockAirButton.setClickingTogglesState(true);
    growLockFxButton.setClickingTogglesState(true);
    growLockSeqButton.setClickingTogglesState(true);
    hoverToggleButton.setToggleState(RootFlow::areHoverEffectsEnabled(), juce::dontSendNotification);
    idleToggleButton.setToggleState(RootFlow::areIdleEffectsEnabled(), juce::dontSendNotification);
    popupToggleButton.setToggleState(RootFlow::arePopupOverlaysEnabled(), juce::dontSendNotification);
    patchMenuButton.setVisible(true);
    presetSaveButton.setVisible(false);
    presetDeleteButton.setVisible(false);
    midiLearnButton.setVisible(false);
    mutateModeButton.setVisible(false);
    growLockRootButton.setVisible(false);
    growLockMotionButton.setVisible(false);
    growLockAirButton.setVisible(false);
    growLockFxButton.setVisible(false);
    growLockSeqButton.setVisible(false);
    hoverToggleButton.setVisible(false);
    idleToggleButton.setVisible(false);
    popupToggleButton.setVisible(false);
    testToneButton.setVisible(false);
    promptEditor.setMultiLine(false);
    promptEditor.setReturnKeyStartsNewLine(false);
    promptEditor.setScrollbarsShown(false);
    promptEditor.setIndents(10, 8);
    promptEditor.setFont(RootFlow::getFont(12.0f));
    promptEditor.setTextToShowWhenEmpty("A: techno pulse / neo-soul pad", RootFlow::textMuted.withAlpha(0.55f));
    promptEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0x00000000));
    promptEditor.setColour(juce::TextEditor::outlineColourId, RootFlow::accent.withAlpha(0.07f));
    promptEditor.setColour(juce::TextEditor::focusedOutlineColourId, RootFlow::amber.withAlpha(0.20f));
    promptEditor.setColour(juce::TextEditor::textColourId, RootFlow::text.withAlpha(0.95f));
    promptEditor.setColour(juce::TextEditor::highlightColourId, RootFlow::accent.withAlpha(0.12f));
    promptEditor.setColour(juce::TextEditor::highlightedTextColourId, juce::Colours::white);
    promptEditor.setColour(juce::CaretComponent::caretColourId, RootFlow::accentSoft);
    morphPromptEditor.setMultiLine(false);
    morphPromptEditor.setReturnKeyStartsNewLine(false);
    morphPromptEditor.setScrollbarsShown(false);
    morphPromptEditor.setIndents(10, 8);
    morphPromptEditor.setFont(RootFlow::getFont(12.0f));
    morphPromptEditor.setTextToShowWhenEmpty("+ B: afro groove / trap bass", RootFlow::textMuted.withAlpha(0.55f));
    morphPromptEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0x00000000));
    morphPromptEditor.setColour(juce::TextEditor::outlineColourId, RootFlow::accent.withAlpha(0.07f));
    morphPromptEditor.setColour(juce::TextEditor::focusedOutlineColourId, RootFlow::amber.withAlpha(0.20f));
    morphPromptEditor.setColour(juce::TextEditor::textColourId, RootFlow::text.withAlpha(0.95f));
    morphPromptEditor.setColour(juce::TextEditor::highlightColourId, RootFlow::accent.withAlpha(0.12f));
    morphPromptEditor.setColour(juce::TextEditor::highlightedTextColourId, juce::Colours::white);
    morphPromptEditor.setColour(juce::CaretComponent::caretColourId, RootFlow::amber);
    promptMorphSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    promptMorphSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    promptMorphSlider.setRange(0.0, 1.0, 0.01);
    promptMorphSlider.setDoubleClickReturnValue(true, 0.5);
    promptMorphSlider.setValue(0.5, juce::dontSendNotification);
    promptMorphSlider.setName("Prompt Morph");
    promptMorphSlider.getProperties().set("rootflowStyle", "pod-horizontal");
    promptMorphSlider.setTooltip("Blend between Seed A and Seed B");
    promptApplyButton.getProperties().set("rootflowStyle", "header-flat");
    promptApplyButton.setTooltip("Apply the current seed pair or use EDIT for style recipes");
    for (size_t i = 0; i < seedMemoryButtons.size(); ++i)
    {
        auto& button = seedMemoryButtons[i];
        button.setButtonText("MEM");
        button.setTooltip("Load a remembered seed");
        button.setVisible(false);
    }

    waveSelector.addItemList({"SINE", "SAW", "PULSE"}, 1);
    waveAttachment = std::make_unique<ComboAttachment>(p.tree, "oscWave", waveSelector);

    addAndMakeVisible(oversamplingSelector);
    oversamplingSelector.addItemList({"1x", "2x", "4x"}, 1);
    oversamplingAttachment = std::make_unique<ComboAttachment>(p.tree, "oversampling", oversamplingSelector);

    presetBox.onChange = [this]
    {
        const int presetIndex = presetBox.getSelectedId() - 1;
        if (presetIndex >= 0)
            audioProcessor.applyCombinedPreset(presetIndex);
    };

    patchMenuButton.onClick = [this]
    {
        showPatchMenu();
    };

    presetSaveButton.onClick = [this]
    {
        audioProcessor.saveCurrentStateAsUserPreset();
        refreshHeaderControlState();
    };

    presetDeleteButton.onClick = [this]
    {
        audioProcessor.deleteCurrentUserPreset();
        refreshHeaderControlState();
    };

    midiLearnButton.onClick = [this]
    {
        setMidiLearnEnabled(midiLearnButton.getToggleState());
    };

    utilityMenuButton.onClick = [this]
    {
        showUtilityMenu();
    };

    testToneButton.onClick = [this]
    {
        setToneProbeEnabled(testToneButton.getToggleState());
    };

    hoverToggleButton.onClick = [this]
    {
        setHoverEffectsEnabled(hoverToggleButton.getToggleState());
    };

    idleToggleButton.onClick = [this]
    {
        setIdleEffectsEnabled(idleToggleButton.getToggleState());
    };

    popupToggleButton.onClick = [this]
    {
        setPopupOverlaysEnabled(popupToggleButton.getToggleState());
    };

    mutateModeButton.onClick = [this]
    {
        audioProcessor.cycleMutationMode();
        refreshHeaderControlState();
        repaint();
    };

    growLockRootButton.onClick = [this]
    {
        audioProcessor.setGrowLockEnabled(RootFlowAudioProcessor::GrowLockGroup::root,
                                          growLockRootButton.getToggleState());
        refreshHeaderControlState();
        repaint();
    };

    growLockMotionButton.onClick = [this]
    {
        audioProcessor.setGrowLockEnabled(RootFlowAudioProcessor::GrowLockGroup::motion,
                                          growLockMotionButton.getToggleState());
        refreshHeaderControlState();
        repaint();
    };

    growLockAirButton.onClick = [this]
    {
        audioProcessor.setGrowLockEnabled(RootFlowAudioProcessor::GrowLockGroup::air,
                                          growLockAirButton.getToggleState());
        refreshHeaderControlState();
        repaint();
    };

    growLockFxButton.onClick = [this]
    {
        audioProcessor.setGrowLockEnabled(RootFlowAudioProcessor::GrowLockGroup::fx,
                                          growLockFxButton.getToggleState());
        refreshHeaderControlState();
        repaint();
    };

    growLockSeqButton.onClick = [this]
    {
        audioProcessor.setGrowLockEnabled(RootFlowAudioProcessor::GrowLockGroup::sequencer,
                                          growLockSeqButton.getToggleState());
        refreshHeaderControlState();
        repaint();
    };

    promptEditor.onReturnKey = [this]
    {
        applyPromptSeed();
    };

    promptEditor.onTextChange = [this]
    {
        refreshHeaderControlState();
        repaint();
    };

    morphPromptEditor.onReturnKey = [this]
    {
        applyPromptSeed();
    };

    morphPromptEditor.onTextChange = [this]
    {
        refreshHeaderControlState();
        repaint();
    };

    promptMorphSlider.onValueChange = [this]
    {
        refreshHeaderControlState();
        repaint();
    };

    promptApplyButton.onClick = [this]
    {
        applyPromptSeed();
    };

    for (size_t i = 0; i < seedMemoryButtons.size(); ++i)
    {
        seedMemoryButtons[i].onClick = [] {};
    }

    mutateButton.onClick = [this] {
        audioProcessor.mutatePlant();
        mainLayout->getCenterComponent().getEnergyDisplay().triggerSporeBurst();
        refreshHeaderControlState();
    };

    for (auto* headerControl : { static_cast<juce::Component*>(&waveSelector),
                                 static_cast<juce::Component*>(&oversamplingSelector),
                                 static_cast<juce::Component*>(&presetBox),
                                 static_cast<juce::Component*>(&patchMenuButton),
                                 static_cast<juce::Component*>(&presetSaveButton),
                                 static_cast<juce::Component*>(&presetDeleteButton),
                                 static_cast<juce::Component*>(&utilityMenuButton),
                                 static_cast<juce::Component*>(&mutateModeButton),
                                 static_cast<juce::Component*>(&mutateButton),
                                 static_cast<juce::Component*>(&growLockRootButton),
                                 static_cast<juce::Component*>(&growLockMotionButton),
                                 static_cast<juce::Component*>(&growLockAirButton),
                                 static_cast<juce::Component*>(&growLockFxButton),
                                 static_cast<juce::Component*>(&growLockSeqButton),
                                 static_cast<juce::Component*>(&promptEditor),
                                 static_cast<juce::Component*>(&morphPromptEditor),
                                 static_cast<juce::Component*>(&promptMorphSlider),
                                 static_cast<juce::Component*>(&promptApplyButton),
                                 static_cast<juce::Component*>(&seedMemoryButtons[0]),
                                 static_cast<juce::Component*>(&seedMemoryButtons[1]),
                                 static_cast<juce::Component*>(&seedMemoryButtons[2]),
                                 static_cast<juce::Component*>(&midiLearnButton) })
    {
        if (headerControl != nullptr)
            registerHeaderControl(*headerControl);
    }

    refreshHeaderControlState();
    updateAnimationTimerState();
    const auto initialBounds = getInitialEditorBounds();
    setSize(initialBounds.getWidth(), initialBounds.getHeight());

    // Overlay MUST be added LAST so it sits on top of every other child in Z-order
    addAndMakeVisible(atmosphericOverlay);

    startTimerHz(30); // Keep motion present without making the UI feel busy
}

RootFlowAudioProcessorEditor::~RootFlowAudioProcessorEditor()
{
    for (auto* headerControl : { static_cast<juce::Component*>(&waveSelector),
                                 static_cast<juce::Component*>(&oversamplingSelector),
                                 static_cast<juce::Component*>(&presetBox),
                                 static_cast<juce::Component*>(&patchMenuButton),
                                 static_cast<juce::Component*>(&presetSaveButton),
                                 static_cast<juce::Component*>(&presetDeleteButton),
                                 static_cast<juce::Component*>(&utilityMenuButton),
                                 static_cast<juce::Component*>(&mutateModeButton),
                                 static_cast<juce::Component*>(&mutateButton),
                                 static_cast<juce::Component*>(&growLockRootButton),
                                 static_cast<juce::Component*>(&growLockMotionButton),
                                 static_cast<juce::Component*>(&growLockAirButton),
                                 static_cast<juce::Component*>(&growLockFxButton),
                                 static_cast<juce::Component*>(&growLockSeqButton),
                                 static_cast<juce::Component*>(&promptEditor),
                                 static_cast<juce::Component*>(&morphPromptEditor),
                                 static_cast<juce::Component*>(&promptMorphSlider),
                                 static_cast<juce::Component*>(&promptApplyButton),
                                 static_cast<juce::Component*>(&seedMemoryButtons[0]),
                                 static_cast<juce::Component*>(&seedMemoryButtons[1]),
                                 static_cast<juce::Component*>(&seedMemoryButtons[2]),
                                 static_cast<juce::Component*>(&midiLearnButton) })
    {
        if (headerControl != nullptr)
            headerControl->removeMouseListener(this);
    }

    setLookAndFeel(nullptr);
}

void RootFlowAudioProcessorEditor::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    
    float phase = (mainLayout != nullptr) ? mainLayout->getPulsePhase() : 0.0f;
    RootFlow::fillBackdrop(g, bounds, phase);

    auto mainArea = bounds;
    const auto headerHeight = getHeaderHeightForSize(getHeight());
    auto headerArea = mainArea.removeFromTop((float) headerHeight);
    juce::ColourGradient headerGrad(juce::Colour(0xff09140f).withAlpha(0.92f), headerArea.getTopLeft(),
                                    juce::Colour(0xff07110d).withAlpha(0.82f), headerArea.getBottomRight(), false);
    g.setGradientFill(headerGrad);
    g.fillRect(headerArea);

    g.setColour(juce::Colours::white.withAlpha(0.035f));
    g.drawLine(18.0f, 9.0f, (float) getWidth() - 18.0f, 9.0f, 1.0f);
    g.setColour(RootFlow::accent.withAlpha(0.05f));
    g.drawLine(0.0f, (float) headerHeight, (float) getWidth(), (float) headerHeight, 1.0f);

    const bool hoverEffectsEnabled = RootFlow::areHoverEffectsEnabled();
    const bool idleEffectsEnabled = RootFlow::areIdleEffectsEnabled();
    const bool popupOverlaysEnabled = RootFlow::arePopupOverlaysEnabled();
    const bool presetDirty = audioProcessor.isCurrentPresetDirty();
    const bool midiArmed = audioProcessor.isMidiLearnArmed();
    const bool testToneEnabled = audioProcessor.isTestToneEnabled();
    const bool hasUserPreset = audioProcessor.getCurrentUserPresetIndex() >= 0;
    const auto promptSummary = audioProcessor.getLastPromptSummary();
    auto* focusControl = headerFocusedControl;
    if (! hoverEffectsEnabled && focusControl != nullptr && ! focusControl->isMouseButtonDown())
        focusControl = nullptr;
    const bool focusWave = focusControl == &waveSelector;
    const bool focusOversampling = focusControl == &oversamplingSelector;
    const bool focusPreset = focusControl == &presetBox;
    const bool focusPatchMenu = focusControl == &patchMenuButton;
    const bool focusMutate = focusControl == &mutateButton;
    const bool focusLockRoot = focusControl == &growLockRootButton;
    const bool focusLockMotion = focusControl == &growLockMotionButton;
    const bool focusLockAir = focusControl == &growLockAirButton;
    const bool focusLockFx = focusControl == &growLockFxButton;
    const bool focusLockSeq = focusControl == &growLockSeqButton;
    const bool focusPrompt = focusControl == &promptEditor;
    const bool focusPromptB = focusControl == &morphPromptEditor;
    const bool focusPromptMorph = focusControl == &promptMorphSlider;
    const bool focusGrow = focusControl == &promptApplyButton;
    const bool focusSeedMemoryA = focusControl == &seedMemoryButtons[0];
    const bool focusSeedMemoryB = focusControl == &seedMemoryButtons[1];
    const bool focusSeedMemoryC = focusControl == &seedMemoryButtons[2];
    const bool focusUtilityMenu = focusControl == &utilityMenuButton;
    const bool anyGrowLockEnabled = audioProcessor.isGrowLockEnabled(RootFlowAudioProcessor::GrowLockGroup::root)
                                 || audioProcessor.isGrowLockEnabled(RootFlowAudioProcessor::GrowLockGroup::motion)
                                 || audioProcessor.isGrowLockEnabled(RootFlowAudioProcessor::GrowLockGroup::air)
                                 || audioProcessor.isGrowLockEnabled(RootFlowAudioProcessor::GrowLockGroup::fx)
                                 || audioProcessor.isGrowLockEnabled(RootFlowAudioProcessor::GrowLockGroup::sequencer);
    const bool focusLeftCluster = focusWave || focusOversampling || focusPreset || focusPatchMenu || focusMutate;
    const bool focusRightCluster = focusUtilityMenu;

    juce::String focusSection;
    juce::String focusValue;
    juce::Colour focusTint = RootFlow::textMuted;

    if (focusWave)
    {
        focusSection = "Osc Field";
        focusValue = waveSelector.getText().isNotEmpty() ? waveSelector.getText() : juce::String("Sine");
        focusTint = RootFlow::accentSoft;
    }
    else if (focusOversampling)
    {
        focusSection = "Upsampling";
        focusValue = oversamplingSelector.getText().isNotEmpty() ? oversamplingSelector.getText() : juce::String("1x");
        focusTint = RootFlow::accentSoft;
    }
    else if (focusPreset)
    {
        focusSection = "Patch Library";
        focusValue = presetBox.getText().isNotEmpty() ? presetBox.getText()
                                                      : juce::String("Factory Seed");
        focusTint = RootFlow::accent;
    }
    else if (focusPatchMenu)
    {
        focusSection = "Patch Menu";
        focusValue = (presetDirty ? juce::String("Unsaved / ") : juce::String())
                   + (hasUserPreset ? juce::String("User Patch / ") : juce::String("Factory Patch / "))
                   + audioProcessor.getMutationModeDisplayName();
        focusTint = presetDirty ? RootFlow::violet
                                : RootFlow::accent.interpolatedWith(RootFlow::amber, 0.20f);
    }
    else if (focusMutate)
    {
        focusSection = "Evolve DNA";
        focusValue = audioProcessor.getMutationModeDisplayName() + " Mutation";
        focusTint = RootFlow::amber;
    }
    else if (focusLockRoot)
    {
        focusSection = "Grow Lock";
        focusValue = "Root / Low Body";
        focusTint = RootFlow::accentSoft;
    }
    else if (focusLockMotion)
    {
        focusSection = "Grow Lock";
        focusValue = "Motion / Pulse";
        focusTint = RootFlow::violet;
    }
    else if (focusLockAir)
    {
        focusSection = "Grow Lock";
        focusValue = "Air / Space";
        focusTint = RootFlow::accent;
    }
    else if (focusLockFx)
    {
        focusSection = "Grow Lock";
        focusValue = "FX / Instability";
        focusTint = RootFlow::amber;
    }
    else if (focusLockSeq)
    {
        focusSection = "Grow Lock";
        focusValue = "Sequencer Pattern";
        focusTint = RootFlow::amber.interpolatedWith(RootFlow::violet, 0.35f);
    }
    else if (focusPrompt)
    {
        focusSection = "Prompt Seed A";
        focusValue = promptEditor.getText().trim().isNotEmpty() ? promptEditor.getText().trim()
                                                                : juce::String("Describe a patch");
        focusTint = RootFlow::accent;
    }
    else if (focusPromptB)
    {
        focusSection = "Prompt Seed B";
        focusValue = morphPromptEditor.getText().trim().isNotEmpty() ? morphPromptEditor.getText().trim()
                                                                     : juce::String("Describe a second patch");
        focusTint = RootFlow::amber;
    }
    else if (focusPromptMorph)
    {
        focusSection = "Morph";
        focusValue = juce::String(juce::roundToInt((float) promptMorphSlider.getValue() * 100.0f)) + "% Seed B";
        focusTint = RootFlow::violet;
    }
    else if (focusGrow)
    {
        focusSection = morphPromptEditor.getText().trim().isNotEmpty() ? juce::String("Morph Patch")
                                                                       : juce::String("Grow Patch");
        focusValue = promptSummary;
        focusTint = RootFlow::amber;
    }
    else if (focusSeedMemoryA || focusSeedMemoryB || focusSeedMemoryC)
    {
        int memoryIndex = 0;
        if (focusSeedMemoryB)
            memoryIndex = 1;
        else if (focusSeedMemoryC)
            memoryIndex = 2;

        focusSection = "Seed Memory";
        focusValue = seedMemoryPrompts[(size_t) memoryIndex].isNotEmpty() ? seedMemoryPrompts[(size_t) memoryIndex]
                                                                          : juce::String("Remembered Seed");
        if (seedMemorySummaries[(size_t) memoryIndex].isNotEmpty())
            focusValue << " / " << seedMemorySummaries[(size_t) memoryIndex];
        focusTint = RootFlow::violet.interpolatedWith(RootFlow::accent, 0.25f);
    }
    else if (focusUtilityMenu)
    {
        focusSection = "Utility Menu";
        focusValue = makeUtilitySummaryText(hoverEffectsEnabled, idleEffectsEnabled, popupOverlaysEnabled, testToneEnabled, audioProcessor);
        focusTint = testToneEnabled ? RootFlow::amber
                                    : RootFlow::accentSoft.interpolatedWith(RootFlow::accent, 0.25f);
    }
    auto leftCluster = waveSelector.getBounds().getUnion(oversamplingSelector.getBounds())
                                      .getUnion(presetBox.getBounds())
                                      .getUnion(patchMenuButton.getBounds())
                                      .getUnion(mutateButton.getBounds())
                                      .toFloat()
                                      .expanded(12.0f, 8.0f);
    auto rightCluster = utilityMenuButton.getBounds().toFloat().expanded(12.0f, 8.0f);

    if (! leftCluster.isEmpty())
    {
        RootFlow::drawGlassPanel(g, leftCluster, 16.0f, focusLeftCluster ? 0.56f : 0.42f);
        g.setColour((focusLeftCluster ? focusTint : juce::Colours::white).withAlpha(focusLeftCluster ? 0.016f : 0.009f));
        g.fillRoundedRectangle(leftCluster.withHeight(leftCluster.getHeight() * 0.46f), 16.0f);
    }

    if (! rightCluster.isEmpty())
    {
        RootFlow::drawGlassPanel(g, rightCluster, 16.0f, focusRightCluster ? 0.54f : 0.40f);
        g.setColour((focusRightCluster ? focusTint : juce::Colours::white).withAlpha(focusRightCluster ? 0.016f : 0.009f));
        g.fillRoundedRectangle(rightCluster.withHeight(rightCluster.getHeight() * 0.46f), 16.0f);
    }

    auto seedCluster = promptEditor.getBounds()
                         .getUnion(morphPromptEditor.getBounds())
                         .getUnion(promptMorphSlider.getBounds())
                         .getUnion(promptApplyButton.getBounds())
                         .toFloat()
                         .expanded(promptEditor.getY() > getHeaderHeightForSize(getHeight()) ? 4.0f : 8.0f,
                                   promptEditor.getY() > getHeaderHeightForSize(getHeight()) ? 4.0f : 8.0f);
    if (seedCluster.getWidth() > 120.0f)
    {
        const bool dockedInTitle = promptEditor.getY() > getHeaderHeightForSize(getHeight());
        const bool promptFocused = focusPrompt || focusPromptB || focusPromptMorph || focusGrow;
        const auto promptTint = promptFocused ? focusTint : RootFlow::accent.interpolatedWith(RootFlow::amber, 0.18f);
        RootFlow::drawGlassPanel(g, seedCluster, dockedInTitle ? 12.0f : 15.0f, promptFocused ? 0.56f : (dockedInTitle ? 0.40f : 0.46f));
        g.setColour(promptTint.withAlpha(promptFocused ? (dockedInTitle ? 0.020f : 0.026f)
                                                       : (dockedInTitle ? 0.008f : 0.012f)));
        g.fillRoundedRectangle(seedCluster.reduced(1.0f), dockedInTitle ? 11.0f : 14.0f);
        if (! dockedInTitle)
        {
            auto promptCaption = juce::Rectangle<float>(72.0f, 14.0f)
                                     .withCentre({ seedCluster.getX() + 46.0f, seedCluster.getY() - 1.0f });
            drawToolbarCaption(g, promptCaption, "SEEDS", promptTint);
        }
    }

    auto memoryCluster = seedMemoryButtons[0].getBounds()
                           .getUnion(seedMemoryButtons[1].getBounds())
                           .getUnion(seedMemoryButtons[2].getBounds())
                           .toFloat()
                           .expanded(seedMemoryButtons[0].getY() > getHeaderHeightForSize(getHeight()) ? 3.0f : 7.0f,
                                     seedMemoryButtons[0].getY() > getHeaderHeightForSize(getHeight()) ? 3.0f : 6.0f);
    if (memoryCluster.getWidth() > 70.0f)
    {
        const bool dockedInTitle = seedMemoryButtons[0].getY() > getHeaderHeightForSize(getHeight());
        const bool memoryFocused = focusSeedMemoryA || focusSeedMemoryB || focusSeedMemoryC;
        const auto memoryTint = memoryFocused ? focusTint : RootFlow::violet.interpolatedWith(RootFlow::accentSoft, 0.22f);
        RootFlow::drawGlassPanel(g, memoryCluster, dockedInTitle ? 10.0f : 13.0f, memoryFocused ? 0.52f : (dockedInTitle ? 0.34f : 0.42f));
        g.setColour(memoryTint.withAlpha(memoryFocused ? (dockedInTitle ? 0.018f : 0.024f)
                                                       : (dockedInTitle ? 0.007f : 0.010f)));
        g.fillRoundedRectangle(memoryCluster.reduced(1.0f), dockedInTitle ? 9.0f : 12.0f);
        if (! dockedInTitle)
        {
            auto memoryCaption = juce::Rectangle<float>(80.0f, 14.0f)
                                     .withCentre({ memoryCluster.getX() + 50.0f, memoryCluster.getY() - 1.0f });
            drawToolbarCaption(g, memoryCaption, "MEMORY", memoryTint);
        }
    }

    const auto actionTint = focusMutate ? RootFlow::amber
                          : focusPatchMenu ? RootFlow::violet.interpolatedWith(RootFlow::amber, 0.35f)
                                           : presetDirty ? RootFlow::violet
                                                         : RootFlow::violet.interpolatedWith(RootFlow::amber, 0.22f);
    const bool utilityCustomised = midiArmed || anyGrowLockEnabled || testToneEnabled || ! hoverEffectsEnabled || ! idleEffectsEnabled || ! popupOverlaysEnabled;
    const auto utilityTint = focusUtilityMenu ? focusTint
                           : testToneEnabled ? RootFlow::amber
                                             : utilityCustomised ? RootFlow::accent
                                                                 : RootFlow::textMuted.interpolatedWith(RootFlow::accentSoft, 0.35f);

    auto wavePreviewArea = waveSelector.getBounds().toFloat().reduced(14.0f, 7.0f);
    wavePreviewArea = wavePreviewArea.withTrimmedTop(wavePreviewArea.getHeight() * 0.56f).withTrimmedRight(22.0f);
    if (wavePreviewArea.getWidth() > 26.0f)
    {
        const float previewPhase = std::fmod((float) juce::Time::getMillisecondCounterHiRes() * 0.00018f, 1.0f);
        auto wavePreview = makeWavePreviewPath(wavePreviewArea, waveSelector.getSelectedId(), previewPhase);

        g.setColour(RootFlow::accentSoft.withAlpha(focusWave ? 0.05f : 0.018f));
        g.strokePath(wavePreview, juce::PathStrokeType(focusWave ? 2.4f : 1.6f,
                                                       juce::PathStrokeType::curved,
                                                       juce::PathStrokeType::rounded));
        g.setColour(RootFlow::accentSoft.withAlpha(focusWave ? 0.14f : 0.06f));
        g.strokePath(wavePreview, juce::PathStrokeType(focusWave ? 1.1f : 0.8f,
                                                       juce::PathStrokeType::curved,
                                                       juce::PathStrokeType::rounded));
    }

    if (! leftCluster.isEmpty())
    {
        auto actionsArea = patchMenuButton.getBounds()
                         .getUnion(mutateButton.getBounds())
                         .toFloat()
                         .expanded(4.0f, 3.0f);
        g.setColour(actionTint.withAlpha((focusPatchMenu || focusMutate) ? 0.034f : 0.016f));
        g.fillRoundedRectangle(actionsArea, 10.0f);
        g.setColour(juce::Colours::white.withAlpha(0.04f));
        g.drawRoundedRectangle(actionsArea, 10.0f, 0.8f);

        const float divider1 = ((float) waveSelector.getRight() + (float) presetBox.getX()) * 0.5f;
        const float divider2 = ((float) presetBox.getRight() + (float) patchMenuButton.getX()) * 0.5f;
        const float divider3 = ((float) patchMenuButton.getRight() + (float) mutateButton.getX()) * 0.5f;
        drawClusterDivider(g, divider1, leftCluster, RootFlow::accentSoft);
        drawClusterDivider(g, divider2, leftCluster, RootFlow::accent);
        drawClusterDivider(g, divider3, leftCluster, RootFlow::amber);
    }

    if (! rightCluster.isEmpty())
    {
        auto utilityArea = utilityMenuButton.getBounds().toFloat().expanded(4.0f, 3.0f);
        g.setColour(utilityTint.withAlpha(focusUtilityMenu ? 0.034f : (utilityCustomised ? 0.022f : 0.012f)));
        g.fillRoundedRectangle(utilityArea, 10.0f);
        g.setColour(juce::Colours::white.withAlpha(0.04f));
        g.drawRoundedRectangle(utilityArea, 10.0f, 0.8f);

    }

    if (popupOverlaysEnabled && focusControl != nullptr)
    {
        auto focusArea = focusControl->getBounds().toFloat().expanded(4.0f, 4.0f);
        g.setColour(focusTint.withAlpha(0.032f));
        g.fillRoundedRectangle(focusArea, 11.0f);
        g.setColour(juce::Colours::white.withAlpha(0.05f));
        g.drawRoundedRectangle(focusArea, 11.0f, 0.8f);
        g.setColour(focusTint.withAlpha(0.10f));
        g.drawRoundedRectangle(focusArea.reduced(1.0f), 10.0f, 0.9f);

        auto focusBubble = juce::Rectangle<float>(juce::jlimit(126.0f, 196.0f, 116.0f + (float) focusValue.length() * 4.2f), 24.0f)
                               .withCentre({ focusArea.getCentreX(), 21.0f });
        focusBubble.setX(juce::jlimit(18.0f, (float) getWidth() - focusBubble.getWidth() - 18.0f, focusBubble.getX()));
        drawHeaderFocusBubble(g, focusBubble, focusSection, focusValue, focusTint, focusControl->isMouseButtonDown());
    }

    auto keyboardArea = juce::Rectangle<float>(20.0f, (float) getHeight() - 124.0f, (float) getWidth() - 40.0f, 108.0f);
    auto keyboardStateChip = juce::Rectangle<float>(keyboardArea.getX() + 18.0f, keyboardArea.getY() - 20.0f, 118.0f, 18.0f);
    auto keyboardBadge = juce::Rectangle<float>(keyboardArea.getCentreX() - 132.0f, keyboardArea.getY() - 20.0f, 264.0f, 18.0f);
    auto keyboardMidiChip = juce::Rectangle<float>(keyboardArea.getRight() - 172.0f, keyboardArea.getY() - 20.0f, 154.0f, 18.0f);
    auto cradleArea = keyboardArea.expanded(14.0f, 18.0f).translated(0.0f, 10.0f);
    auto cradle = makeKeyboardCradle(cradleArea);
    auto cabinetBase = cradleArea.expanded(8.0f, 10.0f).translated(0.0f, 8.0f);
    auto keyFrontApron = juce::Rectangle<float>(keyboardArea.getWidth() + 18.0f, 28.0f)
                             .withCentre({ keyboardArea.getCentreX(), keyboardArea.getBottom() + 8.0f });
    keyFrontApron = keyFrontApron.withY(keyboardArea.getBottom() - 3.0f);
    auto leftFoot = juce::Rectangle<float>(86.0f, 12.0f)
                        .withBottom((float) getHeight() - 4.0f)
                        .withX(keyboardArea.getX() + 72.0f);
    auto rightFoot = leftFoot.withX(keyboardArea.getRight() - leftFoot.getWidth() - 72.0f);
    auto midiSnapshot = audioProcessor.getMidiActivitySnapshot();
    const int heldNotes = countHeldNotes(audioProcessor.getKeyboardState());
    const bool keyboardHovered = hoverEffectsEnabled && keyboardDrawer.isMouseOverOrDragging();
    const bool keyboardActive = heldNotes > 0;
    const bool keyboardFocused = keyboardHovered || keyboardActive;
    const auto midiTint = getMidiActivityTint(midiSnapshot);
    const auto activityText = midiSnapshot.type != RootFlowAudioProcessor::MidiActivitySnapshot::Type::none
        ? getMidiActivityValue(midiSnapshot)
        : juce::String("Awaiting Input");

    if (popupOverlaysEnabled && keyboardFocused)
    {
        drawHeaderFocusBubble(g,
                              keyboardStateChip,
                              "Keys",
                              keyboardActive ? juce::String(heldNotes) + " Active"
                                             : (keyboardHovered ? juce::String("Hover Ready")
                                                                : juce::String("Idle")),
                              keyboardActive ? RootFlow::accentSoft : RootFlow::textMuted,
                              keyboardFocused);
    }

    RootFlow::drawGlassPanel(g, keyboardBadge, keyboardBadge.getHeight() * 0.5f, keyboardFocused ? 0.46f : 0.30f);
    g.setColour((keyboardFocused ? RootFlow::accentSoft : RootFlow::accent).withAlpha(keyboardFocused ? 0.04f : 0.022f));
    g.fillRoundedRectangle(keyboardBadge.reduced(2.0f), keyboardBadge.getHeight() * 0.42f);
    g.setFont(RootFlow::getFont(9.2f).boldened());
    g.setColour((keyboardFocused ? RootFlow::text : RootFlow::textMuted).withAlpha(0.90f));
    g.drawFittedText(keyboardActive ? juce::String("PERFORMANCE KEYS / ") + juce::String(heldNotes) + " ACTIVE"
                                    : (keyboardHovered ? juce::String("PERFORMANCE KEYS / TOUCH BED")
                                                       : juce::String("PERFORMANCE KEYS / ROOT BED")),
                     keyboardBadge.toNearestInt().reduced(10, 0), juce::Justification::centred, 1);

    if (popupOverlaysEnabled
        && (keyboardActive
            || keyboardHovered
            || midiSnapshot.wasMapped
            || midiSnapshot.type != RootFlowAudioProcessor::MidiActivitySnapshot::Type::none))
    {
        drawHeaderFocusBubble(g,
                              keyboardMidiChip,
                              getMidiActivitySection(midiSnapshot),
                              activityText,
                              midiTint,
                              keyboardActive || midiSnapshot.wasMapped);
    }

    auto drawCabinetFoot = [&g](juce::Rectangle<float> area, juce::Colour tint)
    {
        g.setColour(juce::Colours::black.withAlpha(0.22f));
        g.fillRoundedRectangle(area.translated(0.0f, 3.0f), area.getHeight() * 0.54f);
        juce::ColourGradient footGrad(RootFlow::panelSoft.brighter(0.12f).interpolatedWith(tint, 0.06f),
                                      area.getCentreX(), area.getY(),
                                      RootFlow::bg.darker(0.34f),
                                      area.getCentreX(), area.getBottom(), false);
        g.setGradientFill(footGrad);
        g.fillRoundedRectangle(area, area.getHeight() * 0.50f);
        g.setColour(juce::Colours::white.withAlpha(0.08f));
        g.drawRoundedRectangle(area.reduced(0.8f), area.getHeight() * 0.44f, 0.8f);
    };

    g.setColour(juce::Colours::black.withAlpha(0.24f));
    g.fillRoundedRectangle(cabinetBase.translated(0.0f, 10.0f), 28.0f);
    g.setColour(juce::Colours::black.withAlpha(0.10f));
    g.fillRoundedRectangle(cabinetBase.translated(0.0f, 4.0f), 27.0f);

    juce::ColourGradient cabinetBaseGrad(RootFlow::panelSoft.brighter(0.06f), cabinetBase.getCentreX(), cabinetBase.getY(),
                                         RootFlow::bg.darker(0.24f), cabinetBase.getCentreX(), cabinetBase.getBottom(), false);
    g.setGradientFill(cabinetBaseGrad);
    g.fillRoundedRectangle(cabinetBase, 26.0f);
    g.setColour(juce::Colours::white.withAlpha(0.07f));
    g.drawRoundedRectangle(cabinetBase.reduced(1.0f), 25.0f, 0.9f);

    g.setColour(juce::Colours::black.withAlpha(0.20f));
    g.fillRoundedRectangle(keyFrontApron.translated(0.0f, 5.0f), 16.0f);
    juce::ColourGradient keyApronGrad(RootFlow::panelSoft.brighter(0.10f), keyFrontApron.getCentreX(), keyFrontApron.getY(),
                                      RootFlow::bg.darker(0.28f), keyFrontApron.getCentreX(), keyFrontApron.getBottom(), false);
    g.setGradientFill(keyApronGrad);
    g.fillRoundedRectangle(keyFrontApron, 15.0f);
    g.setColour(juce::Colours::white.withAlpha(0.07f));
    g.drawRoundedRectangle(keyFrontApron.reduced(1.0f), 14.0f, 0.8f);
    g.setColour(RootFlow::accent.withAlpha(0.06f));
    g.drawRoundedRectangle(keyFrontApron, 15.0f, 1.0f);

    drawCabinetFoot(leftFoot, RootFlow::accentSoft);
    drawCabinetFoot(rightFoot, RootFlow::violet);

    juce::ColourGradient cradleGrad(RootFlow::panelSoft.brighter(0.18f), cradleArea.getCentreX(), cradleArea.getY(),
                                    RootFlow::panel.darker(0.10f), cradleArea.getCentreX(), cradleArea.getBottom(), false);
    g.setGradientFill(cradleGrad);
    g.fillPath(cradle);

    g.setColour((keyboardFocused ? RootFlow::accentSoft : RootFlow::accent).withAlpha(keyboardFocused ? 0.04f : 0.025f));
    g.strokePath(cradle, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour(juce::Colours::white.withAlpha(0.03f));
    g.strokePath(cradle, juce::PathStrokeType(0.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path stem;
    stem.startNewSubPath(keyboardArea.getCentreX(), keyboardArea.getY() - 150.0f);
    stem.cubicTo(keyboardArea.getCentreX() - 10.0f, keyboardArea.getY() - 92.0f,
                 keyboardArea.getCentreX() + 8.0f, keyboardArea.getY() - 34.0f,
                 keyboardArea.getCentreX(), keyboardArea.getY() + 10.0f);
    g.setColour((keyboardFocused ? RootFlow::accentSoft : RootFlow::accent).withAlpha(keyboardFocused ? 0.032f : 0.022f));
    g.strokePath(stem, juce::PathStrokeType(15.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour((keyboardFocused ? RootFlow::accentSoft : RootFlow::accent).withAlpha(keyboardFocused ? 0.10f : 0.07f));
    g.strokePath(stem, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    RootFlow::drawGlassPanel(g, keyboardArea, 24.0f, keyboardFocused ? 0.72f : 0.64f);
    juce::ColourGradient centerGlow((keyboardFocused ? RootFlow::accentSoft : RootFlow::accent).withAlpha(keyboardFocused ? 0.06f : 0.035f),
                                    keyboardArea.getCentreX(), keyboardArea.getY(),
                                    juce::Colours::transparentBlack, keyboardArea.getCentreX(), keyboardArea.getBottom(), true);
    g.setGradientFill(centerGlow);
    g.fillEllipse(keyboardArea.getCentreX() - 42.0f, keyboardArea.getY() - 10.0f, 84.0f, keyboardArea.getHeight() + 20.0f);

    if (keyboardFocused)
    {
        g.setColour((keyboardActive ? RootFlow::accentSoft : midiTint).withAlpha(0.05f));
        g.drawRoundedRectangle(keyboardArea.expanded(4.0f, 3.0f), 27.0f, 0.9f);
    }

    RootFlow::drawGlowOrb(g, { keyboardArea.getX() + 34.0f, keyboardArea.getY() + 18.0f }, 4.6f, RootFlow::violet, 0.09f);
    RootFlow::drawGlowOrb(g, { keyboardArea.getRight() - 34.0f, keyboardArea.getY() + 18.0f }, 4.6f, RootFlow::accentSoft, 0.09f);
}

void RootFlowAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();

    const int headerHeight = getHeaderHeightForSize(getHeight());
    const int keyboardHeight = getKeyboardHeightForSize(getHeight());
    const bool compactHeaderLayout = getWidth() < 1280 || getHeight() < 780;
    const int outerInsetX = compactHeaderLayout ? 18 : 24;
    const int outerInsetY = compactHeaderLayout ? 7 : 8;
    const int controlHeight = compactHeaderLayout ? 34 : 36;
    const int clusterGap = compactHeaderLayout ? 6 : 8;
    const int microGap = compactHeaderLayout ? 4 : 6;

    auto headerContent = bounds.removeFromTop(headerHeight).reduced(outerInsetX, outerInsetY);
    auto controlRow = headerContent.removeFromBottom(controlHeight);
    waveSelector.setBounds(controlRow.removeFromLeft(compactHeaderLayout ? 88 : 100));
    controlRow.removeFromLeft(microGap);
    oversamplingSelector.setBounds(controlRow.removeFromLeft(compactHeaderLayout ? 54 : 64));
    controlRow.removeFromLeft(clusterGap);
    presetBox.setBounds(controlRow.removeFromLeft(compactHeaderLayout ? 204 : 248));
    controlRow.removeFromLeft(clusterGap);
    patchMenuButton.setBounds(controlRow.removeFromLeft(compactHeaderLayout ? 52 : 60));
    controlRow.removeFromLeft(microGap);
    mutateButton.setBounds(controlRow.removeFromLeft(compactHeaderLayout ? 64 : 78));

    utilityMenuButton.setBounds(controlRow.removeFromRight(compactHeaderLayout ? 72 : 84));
    presetSaveButton.setBounds({});
    presetDeleteButton.setBounds({});
    mutateModeButton.setBounds({});
    midiLearnButton.setBounds({});
    testToneButton.setBounds({});
    popupToggleButton.setBounds({});
    idleToggleButton.setBounds({});
    hoverToggleButton.setBounds({});

    auto keyboardBounds = bounds.removeFromBottom(keyboardHeight)
                               .reduced(compactHeaderLayout ? 20 : 28,
                                        compactHeaderLayout ? 12 : 16);

    if (mainLayout != nullptr)
        mainLayout->setBounds(bounds.reduced(compactHeaderLayout ? 8 : 10, 0));

    for (auto& seedMemoryButton : seedMemoryButtons)
        seedMemoryButton.setBounds({});

    const int promptRowX = mutateButton.getRight() + clusterGap + 10;
    const int promptRowRight = utilityMenuButton.getX() - clusterGap - 10;
    const int minimumSingleSeedWidth = compactHeaderLayout ? 148 : 168;

    int activeSeedMemoryCount = 0;
    for (const auto& prompt : seedMemoryPrompts)
        if (prompt.isNotEmpty())
            ++activeSeedMemoryCount;

    auto promptDock = juce::Rectangle<int>();
    if (mainLayout != nullptr)
    {
        promptDock = mainLayout->getTitleSeedDockBounds()
                         .translated(mainLayout->getX(), mainLayout->getY())
                         .reduced(compactHeaderLayout ? 4 : 6, 0);
    }

    if (promptDock.getWidth() > minimumSingleSeedWidth)
    {
        auto promptRow = promptDock;
        const int memoryGap = compactHeaderLayout ? 4 : 5;
        const int memoryToSeedGap = compactHeaderLayout ? 6 : 8;
        const int minimumPromptWidth = minimumSingleSeedWidth + (compactHeaderLayout ? 54 : 62);
        int visibleMemoryCount = juce::jmin(activeSeedMemoryCount, 1);

        while (visibleMemoryCount > 0)
        {
            const int idealMemoryWidth = compactHeaderLayout ? 64 : 74;
            const int totalMemoryWidth = visibleMemoryCount * idealMemoryWidth + (visibleMemoryCount - 1) * memoryGap;
            const int remainingWidth = promptRow.getWidth() - totalMemoryWidth - memoryToSeedGap;
            if (remainingWidth >= minimumPromptWidth)
                break;

            --visibleMemoryCount;
        }

        if (visibleMemoryCount > 0)
        {
            const int maxMemoryWidth = juce::jmax(0, promptRow.getWidth() - minimumPromptWidth - memoryToSeedGap);
            const int buttonWidth = juce::jlimit(compactHeaderLayout ? 48 : 56,
                                                 compactHeaderLayout ? 76 : 88,
                                                 (maxMemoryWidth - memoryGap * (visibleMemoryCount - 1)) / visibleMemoryCount);
            const int totalMemoryWidth = buttonWidth * visibleMemoryCount + memoryGap * (visibleMemoryCount - 1);
            auto memoryRow = promptRow.removeFromLeft(totalMemoryWidth);
            promptRow.removeFromLeft(memoryToSeedGap);
            const int memoryButtonHeight = compactHeaderLayout ? 14 : 16;

            for (int i = 0; i < visibleMemoryCount; ++i)
            {
                auto memoryButtonArea = memoryRow.removeFromLeft(buttonWidth);
                seedMemoryButtons[(size_t) i].setBounds(memoryButtonArea.withSizeKeepingCentre(buttonWidth, memoryButtonHeight));
                if (! memoryRow.isEmpty())
                    memoryRow.removeFromLeft(memoryGap);
            }
        }

        const bool morphExpanded = morphPromptEditor.getText().trim().isNotEmpty()
                                || morphPromptEditor.hasKeyboardFocus(true);
        const int actionWidth = compactHeaderLayout ? 50 : 56;
        const int compactMorphWidth = compactHeaderLayout ? 52 : 58;
        const int minimumDualSeedWidth = compactHeaderLayout ? 220 : 248;

        if (morphExpanded && promptRow.getWidth() >= minimumDualSeedWidth)
        {
            const int sliderWidth = juce::jlimit(compactHeaderLayout ? 20 : 24,
                                                 compactHeaderLayout ? 30 : 36,
                                                 promptRow.getWidth() / 8);
            const int secondSeedWidth = juce::jmax(compactHeaderLayout ? 90 : 104,
                                                   (promptRow.getWidth() - actionWidth - sliderWidth - microGap * 3) / 3);
            const int firstSeedWidth = juce::jmax(compactHeaderLayout ? 104 : 132,
                                                  promptRow.getWidth() - actionWidth - sliderWidth - secondSeedWidth - microGap * 3);

            promptEditor.setBounds(promptRow.removeFromLeft(firstSeedWidth));
            promptRow.removeFromLeft(microGap);
            promptApplyButton.setBounds(promptRow.removeFromLeft(actionWidth));
            promptRow.removeFromLeft(microGap);
            promptMorphSlider.setBounds(promptRow.removeFromLeft(sliderWidth));
            promptRow.removeFromLeft(microGap);
            morphPromptEditor.setBounds(promptRow.removeFromLeft(juce::jmax(0, secondSeedWidth)));
        }
        else
        {
            const int promptWidth = juce::jmax(compactHeaderLayout ? 164 : 196,
                                               promptRow.getWidth() - actionWidth - compactMorphWidth - microGap * 2);
            promptEditor.setBounds(promptRow.removeFromLeft(promptWidth));
            promptRow.removeFromLeft(microGap);
            promptApplyButton.setBounds(promptRow.removeFromLeft(actionWidth));
            promptRow.removeFromLeft(microGap);
            morphPromptEditor.setBounds(promptRow.removeFromLeft(juce::jmax(0, compactMorphWidth)));
            promptMorphSlider.setBounds({});
        }
    }
    else if (promptRowRight > promptRowX + 90)
    {
        const int promptLaneGap = compactHeaderLayout ? 8 : 10;
        const int stateChipWidth = compactHeaderLayout ? 96 : 106;
        const int monitorChipWidth = compactHeaderLayout ? 112 : 124;
        int patchLaneWidth = promptRowRight - promptRowX - stateChipWidth - monitorChipWidth - promptLaneGap * 2;
        if (patchLaneWidth < minimumSingleSeedWidth + (compactHeaderLayout ? 64 : 72))
            patchLaneWidth = promptRowRight - promptRowX;

        auto promptRow = juce::Rectangle<int>(promptRowX,
                                              outerInsetY + (compactHeaderLayout ? 22 : 23),
                                              juce::jmax(0, patchLaneWidth),
                                              compactHeaderLayout ? 22 : 24);
        const bool morphExpanded = morphPromptEditor.getText().trim().isNotEmpty()
                                || morphPromptEditor.hasKeyboardFocus(true);
        const int actionWidth = compactHeaderLayout ? 50 : 56;
        const int compactMorphWidth = compactHeaderLayout ? 52 : 58;
        const int minimumDualSeedWidth = compactHeaderLayout ? 182 : 208;

        if (morphExpanded && promptRow.getWidth() >= minimumDualSeedWidth)
        {
            const int sliderWidth = juce::jlimit(compactHeaderLayout ? 22 : 26,
                                                 compactHeaderLayout ? 34 : 40,
                                                 promptRow.getWidth() / 6);
            const int secondSeedWidth = juce::jmax(compactHeaderLayout ? 82 : 96,
                                                   (promptRow.getWidth() - actionWidth - sliderWidth - microGap * 3) / 3);
            const int firstSeedWidth = juce::jmax(compactHeaderLayout ? 92 : 110,
                                                  promptRow.getWidth() - actionWidth - sliderWidth - secondSeedWidth - microGap * 3);

            promptEditor.setBounds(promptRow.removeFromLeft(firstSeedWidth));
            promptRow.removeFromLeft(microGap);
            promptApplyButton.setBounds(promptRow.removeFromLeft(actionWidth));
            promptRow.removeFromLeft(microGap);
            promptMorphSlider.setBounds(promptRow.removeFromLeft(sliderWidth));
            promptRow.removeFromLeft(microGap);
            morphPromptEditor.setBounds(promptRow.removeFromLeft(juce::jmax(0, secondSeedWidth)));
        }
        else
        {
            const int promptWidth = juce::jmax(compactHeaderLayout ? 148 : 166,
                                               promptRow.getWidth() - actionWidth - compactMorphWidth - microGap * 2);
            promptEditor.setBounds(promptRow.removeFromLeft(promptWidth));
            promptRow.removeFromLeft(microGap);
            promptApplyButton.setBounds(promptRow.removeFromLeft(actionWidth));
            promptRow.removeFromLeft(microGap);
            morphPromptEditor.setBounds(promptRow.removeFromLeft(juce::jmax(0, compactMorphWidth)));
            promptMorphSlider.setBounds({});
        }

    }
    else
    {
        promptEditor.setBounds({});
        morphPromptEditor.setBounds({});
        promptMorphSlider.setBounds({});
        growLockRootButton.setBounds({});
        growLockMotionButton.setBounds({});
        growLockAirButton.setBounds({});
        growLockFxButton.setBounds({});
        growLockSeqButton.setBounds({});
        promptApplyButton.setBounds({});
        for (auto& seedMemoryButton : seedMemoryButtons)
            seedMemoryButton.setBounds({});
    }

    growLockRootButton.setBounds({});
    growLockMotionButton.setBounds({});
    growLockAirButton.setBounds({});
    growLockFxButton.setBounds({});
    growLockSeqButton.setBounds({});

    keyboardDrawer.setBounds(keyboardBounds);

    // Overlay covers the full window, on top of everything
    atmosphericOverlay.setBounds(getLocalBounds());
}

void RootFlowAudioProcessorEditor::timerCallback()
{
    repaint();

    // Drive overlay from synth params
    float phase          = (mainLayout != nullptr) ? mainLayout->getPulsePhase() : 0.0f;
    float atmosIntensity = *audioProcessor.tree.getRawParameterValue("atmosphere");

    // MIDI-reactive bio-dust: grab latest velocity, smooth decay
    auto midiSnap = audioProcessor.getMidiActivitySnapshot();
    const bool midiActive = (midiSnap.type == RootFlowAudioProcessor::MidiActivitySnapshot::Type::note
                             && midiSnap.value > 0);
    float rawVel = midiActive ? juce::jlimit(0.0f, 1.0f, (float) midiSnap.value / 127.0f) * 0.72f : 0.0f;

    // Smooth: 5-frame attack, ~60-frame decay (≈2 s at 30 Hz)
    if (rawVel > smoothedMidiVelocity)
        smoothedMidiVelocity = rawVel;                    // instant attack
    else
        smoothedMidiVelocity *= 0.93f;                   // slightly shorter decay keeps the overlay calmer

    atmosIntensity *= 0.82f;

    atmosphericOverlay.setPhase(phase);
    atmosphericOverlay.setIntensity(atmosIntensity);
    atmosphericOverlay.setMidiVelocity(smoothedMidiVelocity);
    atmosphericOverlay.repaint();

    refreshHeaderControlState();
}

void RootFlowAudioProcessorEditor::applyPromptSeed()
{
    const auto seedA = promptEditor.getText().trim();
    const auto seedB = morphPromptEditor.getText().trim();

    if (seedA.isNotEmpty() && seedB.isNotEmpty())
        audioProcessor.applyPromptMorph(seedA, seedB, (float) promptMorphSlider.getValue());
    else if (seedA.isNotEmpty() || seedB.isNotEmpty())
        audioProcessor.applyPromptPatch(seedA.isNotEmpty() ? seedA : seedB);
    else
        audioProcessor.applyPromptPatch({});

    if (mainLayout != nullptr)
        mainLayout->getCenterComponent().getEnergyDisplay().triggerSporeBurst();

    refreshHeaderControlState();
    repaint();
}

void RootFlowAudioProcessorEditor::showPatchMenu()
{
    enum MenuIds
    {
        savePatch = 1,
        deletePatch,
        modeSoft,
        modeBalanced,
        modeWild,
        modeSeq,
        recipeStart = 100
    };

    juce::PopupMenu menu;
    menu.addSectionHeader("Patch");
    menu.addItem(savePatch, "Save User Snapshot", true, false);
    menu.addItem(deletePatch, "Delete User Snapshot", audioProcessor.getCurrentUserPresetIndex() >= 0, false);
    menu.addSeparator();
    menu.addSectionHeader("Mutate Mode");

    const auto currentMode = audioProcessor.getMutationMode();
    menu.addItem(modeSoft, "Soft", true, currentMode == RootFlowAudioProcessor::MutationMode::gentle);
    menu.addItem(modeBalanced, "Balanced", true, currentMode == RootFlowAudioProcessor::MutationMode::balanced);
    menu.addItem(modeWild, "Wild", true, currentMode == RootFlowAudioProcessor::MutationMode::wild);
    menu.addItem(modeSeq, "Seq Only", true, currentMode == RootFlowAudioProcessor::MutationMode::sequencer);
    menu.addSeparator();
    menu.addSectionHeader("Style Recipes");

    for (size_t i = 0; i < stylePromptRecipes.size(); ++i)
    {
        const auto& recipe = stylePromptRecipes[i];
        juce::String label(recipe.menuLabel);
        if (recipe.tooltip != nullptr && juce::String(recipe.tooltip).isNotEmpty())
            label << "  |  " << recipe.tooltip;

        menu.addItem(recipeStart + (int) i, label, true, false);
    }

    auto options = juce::PopupMenu::Options()
                       .withTargetComponent(&patchMenuButton)
                       .withMinimumWidth(320);

    menu.showMenuAsync(options, [this] (int result)
    {
        switch (result)
        {
            case savePatch:
                audioProcessor.saveCurrentStateAsUserPreset();
                refreshHeaderControlState();
                repaint();
                break;
            case deletePatch:
                audioProcessor.deleteCurrentUserPreset();
                refreshHeaderControlState();
                repaint();
                break;
            case modeSoft:
                audioProcessor.setMutationMode(RootFlowAudioProcessor::MutationMode::gentle);
                refreshHeaderControlState();
                repaint();
                break;
            case modeBalanced:
                audioProcessor.setMutationMode(RootFlowAudioProcessor::MutationMode::balanced);
                refreshHeaderControlState();
                repaint();
                break;
            case modeWild:
                audioProcessor.setMutationMode(RootFlowAudioProcessor::MutationMode::wild);
                refreshHeaderControlState();
                repaint();
                break;
            case modeSeq:
                audioProcessor.setMutationMode(RootFlowAudioProcessor::MutationMode::sequencer);
                refreshHeaderControlState();
                repaint();
                break;
            default:
                if (result >= recipeStart
                    && result < recipeStart + (int) stylePromptRecipes.size())
                {
                    applyStylePromptRecipe(result - recipeStart);
                }
                break;
        }
    });
}

void RootFlowAudioProcessorEditor::showUtilityMenu()
{
    enum MenuIds
    {
        toggleLockRoot = 1,
        toggleLockMotion,
        toggleLockAir,
        toggleLockFx,
        toggleLockSeq,
        toggleMidiLearn,
        toggleHover,
        toggleIdle,
        togglePopups,
        toggleTone
    };

    juce::PopupMenu menu;
    menu.addSectionHeader("Grow Locks");
    menu.addItem(toggleLockRoot, "Keep Root", true, audioProcessor.isGrowLockEnabled(RootFlowAudioProcessor::GrowLockGroup::root));
    menu.addItem(toggleLockMotion, "Keep Motion", true, audioProcessor.isGrowLockEnabled(RootFlowAudioProcessor::GrowLockGroup::motion));
    menu.addItem(toggleLockAir, "Keep Air", true, audioProcessor.isGrowLockEnabled(RootFlowAudioProcessor::GrowLockGroup::air));
    menu.addItem(toggleLockFx, "Keep FX", true, audioProcessor.isGrowLockEnabled(RootFlowAudioProcessor::GrowLockGroup::fx));
    menu.addItem(toggleLockSeq, "Keep Sequencer", true, audioProcessor.isGrowLockEnabled(RootFlowAudioProcessor::GrowLockGroup::sequencer));
    menu.addSeparator();
    menu.addSectionHeader("Midi");
    menu.addItem(toggleMidiLearn, "Midi Learn", true, audioProcessor.isMidiLearnArmed());
    menu.addSeparator();
    menu.addSectionHeader("View");
    menu.addItem(toggleHover, "Hover FX", true, RootFlow::areHoverEffectsEnabled());
    menu.addItem(toggleIdle, "Idle Motion", true, RootFlow::areIdleEffectsEnabled());
    menu.addItem(togglePopups, "Inspector Popups", true, RootFlow::arePopupOverlaysEnabled());
    menu.addSeparator();
    menu.addSectionHeader("Monitor");
    menu.addItem(toggleTone, "Tone Probe", true, audioProcessor.isTestToneEnabled());

    auto options = juce::PopupMenu::Options()
                       .withTargetComponent(&utilityMenuButton)
                       .withMinimumWidth(180);

    menu.showMenuAsync(options, [this] (int result)
    {
        switch (result)
        {
            case toggleLockRoot:   setGrowLockEnabled(RootFlowAudioProcessor::GrowLockGroup::root, ! audioProcessor.isGrowLockEnabled(RootFlowAudioProcessor::GrowLockGroup::root)); break;
            case toggleLockMotion: setGrowLockEnabled(RootFlowAudioProcessor::GrowLockGroup::motion, ! audioProcessor.isGrowLockEnabled(RootFlowAudioProcessor::GrowLockGroup::motion)); break;
            case toggleLockAir:    setGrowLockEnabled(RootFlowAudioProcessor::GrowLockGroup::air, ! audioProcessor.isGrowLockEnabled(RootFlowAudioProcessor::GrowLockGroup::air)); break;
            case toggleLockFx:     setGrowLockEnabled(RootFlowAudioProcessor::GrowLockGroup::fx, ! audioProcessor.isGrowLockEnabled(RootFlowAudioProcessor::GrowLockGroup::fx)); break;
            case toggleLockSeq:    setGrowLockEnabled(RootFlowAudioProcessor::GrowLockGroup::sequencer, ! audioProcessor.isGrowLockEnabled(RootFlowAudioProcessor::GrowLockGroup::sequencer)); break;
            case toggleMidiLearn:  setMidiLearnEnabled(! audioProcessor.isMidiLearnArmed()); break;
            case toggleHover:  setHoverEffectsEnabled(! RootFlow::areHoverEffectsEnabled()); break;
            case toggleIdle:   setIdleEffectsEnabled(! RootFlow::areIdleEffectsEnabled()); break;
            case togglePopups: setPopupOverlaysEnabled(! RootFlow::arePopupOverlaysEnabled()); break;
            case toggleTone:   setToneProbeEnabled(! audioProcessor.isTestToneEnabled()); break;
            default: break;
        }
    });
}

void RootFlowAudioProcessorEditor::setMidiLearnEnabled(bool enabled)
{
    audioProcessor.setMidiLearnArmed(enabled);
    midiLearnButton.setToggleState(enabled, juce::dontSendNotification);
    refreshHeaderControlState();
    repaint();
}

void RootFlowAudioProcessorEditor::setHoverEffectsEnabled(bool enabled)
{
    RootFlow::setHoverEffectsEnabled(enabled);
    hoverToggleButton.setToggleState(enabled, juce::dontSendNotification);

    if (! enabled)
        headerFocusedControl = nullptr;

    refreshHeaderControlState();
    repaint();
    if (mainLayout != nullptr)
        mainLayout->repaint();
    keyboardDrawer.repaint();
}

void RootFlowAudioProcessorEditor::setIdleEffectsEnabled(bool enabled)
{
    RootFlow::setIdleEffectsEnabled(enabled);
    idleToggleButton.setToggleState(enabled, juce::dontSendNotification);

    refreshHeaderControlState();
    repaint();
    if (mainLayout != nullptr)
        mainLayout->repaint();
}

void RootFlowAudioProcessorEditor::setPopupOverlaysEnabled(bool enabled)
{
    RootFlow::setPopupOverlaysEnabled(enabled);
    popupToggleButton.setToggleState(enabled, juce::dontSendNotification);

    refreshHeaderControlState();
    repaint();
    if (mainLayout != nullptr)
        mainLayout->repaint();
    keyboardDrawer.repaint();
}

void RootFlowAudioProcessorEditor::setToneProbeEnabled(bool enabled)
{
    audioProcessor.setTestToneEnabled(enabled);
    testToneButton.setToggleState(enabled, juce::dontSendNotification);

    refreshHeaderControlState();
    repaint();
}

void RootFlowAudioProcessorEditor::setGrowLockEnabled(RootFlowAudioProcessor::GrowLockGroup group, bool enabled)
{
    audioProcessor.setGrowLockEnabled(group, enabled);

    switch (group)
    {
        case RootFlowAudioProcessor::GrowLockGroup::root:      growLockRootButton.setToggleState(enabled, juce::dontSendNotification); break;
        case RootFlowAudioProcessor::GrowLockGroup::motion:    growLockMotionButton.setToggleState(enabled, juce::dontSendNotification); break;
        case RootFlowAudioProcessor::GrowLockGroup::air:       growLockAirButton.setToggleState(enabled, juce::dontSendNotification); break;
        case RootFlowAudioProcessor::GrowLockGroup::fx:        growLockFxButton.setToggleState(enabled, juce::dontSendNotification); break;
        case RootFlowAudioProcessor::GrowLockGroup::sequencer: growLockSeqButton.setToggleState(enabled, juce::dontSendNotification); break;
    }

    refreshHeaderControlState();
    repaint();
}

void RootFlowAudioProcessorEditor::applyStylePromptRecipe(int recipeIndex)
{
    if (! juce::isPositiveAndBelow(recipeIndex, (int) stylePromptRecipes.size()))
        return;

    const auto& recipe = stylePromptRecipes[(size_t) recipeIndex];
    promptEditor.setText(juce::String(recipe.seedA).trim(), false);
    morphPromptEditor.setText(juce::String(recipe.seedB).trim(), false);
    promptMorphSlider.setValue(recipe.morphAmount, juce::dontSendNotification);
    applyPromptSeed();
}

void RootFlowAudioProcessorEditor::recallSeedMemoryPrompt(int slotIndex)
{
    if (! juce::isPositiveAndBelow(slotIndex, (int) seedMemoryPrompts.size()))
        return;

    const auto recalledPrompt = seedMemoryPrompts[(size_t) slotIndex].trim();
    if (recalledPrompt.isEmpty())
        return;

    const auto seedA = promptEditor.getText().trim();
    const auto seedB = morphPromptEditor.getText().trim();

    if (seedA.isEmpty())
        promptEditor.setText(recalledPrompt, false);
    else if (seedB.isEmpty() && seedA != recalledPrompt)
        morphPromptEditor.setText(recalledPrompt, false);
    else
        promptEditor.setText(recalledPrompt, false);

    refreshHeaderControlState();
    repaint();
}

void RootFlowAudioProcessorEditor::deleteSeedMemoryPrompt(int slotIndex)
{
    if (! juce::isPositiveAndBelow(slotIndex, (int) seedMemoryPrompts.size()))
        return;

    const auto promptToDelete = seedMemoryPrompts[(size_t) slotIndex].trim();
    if (promptToDelete.isEmpty())
        return;

    if (! audioProcessor.removePromptMemoryEntry(promptToDelete))
        return;

    if (promptEditor.getText().trim() == promptToDelete)
        promptEditor.clear();

    if (morphPromptEditor.getText().trim() == promptToDelete)
        morphPromptEditor.clear();

    refreshHeaderControlState();
    repaint();
}

void RootFlowAudioProcessorEditor::refreshHeaderControlState()
{
    const bool learnArmed = audioProcessor.isMidiLearnArmed();
    const bool testToneEnabled = audioProcessor.isTestToneEnabled();
    const bool presetDirty = audioProcessor.isCurrentPresetDirty();
    const int presetItemCount = audioProcessor.getCombinedPresetCount();
    const int currentPresetIndex = audioProcessor.getCurrentPresetMenuIndex();
    const bool hasUserPreset = audioProcessor.getCurrentUserPresetIndex() >= 0;

    if (! headerControlStateInitialised || presetItemCount != cachedPresetItemCount)
    {
        presetBox.clear(juce::dontSendNotification);
        auto names = audioProcessor.getCombinedPresetNames();
        auto sections = audioProcessor.getFactoryPresetMenuSections();

        int currentIndex = 0;

        for (const auto& section : sections)
        {
            if (currentIndex > 0)
                presetBox.addSeparator();

            presetBox.addSectionHeading(section.title);

            for (int i = 0; i < section.count; ++i)
            {
                if (currentIndex < names.size())
                {
                    presetBox.addItem(names[currentIndex], currentIndex + 1);
                    currentIndex++;
                }
            }
        }

        if (currentIndex < names.size())
        {
            presetBox.addSeparator();
            presetBox.addSectionHeading("USER PRESETS");
            while (currentIndex < names.size())
            {
                presetBox.addItem(names[currentIndex], currentIndex + 1);
                currentIndex++;
            }
        }

        cachedPresetItemCount = presetItemCount;
    }

    if (presetBox.getSelectedId() != currentPresetIndex + 1)
        presetBox.setSelectedId(currentPresetIndex + 1, juce::dontSendNotification);

    const bool hoverEnabled = RootFlow::areHoverEffectsEnabled();
    const bool idleEnabled = RootFlow::areIdleEffectsEnabled();
    const bool popupEnabled = RootFlow::arePopupOverlaysEnabled();
    const bool anyGrowLockEnabled = audioProcessor.isGrowLockEnabled(RootFlowAudioProcessor::GrowLockGroup::root)
                                 || audioProcessor.isGrowLockEnabled(RootFlowAudioProcessor::GrowLockGroup::motion)
                                 || audioProcessor.isGrowLockEnabled(RootFlowAudioProcessor::GrowLockGroup::air)
                                 || audioProcessor.isGrowLockEnabled(RootFlowAudioProcessor::GrowLockGroup::fx)
                                 || audioProcessor.isGrowLockEnabled(RootFlowAudioProcessor::GrowLockGroup::sequencer);

    midiLearnButton.setToggleState(learnArmed, juce::dontSendNotification);
    testToneButton.setToggleState(testToneEnabled, juce::dontSendNotification);
    hoverToggleButton.setToggleState(hoverEnabled, juce::dontSendNotification);
    idleToggleButton.setToggleState(idleEnabled, juce::dontSendNotification);
    popupToggleButton.setToggleState(popupEnabled, juce::dontSendNotification);
    growLockRootButton.setToggleState(audioProcessor.isGrowLockEnabled(RootFlowAudioProcessor::GrowLockGroup::root), juce::dontSendNotification);
    growLockMotionButton.setToggleState(audioProcessor.isGrowLockEnabled(RootFlowAudioProcessor::GrowLockGroup::motion), juce::dontSendNotification);
    growLockAirButton.setToggleState(audioProcessor.isGrowLockEnabled(RootFlowAudioProcessor::GrowLockGroup::air), juce::dontSendNotification);
    growLockFxButton.setToggleState(audioProcessor.isGrowLockEnabled(RootFlowAudioProcessor::GrowLockGroup::fx), juce::dontSendNotification);
    growLockSeqButton.setToggleState(audioProcessor.isGrowLockEnabled(RootFlowAudioProcessor::GrowLockGroup::sequencer), juce::dontSendNotification);
    midiLearnButton.setButtonText(learnArmed ? "ARM" : "MAP");
    utilityMenuButton.setButtonText("TOOLS");
    testToneButton.setButtonText(testToneEnabled ? "ON" : "TONE");
    mutateModeButton.setButtonText(audioProcessor.getMutationModeShortLabel());
    presetSaveButton.setButtonText(presetDirty ? "SAVE*" : "SAVE");
    patchMenuButton.setButtonText("EDIT");
    promptApplyButton.setButtonText(promptEditor.getText().trim().isNotEmpty() && morphPromptEditor.getText().trim().isNotEmpty()
                                        ? "MORPH"
                                        : "GROW");
    morphPromptEditor.setAlpha(morphPromptEditor.hasKeyboardFocus(true) || morphPromptEditor.getText().trim().isNotEmpty() ? 1.0f : 0.72f);
    patchMenuButton.setTooltip((presetDirty ? juce::String("Unsaved patch / ") : juce::String())
                               + (hasUserPreset ? juce::String("User preset / ") : juce::String("Factory preset / "))
                               + "Mutate mode " + audioProcessor.getMutationModeDisplayName()
                               + " / click for save, delete, mutate mode and style recipes");
    mutateButton.setTooltip("Mutate current patch / mode " + audioProcessor.getMutationModeDisplayName());
    utilityMenuButton.setTooltip(makeUtilitySummaryText(hoverEnabled, idleEnabled, popupEnabled, testToneEnabled, audioProcessor)
                                 + " / click to open utility controls");

    const auto previews = audioProcessor.getPromptMemoryPreviews((int) seedMemoryButtons.size());
    bool seedMemoryLayoutChanged = false;
    for (size_t i = 0; i < seedMemoryButtons.size(); ++i)
    {
        const auto previousPrompt = seedMemoryPrompts[i];
        const auto previousSummary = seedMemorySummaries[i];
        const bool hadVisibility = seedMemoryButtons[i].isVisible();

        if (i < previews.size())
        {
            seedMemoryPrompts[i] = previews[i].prompt.trim();
            seedMemorySummaries[i] = previews[i].summary.trim();
        }
        else
        {
            seedMemoryPrompts[i].clear();
            seedMemorySummaries[i].clear();
        }

        const bool hasPrompt = seedMemoryPrompts[i].isNotEmpty();
        seedMemoryButtons[i].setVisible(hasPrompt);
        seedMemoryButtons[i].setEnabled(hasPrompt);
        seedMemoryButtons[i].setAlpha(hasPrompt ? 1.0f : 0.0f);
        seedMemoryButtons[i].setButtonText(hasPrompt ? makeSeedMemoryButtonLabel(seedMemoryPrompts[i])
                                                     : juce::String("MEM"));

        if (hasPrompt)
        {
            juce::String tooltip = seedMemoryPrompts[i];
            if (seedMemorySummaries[i].isNotEmpty())
                tooltip << " / " << seedMemorySummaries[i];
            tooltip << " / left click fills Seed A, then Seed B / right click deletes";
            seedMemoryButtons[i].setTooltip(tooltip);
        }
        else
        {
            seedMemoryButtons[i].setTooltip("No remembered seed yet");
        }

        if (previousPrompt != seedMemoryPrompts[i]
            || previousSummary != seedMemorySummaries[i]
            || hadVisibility != hasPrompt)
        {
            seedMemoryLayoutChanged = true;
        }
    }

    presetDeleteButton.setEnabled(hasUserPreset);
    presetDeleteButton.setAlpha(hasUserPreset ? 1.0f : 0.45f);

    headerControlStateInitialised = true;

    if (seedMemoryLayoutChanged)
        resized();
}

void RootFlowAudioProcessorEditor::visibilityChanged()
{
    updateAnimationTimerState();
}

void RootFlowAudioProcessorEditor::parentHierarchyChanged()
{
    updateAnimationTimerState();
}

void RootFlowAudioProcessorEditor::updateAnimationTimerState()
{
    if (isShowing())
    {
        if (! isTimerRunning())
            startTimerHz(30);
    }
    else
    {
        stopTimer();
    }
}

void RootFlowAudioProcessorEditor::registerHeaderControl(juce::Component& component)
{
    component.addMouseListener(this, false);
}

void RootFlowAudioProcessorEditor::clearHeaderFocusIfNeeded(juce::Component& component)
{
    if (headerFocusedControl != &component)
        return;

    if (! RootFlow::areHoverEffectsEnabled() || (! component.isMouseOver(true) && ! component.isMouseButtonDown()))
    {
        headerFocusedControl = nullptr;
        repaint();
    }
}

void RootFlowAudioProcessorEditor::mouseEnter(const juce::MouseEvent& e)
{
    if (! RootFlow::areHoverEffectsEnabled())
        return;

    if (headerFocusedControl != e.eventComponent)
    {
        headerFocusedControl = e.eventComponent;
        repaint();
    }
}

void RootFlowAudioProcessorEditor::mouseExit(const juce::MouseEvent& e)
{
    if (auto* component = e.eventComponent)
        clearHeaderFocusIfNeeded(*component);
}

void RootFlowAudioProcessorEditor::mouseMove(const juce::MouseEvent& e)
{
    if (! RootFlow::areHoverEffectsEnabled())
        return;

    if (headerFocusedControl != e.eventComponent)
    {
        headerFocusedControl = e.eventComponent;
        repaint();
    }
}

void RootFlowAudioProcessorEditor::mouseDown(const juce::MouseEvent& e)
{
    if (headerFocusedControl != e.eventComponent)
    {
        headerFocusedControl = e.eventComponent;
        repaint();
    }
}

void RootFlowAudioProcessorEditor::mouseUp(const juce::MouseEvent& e)
{
    for (size_t i = 0; i < seedMemoryButtons.size(); ++i)
    {
        if (e.eventComponent == &seedMemoryButtons[i])
        {
            if (e.mods.isPopupMenu())
                deleteSeedMemoryPrompt((int) i);
            else
                recallSeedMemoryPrompt((int) i);

            return;
        }
    }

    if (! RootFlow::areHoverEffectsEnabled())
    {
        if (headerFocusedControl != nullptr)
        {
            headerFocusedControl = nullptr;
            repaint();
        }
        return;
    }

    if (headerFocusedControl != e.eventComponent)
    {
        headerFocusedControl = e.eventComponent;
        repaint();
    }
}

void RootFlowAudioProcessorEditor::paintOverChildren(juce::Graphics&) {}
