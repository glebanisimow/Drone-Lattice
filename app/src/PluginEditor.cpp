#include "PluginEditor.h"

#include <algorithm>

using namespace theme;

//------------------------------------------------------------------ ModScope
ModScope::ModScope (DroneProcessor& p) : proc (p), hist ((size_t) (60.0f * p.scopeRate())) {}

void ModScope::push (const std::array<float, 4>& v)
{
    hist[(size_t) head] = v;
    head = (head + 1) % (int) hist.size();
    count = juce::jmin (count + 1, (int) hist.size());
}

void ModScope::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    g.setColour (panel);
    g.fillRoundedRectangle (b, 10.0f);

    auto area = b.reduced (14.0f).withTrimmedTop (22.0f);
    g.setColour (text);
    g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    g.drawText ("MOD BUS", b.reduced (14.0f, 8.0f).withHeight (20.0f), juce::Justification::centredLeft);
    g.setColour (dim);
    g.setFont (juce::FontOptions (12.0f));
    g.drawText ("last 60 s", b.reduced (14.0f, 8.0f).withHeight (20.0f), juce::Justification::centredRight);

    g.setColour (text.withAlpha (0.08f));
    g.drawHorizontalLine ((int) area.getCentreY(), area.getX(), area.getRight());
    g.setColour (text.withAlpha (0.04f));
    g.drawHorizontalLine ((int) area.getY(), area.getX(), area.getRight());
    g.drawHorizontalLine ((int) area.getBottom(), area.getX(), area.getRight());

    const bool on = proc.param ("app.modOn") > 0.5f;
    const int N = (int) hist.size();
    static const char* dest[] = { "focusX", "focusY", "radius", "stretch / couple" };
    for (int c = 0; c < 4; ++c)
    {
        juce::Path p;
        for (int k = 0; k < count; ++k)
        {
            const int idx = (head - count + k + N) % N;
            const float x = area.getRight() - (float) (count - 1 - k) / (float) (N - 1) * area.getWidth();
            const float y = area.getCentreY() - hist[(size_t) idx][(size_t) c] * area.getHeight() * 0.5f;
            if (k == 0) p.startNewSubPath (x, y);
            else p.lineTo (x, y);
        }
        g.setColour (modColour (c).withAlpha (on ? 0.9f : 0.3f));
        g.strokePath (p, juce::PathStrokeType (1.6f));

        g.setFont (juce::FontOptions (11.5f));
        const auto legend = juce::Rectangle<float> (area.getX() + (float) c * area.getWidth() / 4.0f, b.getBottom() - 22.0f,
                                                    area.getWidth() / 4.0f, 16.0f);
        g.drawText ("m" + juce::String (c + 1) + " " + juce::String (juce::CharPointer_UTF8 ("\xe2\x86\x92 ")) + dest[c], legend,
                    juce::Justification::centredLeft);
    }
}

//--------------------------------------------------------------- DroneEditor
DroneEditor::DroneEditor (DroneProcessor& p)
    : AudioProcessorEditor (p), proc (p), lattice (p), scope (p)
{
    setLookAndFeel (&look);

    title.setText ("DRONE LATTICE", juce::dontSendNotification);
    title.setFont (juce::Font (juce::FontOptions (20.0f, juce::Font::bold)));
    title.setColour (juce::Label::textColourId, amber);
    addAndMakeVisible (title);

    const auto& fp = DroneProcessor::factoryPresets();
    for (int i = 0; i < (int) fp.size(); ++i)
        presets.addItem (fp[(size_t) i].name, i + 1);
    presets.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
    presets.onChange = [this] { proc.setCurrentProgram (presets.getSelectedId() - 1); };
    addAndMakeVisible (presets);

    saveBtn.onClick = [this] { savePreset(); };
    loadBtn.onClick = [this] { loadPreset(); };
    seedBtn.onClick = [this] { proc.newSeeds(); };
    for (auto* b : { &saveBtn, &loadBtn, &seedBtn }) addAndMakeVisible (*b);

    modAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "app.modOn", modToggle);
    addAndMakeVisible (modToggle);

    for (auto* l : { &swarmLabel, &meshLabel })
    {
        l->setFont (juce::Font (juce::FontOptions (12.0f, juce::Font::bold)));
        l->setColour (juce::Label::textColourId, dim);
        addAndMakeVisible (*l);
    }
    swarmLabel.setText ("SWARM", juce::dontSendNotification);
    meshLabel.setText ("MESH", juce::dontSendNotification);
    swarmLabel.setJustificationType (juce::Justification::centredRight);
    blendAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "app.blend", blend);
    blend.setDoubleClickReturnValue (true, 0.5);
    addAndMakeVisible (blend);

    level = std::make_unique<Knob> (proc.apvts, "output.level", "level");
    addAndMakeVisible (*level);

    cpuLabel.setColour (juce::Label::textColourId, dim);
    cpuLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (cpuLabel);

    addAndMakeVisible (lattice);
    addAndMakeVisible (scope);

    // knob pages
    auto page = [this] (std::vector<KnobPage::Row> rows) { return new KnobPage (proc.apvts, std::move (rows)); };
    auto split = [] (const juce::String& t, std::vector<ParamInfo> v) { return KnobPage::Row { t, std::move (v) }; };
    const auto tabBg = panel;
    tabs.addTab ("Tuning", tabBg, page ({ split ("root, generators, window offset, glide", group ("tuning")) }), true);
    tabs.addTab ("Lattice", tabBg, page ({ split ("window", group ("lattice")),
                                        split ("mod bus -> engines", group ("mod")) }), true);
    tabs.addTab ("Swarm", tabBg, page ({ split ("additive swarm", group ("swarm")) }), true);
    tabs.addTab ("Mesh", tabBg, page ({ split ("resonator mesh", group ("mesh")) }), true);
    tabs.addTab ("Mod bus", tabBg, page ({ split ("bus", group ("modbus.bus")),
                                        split ("m1 -> focusX     shape: 0 sine, 1 walk, 2 chaos", group ("modbus.m1")),
                                        split ("m2 -> focusY", group ("modbus.m2")),
                                        split ("m3 -> radius", group ("modbus.m3")),
                                        split ("m4 -> stretch / couple", group ("modbus.m4")) }), true);
    tabs.addTab ("MIDI", tabBg, page ({ split ("mode: Off = drone plays alone, Gate = while a key is held, "
                                            "Latch = first note starts it; the note sets the root", group ("midi")) }), true);
    tabs.setTabBarDepth (32);
    tabs.setCurrentTabIndex (0);
    tabs.setOutline (0);
    addAndMakeVisible (tabs);

    setResizable (true, true);
    setResizeLimits (1100, 760, 2400, 1600);
    setSize (1260, 860);
    startTimerHz (30);
}

DroneEditor::~DroneEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

std::vector<ParamInfo> DroneEditor::group (const juce::String& g) const
{
    return paramsOfGroup (proc.paramInfos(), g);
}

void DroneEditor::paint (juce::Graphics& g)
{
    g.fillAll (bg);
}

void DroneEditor::resized()
{
    auto b = getLocalBounds().reduced (14);
    auto head = b.removeFromTop (64);
    title.setBounds (head.removeFromLeft (200).withSizeKeepingCentre (200, 30));
    presets.setBounds (head.removeFromLeft (190).withSizeKeepingCentre (180, 28));
    loadBtn.setBounds (head.removeFromLeft (64).withSizeKeepingCentre (58, 28));
    saveBtn.setBounds (head.removeFromLeft (64).withSizeKeepingCentre (58, 28));
    seedBtn.setBounds (head.removeFromLeft (96).withSizeKeepingCentre (88, 28));
    head.removeFromLeft (12);
    modToggle.setBounds (head.removeFromLeft (100).withSizeKeepingCentre (96, 28));
    cpuLabel.setBounds (head.removeFromRight (90));
    level->setBounds (head.removeFromRight (78).withTrimmedTop (-6));
    head.removeFromRight (10);
    swarmLabel.setBounds (head.removeFromLeft (64));
    meshLabel.setBounds (head.removeFromRight (56));
    blend.setBounds (head.reduced (6, 18));

    b.removeFromTop (8);
    const int side = juce::jmin (b.getHeight(), (int) (b.getWidth() * 0.5f));
    lattice.setBounds (b.removeFromLeft (side));
    b.removeFromLeft (14);
    scope.setBounds (b.removeFromTop (juce::jmax (180, b.getHeight() / 3)));
    b.removeFromTop (14);
    tabs.setBounds (b);
}

void DroneEditor::timerCallback()
{
    std::array<std::array<float, 4>, 64> frames;
    int n;
    while ((n = proc.popScope (frames.data(), (int) frames.size())) > 0)
        for (int i = 0; i < n; ++i) scope.push (frames[(size_t) i]);

    lattice.refresh();
    lattice.repaint();
    scope.repaint();
    cpuLabel.setText ("CPU " + juce::String (proc.cpuLoad() * 100.0f, 1) + " %", juce::dontSendNotification);
    if (presets.getSelectedId() != proc.getCurrentProgram() + 1)
        presets.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
}

void DroneEditor::savePreset()
{
    const auto dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Drone Lattice");
    dir.createDirectory();
    chooser = std::make_unique<juce::FileChooser> ("Save preset", dir.getChildFile ("preset.dlpreset"), "*.dlpreset");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::warnAboutOverwriting,
                          [this] (const juce::FileChooser& fc)
                          {
                              auto f = fc.getResult();
                              if (f == juce::File()) return;
                              if (auto xml = proc.apvts.copyState().createXml())
                                  xml->writeTo (f.withFileExtension ("dlpreset"));
                          });
}

void DroneEditor::loadPreset()
{
    const auto dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Drone Lattice");
    chooser = std::make_unique<juce::FileChooser> ("Load preset", dir, "*.dlpreset");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc)
                          {
                              auto f = fc.getResult();
                              if (! f.existsAsFile()) return;
                              if (auto xml = juce::XmlDocument::parse (f))
                                  if (xml->hasTagName (proc.apvts.state.getType()))
                                      proc.apvts.replaceState (juce::ValueTree::fromXml (*xml));
                          });
}
