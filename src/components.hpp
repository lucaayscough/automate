#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "plugin.hpp"

namespace atmt {

struct DebugTools : juce::Component {
  DebugTools(StateManager& m) : manager(m) {
    addAndMakeVisible(printStateButton);
    addAndMakeVisible(killButton);
    addAndMakeVisible(playButton);
    addAndMakeVisible(stopButton);
    addAndMakeVisible(rewindButton);
    addAndMakeVisible(editModeButton);
    addAndMakeVisible(modulateDiscreteButton);
    addAndMakeVisible(undoButton);
    addAndMakeVisible(redoButton);
    addAndMakeVisible(randomiseButton);

    printStateButton        .onClick = [this] { DBG(manager.valueTreeToXmlString(manager.state)); };
    editModeButton          .onClick = [this] { editModeAttachment.setValue({ !editMode }); };
    killButton              .onClick = [this] { pluginID.setValue("", undoManager); };
    playButton              .onClick = [this] { static_cast<Plugin*>(&proc)->signalPlay(); };
    stopButton              .onClick = [this] { static_cast<Plugin*>(&proc)->signalStop(); };
    rewindButton            .onClick = [this] { static_cast<Plugin*>(&proc)->signalRewind(); };
    undoButton              .onClick = [this] { undoManager->undo(); };
    redoButton              .onClick = [this] { undoManager->redo(); };
    randomiseButton         .onClick = [this] { static_cast<Plugin*>(&proc)->engine.randomiseParameters(); };

    // TODO(luca): this discrete button seems to be broken
    modulateDiscreteButton  .onClick = [this] { modulateDiscreteAttachment.setValue({ !modulateDiscrete }); };
  }
  
  void resized() override {
    auto r = getLocalBounds();
    auto width = getWidth() / getNumChildComponents();
    printStateButton.setBounds(r.removeFromLeft(width));
    editModeButton.setBounds(r.removeFromLeft(width));
    modulateDiscreteButton.setBounds(r.removeFromLeft(width));
    killButton.setBounds(r.removeFromLeft(width));
    playButton.setBounds(r.removeFromLeft(width));
    stopButton.setBounds(r.removeFromLeft(width));
    rewindButton.setBounds(r.removeFromLeft(width));
    undoButton.setBounds(r.removeFromLeft(width));
    redoButton.setBounds(r.removeFromLeft(width));
    randomiseButton.setBounds(r);
  }

  void editModeChangeCallback(const juce::var& v) {
    editModeButton.setToggleState(bool(v), juce::NotificationType::dontSendNotification);
  }

  void modulateDiscreteChangeCallback(const juce::var& v) {
    modulateDiscreteButton.setToggleState(bool(v), juce::NotificationType::dontSendNotification);
  }

  StateManager& manager;
  juce::AudioProcessor& proc { manager.apvts.processor };
  juce::UndoManager* undoManager { manager.undoManager };
  juce::ValueTree editTree { manager.editTree };
  juce::CachedValue<bool> editMode { editTree, ID::editMode, nullptr };
  juce::CachedValue<bool> modulateDiscrete { editTree, ID::modulateDiscrete, undoManager };
  juce::CachedValue<juce::String> pluginID { editTree, ID::pluginID, undoManager };
  juce::TextButton printStateButton { "Print State" };
  juce::TextButton killButton { "Kill" };
  juce::TextButton playButton { "Play" };
  juce::TextButton stopButton { "Stop" };
  juce::TextButton rewindButton { "Rewind" };
  juce::TextButton undoButton { "Undo" };
  juce::TextButton redoButton { "Redo" };
  juce::TextButton randomiseButton { "Randomise" };
  juce::ToggleButton editModeButton { "Edit Mode" };
  juce::ToggleButton modulateDiscreteButton { "Modulate Discrete" };

  StateAttachment editModeAttachment { editTree, ID::editMode, STATE_CB(editModeChangeCallback), nullptr };
  StateAttachment modulateDiscreteAttachment { editTree, ID::modulateDiscrete, STATE_CB(modulateDiscreteChangeCallback), undoManager };
};

struct DebugInfo : juce::Component {
  DebugInfo(UIBridge& b) : uiBridge(b) {
    addAndMakeVisible(info);
    info.setText("some info");
    info.setFont(juce::FontOptions { 12 }, false);
    info.setColour(juce::Colours::white);
  }

  void resized() {
    info.setBounds(getLocalBounds());
  }

  UIBridge& uiBridge;
  juce::DrawableText info;
};

struct DescriptionBar : juce::Component {
  void paint(juce::Graphics& g) override {
    g.setColour(juce::Colours::white);
    g.setFont(getHeight());
    
    if (description) {
      g.drawText(description->name, getLocalBounds(), juce::Justification::left);
    }
  }

  void setDescription(std::unique_ptr<juce::PluginDescription>& desc) {
    description = std::move(desc);
    repaint();
  }

  std::unique_ptr<juce::PluginDescription> description;
};

struct PresetsListPanel : juce::Component, juce::ValueTree::Listener {
  PresetsListPanel(StateManager& m) : manager(m) {
    addAndMakeVisible(title);
    addAndMakeVisible(presetNameInput);
    addAndMakeVisible(savePresetButton);

    // TODO(luca): the vt listeners can probably go now

    savePresetButton.onClick = [this] () -> void {
      auto name = presetNameInput.getText();
      if (presetNameInput.getText() != "" && !manager.doesPresetNameExist(name)) {
        manager.savePreset(name);
      } else {
        juce::MessageBoxOptions options;
        juce::AlertWindow::showAsync(options.withTitle("Error").withMessage("Preset name unavailable."), nullptr);
      }
    };

    presetsTree.addListener(this);

    for (const auto& child : presetsTree) {
      addPreset(child);
    }
  }

  void resized() override {
    auto r = getLocalBounds(); 
    title.setBounds(r.removeFromTop(40));
    for (auto* preset : presets) {
      preset->setBounds(r.removeFromTop(30));
    }
    auto b = r.removeFromBottom(40);
    savePresetButton.setBounds(b.removeFromRight(getWidth() / 2));
    presetNameInput.setBounds(b);
  }

  void addPreset(const juce::ValueTree& presetTree) {
    jassert(presetTree.isValid());
    auto nameVar = presetTree[ID::name];
    jassert(!nameVar.isVoid());
    auto preset = new Preset(manager, nameVar.toString());
    presets.add(preset);
    addAndMakeVisible(preset);
    resized();
  }

  void removePreset(const juce::ValueTree& presetTree) {
    jassert(presetTree.isValid());
    auto nameVar = presetTree[ID::name];
    jassert(!nameVar.isVoid());
    auto name = nameVar.toString();
    for (int i = 0; i < presets.size(); ++i) {
      if (presets[i]->getName() == name) {
        removeChildComponent(presets[i]);
        presets.remove(i);
      }
    }
    resized();
  }

  int getNumPresets() {
    return presets.size();
  }

  void valueTreeChildAdded(juce::ValueTree&, juce::ValueTree& child) override {
    addPreset(child);
  }

  void valueTreeChildRemoved(juce::ValueTree&, juce::ValueTree& child, int) override { 
    removePreset(child);
  }

  struct Title : juce::Component {
    void paint(juce::Graphics& g) override {
      auto r = getLocalBounds();
      g.setColour(juce::Colours::white);
      g.setFont(getHeight());
      g.drawText(text, r, juce::Justification::centred);
    }

    juce::String text { "Presets" };
  };

  struct Preset : juce::Component {
    Preset(StateManager& m, const juce::String& n) : manager(m) {
      setName(n);
      selectorButton.setButtonText(n);

      addAndMakeVisible(selectorButton);
      addAndMakeVisible(overwriteButton);
      addAndMakeVisible(removeButton);

      // TODO(luca): find more appropriate way of doing this 
      selectorButton.onClick = [this] () {
        static_cast<Plugin*>(&proc)->engine.restoreFromPreset(getName());
      };

      overwriteButton.onClick = [this] () { manager.overwritePreset(getName()); };
      removeButton.onClick    = [this] () { manager.removePreset(getName()); };
    }

    void resized() {
      auto r = getLocalBounds();
      r.removeFromLeft(getWidth() / 8);
      removeButton.setBounds(r.removeFromRight(getWidth() / 4));
      overwriteButton.setBounds(r.removeFromLeft(getWidth() / 6));
      selectorButton.setBounds(r);
    }

    void mouseDown(const juce::MouseEvent&) {
      auto container = juce::DragAndDropContainer::findParentDragContainerFor(this);
      if (container) {
        container->startDragging(getName(), this);
      }
    }

    StateManager& manager;
    juce::AudioProcessor& proc { manager.proc };
    juce::TextButton selectorButton;
    juce::TextButton removeButton { "X" };
    juce::TextButton overwriteButton { "Overwrite" };
  };

  StateManager& manager;
  juce::ValueTree presetsTree { manager.presetsTree };
  Title title;
  juce::OwnedArray<Preset> presets;
  juce::TextEditor presetNameInput;
  juce::TextButton savePresetButton { "Save Preset" };

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetsListPanel)
};

struct PluginListComponent : juce::Component, juce::FileDragAndDropTarget {
  PluginListComponent(StateManager& m, juce::AudioPluginFormatManager& fm, juce::KnownPluginList& kpl)
    : manager(m), knownPluginList(kpl), formatManager(fm) {
    for (auto& t : knownPluginList.getTypes()) {
      addPlugin(t);
    }
    updateSize();
    viewport.setScrollBarsShown(true, false);
    viewport.setViewedComponent(this, false);
  }

  void paint(juce::Graphics& g) override {
    g.fillAll(juce::Colours::green);
  }

  void resized() override {
    auto r = getLocalBounds();
    for (auto button : plugins) {
      button->setBounds(r.removeFromTop(buttonHeight));
    }
  }

  void addPlugin(juce::PluginDescription& pd) {
    auto button = new juce::TextButton(pd.pluginFormatName + " - " + pd.name);
    auto id = pd.createIdentifierString();
    addAndMakeVisible(button);
    button->onClick = [id, this] { pluginID.setValue(id, undoManager); };
    plugins.add(button);
  }

  void updateSize() {
    setSize(width, buttonHeight * plugins.size());
  }

  bool isInterestedInFileDrag(const juce::StringArray&)	override {
    return true; 
  }

  void filesDropped(const juce::StringArray& files, int, int) override {
    juce::OwnedArray<juce::PluginDescription> types;
    knownPluginList.scanAndAddDragAndDroppedFiles(formatManager, files, types);
    for (auto t : types) {
      addPlugin(*t);
    }
    updateSize();
  }

  StateManager& manager;
  juce::UndoManager* undoManager { manager.undoManager };
  juce::ValueTree editTree { manager.editTree };
  juce::CachedValue<juce::String> pluginID { editTree, ID::pluginID, undoManager };

  juce::KnownPluginList& knownPluginList;
  juce::AudioPluginFormatManager& formatManager;

  juce::OwnedArray<juce::TextButton> plugins;
  juce::Viewport viewport;
  
  static constexpr int buttonHeight = 25;
  static constexpr int width = 350;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginListComponent)
};

} // namespace atmt
