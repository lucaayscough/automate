#pragma once

#include "grid.hpp"
#include <juce_data_structures/juce_data_structures.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "types.hpp"
#include <span>
#include <assert.h>

namespace atmt {

static constexpr i32 kScrollSpeed = 500;
static constexpr f32 kZoomDeltaScale = 5;
static constexpr i32 kZoomSpeed = 2;

static constexpr i32 kParametersViewHeight = 300;
static constexpr i32 kToolBarHeight = 50;
static constexpr i32 kAutomationLaneHeight = 140;
static constexpr i32 kTimelineHeight = 16;
static constexpr i32 kPresetLaneHeight = 24;
static constexpr i32 kTrackHeight = kTimelineHeight + kPresetLaneHeight * 2 + kAutomationLaneHeight;
static constexpr i32 kTrackWidthRightPadding = 200;
static constexpr i32 kWidth = 600;
static constexpr i32 kHeight = kTrackHeight + kToolBarHeight + kParametersViewHeight;

static constexpr f32 kDefaultPathCurve = 0.5f;
static constexpr i32 kDefaultViewWidth = 600;
static constexpr i32 kDefaultViewHeight = 600;

static bool gShiftKeyPressed = false;
static bool gCmdKeyPressed = false;
static bool gOptKeyPressed = false;

struct Path {
  f32 x = 0;
  f32 y = 0;
  f32 c = 0.5;
};

struct Clip {
  f32 x = 0;
  f32 y = 0;
  f32 c = 0.5;
  std::vector<f32> parameters; 
};

struct AutomationPoint {
  f32 x = 0;
  f32 y = 0;
  f32 c = 0.5;
  u32 id = 0;
  Clip* clip = nullptr;
  Path* path = nullptr;
};

struct Parameter {
  juce::AudioProcessorParameter* parameter = nullptr;
  bool active = true;
};

struct Selection {
  f32 start = 0;
  f32 end = 0;
};

struct LerpPair {
  u32 a; 
  u32 b;
  f32 start;
  f32 end;
  bool interpolate;
  std::vector<bool> parameters;
};

struct UIParameterSync {
  static constexpr bool EngineUpdate = false;
  static constexpr bool UIUpdate = true;

  std::vector<f32> values;
  std::vector<bool> updates;

  std::atomic<bool> mode = EngineUpdate;
};

struct Plugin;
struct Engine;
struct Editor;
struct TrackView;
struct AutomationLane;
struct ParametersView;
struct ToolBar;

struct StateManager : juce::AudioProcessorParameter::Listener, juce::Timer {
  StateManager(juce::AudioProcessor&);

  void addClip(f32, f32, f32);
  void addClipDenorm(f32, f32, f32);
  void duplicateClip(u32, f32, bool);
  void duplicateClipDenorm(u32, f32, bool);
  void moveClip(u32, f32, f32, f32);
  void moveClipDenorm(u32, f32, f32, f32);
  void selectClip(i32);
  void removeClip(u32);

  u32 addPath(f32, f32, f32);
  u32 addPathDenorm(f32, f32, f32);
  void movePath(u32, f32, f32, f32);
  void movePathDenorm(u32, f32, f32, f32);
  void removePath(u32);

  void bendAutomation(f32, f32);
  void bendAutomationDenorm(f32, f32);
  void flattenAutomationCurve(f32);
  void flattenAutomationCurveDenorm(f32);
  void dragAutomationSection(f32, f32);
  void dragAutomationSectionDenorm(f32, f32);

  i32 findAutomationPoint(f32);
  i32 findAutomationPointDenorm(f32);

  void doZoom(f32, i32);
  void doScroll(f32);
  void setEditMode(bool);
  void setDiscreteMode(bool);

  void setSelection(f32, f32);
  void setSelectionDenorm(f32, f32);
  void removeSelection();
  void setPlayheadPosition(f32);
  void setPlayheadPositionDenorm(f32);

  // NOTE(luca): Parameter operations
  bool shouldProcessParameter(u32);
  void randomiseParameters();
  void setAllParametersActive(bool);
  void setParameterActive(u32, bool);

  void parameterValueChanged(i32, f32) override;
  void parameterGestureChanged(i32, bool) override;

  void updateTrackWidth();
  void updateLerpPairs();
  void updateAutomation();
  void updateAutomationView();
  void updateTrackView();
  void updateGrid();
  void updateTrack();
  void updateToolBarView();

  void showDefaultView();
  void showMainView();
  bool loadPlugin(const juce::String&);

  void registerEditor(Editor*);
  void deregisterEditor(Editor*);

  void init();
  void replace(const juce::ValueTree&);
  juce::ValueTree getState();

  void timerCallback() override;

  juce::AudioProcessor& proc;
  Plugin* plugin = nullptr;
  Engine* engine = nullptr;
  Editor* editor = nullptr;
  TrackView* trackView = nullptr;
  AutomationLane* automationView = nullptr;
  ParametersView* parametersView = nullptr;
  ToolBar* toolBarView = nullptr;

  juce::String pluginID = "";
  std::atomic<bool> editMode = false;
  std::atomic<bool> discreteMode = false;
  std::atomic<bool> captureParameterChanges = false;
  std::atomic<bool> releaseParameterChanges = false;
  std::atomic<f32>  randomSpread = 2;
  f32 zoom = 100;

  std::atomic<f32> playheadPosition = 0;
  std::atomic<f32> bpm = 120;
  std::atomic<u32> numerator = 4;
  std::atomic<u32> denominator = 4;
  std::vector<Parameter> parameters;
  Grid grid;

  // NOTE(luca): track
  std::vector<Clip> clips;
  std::vector<Path> paths;
  std::vector<AutomationPoint> points;
  juce::Path automation;
  Selection selection;
  i32 selectedClipID = NONE;
  i32 viewportDeltaX = 0;
  i32 trackWidth = 0;

  UIParameterSync uiParameterSync;

  std::unique_ptr<juce::AudioPluginInstance> instance;
  std::unique_ptr<juce::AudioProcessorEditor> instanceEditor;
};

} // namespace atmt 
