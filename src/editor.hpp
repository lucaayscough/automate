#pragma once

#include "plugin.hpp"
#include <numbers>

namespace atmt {

struct Colours {
  static const juce::Colour eerieBlack;
  static const juce::Colour jet;
  static const juce::Colour frenchGray;
  static const juce::Colour isabelline;
  static const juce::Colour glaucous;
  static const juce::Colour shamrockGreen;
  static const juce::Colour auburn;
  static const juce::Colour outerSpace; static const juce::Colour atomicTangerine;
};

struct Fonts {
  static const juce::FontOptions sofiaProLight;
  static const juce::FontOptions sofiaProRegular;
  static const juce::FontOptions sofiaProMedium;
};

struct Style {
  static constexpr f32 lineThickness = 1.25f; 
  static constexpr f32 lineThicknessHighlighted = 1.75f;
};

static const juce::URL supportURL { "https://patreon.com/lucaayscough" };

struct Button : juce::Button {
  enum struct Type { trigger, toggle };

  Button(const juce::String&, Type);
  void paintButton(juce::Graphics&, bool, bool) override;
  void resized() override;

  juce::Rectangle<f32> rectBounds;
  juce::Rectangle<f32> textBounds;
  const juce::Font font { Fonts::sofiaProLight.withHeight(14) };
};

struct Dial : juce::Slider {
  Dial();
  void paint(juce::Graphics& g) override;
  void resized() override;
  void mouseDown(const juce::MouseEvent& e) override;
  void mouseUp(const juce::MouseEvent& e) override;

  bool active = false;
  static constexpr f32 pi = f32(std::numbers::pi);
  static constexpr f32 tau = 2 * pi;
  static constexpr f32 offset = pi + pi * 0.25f;
  static constexpr f32 dotSize = 5;
  static constexpr f32 dotOffset = dotSize * 0.5f;
};

struct InfoView : juce::Component {
  struct Command {
    const char* name;      
    const char* binding;
  };

  static constexpr i32 commandHeight = 18;
  static constexpr Command commands[] =
  {  
    { "Enable all parameters",      "Command + E" },
    { "Disable all parameters",     "Command + D" },
    { "Capture parameter",          "Command + Click" },
    { "Release parameter",          "Command + Shift + Click " },
    { "Randomise parameters",       "R" },
    { "Kill instance",              "K" },
    { "Narrow grid",                "Command + 1" },
    { "Widen grid",                 "Command + 2" },
    { "Toggle triplet grid",        "Command + 3" },
    { "Toggle grid snapping",       "Command + 4" },
    { "Zoom in",                    "Command + Scroll / +" },
    { "Zoom out",                   "Command + Scroll / -" },
    { "Scroll",                     "Shift + Scroll" },
    { "Create clip",                "Double click on clip lane" },
    { "Delete clip",                "Double click on clip" },
    { "Duplicate clip",             "Alt/Opt + click + drag" },
    { "Delete selection",           "Backspace" },
    { "Toggle info view",           "I" }
  };

  static constexpr i32 numCommands = std::size(commands);

  void paint(juce::Graphics& g) override;
  void mouseDown(const juce::MouseEvent&) override;

  std::function<void()> mainViewUpdateCallback;
  const juce::Font font { Fonts::sofiaProLight.withHeight(12) };
};

struct PathView : juce::Component {
  PathView();
  void paint(juce::Graphics&) override;
  void mouseDrag(const juce::MouseEvent&) override;
  void mouseDoubleClick(const juce::MouseEvent&) override;

  std::function<void(u32, f32, f32)> move;
  std::function<void(u32)> remove;
  u32 id = 0;
  static constexpr i32 size = 20;
  static constexpr i32 posOffset = size / 2;
};

struct ClipView : juce::Component {
  ClipView();
  void paint(juce::Graphics&) override;
  void mouseDown(const juce::MouseEvent&) override;
  void mouseDoubleClick(const juce::MouseEvent&) override;
  void mouseUp(const juce::MouseEvent&) override;
  void mouseDrag(const juce::MouseEvent&) override;

  std::function<void(u32, f32, f32)> move;
  std::function<void(u32)> remove;
  std::function<void(i32)> select;
  bool selected = false;
  u32 id = 0;
  static constexpr i32 trimThreshold = 20;
  bool isTrimDrag = false;
  bool isLeftTrimDrag = false;
  f32 mouseDownOffset = 0;
};

struct AutomationLane : juce::Component {
  enum class GestureType { none, bend, drag, select, addPath };

  void paint(juce::Graphics&) override;
  
  auto getAutomationPoint(juce::Point<f32>);
  f32 getDistanceFromPoint(juce::Point<f32>);

  void mouseMove(const juce::MouseEvent&) override;
  void mouseDown(const juce::MouseEvent&) override;
  void mouseUp(const juce::MouseEvent&) override;
  void mouseExit(const juce::MouseEvent&) override;
  void mouseDrag(const juce::MouseEvent&) override;
  void mouseDoubleClick(const juce::MouseEvent&) override;
  
  std::function<void(f32, f32)> setSelection;
  std::function<void(f32)> setPlayheadPosition; // TODO(luca): do we need this? 
  std::function<u32(f32, f32, f32)> addPath;
  std::function<void(f32, f32)> bendAutomation;
  std::function<void(f32)> flattenAutomationCurve;
  std::function<void(f32, f32)> dragAutomationSection;
  std::function<void(u32, f32, f32)> movePath;

  juce::Path automation;
  juce::OwnedArray<PathView> pathViews;

  bool paintHoverPoint = false;
  juce::Rectangle<f32> hoverBounds;

  f32 xHighlightedSegment = NONE;
  static constexpr i32 mouseIntersectDistance = 5;
  static constexpr i32 mouseOverDistance = 15;
  juce::Point<i32> lastMouseDragOffset;

  f32 kDragIncrement = 100;
  f32 bendMouseDistanceProportion = 0;

  GestureType activeGesture = GestureType::none;
  Selection selection;
  u32 lastPathAddedID = 0; 

  static constexpr f32 lineThickness = 2;
};

struct TrackView : juce::Component, juce::DragAndDropContainer, juce::DragAndDropTarget {
  TrackView();
  void paint(juce::Graphics&) override;
  void resized() override;
  void mouseMagnify(const juce::MouseEvent&, f32) override;
  void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
  void mouseDoubleClick(const juce::MouseEvent&) override;

  std::function<void(f32, f32, f32)> addClip;
  std::function<void(u32, f32, bool)> duplicateClip;
  std::function<void(f32, i32)> doZoom;
  std::function<void(f32)> doScroll;

  using Details = juce::DragAndDropTarget::SourceDetails;
  bool isInterestedInDragSource(const Details&) override;
  void itemDropped(const Details&) override;

  Grid* grid = nullptr;
  AutomationLane automationLane;
  juce::OwnedArray<ClipView> clipViews;

  struct Playhead {
    static constexpr f32 width = 1.25f;
    f32 x =  0;
  };
  
  Playhead playhead;

  struct Bounds {
    juce::Rectangle<i32> timeline;
    juce::Rectangle<i32> presetLaneTop;
    juce::Rectangle<i32> presetLaneBottom;
  };

  Bounds b;

  const juce::Font font { Fonts::sofiaProRegular.withHeight(12) };

  static constexpr i32 beatTextWidth = 40;
  static constexpr i32 beatTextHeight = 20;
  static constexpr i32 beatTextOffset = 4;
};

struct ToolBar : juce::Component {
  struct InfoButton : juce::Button {
    InfoButton();
    void paintButton(juce::Graphics&, bool, bool) override;
    void resized() override;

    juce::Rectangle<f32> ellipseBounds;
    juce::Rectangle<f32> iBounds;
    const juce::Font font { Fonts::sofiaProRegular.withHeight(kToolBarHeight / 3) };
  };

  struct KillButton : juce::Button {
    KillButton();
    void paintButton(juce::Graphics&, bool, bool) override;
  };

  ToolBar();
  void resized() override;
  void paint(juce::Graphics& g) override;

  InfoButton infoButton;
  Button editModeButton { "Edit Mode", Button::Type::toggle };
  Button discreteModeButton { "Discrete Mode", Button::Type::toggle };
  Button supportLinkButton { "Support", Button::Type::trigger };
  KillButton killButton;

  static constexpr i32 buttonWidth = 125;
  static constexpr i32 padding = 10;
  static constexpr i32 buttonPadding = 16;
};

struct DefaultView : juce::Component, juce::FileDragAndDropTarget {
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

    void paint(juce::Graphics&);
    void mouseMove(const juce::MouseEvent&);
    void mouseDown(const juce::MouseEvent&);
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&);
    void resized();
    void update(juce::Array<juce::PluginDescription>);
    void updateManufacturerFilter(const juce::String&);

    std::function<void(const juce::String&)> loadPlugin;
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

  PluginsPanel pluginsPanel;
  ManufacturersPanel manufacturersPanel { pluginsPanel};
};

struct ParameterView : juce::Component {
  struct Button : juce::ToggleButton {
    Button() {
      setTriggeredOnMouseDown(true);
    }

    void paintButton(juce::Graphics& g, bool highlighted, bool) {
      g.setColour(highlighted ? Colours::isabelline : Colours::frenchGray);
      g.fillRoundedRectangle(getLocalBounds().toFloat(), 2);
      g.setColour(Colours::eerieBlack);
      g.setFont(Fonts::sofiaProMedium.withHeight(9));

      if (getToggleState()) {
        g.drawText("ON", getLocalBounds(), juce::Justification::centred);
      } else {
        g.drawText("OFF", getLocalBounds(), juce::Justification::centred);
      }
    }
  };

  ParameterView();
  void resized() override;
  void paint(juce::Graphics& g) override;

  Dial dial;
  juce::Rectangle<i32> nameBounds;
  Button activeToggle;
  juce::String name;
  u32 id = 0;

  static constexpr i32 dialSize = 60;
  static constexpr i32 nameHeight = 20;
  static constexpr i32 buttonSize = 20;
  static constexpr i32 padding = 8;
  static constexpr i32 height = dialSize + nameHeight + buttonSize + 3 * padding;
};

struct MainView : juce::Component {
  MainView();
  void resized() override;
  void paint(juce::Graphics& g) override;
  void toggleInfoView();

  InfoView infoView;
  TrackView track;
  ToolBar toolBar;
};

struct InstanceWindow : juce::DocumentWindow {
  InstanceWindow(juce::AudioProcessorEditor* instance) : juce::DocumentWindow(instance->getName(), juce::Colours::black, 0) {
    setVisible(true);
    setContentNonOwned(instance, true);
    setUsingNativeTitleBar(true);
    setResizable(false, false);
    toFront(false);
  }
};

struct Editor : juce::AudioProcessorEditor, juce::DragAndDropContainer {
  explicit Editor(Plugin&);
  ~Editor() override;

  void paint(juce::Graphics&) override;
  void resized() override;

  bool keyPressed(const juce::KeyPress&) override;
  void modifierKeysChanged(const juce::ModifierKeys&) override;

  void focusGained(juce::Component::FocusChangeType) override {
    if (instanceWindow) {
      instanceWindow->toFront(false);
      toFront(true);
    }
  }

  Plugin& proc;
  StateManager& manager { proc.manager };

  std::unique_ptr<InstanceWindow> instanceWindow;
  MainView mainView;
  DefaultView defaultView { manager, proc.apfm, proc.knownPluginList };
};

} // namespace atmt
