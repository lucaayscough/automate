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

  bool reset(f64, TimeSignature);
  void reset();
  f64 snap(f64);

  void narrow();
  void widen();
  void triplet();
  void toggleSnap();

  TimeSignature ts;
  f64 maxWidth = 0;

  static constexpr f64 intervalMin = 40;
  f64 snapInterval = 0;
  bool tripletMode = false;
  i32 gridWidth = 0;
  bool snapOn = true;
  f64 zoom_ = 0;

  std::vector<Beat> beats;
  std::vector<f64> lines;
};

struct PathView : juce::Component {
  PathView(StateManager&, Grid&, Path*);
  void paint(juce::Graphics&) override;
  void mouseDrag(const juce::MouseEvent&) override;
  void mouseDoubleClick(const juce::MouseEvent&) override;

  StateManager& manager;
  Grid& grid;
  Path* path = nullptr;

  static constexpr i32 size = 10;
  static constexpr i32 posOffset = size / 2;
};

struct ClipView : juce::Component, juce::SettableTooltipClient {
  ClipView(StateManager&, Grid&, Clip*);
  void paint(juce::Graphics&) override;
  void mouseDown(const juce::MouseEvent&) override;
  void mouseDoubleClick(const juce::MouseEvent&) override;
  void mouseUp(const juce::MouseEvent&) override;
  void mouseDrag(const juce::MouseEvent&) override;

  StateManager& manager;
  Grid& grid;
  Clip* clip = nullptr;
  static constexpr i32 trimThreshold = 20;
  bool isTrimDrag = false;
  bool isLeftTrimDrag = false;
  f64 mouseDownOffset = 0;
};

struct AutomationLane : juce::Component {
  enum class GestureType { none, bend, drag, select };

  AutomationLane(StateManager&, Grid&);
  ~AutomationLane() override;
  void paint(juce::Graphics&) override;
  void resized() override;
  
  auto getAutomationPoint(juce::Point<f32>);
  f64 getDistanceFromPoint(juce::Point<f32>);

  void mouseMove(const juce::MouseEvent&) override;
  void mouseDown(const juce::MouseEvent&) override;
  void mouseUp(const juce::MouseEvent&) override;
  void mouseExit(const juce::MouseEvent&) override;
  void mouseDrag(const juce::MouseEvent&) override;
  void mouseDoubleClick(const juce::MouseEvent&) override;

  StateManager& manager;
  Grid& grid;
  juce::Path automation;
  juce::OwnedArray<PathView> paths;

  juce::Rectangle<f32> hoverBounds;
  f64 xHighlightedSegment = -1;
  static constexpr i32 mouseIntersectDistance = 5;
  static constexpr i32 mouseOverDistance = 15;
  bool optKeyPressed = false;
  juce::Point<i32> lastMouseDragOffset;

  f64 kDragIncrement = 50;
  f64 bendMouseDistanceProportion = 0;

  GestureType activeGesture = GestureType::none;
  Selection selection;
};

struct Track : juce::Component, juce::Timer {
  Track(StateManager&, UIBridge&);
  ~Track() override;
  void paint(juce::Graphics&) override;
  void resized() override;
  void timerCallback() override;
  void resetGrid();
  i32 getTrackWidth();
  void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
  void mouseDoubleClick(const juce::MouseEvent&) override;

  void zoomTrack(f64);
  void scroll(f64);

  StateManager& manager;
  UIBridge& uiBridge;

  Grid grid;
  juce::OwnedArray<ClipView> clips;

  AutomationLane automationLane { manager, grid };
  static constexpr f64 zoomDeltaScale = 5;
  f64 playheadPosition = 0;

  static constexpr i32 timelineHeight = 20;
  static constexpr i32 presetLaneHeight = 25;
  static constexpr i32 height = 200;

  struct Bounds {
    juce::Rectangle<i32> timeline;
    juce::Rectangle<i32> automation;
    juce::Rectangle<i32> presetLaneTop;
    juce::Rectangle<i32> presetLaneBottom;
  };

  Bounds b;

  bool shiftKeyPressed = false;
  bool cmdKeyPressed = false;
  static constexpr i32 kScrollSpeed = 500;
  static constexpr i32 kZoomSpeed = 2;
  f64 viewportDeltaX = 0;
};

struct DebugTools : juce::Component {
  DebugTools(StateManager&);
  ~DebugTools() override;
  void resized() override;

  StateManager& manager;
  juce::AudioProcessor& proc { manager.proc };
  juce::TextButton printStateButton { "Print State" };
  juce::TextButton killButton { "Kill" };
  juce::ToggleButton parametersToggleButton { "Show Parameters" };
  juce::ToggleButton editModeButton { "Edit Mode" };
  juce::ToggleButton modulateDiscreteButton { "Modulate Discrete" };
  juce::ToggleButton captureParameterChangesButton { "Capture Parameters" };
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
  struct ParameterView : juce::Component, juce::AudioProcessorParameter::Listener {
    ParameterView(StateManager&, Parameter*);
    ~ParameterView() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    void update();
    void parameterValueChanged(i32, f32) override;
    void parameterGestureChanged(i32, bool) override;

    StateManager& manager;
    Parameter* parameter = nullptr;
    juce::Slider slider;
    juce::Label name;
    juce::ToggleButton activeToggle;
  };

  struct Contents : juce::Component {
    Contents(StateManager&);
    void resized() override;

    StateManager& manager;
    juce::OwnedArray<ParameterView> parameters;
  };

  ParametersView(StateManager&);
  ~ParametersView() override;
  void resized() override;
  void updateParameters();

  StateManager& manager;
  Contents c;
};

struct MainView : juce::Component {
  MainView(StateManager&, UIBridge&, juce::AudioProcessorEditor*);
  void resized() override;
  void toggleParametersView();
  void childBoundsChanged(juce::Component*) override;

  StateManager& manager;
  UIBridge& uiBridge;
  Track track { manager, uiBridge };
  std::unique_ptr<juce::AudioProcessorEditor> instance;
  ParametersView parametersView { manager };
};

struct Editor : juce::AudioProcessorEditor, juce::DragAndDropContainer {
  explicit Editor(Plugin&);

  void paint(juce::Graphics&) override;
  void resized() override;

  void showDefaultView();
  void showMainView();

  void createInstanceChangeCallback();
  void killInstanceChangeCallback();
  void childBoundsChanged(juce::Component*) override;
  bool keyPressed(const juce::KeyPress&) override;
  void modifierKeysChanged(const juce::ModifierKeys&) override;

  Plugin& proc;
  StateManager& manager { proc.manager };
  UIBridge& uiBridge { proc.uiBridge };
  Engine& engine { proc.engine };

  juce::TooltipWindow tooltipWindow { this, 0 };

  int width  = 350;
  int height = 350;

  int debugToolsHeight = 30;

  bool useMainView = false;

  DebugTools debugTools { manager };

  std::unique_ptr<MainView> mainView;
  DefaultView defaultView { manager, proc.knownPluginList, proc.apfm };

  ChangeAttachment createInstanceAttachment { proc.engine.createInstanceBroadcaster, CHANGE_CB(createInstanceChangeCallback) };
  ChangeAttachment killInstanceAttachment   { proc.engine.killInstanceBroadcaster, CHANGE_CB(killInstanceChangeCallback) };
};

} // namespace atmt
