#include "state_manager.hpp"
#include "plugin.hpp"
#include "editor.hpp"
#include <assert.h>
#include "scoped_timer.hpp"

namespace atmt {

static juce::String pluginID = "";
static f64 zoom = 100;
static std::atomic<bool> editMode = false;
static std::atomic<bool> modulateDiscrete = false;
static std::atomic<bool> captureParameterChanges = false;
static std::atomic<bool> releaseParameterChanges = false;
static std::atomic<f32>  randomSpread = 2;

static std::random_device randDevice;
static std::mt19937 randGen { randDevice() };
static std::normal_distribution<f32> rand_ { 0.0, 1.0 };

inline bool isNormalised(f64 v) {
  return v >= 0.0 && v <= 1.0;
}

inline f32 random() {
  f32 v = rand_(randGen) / randomSpread; // NOTE(luca): parameter that tightens random distribution
  v = std::clamp(v, -1.f, 1.f);
  v = (v * 0.5f) + 0.5f;
  return v;
}

StateManager::StateManager(juce::AudioProcessor& a) : proc(a) {}

void StateManager::init() {
  plugin = static_cast<Plugin*>(&proc); 
  engine = &plugin->engine;
}

void StateManager::replace(const juce::ValueTree& tree) {
  JUCE_ASSERT_MESSAGE_THREAD

  juce::MessageManagerLock lk(juce::Thread::getCurrentThread());

  if (lk.lockWasGained()) {
    setPluginID(tree["pluginID"]);

    if (engine->hasInstance()) {
      auto mb = tree["pluginData"].getBinaryData();
      engine->instance->setStateInformation(mb->getData(), mb->getSize());
    }
    
    setZoom(tree["zoom"]);
    setEditMode(tree["editMode"]);
    setModulateDiscrete(tree["modulateDiscrete"]);
    
    auto clipsTree = tree.getChild(0);
    for (auto c : clipsTree) {
      clips.emplace_back();
      auto& clip = clips.back();

      clip.x = c["x"];
      clip.y = c["y"];
      clip.c = c["c"];

      auto mb = c["parameters"].getBinaryData(); 
      auto parameters_ = (f32*)mb->getData();
      auto numParameters = mb->getSize() / sizeof(f32);
      clip.parameters.reserve(numParameters); 
      for (u32 i = 0; i < numParameters; ++i) {
        clip.parameters.emplace_back(parameters_[i]); 
      }
    }

    auto pathsTree = tree.getChild(1);
    for (auto p : pathsTree) {
      paths.emplace_back();
      auto& path = paths.back();

      path.x = p["x"];
      path.y = p["y"];
      path.c = p["c"];
    }

    DBG(tree.toXmlString());

    updateTrack(); 
  }
}

juce::ValueTree StateManager::getState() {
  JUCE_ASSERT_MESSAGE_THREAD

  juce::MessageManagerLock lk(juce::Thread::getCurrentThread());

  if (lk.lockWasGained()) {
    juce::ValueTree tree("tree");

    juce::MemoryBlock mb;

    if (engine->hasInstance()) {
      engine->instance->getStateInformation(mb);
    }

    tree.setProperty("zoom", zoom, nullptr)
        .setProperty("editMode", editMode.load(), nullptr)
        .setProperty("modulateDiscrete", modulateDiscrete.load(), nullptr)
        .setProperty("pluginID", pluginID, nullptr)
        .setProperty("pluginData", mb, nullptr);

    juce::ValueTree clipsTree("clips");
    for (auto& c : clips) {
      juce::ValueTree clip("clip");
      clip.setProperty("x", c.x, nullptr)
          .setProperty("y", c.y, nullptr)
          .setProperty("c", c.c, nullptr)
          .setProperty("parameters", { c.parameters.data(), c.parameters.size() * sizeof(f32) }, nullptr);
      clipsTree.appendChild(clip, nullptr);
    }

    juce::ValueTree pathsTree("paths");
    for (auto& p : paths) {
      juce::ValueTree path("path");
      path.setProperty("x", p.x, nullptr)
          .setProperty("y", p.y, nullptr)
          .setProperty("c", p.c, nullptr);
      pathsTree.appendChild(path, nullptr);
    }

    tree.appendChild(clipsTree, nullptr);
    tree.appendChild(pathsTree, nullptr);

    DBG(tree.toXmlString());
    return tree;
  }

  return {};
}

void StateManager::addClip(f64 x, f64 y) {
  JUCE_ASSERT_MESSAGE_THREAD
  DBG("StateManager::addClip()");

  assert(x >= 0 && y >= 0 && y <= 1);
  assert(engine->hasInstance());

  {
    ScopedProcLock lk(proc);

    clips.emplace_back();
    auto& c = clips.back();

    c.x = x;
    c.y = y;

    c.parameters.reserve(parameters.size());

    for (auto& parameter : parameters) {
      c.parameters.push_back(parameter.parameter->getValue());
      assert(isNormalised(c.parameters.back()));
    }
  }

  updateTrack();
}

void StateManager::removeClip(Clip* c) {
  JUCE_ASSERT_MESSAGE_THREAD
  assert(c);

  {
    ScopedProcLock lk(proc);
    std::erase_if(clips, [c] (Clip& o) { return c == &o; });
  }

  updateTrack();
}

void StateManager::moveClip(Clip* c, f64 x, f64 y, f64 curve) {
  JUCE_ASSERT_MESSAGE_THREAD
  assert(c);

  {
    ScopedProcLock lk(proc);
    c->x = x < 0 ? 0 : x;
    c->y = std::clamp(y, 0.0, 1.0);
    c->c = std::clamp(curve, 0.0, 1.0);
  }

  updateTrack();
}

void StateManager::randomiseParameters() {
  JUCE_ASSERT_MESSAGE_THREAD

  setEditMode(true);

  {
    ScopedProcLock lk(proc);
    for (auto& p : parameters) {
      if (shouldProcessParameter(&p)) {
        p.parameter->setValue(random());
      }
    }
  }
}

bool StateManager::shouldProcessParameter(Parameter* p) {
  // TODO(luca): make active responsible for discrete and bool parameters too

  if (p->active && p->parameter->isAutomatable()) {
    return p->parameter->isDiscrete() ? modulateDiscrete.load() : true; 
  }
  return false;
}

void StateManager::addPath(double x, double y) {
  JUCE_ASSERT_MESSAGE_THREAD
  assert(x >= 0 && y >= 0 && y <= 1);

  {
    ScopedProcLock lk(proc);
    paths.emplace_back();
    auto& path = paths.back();
    path.x = x;
    path.y = y;
  }

  updateTrack();
}

void StateManager::removePath(Path* p) {
  JUCE_ASSERT_MESSAGE_THREAD

  {
    ScopedProcLock lk(proc);
    std::erase_if(paths, [p] (Path& o) { return p == &o; });
  }

  updateTrack();
}

void StateManager::movePath(Path* p, f64 x, f64 y, f64 c) {
  JUCE_ASSERT_MESSAGE_THREAD

  {
    ScopedProcLock lk(proc);
    p->x = x < 0 ? 0 : x;
    p->y = std::clamp(y, 0.0, 1.0);
    p->c = std::clamp(c, 0.0, 1.0);
  }

  updateTrack(); 
}

void StateManager::removeSelection(Selection selection) {
  JUCE_ASSERT_MESSAGE_THREAD

  assert(selection.start >= 0 && selection.end >= 0);

  if (std::abs(selection.start - selection.end) > EPSILON) {
    ScopedProcLock lk(proc);
    std::erase_if(clips, [selection] (Clip& c) { return c.x >= selection.start && c.x <= selection.end; }); 
    std::erase_if(paths, [selection] (Path& p) { return p.x >= selection.start && p.x <= selection.end; }); 
  }

  updateTrack();
}

void StateManager::setPluginID(const juce::String& id) {
  JUCE_ASSERT_MESSAGE_THREAD

  DBG("StateManager::setPluginID()");

  {
    ScopedProcLock lk(proc);
    clear();

    pluginID = id;
    juce::String errorMessage;

    if (id != "") {
      auto description = plugin->knownPluginList.getTypeForIdentifierString(id);

      if (description) {
        auto instance = plugin->apfm.createPluginInstance(*description, proc.getSampleRate(), proc.getBlockSize(), errorMessage);

        if (instance) {
          auto processorParameters = instance->getParameters();
          u32 numParameters = u32(processorParameters.size());
          parameters.reserve(numParameters);

          for (u32 i = 0; i < numParameters; ++i) {
            parameters.emplace_back();
            parameters.back().parameter = processorParameters[i32(i)];
          }

          engine->setPluginInstance(instance);
          proc.prepareToPlay(proc.getSampleRate(), proc.getBlockSize());
        }
      }
    } else {
      engine->kill();
    }
  }
}

void StateManager::setZoom(f64 z) {
  JUCE_ASSERT_MESSAGE_THREAD

  zoom = z;
  updateTrack(); 
}

void StateManager::setEditMode(bool m) {
  JUCE_ASSERT_MESSAGE_THREAD

  editMode = m;
  updateDebugView();
}

void StateManager::setModulateDiscrete(bool m) {
  JUCE_ASSERT_MESSAGE_THREAD

  modulateDiscrete = m;
  updateDebugView();
}

void StateManager::setAllParametersActive(bool v) {
  JUCE_ASSERT_MESSAGE_THREAD
  
  DBG("StateManager::setParametersActive()");

  {
    ScopedProcLock lk(proc);
    for (auto& p : parameters) {
      p.active = v;
    }
  }

  updateParametersView(); 
}

void StateManager::setParameterActive(Parameter* p, bool a) {
  JUCE_ASSERT_MESSAGE_THREAD

  {
    ScopedProcLock lk(proc);
    p->active = a;
  }

  updateParametersView();
}

void StateManager::clear() {
  {
    ScopedProcLock lk(proc);

    paths.clear();
    clips.clear();
    parameters.clear();

    // TODO(luca): this is temporary hack to avoid the vector reallocating
    // its memory when adding new element invalidating all pointers
    paths.reserve(1000);
    clips.reserve(1000);
    parameters.reserve(1000);
  }

  updateTrack();
}

auto StateManager::findAutomationPoint(f64 x) {
  for (auto it = points.begin(); it != points.end(); ++it) {
    if (it != points.begin()) {
      auto& a = *std::prev(it);
      auto& b = *it;

      if (x >= a.x && x <= b.x) {
        return it;
      }
    }
  }
  assert(false);
  return points.end();
}

void StateManager::updateParametersView() {
  if (parametersView) {
    parametersView->updateParameters();
  }
}

void StateManager::updateAutomation() {
  DBG("StateManager::updateAutomation()");

  {
    ScopedProcLock lk(proc);

    automation.clear();

    points.resize(clips.size() + paths.size());

    u32 n = 0;
    for (; n < clips.size(); ++n) {
      points[n].x = clips[n].x; 
      points[n].y = clips[n].y; 
      points[n].c = clips[n].c; 
      points[n].clip = &clips[n];
      points[n].path = nullptr;
    }
    for (u32 i = 0; i < paths.size(); ++i) {
      points[i + n].x = paths[i].x; 
      points[i + n].y = paths[i].y; 
      points[i + n].c = paths[i].c; 
      points[i + n].clip = nullptr;
      points[i + n].path = &paths[i];
    }

    std::sort(points.begin(), points.end(), [] (AutomationPoint& a, AutomationPoint& b) { return a.x < b.x; });

    // TODO(luca): tidy this 
    for (auto& p : points) {
      auto c = automation.getCurrentPosition();
      f64 curve = p.c;
      f64 cx = c.x + (p.x - c.x) * (c.y < p.y ? curve : 1.0 - curve); 
      f64 cy = (c.y < p.y ? c.y : f64(p.y)) + std::abs(p.y - c.y) * (1.0 - curve);
      automation.quadraticTo(f32(cx), f32(cy), f32(p.x), f32(p.y));
    }
  }

  if (automationView) {
    automationView->resized();
    automationView->repaint();
  }
}

void StateManager::updateTrack() {
  updateAutomation();

  if (trackView) {
    trackView->resized();
    trackView->repaint();
  }
}

void StateManager::updateDebugView() {
  if (debugView) {
    debugView->resized(); 
  }
}

} // namespace atmt 
