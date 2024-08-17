#pragma once

#include "identifiers.hpp"
#include <juce_data_structures/juce_data_structures.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <span>
#include "types.h"

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
struct juce::VariantConverter<std::vector<f32>> {
  static std::vector<f32> fromVar(const var& v) {
    auto mb = v.getBinaryData();
    auto len = std::size_t(mb->getSize());
    auto data = (f32*)mb->getData();
    std::vector<f32> vec; 
    vec.resize(len / sizeof(f32));
    std::memcpy(vec.data(), data, len);
    return vec;
  }
  
  static juce::var toVar(const std::vector<f32>& vec) {
    return { vec.data(), vec.size() * sizeof(f32) };
  }
};

namespace atmt {

#define STATE_CB(func) [this] (juce::var v) { func(v); }

class StateAttachment : public juce::ValueTree::Listener
{
public:
  StateAttachment(juce::ValueTree&, const juce::Identifier&, std::function<void(juce::var)>, juce::UndoManager*);
  ~StateAttachment() override;
  void setValue(const juce::var& v);

private:
  juce::var getValue();
  void performUpdate();
  void valueTreePropertyChanged(juce::ValueTree&, const juce::Identifier& i) override;

  juce::ValueTree state;
  juce::Identifier identifier;
  std::function<void(juce::var)> callback; 
  juce::UndoManager* undoManager = nullptr;
};

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
      std::span<f32> p { (f32*)mb->getData(), mb->getSize() / sizeof(f32) };
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

  juce::CachedValue<std::vector<f32>> parameters { state, ID::parameters, undoManager };
  juce::CachedValue<juce::String> name { state, ID::name, undoManager };

  std::atomic<std::int64_t> _id = 0; 
  std::deque<std::atomic<f32>> _parameters;
  std::atomic<std::size_t> _numParameters = 0;
};

struct Clip : TreeWrapper {
  Clip(juce::ValueTree& v, juce::UndoManager* um) : TreeWrapper(v, um) {
    jassert(v.hasType(ID::CLIP));
    rebuild();
  }

  void rebuild() override {
    name.forceUpdateOfCachedValue();
    x.forceUpdateOfCachedValue();
    y.forceUpdateOfCachedValue();

    _id = std::int64_t(state[ID::id]);
    _x = x;
    _y = y;
  }

  juce::CachedValue<juce::String> name { state, ID::name, undoManager };
  juce::CachedValue<f64> x { state, ID::x, undoManager };
  juce::CachedValue<bool> y { state, ID::y, undoManager };

  std::atomic<std::int64_t> _id = 0;
  std::atomic<f64> _x = 0;
  std::atomic<f64> _y = 0;
};

struct Path : TreeWrapper {
  Path(juce::ValueTree& v, juce::UndoManager* um) : TreeWrapper(v, um) {
    jassert(v.hasType(ID::PATH));
    rebuild();
  }

  void rebuild() override {
    x.forceUpdateOfCachedValue();
    y.forceUpdateOfCachedValue();
    _x = x;
    _y = y;
  }

  juce::CachedValue<f64> x { state, ID::x, undoManager };
  juce::CachedValue<f64> y { state, ID::y, undoManager };

  std::atomic<f64> _x = 0;
  std::atomic<f64> _y = 0;
};

struct ClipPath : TreeWrapper {
  ClipPath(juce::ValueTree& v, juce::UndoManager* um) : TreeWrapper(v, um) {
    jassert(v.hasType(ID::PATH) || v.hasType(ID::CLIP));
    rebuild();
  }

  void rebuild() override {
    x.forceUpdateOfCachedValue();
    y.forceUpdateOfCachedValue();
    curve.forceUpdateOfCachedValue();

    _x = x;
    _y = y;

    jassert(x >= 0);
    jassert(y >= 0 && y <= 1);
    jassert(curve >= 0 && curve <= 1);
  }

  juce::CachedValue<f64> x { state, ID::x, undoManager };
  juce::CachedValue<f64> y { state, ID::y, undoManager };
  juce::CachedValue<f64> curve { state, ID::curve, undoManager };

  std::atomic<f64> _x = 0;
  std::atomic<f64> _y = 0;
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

  auto begin()        { return objects.begin();   }
  auto end()          { return objects.end();     }
  auto begin() const  { return objects.begin();   }
  auto end()   const  { return objects.end();     }
  auto rbegin()       { return objects.rbegin();  }
  auto rend()         { return objects.rend();    }
  auto rbegin() const { return objects.rbegin();  }
  auto rend() const   { return objects.rend();    }
  std::size_t size()  { return objects.size();    }
  bool empty()        { return objects.empty();   }
  auto& front()       { return objects.front();   }
   
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
    auto cmp = [] (std::unique_ptr<Clip>& a, std::unique_ptr<Clip>& b) { return a->x < b->x; };
    std::sort(objects.begin(), objects.end(), cmp);
  }
};

struct Paths : ObjectList<Path> {
  Paths OBJECT_LIST_INIT { 
    jassert(v.hasType(ID::EDIT));
    rebuild();
  }

  bool isType(const juce::ValueTree& v) override { return v.hasType(ID::PATH); }

  void sort() override {
    auto cmp = [] (std::unique_ptr<Path>& a, std::unique_ptr<Path>& b) { return a->x < b->x; };
    std::sort(begin(), end(), cmp);
  }
};

struct ClipPaths : ObjectList<ClipPath> {
  ClipPaths OBJECT_LIST_INIT { 
    jassert(v.hasType(ID::EDIT));
    rebuild();
  }

  bool isType(const juce::ValueTree& v) override { return v.hasType(ID::PATH) || v.hasType(ID::CLIP); }

  void sort() override {
    auto cmp = [] (std::unique_ptr<ClipPath>& a, std::unique_ptr<ClipPath>& b) { return a->x < b->x; };
    std::sort(begin(), end(), cmp);
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
  Automation(juce::ValueTree&, juce::UndoManager*, juce::AudioProcessor*);
  
  auto getPointFromXIntersection(f64);
  f64 getYFromX(f64);
  ClipPath* getClipPathForX(f64);

  void rebuild() override;
  juce::Path& get();

  juce::AudioProcessor* proc = nullptr;
  juce::Path automation;
  static constexpr f32 kFlat = 0.000001f;
  static constexpr f32 kExpand = 1000000.f;
  ClipPaths clipPaths { state, undoManager, proc };
};

struct StateManager
{
  StateManager(juce::AudioProcessorValueTreeState&);

  void init();
  void replace(const juce::ValueTree&);
  juce::ValueTree getState();
  void validate();

  void addClip(std::int64_t, const juce::String&, f64, f64);
  void removeClip(const juce::ValueTree& vt);
  void removeClipsIfInvalid(const juce::var&);
  void clearEdit();
  void overwritePreset(const juce::String& name);
  void savePreset(const juce::String& name);
  void removePreset(const juce::String& name);
  bool doesPresetNameExist(const juce::String&);
  void clearPresets();
  void addPath(f64, f64);
  void removePath(const juce::ValueTree&);

  static juce::String valueTreeToXmlString(const juce::ValueTree&);

  juce::AudioProcessorValueTreeState& apvts;
  juce::AudioProcessor& proc { apvts.processor };
  juce::UndoManager* undoManager { apvts.undoManager };

  juce::ValueTree state             { ID::AUTOMATE };
  juce::ValueTree parametersTree    { apvts.state  };
  juce::ValueTree editTree          { ID::EDIT     };
  juce::ValueTree presetsTree       { ID::PRESETS  };
 
  Presets presets { presetsTree, undoManager };
  Clips clips     { editTree,    undoManager };

  juce::Random rand;
  static constexpr f64 defaultZoomValue = 100;
};

} // namespace atmt 
