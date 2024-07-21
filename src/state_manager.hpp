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

template<>
struct juce::VariantConverter<std::vector<float>> {
  static std::vector<float> fromVar(const var& v) {
    auto mb = v.getBinaryData();
    auto len = std::size_t(mb->getSize());
    auto data = (float*)mb->getData();
    std::vector<float> vec; 
    vec.resize(len / sizeof(float));
    std::memcpy(vec.data(), data, len);
    return vec;
  }
  
  static juce::var toVar(const std::vector<float>& vec) {
    return { vec.data(), vec.size() * sizeof(float) };
  }
};

namespace atmt {

struct TreeWrapper {
  TreeWrapper(juce::ValueTree& v, juce::UndoManager* um) : state(v), undoManager(um) {
    jassert(v.isValid());
  }

  juce::ValueTree state;
  juce::UndoManager* undoManager = nullptr;
};

struct Preset : TreeWrapper {
  Preset(juce::ValueTree& v, juce::UndoManager* um) : TreeWrapper(v, um) {
    jassert(v.hasType(ID::PRESET));
  }

  juce::CachedValue<juce::String> name { state, ID::name, undoManager };
  juce::CachedValue<std::vector<float>> parameters { state, ID::parameters, undoManager };
};

struct Clip : TreeWrapper {
  Clip(juce::ValueTree& v, juce::UndoManager* um) : TreeWrapper(v, um) {
    jassert(v.hasType(ID::CLIP));
  }

  juce::CachedValue<juce::String> name { state, ID::name, undoManager };
  juce::CachedValue<double> start { state, ID::start, undoManager };
  juce::CachedValue<bool> top { state, ID::top, undoManager };
};

template<typename T>
struct ObjectList : TreeWrapper, juce::ValueTree::Listener {
  ObjectList(juce::ValueTree& v, juce::UndoManager* um, juce::AudioProcessor* p=nullptr) : TreeWrapper(v, um), proc(p) {
    rebuild();
    state.addListener(this);
  }

  virtual bool isType(const juce::ValueTree&) = 0;
  virtual void sort() {}
  
  void rebuild() {
    if (proc) proc->suspendProcessing(true);

    objects.clear();
    for (auto child : state) {
      if (isType(child)) {
        objects.emplace_back(std::make_unique<T>(child, undoManager)); 
      }
    }
    sort();

    if (proc) proc->suspendProcessing(false);
  }

  void valueTreeChildAdded(juce::ValueTree&, juce::ValueTree&)              override { rebuild(); }
  void valueTreeChildRemoved(juce::ValueTree&, juce::ValueTree&, int)       override { rebuild(); }
  void valueTreePropertyChanged(juce::ValueTree&, const juce::Identifier&)  override { rebuild(); }

  auto begin() { return objects.begin(); }
  auto end() { return objects.end(); }

  auto begin() const { return objects.begin(); }
  auto end() const { return objects.end(); }

  bool empty() { return objects.empty(); }
  auto& front() { return objects.front(); }
   
  juce::AudioProcessor* proc = nullptr;
  std::vector<std::unique_ptr<T>> objects;
};

#define OBJECT_LIST_INIT (juce::ValueTree& v, juce::UndoManager* um, juce::AudioProcessor* p=nullptr) : ObjectList(v, um, p)

struct Clips : ObjectList<Clip> {
  Clips OBJECT_LIST_INIT {
    v.hasType(ID::EDIT);
  }

  bool isType(const juce::ValueTree& v) override { return v.hasType(ID::CLIP); }

  void sort() override {
    auto cmp = [] (std::unique_ptr<Clip>& a, std::unique_ptr<Clip>& b) { return a->start < b->start; };
    std::sort(objects.begin(), objects.end(), cmp);
  }
};

struct Presets : ObjectList<Preset> {
  Presets OBJECT_LIST_INIT { v.hasType(ID::PRESETS); }

  bool isType(const juce::ValueTree& v) override { return v.hasType(ID::PRESET); }

  Preset* getPresetForClip(Clip* clip) {
    for (auto& preset : objects) {
      if (preset->name == clip->name) {
        return preset.get();
      }
    }
    jassertfalse;
    return nullptr;
  }

  Preset* getPresetFromName(const juce::String& name) {
      auto cond = [name] (std::unique_ptr<Preset>& other) { return other->name == name; };
      auto it = std::find_if(begin(), end(), cond); 
      return it != end() ? it->get() : nullptr;
  }
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
