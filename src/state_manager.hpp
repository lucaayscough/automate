#pragma once

#include <assert.h>
#include <juce_data_structures/juce_data_structures.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <span>
#include "types.hpp"

namespace atmt {

juce::String pluginID = "";
f64 zoom = 100;
std::atomic<bool> editMode = false;
std::atomic<bool> modulateDiscrete = false;

struct Preset {
  juce::String name;
  std::vector<f32> parameters;
};

struct Path {
  f64 x = 0;
  f64 y = 0;
  f64 c = 0.5;
};

struct Clip {
  Preset* preset; 
  juce::String name;
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

struct StateManager {
  StateManager(juce::AudioProcessor&);

  void replace(const juce::ValueTree&);
  juce::ValueTree getState();

  void addClip(Preset*, f64, f64);
  void removeClip(Clip* c);
  void moveClip(Clip*, f64, f64, f64);

  void savePreset(const juce::String&);
  void removePreset(Preset*);
  void overwritePreset(Preset*);
  void loadPreset(Preset*);

  // TODO(luca): remove this
  bool doesPresetNameExist(const juce::String&);

  void addPath(f64, f64);
  void removePath(Path*);
  void movePath(Path*, f64, f64, f64);

  void setPluginID(const juce::String&);
  void setZoom(f64);
  void setEditMode(bool);
  void setModulateDiscrete(bool);

  void clear();

  auto findAutomationPoint(f64);

  void updateAutomation();
  void updateTrack();
  void updatePresetList();

  static juce::String valueTreeToXmlString(const juce::ValueTree&);

  juce::AudioProcessor& proc;

  juce::Component* automationView = nullptr;
  juce::Component* trackView = nullptr;
  juce::Component* presetView = nullptr;
  juce::Component* debugView = nullptr;

  std::vector<Preset> presets;
  std::vector<Clip> clips;
  std::vector<Path> paths;
  std::vector<AutomationPoint> points;

  juce::Path automation;

  juce::Random rand;
};

} // namespace atmt 
