#pragma once

#include "identifiers.hpp"
#include <juce_data_structures/juce_data_structures.h>
#include <juce_audio_processors/juce_audio_processors.h>

template<>
struct juce::VariantConverter<juce::Path> {
  static juce::Path fromVar(const var& v) {
    juce::Path p;
    p.restoreFromString(v.toString());
    return p;
  }
  
  static juce::var toVar(const juce::Path& p) {
    return { p.toString() };
  }
};

namespace atmt {

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

  juce::CachedValue<juce::String> name { state, ID::name,  undoManager };
  juce::CachedValue<double> start      { state, ID::start, undoManager };
  juce::CachedValue<bool>   top        { state, ID::top,   undoManager };
};

struct StateManager : juce::ValueTree::Listener
{
  StateManager(juce::AudioProcessorValueTreeState&);

  void init();
  void replace(const juce::ValueTree&);
  juce::ValueTree getState();
  void validate();

  void addClip(const juce::String&, double, bool);
  void removeClip(const juce::ValueTree& vt);
  void removeClipsIfInvalid(const juce::var&);
  void savePreset(const juce::String& name);
  void removePreset(const juce::String& name);
  bool doesPresetNameExist(const juce::String&);
  void updateAutomation();

  void valueTreeChildAdded(juce::ValueTree&, juce::ValueTree&) override;
  void valueTreeChildRemoved(juce::ValueTree&, juce::ValueTree&, int) override;
  void valueTreePropertyChanged(juce::ValueTree&, const juce::Identifier&) override;

  static juce::String valueTreeToXmlString(const juce::ValueTree&);

  juce::AudioProcessorValueTreeState& apvts;
  juce::UndoManager* undoManager;

  juce::ValueTree state         { ID::AUTOMATE };
  juce::ValueTree parameters    { apvts.state  };
  juce::ValueTree edit          { ID::EDIT     };
  juce::ValueTree presets       { ID::PRESETS  };

  std::vector<std::unique_ptr<Clip>> clips;

  static constexpr double defaultZoomValue = 100;
};

} // namespace atmt 
