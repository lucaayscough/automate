#pragma once

#include "timeline.hpp"
#include <juce_gui_basics/juce_gui_basics.h>
#include "types.h"
#include "plugin.hpp"

namespace atmt {

struct DebugTools : juce::Component {
  DebugTools(StateManager&);
  void resized() override;
  void editModeChangeCallback(const juce::var&);
  void modulateDiscreteChangeCallback(const juce::var&);

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
  juce::ToggleButton parametersToggleButton { "Show Parameters" };
  juce::ToggleButton editModeButton { "Edit Mode" };
  juce::ToggleButton modulateDiscreteButton { "Modulate Discrete" };

  StateAttachment editModeAttachment { editTree, ID::editMode, STATE_CB(editModeChangeCallback), nullptr };
  StateAttachment modulateDiscreteAttachment { editTree, ID::modulateDiscrete, STATE_CB(modulateDiscreteChangeCallback), undoManager };
};

struct DebugInfo : juce::Component {
  DebugInfo(UIBridge&);
  void resized();

  UIBridge& uiBridge;
  juce::DrawableText info;
};

struct PresetsListPanel : juce::Component, juce::ValueTree::Listener {
  struct Title : juce::Component {
    void paint(juce::Graphics&) override;
    juce::String text { "Presets" };
  };

  struct Preset : juce::Component {
    Preset(StateManager&, const juce::String&);
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;

    StateManager& manager;
    juce::UndoManager* undoManager { manager.undoManager };
    juce::AudioProcessor& proc { manager.proc };
    juce::TextButton selectorButton;
    juce::TextButton removeButton { "X" };
    juce::TextButton overwriteButton { "Overwrite" };
  };

  PresetsListPanel(StateManager&);
  void resized() override;
  void addPreset(const juce::ValueTree&);
  void removePreset(const juce::ValueTree&);
  i32 getNumPresets();
  void valueTreeChildAdded(juce::ValueTree&, juce::ValueTree&) override;
  void valueTreeChildRemoved(juce::ValueTree&, juce::ValueTree&, i32) override;

  StateManager& manager;
  juce::UndoManager* undoManager { manager.undoManager };
  juce::ValueTree presetsTree { manager.presetsTree };
  Title title;
  juce::OwnedArray<Preset> presets;
  juce::TextEditor presetNameInput;
  juce::TextButton savePresetButton { "Save Preset" };
};

struct PluginListView : juce::Viewport {
  struct Contents : juce::Component, juce::FileDragAndDropTarget {
    Contents(StateManager&, juce::AudioPluginFormatManager&, juce::KnownPluginList&);
    void paint(juce::Graphics&) override;
    void resized() override;
    void addPlugin(juce::PluginDescription&);
    bool isInterestedInFileDrag(const juce::StringArray&)	override;
    void filesDropped(const juce::StringArray&, i32, i32) override;

    StateManager& manager;
    juce::UndoManager* undoManager { manager.undoManager };
    juce::ValueTree editTree { manager.editTree };
    juce::CachedValue<juce::String> pluginID { editTree, ID::pluginID, undoManager };
    juce::KnownPluginList& knownPluginList;
    juce::AudioPluginFormatManager& formatManager;
    juce::OwnedArray<juce::TextButton> plugins;
    static constexpr i32 buttonHeight = 25;
  };

  PluginListView(StateManager&, juce::AudioPluginFormatManager&, juce::KnownPluginList&);
  void resized();
  Contents c;
};

struct DefaultView : juce::Component {
  DefaultView(StateManager&, juce::KnownPluginList&, juce::AudioPluginFormatManager&);
  void resized() override;
  PluginListView list;
};

struct ParametersView : juce::Viewport {
  struct Parameter : juce::Component, juce::AudioProcessorParameter::Listener {
    Parameter(juce::AudioProcessorParameter*);
    ~Parameter() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    void parameterValueChanged(i32, f32) override;
    void parameterGestureChanged(i32, bool) override;

    juce::AudioProcessorParameter* parameter;
    juce::Slider slider;
    juce::Label name;
  };

  struct Contents : juce::Component {
    Contents(const juce::Array<juce::AudioProcessorParameter*>&);
    void resized() override;
    juce::OwnedArray<Parameter> parameters;
  };

  ParametersView(const juce::Array<juce::AudioProcessorParameter*>&);
  void resized() override;
  Contents c;
};

struct MainView : juce::Component {
  MainView(StateManager&, UIBridge&, juce::AudioProcessorEditor*, const juce::Array<juce::AudioProcessorParameter*>&);
  void resized() override;
  void toggleParametersView();
  void childBoundsChanged(juce::Component*) override;

  StateManager& manager;
  UIBridge& uiBridge;
  PresetsListPanel statesPanel { manager };
  Track track { manager, uiBridge };
  std::unique_ptr<juce::AudioProcessorEditor> instance;
  ParametersView parametersView;
  i32 statesPanelWidth = 150;
};

} // namespace atmt
