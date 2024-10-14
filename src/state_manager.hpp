#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "types.hpp"
#include <span>
#include <assert.h>
#include <random>

namespace atmt {

static constexpr i32 kScrollSpeed = 500;
static constexpr f32 kZoomDeltaScale = 5;
static constexpr i32 kZoomSpeed = 2;

static constexpr i32 kToolBarHeight = 50;
static constexpr i32 kAutomationLaneHeight = 140;
static constexpr i32 kTimelineHeight = 16;
static constexpr i32 kPresetLaneHeight = 24;
static constexpr i32 kTrackHeight = kTimelineHeight + kPresetLaneHeight * 2 + kAutomationLaneHeight;
static constexpr i32 kTrackWidthRightPadding = 200;

static constexpr f32 kDefaultPathCurve = 0.5f;

static bool gShiftKeyPressed = false;
static bool gCmdKeyPressed = false;
static bool gOptKeyPressed = false;

struct ScopedProcLock {
  ScopedProcLock(juce::AudioProcessor& p) : proc(p) {
    proc.suspendProcessing(true);
  }

  ~ScopedProcLock() {
    proc.suspendProcessing(false);
  }

  juce::AudioProcessor& proc;
};

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

struct State {
  juce::String pluginID = "";
  std::atomic<bool> editMode = false;
  f32 zoom = 100;
  std::atomic<bool> modulateDiscrete = false;
  std::atomic<bool> captureParameterChanges = false;
  std::atomic<bool> releaseParameterChanges = false;
  std::atomic<f32>  randomSpread = 2;

  std::atomic<f32> playheadPosition = 0;
  std::atomic<f32> bpm = 120;
  std::atomic<u32> numerator = 4;
  std::atomic<u32> denominator = 4;

  std::vector<Clip> clips;
  std::vector<Path> paths;
  std::vector<AutomationPoint> points;
  std::vector<Parameter> parameters;
  juce::Path automation;
  Selection selection;
};

struct Plugin;
struct Engine;
struct TrackView;
struct AutomationLane;
struct ParametersView;

struct StateManager {
  StateManager(juce::AudioProcessor&);

  void init();
  void replace(const juce::ValueTree&);
  juce::ValueTree getState();

  void addClip(f32, f32, f32);
  void duplicateClip(u32, f32, bool);
  void duplicateClipDenorm(u32, f32, bool);
  void removeClip(u32);
  void moveClip(u32, f32, f32, f32);
  void moveClipDenorm(u32, f32, f32, f32);

  void randomiseParameters();
  bool shouldProcessParameter(Parameter*);

  u32 addPath(f32, f32, f32);
  u32 addPathDenorm(f32, f32, f32);
  void removePath(u32);
  void movePath(u32, f32, f32, f32);
  void movePathDenorm(u32, f32, f32, f32);

  void setSelection(f32, f32);
  void setSelectionDenorm(f32, f32);
  void removeSelection();

  void setPlayheadPosition(f32);
  void setPlayheadPositionDenorm(f32);

  bool setPluginID(const juce::String&);
  void setZoom(f32);
  void setEditMode(bool);
  void setModulateDiscrete(bool);

  void setAllParametersActive(bool);
  void setParameterActive(Parameter*, bool);

  void clear();

  auto findAutomationPoint(f32);
  auto findAutomationPointDenorm(f32);

  void updateTrack();
  void updateParametersView();
  void updateDebugView();

  void registerAutomationLaneView(AutomationLane* view) {
    automationLaneView = view;
  }
  
  void deregisterAutomationLaneView() {
    automationLaneView = nullptr;
  }

  void registerTrackView(TrackView* view) {
    trackView = view;
  }

  void deregisterTrackView() {
    trackView = nullptr;  
  }

  void registerMainView() {
    updateTrack();
  }

  juce::AudioProcessor& proc;
  Plugin* plugin = nullptr;
  Engine* engine = nullptr;

  TrackView* trackView = nullptr;
  AutomationLane* automationLaneView = nullptr;

  ParametersView* parametersView = nullptr;
  juce::Component* debugView = nullptr;

  State state;
};

} // namespace atmt 
