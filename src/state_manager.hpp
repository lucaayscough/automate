#pragma once

#include <assert.h>
#include <juce_data_structures/juce_data_structures.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <span>
#include "types.hpp"
#include <random>

namespace atmt {

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
  std::vector<f32> parameters; 
  f32 x = 0;
  f32 y = 0;
  f32 c = 0.5;
};

struct AutomationPoint {
  f32 x = 0;
  f32 y = 0;
  f32 c = 0.5;
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

struct Plugin;
struct Engine;
struct Track;
struct ParametersView;

struct StateManager {
  StateManager(juce::AudioProcessor&);

  void init();
  void replace(const juce::ValueTree&);
  juce::ValueTree getState();

  void addClip(f32, f32);
  void removeClip(Clip* c);
  void moveClip(Clip*, f32, f32, f32);

  void randomiseParameters();
  bool shouldProcessParameter(Parameter*);

  void addPath(f32, f32);
  void removePath(Path*);
  void movePath(Path*, f32, f32, f32);
  void removeSelection(Selection selection);

  void setPluginID(const juce::String&);
  void setZoom(f32);
  void setEditMode(bool);
  void setModulateDiscrete(bool);

  void setAllParametersActive(bool);
  void setParameterActive(Parameter*, bool);

  void clear();

  auto findAutomationPoint(f32);

  void updateParametersView();
  void updateAutomation();
  void updateTrack();
  void updateDebugView();

  juce::AudioProcessor& proc;
  Plugin* plugin = nullptr;
  Engine* engine = nullptr;

  Track* trackView = nullptr;
  ParametersView* parametersView = nullptr;
  juce::Component* automationView = nullptr;
  juce::Component* debugView = nullptr;

  std::vector<Clip> clips;
  std::vector<Path> paths;
  std::vector<AutomationPoint> points;
  std::vector<Parameter> parameters;

  juce::Path automation;
};

} // namespace atmt 
