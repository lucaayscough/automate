#pragma once

#include "change_attachment.hpp"
#include "plugin.hpp"
#include <numbers>

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
  static const juce::Colour outerSpace;
  static const juce::Colour atomicTangerine;
};

struct Fonts {
  static const juce::FontOptions sofiaProRegular;
  static const juce::FontOptions sofiaProMedium;
};

struct Style {
  static constexpr f32 lineThickness = 1.5f; 
  static constexpr f32 lineThicknessHighlighted = 2.25f;
  static constexpr i32 minWidth = 600;
};

struct Constants {
  static constexpr i32 kScrollSpeed = 500;
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

struct Dial : juce::Slider {
  Dial(const Parameter*);
  void paint(juce::Graphics& g) override;
  void resized() override;
  void mouseDown(const juce::MouseEvent& e) override;
  void mouseUp(const juce::MouseEvent& e) override;

  static constexpr f32 pi = f32(std::numbers::pi);
  static constexpr f32 tau = 2 * pi;
  static constexpr f32 offset = pi + pi * 0.25f;
  static constexpr f32 dotSize = 5;
  static constexpr f32 dotOffset = dotSize * 0.5f;

  const Parameter* parameter;
};

struct InfoView : juce::Component {
  struct Command {
    const char* name;      
    const char* binding;
  };

  static constexpr i32 commandHeight = 24;
  static constexpr Command commands[] =
  {  
    { "Enable all parameters",      "Command + E" },
    { "Disable all parameters",     "Command + D" },
    { "Capture parameter",          "Command + Click" },
    { "Release parameter",          "Command + Shift + Click " },
    { "Randomise parameters",       "R" },
    { "Kill instance",              "K" },
    { "Toggle parameters view",     "V" },
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
  const juce::Font font { Fonts::sofiaProRegular.withHeight(16) };
};

struct Grid {
  struct Beat {
    u32 bar = 1;
    u32 beat = 1;
    f32 x = 0;
  };

  struct TimeSignature {
    u32 numerator = 4;
    u32 denominator = 4;
  };

  bool reset(f32, f32, TimeSignature);
  void reset();
  f32 snap(f32);

  void narrow();
  void widen();
  void triplet();
  void toggleSnap();

  TimeSignature ts;
  f32 maxWidth = 0;

  static constexpr f32 intervalMin = 40;
  f32 snapInterval = 0;
  bool tripletMode = false;
  i32 gridWidth = 0;
  bool snapOn = true;
  f32 zoom = 0;

  std::vector<Beat> beats;
  std::vector<f32> lines;
};

struct PathView : juce::Component {
  PathView(Grid&);
  void paint(juce::Graphics&) override;
  void mouseDrag(const juce::MouseEvent&) override;
  void mouseDoubleClick(const juce::MouseEvent&) override;

  std::function<void(u32, f32, f32)> move;
  std::function<void(u32)> remove;

  Grid& grid;
  u32 id = 0;
  f32 zoom = 1;

  static constexpr i32 size = 20;
  static constexpr i32 posOffset = size / 2;
};

struct ClipView : juce::Component {
  ClipView(Grid&);
  void paint(juce::Graphics&) override;
  void mouseDown(const juce::MouseEvent&) override;
  void mouseDoubleClick(const juce::MouseEvent&) override;
  void mouseUp(const juce::MouseEvent&) override;
  void mouseDrag(const juce::MouseEvent&) override;

  std::function<void(u32, f32, f32)> move;
  std::function<void(u32)> remove;

  u32 id = 0;
  bool optKeyPressed = false;
  Grid& grid;

  static constexpr i32 trimThreshold = 20;
  bool isTrimDrag = false;
  bool isLeftTrimDrag = false;
  f32 mouseDownOffset = 0;
};

struct AutomationLane : juce::Component {
  enum class GestureType { none, bend, drag, select, addPath };

  AutomationLane(StateManager&, Grid&);
  ~AutomationLane() override;
  void paint(juce::Graphics&) override;
  
  auto getAutomationPoint(juce::Point<f32>);
  f32 getDistanceFromPoint(juce::Point<f32>);

  void mouseMove(const juce::MouseEvent&) override;
  void mouseDown(const juce::MouseEvent&) override;
  void mouseUp(const juce::MouseEvent&) override;
  void mouseExit(const juce::MouseEvent&) override;
  void mouseDrag(const juce::MouseEvent&) override;
  void mouseDoubleClick(const juce::MouseEvent&) override;

  void update(const std::vector<Path>&, juce::Path, f32 zoom);

  StateManager& manager;
  Grid& grid;
  juce::Path automation;
  juce::OwnedArray<PathView> pathViews;

  juce::Rectangle<f32> hoverBounds;
  f32 xHighlightedSegment = -1;
  static constexpr i32 mouseIntersectDistance = 5;
  static constexpr i32 mouseOverDistance = 15;
  bool optKeyPressed = false;
  juce::Point<i32> lastMouseDragOffset;

  f32 kDragIncrement = 100;
  f32 bendMouseDistanceProportion = 0;

  GestureType activeGesture = GestureType::none;
  Selection selection;
  u32 lastPathAddedID = 0; 

  static constexpr i32 height = 180;
  static constexpr f32 lineThickness = 2;
};

struct Track : juce::Component, juce::Timer, juce::DragAndDropContainer, juce::DragAndDropTarget {
  Track(StateManager&);
  ~Track() override;
  void paint(juce::Graphics&) override;
  void resized() override;
  void timerCallback() override;
  void resetGrid();
  i32 getTrackWidth();
  void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
  void mouseDoubleClick(const juce::MouseEvent&) override;

  void zoomTrack(f32);
  void scroll(f32);

  void update(const std::vector<Clip>&, f32);

  using Details = juce::DragAndDropTarget::SourceDetails;
  bool isInterestedInDragSource(const Details&) override;
  void itemDropped(const Details&) override;

  StateManager& manager;

  Grid grid;
  juce::OwnedArray<ClipView> clipViews;

  AutomationLane automationLane { manager, grid };
  static constexpr f32 zoomDeltaScale = 5;

  struct Playhead {
    static constexpr f32 width = 1.25f;
    f32 x =  0;
  };
  
  Playhead playhead;

  f32 zoom = 0;

  static constexpr i32 timelineHeight = 20;
  static constexpr i32 presetLaneHeight = 25;
  static constexpr i32 height = timelineHeight + presetLaneHeight * 2 + AutomationLane::height;
  static constexpr i32 rightPadding = 200;

  struct Bounds {
    juce::Rectangle<i32> timeline;
    juce::Rectangle<i32> presetLaneTop;
    juce::Rectangle<i32> presetLaneBottom;
  };

  Bounds b;

  bool shiftKeyPressed = false;
  bool cmdKeyPressed = false;
  static constexpr i32 kZoomSpeed = 2;
  f32 viewportDeltaX = 0;
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
    const juce::Font font { Fonts::sofiaProMedium.withHeight(24) };
  };

  struct KillButton : juce::Button {
    KillButton();
    void paintButton(juce::Graphics&, bool, bool) override;
  };

  ToolBar(StateManager&);
  ~ToolBar() override;
  void resized() override;
  void paint(juce::Graphics& g) override;

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

    void paint(juce::Graphics&);
    void mouseMove(const juce::MouseEvent&);
    void mouseDown(const juce::MouseEvent&);
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&);
    void resized();
    void update(juce::Array<juce::PluginDescription>);
    void updateManufacturerFilter(const juce::String&);

    std::function<void(const juce::String&)> setPluginID;
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

struct ParametersView : juce::Component {
  struct ParameterView : juce::Component, juce::AudioProcessorParameter::Listener {
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

    ParameterView(StateManager&, Parameter*);
    ~ParameterView() override;
    void resized() override;
    void paint(juce::Graphics& g) override;
    void update();
    void parameterValueChanged(i32, f32) override;
    void parameterGestureChanged(i32, bool) override;

    StateManager& manager;
    Parameter* parameter = nullptr;

    Dial dial { parameter };
    juce::Rectangle<i32> nameBounds;
    Button activeToggle;

    static constexpr i32 dialSize = 60;
    static constexpr i32 nameHeight = 20;
    static constexpr i32 buttonSize = 20;
    static constexpr i32 padding = 8;
    static constexpr i32 height = dialSize + nameHeight + buttonSize + 3 * padding;
  };

  ParametersView(StateManager&);
  ~ParametersView() override;
  void resized() override;
  void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
  void updateParameters();

  StateManager& manager;
  juce::OwnedArray<ParameterView> parameters;
  static constexpr i32 padding = 10;
  i32 viewportHeight = 0;
};

struct MainView : juce::Component {
  MainView(StateManager&, juce::AudioProcessorEditor*);
  void resized() override;
  void paint(juce::Graphics& g) override;
  void toggleParametersView();
  void toggleInfoView();
  void childBoundsChanged(juce::Component*) override;

  StateManager& manager;
  InfoView infoView;
  Track track { manager };
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
  Engine& engine { proc.engine };

  bool useMainView = false;

  std::unique_ptr<MainView> mainView;
  DefaultView defaultView { manager, proc.apfm, proc.knownPluginList };

  ChangeAttachment createInstanceAttachment { proc.engine.createInstanceBroadcaster, CHANGE_CB(createInstanceChangeCallback) };
  ChangeAttachment killInstanceAttachment   { proc.engine.killInstanceBroadcaster, CHANGE_CB(killInstanceChangeCallback) };
};

} // namespace atmt
