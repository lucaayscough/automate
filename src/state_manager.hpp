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

  void addEditClip(const juce::String&, int);
  void removeEditClipsIfInvalid(const juce::var&);
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

} // namespace atmt 
