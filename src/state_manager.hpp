#pragma once

#include "identifiers.hpp"
#include <juce_data_structures/juce_data_structures.h>
#include <juce_audio_processors/juce_audio_processors.h>

namespace atmt {

struct StateManager : juce::ValueTree::Listener
{
  StateManager(juce::AudioProcessorValueTreeState&);

  void init();
  void replace(const juce::ValueTree&);
  void validate();

  void addClip(const juce::String&, int);
  void removeClipsIfInvalid(const juce::var&);
  void savePreset(const juce::String& name);
  void removePreset(const juce::String& name);
  bool doesPresetNameExist(const juce::String&);

  void valueTreeChildRemoved(juce::ValueTree&, juce::ValueTree&, int);

  static juce::String valueTreeToXmlString(const juce::ValueTree&);

  juce::AudioProcessorValueTreeState& apvts;
  juce::UndoManager* undoManager;
  juce::ValueTree state;
  juce::ValueTree undoable;
  juce::ValueTree edit;
  juce::ValueTree presets;

  static constexpr int defaultClipLength = 1000;
  static constexpr float defaultZoomValue = 0.2f;
};

struct Preset {
  Preset(juce::ValueTree& t, juce::UndoManager* um) : state(t), undoManager(um) {
    jassert(t.isValid());
    jassert(t.hasType(ID::PRESET));
  }

  juce::ValueTree state;
  juce::UndoManager* undoManager;

  juce::CachedValue<juce::String> name { state, ID::name, undoManager };
  std::vector<float> parameters;
};

struct Clip {
  Clip(juce::ValueTree& t, juce::UndoManager* um) : state(t), undoManager(um) {
    jassert(t.isValid());
    jassert(t.hasType(ID::CLIP));
  }

  juce::ValueTree state;
  juce::UndoManager* undoManager;

  juce::CachedValue<juce::String> name { state, ID::name, undoManager };
  juce::CachedValue<int> start { state, ID::start, undoManager };
  juce::CachedValue<int> end { state, ID::end, undoManager };
};

} // namespace atmt 
