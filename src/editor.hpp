#pragma once

#include "change_attachment.hpp"
#include "plugin.hpp"
#include "ui_bridge.hpp"

#define DONT_NOTIFY juce::NotificationType::dontSendNotification

namespace atmt {

struct Colours {
  static const juce::Colour eerieBlack;
  static const juce::Colour jet;
  static const juce::Colour frenchGray;
  static const juce::Colour isabelline;
  static const juce::Colour glaucous;
  static const juce::Colour shamrockGreen;
  static const juce::Colour auburn;
};

struct Fonts {
  static const juce::FontOptions sofiaProRegular;
  static const juce::FontOptions sofiaProMedium;
};

struct Style {
  static constexpr f32 lineThickness = 1.5f; 
  static constexpr f32 lineThicknessHighlighted = 2.25f;
};

static const juce::URL supportURL { "https://patreon.com/lucaayscough" };

struct Button : juce::Button {
  enum struct Type { trigger, toggle };

  Button(const juce::String&, Type);
  void paintButton(juce::Graphics&, bool, bool) override;
  void resized() override;

  juce::Rectangle<f32> rectBounds;
  juce::Rectangle<f32> textBounds;
  const juce::Font font { Fonts::sofiaProRegular.withHeight(14) };
};

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

struct ClipView : juce::Component {
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

struct ToolBar : juce::Component {
  struct InfoButton : juce::Button {
    InfoButton();
    void paintButton(juce::Graphics&, bool, bool) override;
    void resized() override;

    juce::Rectangle<f32> ellipseBounds;
    juce::Rectangle<f32> iBounds;
    const juce::Font font { Fonts::sofiaProMedium.withHeight(24) };
  };

  struct KillButton : juce::Button {
    KillButton();
    void paintButton(juce::Graphics&, bool, bool) override;
  };

  ToolBar(StateManager&);
  ~ToolBar() override;
  void resized() override;
  void paint(juce::Graphics&) override;

  StateManager& manager;
  juce::AudioProcessor& proc { manager.proc };

  InfoButton infoButton;
  Button editModeButton { "Edit Mode", Button::Type::toggle };
  Button modulateDiscreteButton { "Discrete Mode", Button::Type::toggle };
  Button supportLinkButton { "Support", Button::Type::trigger };
  KillButton killButton;
  static constexpr i32 height = 60;
  static constexpr i32 buttonWidth = 125;
  static constexpr i32 padding = 10;
  static constexpr i32 buttonPadding = 16;
};

struct DefaultView : juce::Component, juce::FileDragAndDropTarget {
  static constexpr i32 width  = 600;
  static constexpr i32 height = 600;
  static constexpr i32 titleHeight = 76;
  static constexpr i32 titleFontHeight = 28;
  static constexpr i32 buttonHeight = 25;
  static constexpr i32 buttonFontHeight = 14;
  static constexpr i32 panelPadding = 20;

  struct PluginsPanel : juce::Component {
    struct Button {   
      juce::PluginDescription description;
      juce::Rectangle<i32> r {};
      bool visible = false;
      juce::String format = "";
      static constexpr i32 namePadding = 80;
    };

    PluginsPanel(StateManager&);

    void paint(juce::Graphics&);
    void mouseMove(const juce::MouseEvent&);
    void mouseDown(const juce::MouseEvent&);
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&);
    void resized();
    void update(juce::Array<juce::PluginDescription>);
    void updateManufacturerFilter(const juce::String&);

    StateManager& manager;
    juce::Rectangle<i32> titleBounds;
    std::vector<Button> plugins;
    juce::String filter;
  };

  struct ManufacturersPanel : juce::Component {
    struct Button {
      juce::String name;
      juce::Rectangle<i32> r;
    };

    ManufacturersPanel(PluginsPanel&);

    void paint(juce::Graphics& g);
    void mouseMove(const juce::MouseEvent&);
    void mouseDown(const juce::MouseEvent&);
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&);
    void resized();
    void update(juce::Array<juce::PluginDescription>);

    PluginsPanel& pluginsPanel;
    juce::Rectangle<i32> titleBounds;
    std::vector<Button> manufacturers;
    u32 activeButton = 0;
  };

  DefaultView(StateManager&, juce::AudioPluginFormatManager&, juce::KnownPluginList&);
  void resized() override;
  bool isInterestedInFileDrag(const juce::StringArray&)	override;
  void filesDropped(const juce::StringArray&, i32, i32) override;

  StateManager& manager;
  juce::KnownPluginList& knownPluginList;
  juce::AudioPluginFormatManager& formatManager;

  PluginsPanel pluginsPanel { manager };
  ManufacturersPanel manufacturersPanel { pluginsPanel};
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
  ToolBar toolBar { manager };
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

  bool useMainView = false;

  std::unique_ptr<MainView> mainView;
  DefaultView defaultView { manager, proc.apfm, proc.knownPluginList };

  ChangeAttachment createInstanceAttachment { proc.engine.createInstanceBroadcaster, CHANGE_CB(createInstanceChangeCallback) };
  ChangeAttachment killInstanceAttachment   { proc.engine.killInstanceBroadcaster, CHANGE_CB(killInstanceChangeCallback) };
};

} // namespace atmt
