#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
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
    keyboardDrawer.setColour(juce::MidiKeyboardComponent::keyDownOverlayColourId, RootFlow::accent.withAlpha(0.48f));
    keyboardDrawer.setColour(juce::MidiKeyboardComponent::keySeparatorLineColourId, RootFlow::accent.withAlpha(0.08f));
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
    RootFlow::drawGlassPanel(g, area, area.getHeight() * 0.5f, emphasise ? 0.72f : 0.56f);
    g.setColour(tint.withAlpha(emphasise ? 0.055f : 0.030f));
    g.fillRoundedRectangle(area.reduced(2.0f), area.getHeight() * 0.44f);

    auto inner = area.reduced(12.0f, 5.0f);
    auto labelArea = inner.removeFromTop(area.getHeight() * 0.34f);

    g.setColour(RootFlow::textMuted.withAlpha(emphasise ? 0.76f : 0.68f));
    g.setFont(RootFlow::getFont(9.1f));
    g.drawText(label.toUpperCase(), labelArea, juce::Justification::centredLeft, false);

    g.setColour((emphasise ? tint.brighter(0.12f) : RootFlow::text).withAlpha(emphasise ? 0.94f : 0.88f));
    g.setFont(RootFlow::getFont(12.2f).boldened());
    g.drawFittedText(value, inner.toNearestInt(), juce::Justification::centredLeft, 1);

    RootFlow::drawGlowOrb(g, { area.getRight() - 14.0f, area.getCentreY() }, 2.7f, tint, emphasise ? 0.44f : 0.26f);
}

void drawHeaderFocusBubble(juce::Graphics& g,
                           juce::Rectangle<float> area,
                           const juce::String& section,
                           const juce::String& value,
                           juce::Colour tint,
                           bool emphasise = false)
{
    RootFlow::drawGlassPanel(g, area, area.getHeight() * 0.5f, emphasise ? 0.78f : 0.66f);

    g.setColour(tint.withAlpha(emphasise ? 0.10f : 0.06f));
    g.fillRoundedRectangle(area.reduced(2.0f), area.getHeight() * 0.44f);

    auto inner = area.reduced(10.0f, 4.0f);
    auto sectionArea = inner.removeFromTop(8.0f);

    g.setColour(RootFlow::textMuted.withAlpha(0.86f));
    g.setFont(RootFlow::getFont(7.8f).boldened());
    g.drawText(section.toUpperCase(), sectionArea, juce::Justification::centredLeft, false);

    g.setColour((emphasise ? tint.brighter(0.20f) : RootFlow::text).withAlpha(0.96f));
    g.setFont(RootFlow::getFont(10.2f).boldened());
    g.drawFittedText(value.toUpperCase(), inner.toNearestInt(), juce::Justification::centredLeft, 1);

    RootFlow::drawGlowOrb(g, { area.getRight() - 10.0f, area.getCentreY() }, 2.5f, tint, emphasise ? 0.54f : 0.34f);
}

void drawToolbarCaption(juce::Graphics& g,
                        juce::Rectangle<float> area,
                        const juce::String& text,
                        juce::Colour tint,
                        juce::Justification justification = juce::Justification::centred)
{
    g.setFont(RootFlow::getFont(9.6f).boldened());
    g.setColour(tint.withAlpha(0.92f));
    g.drawFittedText(text, area.toNearestInt(), justification, 1);

    auto lineY = area.getBottom() - 1.5f;
    const float lineInset = 6.0f;
    g.setColour(tint.withAlpha(0.09f));
    g.drawLine(area.getX() + lineInset, lineY, area.getRight() - lineInset, lineY, 1.0f);
}

void drawClusterDivider(juce::Graphics& g, float x, juce::Rectangle<float> cluster, juce::Colour tint)
{
    const float top = cluster.getY() + 7.0f;
    const float bottom = cluster.getBottom() - 7.0f;
    g.setColour(tint.withAlpha(0.08f));
    g.drawLine(x, top, x, bottom, 1.0f);
    g.setColour(juce::Colours::white.withAlpha(0.03f));
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
}

RootFlowAudioProcessorEditor::RootFlowAudioProcessorEditor(RootFlowAudioProcessor& p)
    : juce::AudioProcessorEditor(p),
      audioProcessor(p),
      keyboardDrawer(p.getKeyboardState(), juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel(&look);
    setOpaque(true);
    setResizable(true, true);
    setResizeLimits(960, 690, 1800, 1300);

    styleKeyboardDrawer(keyboardDrawer);

    // Anchoring Glow (Soft Upward Radiance)
    auto* kbdGlow = new juce::GlowEffect();
    kbdGlow->setGlowProperties(6.5f, RootFlow::accent.withAlpha(0.28f));
    keyboardDrawer.setComponentEffect(kbdGlow);

    // Main Layout components are added and initialized
    mainLayout = std::make_unique<MainLayoutComponent>(p);
    addAndMakeVisible(*mainLayout);

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
    cc.getEnergyDisplay().setProcessor(&p);

    auto& bp = mainLayout->getBottomPanel();
    bloomAtt    = std::make_unique<Attachment>(p.tree, "bloom", bp.getBloomSlider());
    rainAtt     = std::make_unique<Attachment>(p.tree, "rain",  bp.getRainSlider());
    sunAtt      = std::make_unique<Attachment>(p.tree, "sun",   bp.getSunSlider());

    // --- GLOBAL HEADER UI ---
    addAndMakeVisible(presetBox);
    addAndMakeVisible(presetSaveButton);
    addAndMakeVisible(presetDeleteButton);
    addAndMakeVisible(midiLearnButton);
    addAndMakeVisible(testToneButton);
    addAndMakeVisible(hoverToggleButton);
    addAndMakeVisible(idleToggleButton);
    addAndMakeVisible(popupToggleButton);
    addAndMakeVisible(mutateButton);
    addAndMakeVisible(waveSelector);
    addAndMakeVisible(keyboardDrawer);

    presetBox.setText("FACTORY SEED");
    presetSaveButton.setButtonText("SAVE");
    presetDeleteButton.setButtonText("DEL");
    midiLearnButton.setButtonText("MAP");
    testToneButton.setButtonText("TONE");
    hoverToggleButton.setButtonText("HOVER");
    idleToggleButton.setButtonText("IDLE");
    popupToggleButton.setButtonText("POPUP");
    mutateButton.setButtonText("MUTATE");
    presetBox.setTextWhenNothingSelected("PRESET");
    midiLearnButton.setClickingTogglesState(true);
    testToneButton.setClickingTogglesState(true);
    hoverToggleButton.setClickingTogglesState(true);
    idleToggleButton.setClickingTogglesState(true);
    popupToggleButton.setClickingTogglesState(true);
    hoverToggleButton.setToggleState(RootFlow::areHoverEffectsEnabled(), juce::dontSendNotification);
    idleToggleButton.setToggleState(RootFlow::areIdleEffectsEnabled(), juce::dontSendNotification);
    popupToggleButton.setToggleState(RootFlow::arePopupOverlaysEnabled(), juce::dontSendNotification);

    waveSelector.addItemList({"SINE", "SAW", "PULSE"}, 1);
    waveAttachment = std::make_unique<ComboAttachment>(p.tree, "oscWave", waveSelector);

    presetBox.onChange = [this]
    {
        const int presetIndex = presetBox.getSelectedId() - 1;
        if (presetIndex >= 0)
            audioProcessor.applyCombinedPreset(presetIndex);
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
        audioProcessor.setMidiLearnArmed(midiLearnButton.getToggleState());
        refreshHeaderControlState();
    };

    testToneButton.onClick = [this]
    {
        audioProcessor.setTestToneEnabled(testToneButton.getToggleState());
        refreshHeaderControlState();
    };

    hoverToggleButton.onClick = [this]
    {
        RootFlow::setHoverEffectsEnabled(hoverToggleButton.getToggleState());
        if (! hoverToggleButton.getToggleState())
            headerFocusedControl = nullptr;

        repaint();
        if (mainLayout != nullptr)
            mainLayout->repaint();
        keyboardDrawer.repaint();
    };

    idleToggleButton.onClick = [this]
    {
        RootFlow::setIdleEffectsEnabled(idleToggleButton.getToggleState());
        repaint();
        if (mainLayout != nullptr)
            mainLayout->repaint();
    };

    popupToggleButton.onClick = [this]
    {
        RootFlow::setPopupOverlaysEnabled(popupToggleButton.getToggleState());
        repaint();
        if (mainLayout != nullptr)
            mainLayout->repaint();
        keyboardDrawer.repaint();
    };

    mutateButton.onClick = [this] {
        audioProcessor.mutatePlant();
        mainLayout->getCenterComponent().getEnergyDisplay().triggerSporeBurst();
    };

    for (auto* headerControl : { static_cast<juce::Component*>(&waveSelector),
                                 static_cast<juce::Component*>(&presetBox),
                                 static_cast<juce::Component*>(&presetSaveButton),
                                 static_cast<juce::Component*>(&presetDeleteButton),
                                 static_cast<juce::Component*>(&mutateButton),
                                 static_cast<juce::Component*>(&hoverToggleButton),
                                 static_cast<juce::Component*>(&idleToggleButton),
                                 static_cast<juce::Component*>(&popupToggleButton),
                                 static_cast<juce::Component*>(&midiLearnButton),
                                 static_cast<juce::Component*>(&testToneButton) })
    {
        if (headerControl != nullptr)
            registerHeaderControl(*headerControl);
    }

    refreshHeaderControlState();
    updateAnimationTimerState();
    setSize(1240, 820);
}

RootFlowAudioProcessorEditor::~RootFlowAudioProcessorEditor()
{
    for (auto* headerControl : { static_cast<juce::Component*>(&waveSelector),
                                 static_cast<juce::Component*>(&presetBox),
                                 static_cast<juce::Component*>(&presetSaveButton),
                                 static_cast<juce::Component*>(&presetDeleteButton),
                                 static_cast<juce::Component*>(&mutateButton),
                                 static_cast<juce::Component*>(&hoverToggleButton),
                                 static_cast<juce::Component*>(&idleToggleButton),
                                 static_cast<juce::Component*>(&popupToggleButton),
                                 static_cast<juce::Component*>(&midiLearnButton),
                                 static_cast<juce::Component*>(&testToneButton) })
    {
        if (headerControl != nullptr)
            headerControl->removeMouseListener(this);
    }

    setLookAndFeel(nullptr);
}

void RootFlowAudioProcessorEditor::paint(juce::Graphics& g)
{
    auto full = getLocalBounds().toFloat();
    RootFlow::fillBackdrop(g, full);

    const auto headerHeight = getHeaderHeightForSize(getHeight());
    auto headerArea = full.removeFromTop((float) headerHeight);
    juce::ColourGradient headerGrad(juce::Colour(0xff09140f).withAlpha(0.92f), headerArea.getTopLeft(),
                                    juce::Colour(0xff07110d).withAlpha(0.82f), headerArea.getBottomRight(), false);
    g.setGradientFill(headerGrad);
    g.fillRect(headerArea);

    g.setColour(juce::Colours::white.withAlpha(0.05f));
    g.drawLine(18.0f, 9.0f, (float) getWidth() - 18.0f, 9.0f, 1.0f);
    g.setColour(RootFlow::accent.withAlpha(0.08f));
    g.drawLine(0.0f, (float) headerHeight, (float) getWidth(), (float) headerHeight, 1.0f);

    const bool hoverEffectsEnabled = RootFlow::areHoverEffectsEnabled();
    const bool idleEffectsEnabled = RootFlow::areIdleEffectsEnabled();
    const bool popupOverlaysEnabled = RootFlow::arePopupOverlaysEnabled();
    const bool presetDirty = audioProcessor.isCurrentPresetDirty();
    const bool midiArmed = audioProcessor.isMidiLearnArmed();
    const bool testToneEnabled = audioProcessor.isTestToneEnabled();
    const bool hasUserPreset = audioProcessor.getCurrentUserPresetIndex() >= 0;
    auto* focusControl = headerFocusedControl;
    if (! hoverEffectsEnabled && focusControl != nullptr && ! focusControl->isMouseButtonDown())
        focusControl = nullptr;
    const bool focusWave = focusControl == &waveSelector;
    const bool focusPreset = focusControl == &presetBox;
    const bool focusSave = focusControl == &presetSaveButton;
    const bool focusDelete = focusControl == &presetDeleteButton;
    const bool focusMutate = focusControl == &mutateButton;
    const bool focusHoverToggle = focusControl == &hoverToggleButton;
    const bool focusIdleToggle = focusControl == &idleToggleButton;
    const bool focusPopupToggle = focusControl == &popupToggleButton;
    const bool focusMidi = focusControl == &midiLearnButton;
    const bool focusTone = focusControl == &testToneButton;
    const bool focusLeftCluster = focusWave || focusPreset || focusSave || focusDelete || focusMutate;
    const bool focusRightCluster = focusHoverToggle || focusIdleToggle || focusPopupToggle || focusMidi || focusTone;

    juce::String focusSection;
    juce::String focusValue;
    juce::Colour focusTint = RootFlow::textMuted;

    if (focusWave)
    {
        focusSection = "Osc Field";
        focusValue = waveSelector.getText().isNotEmpty() ? waveSelector.getText() : juce::String("Sine");
        focusTint = RootFlow::accentSoft;
    }
    else if (focusPreset)
    {
        focusSection = "Patch Library";
        focusValue = presetBox.getText().isNotEmpty() ? presetBox.getText()
                                                      : juce::String("Factory Seed");
        focusTint = RootFlow::accent;
    }
    else if (focusSave)
    {
        focusSection = "Store Patch";
        focusValue = presetDirty ? juce::String("Write User Snapshot")
                                 : juce::String("Patch Synced");
        focusTint = RootFlow::violet;
    }
    else if (focusDelete)
    {
        focusSection = "Delete Patch";
        focusValue = hasUserPreset ? juce::String("Remove User Variant")
                                   : juce::String("Factory Patch Locked");
        focusTint = RootFlow::violet;
    }
    else if (focusMutate)
    {
        focusSection = "Evolve DNA";
        focusValue = juce::String("Mutate Current Patch");
        focusTint = RootFlow::amber;
    }
    else if (focusHoverToggle)
    {
        focusSection = "Hover Field";
        focusValue = hoverEffectsEnabled ? juce::String("Visual Hover Enabled")
                                         : juce::String("Hover Visuals Disabled");
        focusTint = hoverEffectsEnabled ? RootFlow::accentSoft : RootFlow::textMuted;
    }
    else if (focusIdleToggle)
    {
        focusSection = "Idle Field";
        focusValue = idleEffectsEnabled ? juce::String("Organic Idle Enabled")
                                        : juce::String("Idle Visuals Paused");
        focusTint = idleEffectsEnabled ? RootFlow::accent : RootFlow::textMuted;
    }
    else if (focusPopupToggle)
    {
        focusSection = "Popup Field";
        focusValue = popupOverlaysEnabled ? juce::String("Inspector Popups Enabled")
                                          : juce::String("Inspector Popups Hidden");
        focusTint = popupOverlaysEnabled ? RootFlow::amber : RootFlow::textMuted;
    }
    else if (focusMidi)
    {
        focusSection = "Midi Learn";
        auto targetParameterID = audioProcessor.getMidiLearnTargetParameterID();
        focusValue = midiArmed
            ? (targetParameterID.isNotEmpty() ? audioProcessor.getParameterDisplayName(targetParameterID)
                                              : juce::String("Awaiting Target"))
            : juce::String("Arm Controller Map");
        focusTint = getStatusMapColour();
    }
    else if (focusTone)
    {
        focusSection = "Tone Probe";
        focusValue = testToneEnabled ? juce::String("Osc Probe Enabled")
                                     : juce::String("Trigger Test Osc");
        focusTint = RootFlow::accentSoft;
    }

    auto leftCluster = waveSelector.getBounds().getUnion(presetBox.getBounds())
                                      .getUnion(presetSaveButton.getBounds())
                                      .getUnion(presetDeleteButton.getBounds())
                                      .getUnion(mutateButton.getBounds())
                                      .toFloat()
                                      .expanded(12.0f, 8.0f);
    auto rightCluster = hoverToggleButton.getBounds()
                                       .getUnion(idleToggleButton.getBounds())
                                       .getUnion(popupToggleButton.getBounds())
                                       .getUnion(midiLearnButton.getBounds())
                                       .getUnion(testToneButton.getBounds())
                                       .toFloat()
                                       .expanded(12.0f, 8.0f);

    if (! leftCluster.isEmpty())
    {
        RootFlow::drawGlassPanel(g, leftCluster, 16.0f, focusLeftCluster ? 0.76f : 0.62f);
        g.setColour((focusLeftCluster ? focusTint : juce::Colours::white).withAlpha(focusLeftCluster ? 0.03f : 0.018f));
        g.fillRoundedRectangle(leftCluster.withHeight(leftCluster.getHeight() * 0.46f), 16.0f);
    }

    if (! rightCluster.isEmpty())
    {
        RootFlow::drawGlassPanel(g, rightCluster, 16.0f, focusRightCluster ? 0.74f : 0.60f);
        g.setColour((focusRightCluster ? focusTint : juce::Colours::white).withAlpha(focusRightCluster ? 0.03f : 0.018f));
        g.fillRoundedRectangle(rightCluster.withHeight(rightCluster.getHeight() * 0.46f), 16.0f);
    }

    auto waveCaptionArea = waveSelector.getBounds().toFloat().translated(0.0f, -15.0f).withHeight(11.0f);
    auto presetCaptionArea = presetBox.getBounds().toFloat().translated(0.0f, -15.0f).withHeight(11.0f);
    auto actionsCaptionArea = presetSaveButton.getBounds()
                               .getUnion(presetDeleteButton.getBounds())
                               .getUnion(mutateButton.getBounds())
                               .toFloat()
                               .translated(0.0f, -15.0f)
                               .withHeight(11.0f);
    auto visualsCaptionArea = hoverToggleButton.getBounds()
                               .getUnion(idleToggleButton.getBounds())
                               .getUnion(popupToggleButton.getBounds())
                               .toFloat()
                               .translated(0.0f, -15.0f)
                               .withHeight(11.0f);
    auto midiCaptionArea = midiLearnButton.getBounds().toFloat().translated(0.0f, -15.0f).withHeight(11.0f);
    auto toneCaptionArea = testToneButton.getBounds().toFloat().translated(0.0f, -15.0f).withHeight(11.0f);
    const auto actionTint = focusMutate ? RootFlow::amber
                          : (focusSave || focusDelete || presetDirty) ? RootFlow::violet
                                                                      : RootFlow::violet.interpolatedWith(RootFlow::amber, 0.45f);

    drawToolbarCaption(g, waveCaptionArea, "OSC", RootFlow::accentSoft);
    drawToolbarCaption(g, presetCaptionArea, "PATCH", RootFlow::accent);
    drawToolbarCaption(g, actionsCaptionArea, "ACTIONS", actionTint);
    const auto visualsTint = popupOverlaysEnabled ? RootFlow::amber
                           : hoverEffectsEnabled ? RootFlow::accentSoft
                                                 : idleEffectsEnabled ? RootFlow::accent
                                                                      : RootFlow::textMuted;
    drawToolbarCaption(g, visualsCaptionArea, "VISUALS", visualsTint);
    drawToolbarCaption(g, midiCaptionArea, "MIDI", getStatusMapColour());
    drawToolbarCaption(g, toneCaptionArea, "TONE", RootFlow::accentSoft);

    auto wavePreviewArea = waveSelector.getBounds().toFloat().reduced(14.0f, 7.0f);
    wavePreviewArea = wavePreviewArea.withTrimmedTop(wavePreviewArea.getHeight() * 0.56f).withTrimmedRight(22.0f);
    if (wavePreviewArea.getWidth() > 26.0f)
    {
        const float previewPhase = std::fmod((float) juce::Time::getMillisecondCounterHiRes() * 0.00018f, 1.0f);
        auto wavePreview = makeWavePreviewPath(wavePreviewArea, waveSelector.getSelectedId(), previewPhase);

        g.setColour(RootFlow::accentSoft.withAlpha(focusWave ? 0.10f : 0.04f));
        g.strokePath(wavePreview, juce::PathStrokeType(focusWave ? 2.4f : 1.6f,
                                                       juce::PathStrokeType::curved,
                                                       juce::PathStrokeType::rounded));
        g.setColour(RootFlow::accentSoft.withAlpha(focusWave ? 0.32f : 0.16f));
        g.strokePath(wavePreview, juce::PathStrokeType(focusWave ? 1.1f : 0.8f,
                                                       juce::PathStrokeType::curved,
                                                       juce::PathStrokeType::rounded));
    }

    if (! leftCluster.isEmpty())
    {
        auto actionsArea = presetSaveButton.getBounds()
                         .getUnion(presetDeleteButton.getBounds())
                         .getUnion(mutateButton.getBounds())
                         .toFloat()
                         .expanded(4.0f, 3.0f);
        g.setColour(actionTint.withAlpha((focusSave || focusDelete || focusMutate) ? 0.07f : 0.035f));
        g.fillRoundedRectangle(actionsArea, 10.0f);
        g.setColour(juce::Colours::white.withAlpha(0.08f));
        g.drawRoundedRectangle(actionsArea, 10.0f, 0.8f);

        const float divider1 = ((float) waveSelector.getRight() + (float) presetBox.getX()) * 0.5f;
        const float divider2 = ((float) presetBox.getRight() + (float) presetSaveButton.getX()) * 0.5f;
        const float divider3 = ((float) presetDeleteButton.getRight() + (float) mutateButton.getX()) * 0.5f;
        drawClusterDivider(g, divider1, leftCluster, RootFlow::accentSoft);
        drawClusterDivider(g, divider2, leftCluster, RootFlow::accent);
        drawClusterDivider(g, divider3, leftCluster, RootFlow::amber);
    }

    if (! rightCluster.isEmpty())
    {
        auto visualsArea = hoverToggleButton.getBounds()
                         .getUnion(idleToggleButton.getBounds())
                         .getUnion(popupToggleButton.getBounds())
                         .toFloat()
                         .expanded(4.0f, 3.0f);
        g.setColour(visualsTint.withAlpha((focusHoverToggle || focusIdleToggle || focusPopupToggle) ? 0.07f : 0.035f));
        g.fillRoundedRectangle(visualsArea, 10.0f);
        g.setColour(juce::Colours::white.withAlpha(0.08f));
        g.drawRoundedRectangle(visualsArea, 10.0f, 0.8f);

        const float divider1 = ((float) hoverToggleButton.getRight() + (float) idleToggleButton.getX()) * 0.5f;
        const float divider2 = ((float) idleToggleButton.getRight() + (float) popupToggleButton.getX()) * 0.5f;
        const float divider3 = ((float) popupToggleButton.getRight() + (float) midiLearnButton.getX()) * 0.5f;
        const float divider4 = ((float) midiLearnButton.getRight() + (float) testToneButton.getX()) * 0.5f;
        drawClusterDivider(g, divider1, rightCluster, hoverEffectsEnabled ? RootFlow::accentSoft : RootFlow::textMuted);
        drawClusterDivider(g, divider2, rightCluster, idleEffectsEnabled ? RootFlow::accent : RootFlow::textMuted);
        drawClusterDivider(g, divider3, rightCluster, popupOverlaysEnabled ? RootFlow::amber : RootFlow::textMuted);
        drawClusterDivider(g, divider4, rightCluster, getStatusMapColour());
    }

    if (popupOverlaysEnabled && focusControl != nullptr)
    {
        auto focusArea = focusControl->getBounds().toFloat().expanded(4.0f, 4.0f);
        g.setColour(focusTint.withAlpha(0.065f));
        g.fillRoundedRectangle(focusArea, 11.0f);
        g.setColour(juce::Colours::white.withAlpha(0.10f));
        g.drawRoundedRectangle(focusArea, 11.0f, 0.8f);
        g.setColour(focusTint.withAlpha(0.20f));
        g.drawRoundedRectangle(focusArea.reduced(1.0f), 10.0f, 0.9f);

        auto focusBubble = juce::Rectangle<float>(juce::jlimit(126.0f, 196.0f, 116.0f + (float) focusValue.length() * 4.2f), 24.0f)
                               .withCentre({ focusArea.getCentreX(), 21.0f });
        focusBubble.setX(juce::jlimit(18.0f, (float) getWidth() - focusBubble.getWidth() - 18.0f, focusBubble.getX()));
        drawHeaderFocusBubble(g, focusBubble, focusSection, focusValue, focusTint, focusControl->isMouseButtonDown());
    }

    auto brandArea = juce::Rectangle<float>(leftCluster.getRight() + 18.0f,
                                            7.0f,
                                            rightCluster.getX() - leftCluster.getRight() - 36.0f,
                                            14.0f);
    if (brandArea.getWidth() > 220.0f)
    {
        drawToolbarCaption(g,
                           brandArea,
                           focusControl != nullptr ? (focusSection + " / " + focusValue).toUpperCase()
                                                   : juce::String("FUNKY MOOSE / ROOT FLOW"),
                           focusControl != nullptr ? focusTint : RootFlow::textMuted);
    }

    auto centerBand = juce::Rectangle<float>(leftCluster.getRight() + 18.0f,
                                             32.0f,
                                             rightCluster.getX() - leftCluster.getRight() - 36.0f,
                                             36.0f);

    if (centerBand.getWidth() > 260.0f)
    {
        const auto presetName = presetBox.getText().isNotEmpty() ? presetBox.getText().toUpperCase()
                                                                 : juce::String("FACTORY SEED");
        const auto presetState = presetDirty ? juce::String("UNSAVED")
                                             : (audioProcessor.getCurrentUserPresetIndex() >= 0 ? juce::String("USER")
                                                                                                 : juce::String("FACTORY"));

        juce::String monitorState = "LIVE";
        if (midiArmed && testToneEnabled)
            monitorState = "ARM + OSC";
        else if (midiArmed)
            monitorState = "MIDI ARM";
        else if (testToneEnabled)
            monitorState = "TEST OSC";

        const float chipGap = 8.0f;
        const float stateWidth = 106.0f;
        const float monitorWidth = 124.0f;
        const float patchWidth = centerBand.getWidth() - stateWidth - monitorWidth - chipGap * 2.0f;

        if (patchWidth > 120.0f)
        {
            auto patchChip = centerBand.removeFromLeft(patchWidth);
            centerBand.removeFromLeft(chipGap);
            auto stateChip = centerBand.removeFromLeft(stateWidth);
            centerBand.removeFromLeft(chipGap);
            auto monitorChip = centerBand.removeFromLeft(monitorWidth);

            drawHeaderChip(g, patchChip, "Patch", presetName, RootFlow::accentSoft, presetDirty || focusWave || focusPreset);
            drawHeaderChip(g, stateChip, "State", presetState, presetDirty ? getStatusMapColour() : RootFlow::accent,
                           presetDirty || focusSave || focusDelete || focusMutate);
            drawHeaderChip(g, monitorChip, "Monitor", monitorState, midiArmed ? getStatusMapColour() : getStatusReadyColour(),
                           midiArmed || testToneEnabled || focusMidi || focusTone);
        }
    }

    auto keyboardArea = juce::Rectangle<float>(20.0f, (float) getHeight() - 124.0f, (float) getWidth() - 40.0f, 108.0f);
    auto keyboardStateChip = juce::Rectangle<float>(keyboardArea.getX() + 18.0f, keyboardArea.getY() - 20.0f, 118.0f, 18.0f);
    auto keyboardBadge = juce::Rectangle<float>(keyboardArea.getCentreX() - 132.0f, keyboardArea.getY() - 20.0f, 264.0f, 18.0f);
    auto keyboardMidiChip = juce::Rectangle<float>(keyboardArea.getRight() - 172.0f, keyboardArea.getY() - 20.0f, 154.0f, 18.0f);
    auto cradleArea = keyboardArea.expanded(14.0f, 18.0f).translated(0.0f, 10.0f);
    auto cradle = makeKeyboardCradle(cradleArea);
    auto midiSnapshot = audioProcessor.getMidiActivitySnapshot();
    const int heldNotes = countHeldNotes(audioProcessor.getKeyboardState());
    const bool keyboardHovered = hoverEffectsEnabled && keyboardDrawer.isMouseOverOrDragging();
    const bool keyboardActive = heldNotes > 0;
    const bool keyboardFocused = keyboardHovered || keyboardActive;
    const auto midiTint = getMidiActivityTint(midiSnapshot);
    const auto activityText = midiSnapshot.type != RootFlowAudioProcessor::MidiActivitySnapshot::Type::none
        ? getMidiActivityValue(midiSnapshot)
        : juce::String("Awaiting Input");

    if (popupOverlaysEnabled)
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

    RootFlow::drawGlassPanel(g, keyboardBadge, keyboardBadge.getHeight() * 0.5f, keyboardFocused ? 0.56f : 0.36f);
    g.setColour((keyboardFocused ? RootFlow::accentSoft : RootFlow::accent).withAlpha(keyboardFocused ? 0.06f : 0.035f));
    g.fillRoundedRectangle(keyboardBadge.reduced(2.0f), keyboardBadge.getHeight() * 0.42f);
    g.setFont(RootFlow::getFont(9.2f).boldened());
    g.setColour((keyboardFocused ? RootFlow::text : RootFlow::textMuted).withAlpha(0.90f));
    g.drawFittedText(keyboardActive ? juce::String("PERFORMANCE KEYS / ") + juce::String(heldNotes) + " ACTIVE"
                                    : (keyboardHovered ? juce::String("PERFORMANCE KEYS / TOUCH BED")
                                                       : juce::String("PERFORMANCE KEYS / ROOT BED")),
                     keyboardBadge.toNearestInt().reduced(10, 0), juce::Justification::centred, 1);

    if (popupOverlaysEnabled)
    {
        drawHeaderFocusBubble(g,
                              keyboardMidiChip,
                              getMidiActivitySection(midiSnapshot),
                              activityText,
                              midiTint,
                              keyboardActive || midiSnapshot.wasMapped);
    }

    juce::ColourGradient cradleGrad(RootFlow::panelSoft.brighter(0.18f), cradleArea.getCentreX(), cradleArea.getY(),
                                    RootFlow::panel.darker(0.10f), cradleArea.getCentreX(), cradleArea.getBottom(), false);
    g.setGradientFill(cradleGrad);
    g.fillPath(cradle);

    g.setColour((keyboardFocused ? RootFlow::accentSoft : RootFlow::accent).withAlpha(keyboardFocused ? 0.09f : 0.06f));
    g.strokePath(cradle, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour(juce::Colours::white.withAlpha(0.07f));
    g.strokePath(cradle, juce::PathStrokeType(0.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path stem;
    stem.startNewSubPath(keyboardArea.getCentreX(), keyboardArea.getY() - 150.0f);
    stem.cubicTo(keyboardArea.getCentreX() - 10.0f, keyboardArea.getY() - 92.0f,
                 keyboardArea.getCentreX() + 8.0f, keyboardArea.getY() - 34.0f,
                 keyboardArea.getCentreX(), keyboardArea.getY() + 10.0f);
    g.setColour((keyboardFocused ? RootFlow::accentSoft : RootFlow::accent).withAlpha(keyboardFocused ? 0.08f : 0.055f));
    g.strokePath(stem, juce::PathStrokeType(15.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour((keyboardFocused ? RootFlow::accentSoft : RootFlow::accent).withAlpha(keyboardFocused ? 0.26f : 0.20f));
    g.strokePath(stem, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    RootFlow::drawGlassPanel(g, keyboardArea, 24.0f, keyboardFocused ? 1.02f : 0.92f);
    juce::ColourGradient centerGlow((keyboardFocused ? RootFlow::accentSoft : RootFlow::accent).withAlpha(keyboardFocused ? 0.15f : 0.10f),
                                    keyboardArea.getCentreX(), keyboardArea.getY(),
                                    juce::Colours::transparentBlack, keyboardArea.getCentreX(), keyboardArea.getBottom(), true);
    g.setGradientFill(centerGlow);
    g.fillEllipse(keyboardArea.getCentreX() - 42.0f, keyboardArea.getY() - 10.0f, 84.0f, keyboardArea.getHeight() + 20.0f);

    if (keyboardFocused)
    {
        g.setColour((keyboardActive ? RootFlow::accentSoft : midiTint).withAlpha(0.10f));
        g.drawRoundedRectangle(keyboardArea.expanded(4.0f, 3.0f), 27.0f, 0.9f);
    }

    RootFlow::drawGlowOrb(g, { keyboardArea.getX() + 34.0f, keyboardArea.getY() + 18.0f }, 5.0f, RootFlow::violet, 0.24f);
    RootFlow::drawGlowOrb(g, { keyboardArea.getRight() - 34.0f, keyboardArea.getY() + 18.0f }, 5.0f, RootFlow::accentSoft, 0.24f);
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
    controlRow.removeFromLeft(clusterGap);
    presetBox.setBounds(controlRow.removeFromLeft(compactHeaderLayout ? 190 : 228));
    controlRow.removeFromLeft(clusterGap);
    presetSaveButton.setBounds(controlRow.removeFromLeft(compactHeaderLayout ? 50 : 54));
    controlRow.removeFromLeft(microGap);
    presetDeleteButton.setBounds(controlRow.removeFromLeft(compactHeaderLayout ? 40 : 42));
    controlRow.removeFromLeft(clusterGap);
    mutateButton.setBounds(controlRow.removeFromLeft(compactHeaderLayout ? 72 : 80));

    testToneButton.setBounds(controlRow.removeFromRight(compactHeaderLayout ? 64 : 76));
    controlRow.removeFromRight(microGap);
    midiLearnButton.setBounds(controlRow.removeFromRight(compactHeaderLayout ? 62 : 70));
    controlRow.removeFromRight(microGap);
    popupToggleButton.setBounds(controlRow.removeFromRight(compactHeaderLayout ? 68 : 78));
    controlRow.removeFromRight(microGap);
    idleToggleButton.setBounds(controlRow.removeFromRight(compactHeaderLayout ? 62 : 68));
    controlRow.removeFromRight(microGap);
    hoverToggleButton.setBounds(controlRow.removeFromRight(compactHeaderLayout ? 68 : 76));

    keyboardDrawer.setBounds(bounds.removeFromBottom(keyboardHeight)
                                   .reduced(compactHeaderLayout ? 20 : 28,
                                            compactHeaderLayout ? 12 : 16));

    if (mainLayout != nullptr)
        mainLayout->setBounds(bounds.reduced(compactHeaderLayout ? 8 : 10, 0));
}

void RootFlowAudioProcessorEditor::timerCallback()
{
    refreshHeaderControlState();
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

    midiLearnButton.setToggleState(learnArmed, juce::dontSendNotification);
    testToneButton.setToggleState(testToneEnabled, juce::dontSendNotification);
    hoverToggleButton.setToggleState(RootFlow::areHoverEffectsEnabled(), juce::dontSendNotification);
    idleToggleButton.setToggleState(RootFlow::areIdleEffectsEnabled(), juce::dontSendNotification);
    popupToggleButton.setToggleState(RootFlow::arePopupOverlaysEnabled(), juce::dontSendNotification);
    midiLearnButton.setButtonText(learnArmed ? "ARM" : "MAP");
    testToneButton.setButtonText(testToneEnabled ? "ON" : "TONE");
    presetSaveButton.setButtonText(presetDirty ? "SAVE*" : "SAVE");
    presetDeleteButton.setEnabled(hasUserPreset);
    presetDeleteButton.setAlpha(hasUserPreset ? 1.0f : 0.45f);

    headerControlStateInitialised = true;
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
            startTimerHz(60);
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
