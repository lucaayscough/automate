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

struct TreeWrapper {};

struct Preset : TreeWrapper {
  Preset(juce::ValueTree& t, juce::UndoManager* um) : state(t), undoManager(um) {
    jassert(t.isValid());
    jassert(t.hasType(ID::PRESET));
  }

  juce::ValueTree state;
  juce::UndoManager* undoManager;

  juce::CachedValue<juce::String> name { state, ID::name, undoManager };
  std::vector<float> parameters;
};

struct Clip : TreeWrapper {
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

struct Clips : juce::ValueTree::Listener {
  Clips(juce::ValueTree& t, juce::UndoManager* um)
    : state(t), undoManager(um) {
    t.hasType(ID::EDIT);
    state.addListener(this);
  }

  void rebuild() {
    clips.clear();
    for (auto child : state) {
      if (child.hasType(ID::CLIP)) {
        clips.emplace_back(std::make_unique<Clip>(child, undoManager)); 
      }
    }
    auto cmp = [] (const std::unique_ptr<Clip>& a, const std::unique_ptr<Clip>& b) { return a->start < b->start; };
    std::sort(clips.begin(), clips.end(), cmp);
  }
  
  void valueTreeChildAdded(juce::ValueTree&, juce::ValueTree&) {
    rebuild();
  }

  void valueTreeChildRemoved(juce::ValueTree&, juce::ValueTree&, int) {
    rebuild();
  }
   
  juce::ValueTree state;
  juce::UndoManager* undoManager;
  std::vector<std::unique_ptr<Clip>> clips;
};

struct StateManager
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

  static juce::String valueTreeToXmlString(const juce::ValueTree&);

  juce::AudioProcessorValueTreeState& apvts;
  juce::UndoManager* undoManager { apvts.undoManager };

  juce::ValueTree state         { ID::AUTOMATE };
  juce::ValueTree parameters    { apvts.state  };
  juce::ValueTree edit          { ID::EDIT     };
  juce::ValueTree presets       { ID::PRESETS  };
 
  Clips clips { edit, undoManager };

  static constexpr double defaultZoomValue = 100;
};

} // namespace atmt 
