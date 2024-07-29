#pragma once

#include "identifiers.hpp"
#include <juce_data_structures/juce_data_structures.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <span>

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

struct TreeWrapper : juce::ValueTree::Listener {
  TreeWrapper(juce::ValueTree& v, juce::UndoManager* um) : state(v), undoManager(um) {
    jassert(v.isValid());
    state.addListener(this);
  }

  virtual void rebuild() {}

  void valueTreeChildAdded(juce::ValueTree&, juce::ValueTree&)              override { rebuild(); }
  void valueTreeChildRemoved(juce::ValueTree&, juce::ValueTree&, int)       override { rebuild(); }
  void valueTreePropertyChanged(juce::ValueTree&, const juce::Identifier&)  override { rebuild(); }

  juce::ValueTree state;
  juce::UndoManager* undoManager = nullptr;
};

struct Preset : TreeWrapper {
  Preset(juce::ValueTree& v, juce::UndoManager* um) : TreeWrapper(v, um) {
    jassert(v.hasType(ID::PRESET));
    rebuild();
  }

  void rebuild() override {
    _id = std::int64_t(state[ID::id]);

    {
      auto mb = state[ID::parameters].getBinaryData();
      std::span<float> p { (float*)mb->getData(), mb->getSize() / sizeof(float) };
      for (std::size_t i = 0; i < p.size(); ++i) {
        if (i < _numParameters) {
          _parameters[i] = p[i]; 
        } else {
          _parameters.emplace_back(p[i]);
        }
      }
      _numParameters = p.size();
    }
  }

  juce::CachedValue<juce::String> name { state, ID::name, undoManager };

  std::atomic<std::int64_t> _id = 0; 
  std::deque<std::atomic<float>> _parameters;
  std::atomic<std::size_t> _numParameters = 0;
};

struct Clip : TreeWrapper {
  Clip(juce::ValueTree& v, juce::UndoManager* um) : TreeWrapper(v, um) {
    jassert(v.hasType(ID::CLIP));
    rebuild();
  }

  void rebuild() override {
    _id = std::int64_t(state[ID::id]);
    _start = double(state[ID::start]);
    _top = bool(state[ID::top]);
  }

  juce::CachedValue<juce::String> name { state, ID::name, undoManager };
  juce::CachedValue<double> start { state, ID::start, undoManager };
  juce::CachedValue<bool> top { state, ID::top, undoManager };

  std::atomic<std::int64_t> _id = 0;
  std::atomic<double> _start = 0;
  std::atomic<bool> _top = false;
};

template<typename T>
struct ObjectList : TreeWrapper {
  ObjectList(juce::ValueTree& v, juce::UndoManager* um, juce::AudioProcessor* p=nullptr) : TreeWrapper(v, um), proc(p) {}

  virtual bool isType(const juce::ValueTree&) = 0;
  virtual void sort() {}
  
  void rebuild() override {
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

  auto begin()        { return objects.begin(); }
  auto end()          { return objects.end();   }
  auto begin() const  { return objects.begin(); }
  auto end()   const  { return objects.end();   }
  std::size_t size()  { return objects.size();  }
  bool empty()        { return objects.empty(); }
  auto& front()       { return objects.front(); }
   
  juce::AudioProcessor* proc = nullptr;
  std::vector<std::unique_ptr<T>> objects;
};

#define OBJECT_LIST_INIT (juce::ValueTree& v, juce::UndoManager* um, juce::AudioProcessor* p=nullptr) : ObjectList(v, um, p)

struct Clips : ObjectList<Clip> {
  Clips OBJECT_LIST_INIT { 
    jassert(v.hasType(ID::EDIT));
    rebuild();
  }

  bool isType(const juce::ValueTree& v) override { return v.hasType(ID::CLIP); }

  void sort() override {
    auto cmp = [] (std::unique_ptr<Clip>& a, std::unique_ptr<Clip>& b) { return a->start < b->start; };
    std::sort(objects.begin(), objects.end(), cmp);
  }
};

struct Presets : ObjectList<Preset> {
  Presets OBJECT_LIST_INIT {
    jassert(v.hasType(ID::PRESETS));
    rebuild();
  }

  bool isType(const juce::ValueTree& v) override { return v.hasType(ID::PRESET); }

  Preset* getPresetForClip(Clip* clip) {
    auto cond = [clip] (const std::unique_ptr<Preset>& p) { return p->_id == clip->_id; };
    auto it = std::find_if(begin(), end(), cond);
    return it != end() ? it->get() : nullptr;
  }

  Preset* getPresetFromName(const juce::String& name) {
    auto cond = [name] (std::unique_ptr<Preset>& other) { return other->name == name; };
    auto it = std::find_if(begin(), end(), cond); 
    return it != end() ? it->get() : nullptr;
  }
};

struct Automation : TreeWrapper {
  Automation(juce::ValueTree& v, juce::UndoManager* um, juce::AudioProcessor* p=nullptr) : TreeWrapper(v, um), proc(p) {
    jassert(v.hasType(ID::EDIT));
    rebuild();
  }
  
  double getLerpPos(double time) {
    return getPointFromXIntersection(time).y * kExpand;
  }

  juce::Point<float> getPointFromXIntersection(double x) {
    return automation.getPointAlongPath(float(x), juce::AffineTransform::scale(1, kFlat));
  }

  void rebuild() override {
    if (proc) proc->suspendProcessing(true);

    automation.clear();

    Clips clips { state, undoManager };

    for (const auto& clip : clips) {
      automation.lineTo(float(clip->start), clip->top ? 0 : 1);
    }

    if (proc) proc->suspendProcessing(false);
  }

  juce::Path& get() { return automation; }

  juce::AudioProcessor* proc = nullptr;
  juce::Path automation;
  static constexpr float kFlat = 0.000001f;
  static constexpr float kExpand = 1000000.f;
};

struct StateManager
{
  StateManager(juce::AudioProcessorValueTreeState&);

  void init();
  void replace(const juce::ValueTree&);
  juce::ValueTree getState();
  void validate();

  // TODO(luca): these will need to be refactored
  void addClip(std::int64_t, const juce::String&, double, bool);
  void removeClip(const juce::ValueTree& vt);
  void removeClipsIfInvalid(const juce::var&);
  void savePreset(const juce::String& name);
  void removePreset(const juce::String& name);
  bool doesPresetNameExist(const juce::String&);

  static juce::String valueTreeToXmlString(const juce::ValueTree&);

  juce::AudioProcessorValueTreeState& apvts;
  juce::UndoManager* undoManager { apvts.undoManager };

  juce::ValueTree state             { ID::AUTOMATE };
  juce::ValueTree parametersTree    { apvts.state  };
  juce::ValueTree editTree          { ID::EDIT     };
  juce::ValueTree presetsTree       { ID::PRESETS  };
 
  Presets presets { presetsTree, undoManager };
  Clips clips     { editTree,    undoManager };

  juce::Random rand;
  static constexpr double defaultZoomValue = 100;
};

} // namespace atmt 
