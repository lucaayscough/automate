#pragma once

#include "change_attachment.hpp"
#include "plugin.hpp"
#include "ui_bridge.hpp"

namespace atmt {

struct Grid {
  struct Beat {
    u32 bar = 1;
    u32 beat = 1;
    f64 x = 0;
  };

  struct TimeSignature {
    u32 numerator = 4;
    u32 denominator = 4;
  };

  bool reset(f64, f64, TimeSignature);
  void reset();
  f64 snap(f64);

  void narrow();
  void widen();
  void triplet();
  void toggleSnap();

  TimeSignature ts;
  f64 zoom = 0;
  f64 maxWidth = 0;

  static constexpr f64 intervalMin = 40;
  f64 snapInterval = 0;
  bool tripletMode = false;
  i32 gridWidth = 0;
  bool snapOn = true;

  std::vector<Beat> beats;
  std::vector<f64> lines;
};

struct ClipView : juce::Component, Clip, juce::SettableTooltipClient {
  ClipView(StateManager&, juce::ValueTree&, juce::UndoManager*, Grid&);
  void paint(juce::Graphics&) override;
  void mouseDown(const juce::MouseEvent&) override;
  void mouseDoubleClick(const juce::MouseEvent&) override;
  void mouseUp(const juce::MouseEvent&) override;
  void mouseDrag(const juce::MouseEvent&) override;
  void beginDrag(const juce::MouseEvent&);
  void endDrag();

  StateManager& manager;
  juce::ValueTree editValueTree { manager.editTree };
  Grid& grid;
  juce::CachedValue<f64> zoom { editValueTree, ID::zoom, nullptr };
  static constexpr i32 trimThreshold = 20;
  bool isTrimDrag = false;
  bool isLeftTrimDrag = false;
  f64 mouseDownOffset = 0;
};

struct PathView : juce::Component, Path {
  PathView(StateManager&, juce::ValueTree&, juce::UndoManager*, Grid&);
  void paint(juce::Graphics&) override;
  void mouseDown(const juce::MouseEvent&) override;
  void mouseDrag(const juce::MouseEvent&) override;
  void mouseDoubleClick(const juce::MouseEvent&) override;

  StateManager& manager;
  Grid& grid;
  juce::CachedValue<f64> zoom { manager.editTree, ID::zoom, nullptr };
  static constexpr i32 size = 10;
  static constexpr i32 posOffset = size / 2;
};

struct AutomationLane : juce::Component {
  enum class GestureType { none, bend, drag, select };

  struct Selection {
    f64 start = 0;
    f64 end = 0;
  };

  AutomationLane(StateManager&, Grid&);
  void paint(juce::Graphics&) override;
  void resized() override;
  
  auto getAutomationPoint(juce::Point<f32>);
  f64 getDistanceFromPoint(juce::Point<f32>);

  void addPath(juce::ValueTree&);

  void mouseMove(const juce::MouseEvent&) override;
  void mouseDown(const juce::MouseEvent&) override;
  void mouseUp(const juce::MouseEvent&) override;
  void mouseExit(const juce::MouseEvent&) override;
  void mouseDrag(const juce::MouseEvent&) override;
  void mouseDoubleClick(const juce::MouseEvent&) override;

  StateManager& manager;
  Grid& grid;
  juce::UndoManager* undoManager { manager.undoManager };
  juce::ValueTree editTree { manager.editTree };
  juce::CachedValue<f64> zoom { editTree, ID::zoom, nullptr };
  Automation automation { editTree, undoManager };
  juce::OwnedArray<PathView> paths;

  juce::Rectangle<f32> hoverBounds;
  f64 xHighlightedSegment = -1;
  static constexpr i32 mouseIntersectDistance = 5;
  static constexpr i32 mouseOverDistance = 15;
  bool optKeyPressed = false;
  juce::Point<i32> lastMouseDragOffset;

  f64 kMoveIncrement = 50;
  f64 bendMouseDistanceProportion = 0;

  GestureType activeGesture = GestureType::none;
  Selection selection;
};

struct Track : juce::Component, juce::ValueTree::Listener, juce::DragAndDropTarget, juce::Timer {
  Track(StateManager&, UIBridge&);
  void paint(juce::Graphics&) override;
  void resized() override;
  void timerCallback() override;
  i32 getTrackWidth();
  void rebuildClips();
  bool isInterestedInDragSource(const juce::DragAndDropTarget::SourceDetails&) override;
  void itemDropped(const juce::DragAndDropTarget::SourceDetails&) override;
  void valueTreeChildAdded(juce::ValueTree&, juce::ValueTree&) override;
  void valueTreeChildRemoved(juce::ValueTree&, juce::ValueTree&, i32) override;
  void valueTreePropertyChanged(juce::ValueTree&, const juce::Identifier&) override;
  void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
  void zoomTrack(f64, f64);

  StateManager& manager;
  UIBridge& uiBridge;
  juce::UndoManager* undoManager { manager.undoManager };
  juce::ValueTree editTree { manager.editTree };
  juce::ValueTree presetsTree { manager.presetsTree };
  Presets presets { presetsTree, undoManager };

  Grid grid;

  AutomationLane automationLane { manager, grid };
  juce::OwnedArray<ClipView> clips;
  juce::CachedValue<f64> zoom { editTree, ID::zoom, nullptr };
  static constexpr f64 zoomDeltaScale = 5.0;
  f64 playheadPosition = 0;

  juce::Viewport viewport;

  static constexpr i32 timelineHeight = 20;
  static constexpr i32 presetLaneHeight = 25;
  static constexpr i32 height = 200;

  juce::Rectangle<i32> timelineBounds;
  juce::Rectangle<i32> presetLaneTopBounds;
  juce::Rectangle<i32> presetLaneBottomBounds;

  bool shiftKeyPressed = false;
  bool cmdKeyPressed = false;
  static constexpr i32 kScrollSpeed = 500;
  static constexpr i32 kZoomSpeed = 2;
};

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

struct Editor : juce::AudioProcessorEditor, juce::DragAndDropContainer {
  explicit Editor(Plugin&);

  void paint(juce::Graphics&) override;
  void resized() override;

  void showDefaultView();
  void showMainView();

  void pluginIDChangeCallback(const juce::var&);
  void createInstanceChangeCallback();
  void killInstanceChangeCallback();
  void childBoundsChanged(juce::Component*) override;
  bool keyPressed(const juce::KeyPress&) override;
  void modifierKeysChanged(const juce::ModifierKeys&) override;

  Plugin& proc;
  StateManager& manager { proc.manager };
  juce::UndoManager* undoManager { manager.undoManager };
  UIBridge& uiBridge { proc.uiBridge };
  Engine& engine { proc.engine };

  juce::TooltipWindow tooltipWindow { this, 0 };

  int width  = 350;
  int height = 350;

  int debugToolsHeight = 30;
  int debugInfoHeight = 60;

  bool useMainView = false;

  DebugTools debugTools { manager };
  DebugInfo debugInfo { uiBridge };

  std::unique_ptr<MainView> mainView;
  DefaultView defaultView { manager, proc.knownPluginList, proc.apfm };

  ChangeAttachment createInstanceAttachment { proc.engine.createInstanceBroadcaster, CHANGE_CB(createInstanceChangeCallback) };
  ChangeAttachment killInstanceAttachment   { proc.engine.killInstanceBroadcaster, CHANGE_CB(killInstanceChangeCallback) };
  StateAttachment  pluginIDAttachment       { manager.editTree, ID::pluginID, STATE_CB(pluginIDChangeCallback), manager.undoManager };

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Editor)
};

} // namespace atmt
