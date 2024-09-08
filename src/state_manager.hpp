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
  f64 x = 0;
  f64 y = 0;
  f64 c = 0.5;
};

struct Clip {
  std::vector<f32> parameters; 
  f64 x = 0;
  f64 y = 0;
  f64 c = 0.5;
};

struct AutomationPoint {
  f64 x = 0;
  f64 y = 0;
  f64 c = 0.5;
  Clip* clip = nullptr;
  Path* path = nullptr;
};

struct Parameter {
  juce::AudioProcessorParameter* parameter = nullptr;
  bool active = true;
};

struct Selection {
  f64 start = 0;
  f64 end = 0;
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

  void addClip(f64, f64);
  void removeClip(Clip* c);
  void moveClip(Clip*, f64, f64, f64);

  void randomiseParameters();
  bool shouldProcessParameter(Parameter*);

  void addPath(f64, f64);
  void removePath(Path*);
  void movePath(Path*, f64, f64, f64);
  void removeSelection(Selection selection);

  void setPluginID(const juce::String&);
  void setZoom(f64);
  void setEditMode(bool);
  void setModulateDiscrete(bool);

  void setCaptureParameterChanges(bool);
  void setParameterActive(Parameter*, bool);

  void clear();

  auto findAutomationPoint(f64);

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
