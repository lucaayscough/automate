#include "state_manager.hpp"
#include "plugin.hpp"
#include "editor.hpp"
#include <assert.h>
#include "scoped_timer.hpp"
#include "logger.hpp"

namespace atmt {

static std::random_device randDevice;
static std::mt19937 randGen { randDevice() };
static std::normal_distribution<f32> rand_ { 0.0, 1.0 };
static std::atomic<u32> uniqueCounter = 0;

inline bool isNormalised(f32 v) {
  return v >= 0.0 && v <= 1.0;
}

inline f32 random(f32 randomSpread) {
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
    if (!setPluginID(tree["pluginID"])) {
      return;
    }

    if (engine->hasInstance()) {
      auto mb = tree["pluginData"].getBinaryData();
      engine->instance->setStateInformation(mb->getData(), i32(mb->getSize()));
    }
    
    setZoom(tree["zoom"]);
    setEditMode(tree["editMode"]);
    setModulateDiscrete(tree["modulateDiscrete"]);
    
    auto clipsTree = tree.getChild(0);
    for (auto c : clipsTree) {
      addClip(c["x"], c["y"], c["c"]);
      auto& clip = state.clips.back();
      clip.parameters.clear();

      auto mb = c["parameters"].getBinaryData(); 
      auto parameters_ = (f32*)mb->getData();
      auto numParameters = mb->getSize() / sizeof(f32);
      clip.parameters.reserve(numParameters); 

      for (u32 i = 0; i < numParameters; ++i) {
        clip.parameters.emplace_back(parameters_[i]); 
      }

      assert(clip.parameters.size() == numParameters);
    }

    auto pathsTree = tree.getChild(1);
    for (const auto& p : pathsTree) {
      addPath(p["x"], p["y"], p["c"]);
    }

    if (engine->hasInstance()) {
      auto parametersTree = tree.getChild(2);

      u32 i = 0;
      for (auto v : parametersTree) {
        juce::String name = v["name"];

        if (name != state.parameters[i].parameter->getName(1024)) {
          assert(false);

          for (u32 j = 0; j < state.parameters.size(); ++j) {
            if (name == state.parameters[j].parameter->getName(1024)) {
              state.parameters[j].active = v["active"];
              break; 
            }
          }
        } else {
          state.parameters[i].active = v["active"];
        }

        ++i;
      }
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

    tree.setProperty("zoom", state.zoom, nullptr)
        .setProperty("editMode", state.editMode.load(), nullptr)
        .setProperty("modulateDiscrete", state.modulateDiscrete.load(), nullptr)
        .setProperty("pluginID", state.pluginID, nullptr)
        .setProperty("pluginData", mb, nullptr);

    juce::ValueTree clipsTree("clips");
    for (const auto& c : state.clips) {
      juce::ValueTree clip("clip");
      clip.setProperty("x", c.x, nullptr)
          .setProperty("y", c.y, nullptr)
          .setProperty("c", c.c, nullptr)
          .setProperty("parameters", { c.parameters.data(), c.parameters.size() * sizeof(f32) }, nullptr);
      clipsTree.appendChild(clip, nullptr);
    }

    juce::ValueTree pathsTree("paths");
    for (const auto& p : state.paths) {
      juce::ValueTree path("path");
      path.setProperty("x", p.x, nullptr)
          .setProperty("y", p.y, nullptr)
          .setProperty("c", p.c, nullptr);
      pathsTree.appendChild(path, nullptr);
    }

    juce::ValueTree parametersTree("parameters");
    for (const auto& p : state.parameters) {
      juce::ValueTree parameter("parameter");
      parameter.setProperty("name", p.parameter->getName(1024), nullptr)
               .setProperty("active", p.active, nullptr);
      parametersTree.appendChild(parameter, nullptr);
    }

    tree.appendChild(clipsTree, nullptr);
    tree.appendChild(pathsTree, nullptr);
    tree.appendChild(parametersTree, nullptr);

    DBG(tree.toXmlString());
    return tree;
  }

  return {};
}

void StateManager::addClip(f32 x, f32 y, f32 curve) {
  JUCE_ASSERT_MESSAGE_THREAD
  scoped_timer t("StateManager::addClip()");

  assert(x >= 0 && y >= 0 && y <= 1);
  assert(engine->hasInstance());

  {
    ScopedProcLock lk(proc);

    state.clips.emplace_back();
    auto& clip = state.clips.back();

    clip.x = x;
    clip.y = y;
    clip.c = curve;

    clip.parameters.reserve(state.parameters.size());

    for (auto& parameter : state.parameters) {
      clip.parameters.push_back(parameter.parameter->getValue());
      assert(isNormalised(clip.parameters.back()));
    }
  }

  updateTrack();
}

void StateManager::duplicateClip(u32 id, f32 x, bool top) {
  assert(id < state.clips.size());

  state.clips.push_back(state.clips[id]);
  auto& newClip = state.clips.back();

  newClip.x = x;
  newClip.y = f32(!top);
  newClip.c = 0.5f;

  if (state.editMode) {
    engine->interpolate();
  }

  updateTrack();
}

void StateManager::duplicateClipDenorm(u32 id, f32 x, bool top) {
  assert(id < state.clips.size());

  duplicateClip(id, x / state.zoom, top);
}

void StateManager::removeClip(u32 id) {
  JUCE_ASSERT_MESSAGE_THREAD
  assert(id < state.clips.size());

  auto& clips = state.clips;

  {
    ScopedProcLock lk(proc);
    clips.erase(clips.begin() + id);
  }

  if (state.editMode) {
    engine->interpolate();
  }

  updateTrack();
}

void StateManager::moveClip(u32 id, f32 x, f32 y, f32 curve) {
  JUCE_ASSERT_MESSAGE_THREAD
  assert(id < state.clips.size());

  {
    ScopedProcLock lk(proc);

    auto& clips = state.clips;
    clips[id].x = x < 0 ? 0 : x;
    clips[id].y = std::clamp(y, 0.f, 1.f);
    clips[id].c = std::clamp(curve, 0.f, 1.f);
  }

  if (state.editMode) {
    engine->interpolate();
  }

  updateTrack();
}

void StateManager::moveClipDenorm(u32 id, f32 x, f32 y, f32 curve) {
  assert(id < state.clips.size());

  moveClip(id, x / state.zoom, y, curve);
}

void StateManager::randomiseParameters() {
  JUCE_ASSERT_MESSAGE_THREAD

  setEditMode(true);

  {
    ScopedProcLock lk(proc);

    for (auto& p : state.parameters) {
      if (shouldProcessParameter(&p)) {
        p.parameter->setValue(random(state.randomSpread));
      }
    }
  }
}

bool StateManager::shouldProcessParameter(Parameter* p) {
  if (p->active && p->parameter->isAutomatable()) {
    return p->parameter->isDiscrete() ? state.modulateDiscrete.load() : true; 
  }
  return false;
}

u32 StateManager::addPath(f32 x, f32 y, f32 curve) {
  JUCE_ASSERT_MESSAGE_THREAD
  assert(x >= 0 && y >= 0 && y <= 1 && curve >= 0 && curve <= 1);

  {
    ScopedProcLock lk(proc);
    state.paths.emplace_back();
    auto& path = state.paths.back();
    path.x = x;
    path.y = y;
    path.c = curve;
  }

  if (state.editMode) {
    engine->interpolate();
  }

  updateTrack();

  return u32(state.paths.size() - 1);
}

u32 StateManager::addPathDenorm(f32 x, f32 y, f32 curve) {
  JUCE_ASSERT_MESSAGE_THREAD
  assert(x >= 0 && y >= 0 && y <= 1 && curve >= 0 && curve <= 1);

  return addPath(x / state.zoom, y, curve);
}

void StateManager::removePath(u32 id) {
  JUCE_ASSERT_MESSAGE_THREAD
  assert(id < state.paths.size());

  {
    ScopedProcLock lk(proc);
    auto& paths = state.paths;
    paths.erase(paths.begin() + id);
  }

  if (state.editMode) {
    engine->interpolate();
  }

  updateTrack();
}

void StateManager::movePath(u32 id, f32 x, f32 y, f32 c) {
  JUCE_ASSERT_MESSAGE_THREAD
  assert(id < state.paths.size());

  {
    ScopedProcLock lk(proc);
    auto& paths = state.paths;
    paths[id].x = x < 0 ? 0 : x;
    paths[id].y = std::clamp(y, 0.f, 1.f);
    paths[id].c = std::clamp(c, 0.f, 1.f);
  }

  if (state.editMode) {
    engine->interpolate();
  }

  updateTrack(); 
}

void StateManager::movePathDenorm(u32 id, f32 x, f32 y, f32 c) {
  assert(id < state.paths.size());
  movePath(id, x / state.zoom, y, c);
}

void StateManager::setSelection(f32 start, f32 end) {
  state.selection.start = start; 
  state.selection.end = end;

  updateTrack();
}

void StateManager::setSelectionDenorm(f32 start, f32 end) {
  setSelection(start / state.zoom, end / state.zoom);
}

void StateManager::removeSelection() {
  JUCE_ASSERT_MESSAGE_THREAD

  Selection selection = state.selection;

  assert(selection.start >= 0 && selection.end >= 0);

  if (selection.start > selection.end) {
    std::swap(selection.start, selection.end);
  }

  if (std::abs(selection.start - selection.end) > EPSILON) {
    ScopedProcLock lk(proc);

    std::erase_if(state.clips, [selection] (const Clip& c) { return c.x >= selection.start && c.x <= selection.end; }); 
    std::erase_if(state.paths, [selection] (const Path& p) { return p.x >= selection.start && p.x <= selection.end; }); 
  }

  updateTrack();
}

void StateManager::setPlayheadPosition(f32 x) {
  assert(x >= 0);

  if (state.editMode) {
    state.playheadPosition = x;

    {
      ScopedProcLock lk(proc);
      engine->interpolate();
    }
  }
}

void StateManager::setPlayheadPositionDenorm(f32 x) {
  setPlayheadPosition(x / state.zoom);
}

bool StateManager::setPluginID(const juce::String& id) {
  JUCE_ASSERT_MESSAGE_THREAD

  scoped_timer t("StateManager::setPluginID()");

  {
    ScopedProcLock lk(proc);
    clear();

    state.pluginID = id;
    juce::String errorMessage;

    if (id != "") {
      auto description = plugin->knownPluginList.getTypeForIdentifierString(id);

      if (description) {
        auto instance = plugin->apfm.createPluginInstance(*description, proc.getSampleRate(), proc.getBlockSize(), errorMessage);

        if (instance) {
          auto processorParameters = instance->getParameters();
          u32 numParameters = u32(processorParameters.size());
          state.parameters.reserve(numParameters);

          for (u32 i = 0; i < numParameters; ++i) {
            state.parameters.emplace_back();
            state.parameters.back().parameter = processorParameters[i32(i)];
          }

          engine->setPluginInstance(instance);
          proc.prepareToPlay(proc.getSampleRate(), proc.getBlockSize());
        }
      } else {
        return false;
      }
    } else {
      engine->kill();
      return false;
    }
  }

  return true;
}

void StateManager::setZoom(f32 z) {
  JUCE_ASSERT_MESSAGE_THREAD

  assert(z > 0);

  state.zoom = z;
  updateTrack(); 
}

void StateManager::setEditMode(bool m) {
  JUCE_ASSERT_MESSAGE_THREAD

  state.editMode = m;
  updateDebugView();
}

void StateManager::setModulateDiscrete(bool m) {
  JUCE_ASSERT_MESSAGE_THREAD

  state.modulateDiscrete = m;
  updateDebugView();
}

void StateManager::setAllParametersActive(bool v) {
  JUCE_ASSERT_MESSAGE_THREAD
  
  DBG("StateManager::setParametersActive()");

  {
    ScopedProcLock lk(proc);
    for (auto& p : state.parameters) {
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

    state.paths.clear();
    state.clips.clear();
    state.parameters.clear();

    // TODO(luca): this is temporary hack to avoid the vector reallocating
    // its memory when adding new element invalidating all pointers
    state.paths.reserve(1000);
    state.clips.reserve(1000);
    state.parameters.reserve(1000);
  }

  updateTrack();
}

auto StateManager::findAutomationPoint(f32 x) {
  // TODO(luca): move his method to the editor because it has no use outside
  assert(x >= 0);

  for (auto it = state.points.begin(); it != state.points.end(); ++it) {
    if (it != state.points.begin()) {
      const auto& a = *std::prev(it);
      const auto& b = *it;

      if (x >= a.x && x <= b.x) {
        return it;
      } 
    } else if (it != state.points.end()) {
      const auto& a = *it;

      if (x <= a.x) {
        return it;
      }
    }
  }

  if (state.points.size() > 0) { auto it = std::prev(state.points.end());
    assert(it->x <= x);
    return it;  
  }

  return state.points.end();
}

auto StateManager::findAutomationPointDenorm(f32 x) {
  return findAutomationPoint(x / state.zoom);
}

void StateManager::updateParametersView() {
  if (parametersView) {
    parametersView->updateParameters();
  }
}

void StateManager::updateTrack() {
  //scoped_timer t("StateManager::updateTrack()");

  if (trackView) {
    automationLaneView->selection.start = state.selection.start * state.zoom;
    automationLaneView->selection.end = state.selection.end * state.zoom;

    trackView->zoom = state.zoom;

    auto& clips = state.clips;
    auto& clipViews = trackView->clipViews;

    u32 numClips = u32(clips.size());
    u32 numViews = u32(clipViews.size());

    if (numViews < numClips) {
      u32 diff = numClips - numViews;

      while (diff--) {
        clipViews.add(new ClipView(trackView->grid));
        trackView->addAndMakeVisible(clipViews.getLast());
      }
    } else if (numViews > numClips) {
      i32 diff = i32(numViews - numClips);
      clipViews.removeLast(diff);
    }

    for (u32 i = 0; i < numClips; ++i) {
      const auto& clip = clips[i];
      auto* view = clipViews[i32(i)];
     
      view->id     = i;
      view->move   = [&clip, this] (u32 id, f32 x, f32 y) { moveClipDenorm(id, x, y, clip.c); };
      view->remove = [this] (u32 id) { removeClip(id); };

      i32 h = kPresetLaneHeight;
      i32 w = kPresetLaneHeight;
      i32 x = i32((clip.x * state.zoom) - w * 0.5f);
      i32 y = !bool(clip.y) ? trackView->b.presetLaneTop.getY() : trackView->b.presetLaneBottom.getY();
         
      view->setBounds(x, y, w, h);
    }

    trackView->setSize(trackView->getTrackWidth(), trackView->getHeight());
  }

  {
    ScopedProcLock lk(proc);

    state.automation.clear();

    auto& points = state.points;
    auto& clips = state.clips;
    auto& paths = state.paths;

    points.resize(clips.size() + paths.size());

    u32 n = 0;
    for (; n < clips.size(); ++n) {
      points[n].x = clips[n].x; 
      points[n].y = clips[n].y; 
      points[n].c = clips[n].c; 
      points[n].id = n;
      points[n].clip = &clips[n];
      points[n].path = nullptr;
    }
    for (u32 i = 0; i < paths.size(); ++i) {
      points[i + n].x = paths[i].x; 
      points[i + n].y = paths[i].y; 
      points[i + n].c = paths[i].c; 
      points[i + n].id = i; 
      points[i + n].clip = nullptr;
      points[i + n].path = &paths[i];
    }

    std::sort(points.begin(), points.end(), [] (const AutomationPoint& a, const AutomationPoint& b) { return a.x < b.x; });

    if (points.size() > 0) {
      state.automation.startNewSubPath(0, points[0].y);
    }

    for (auto& p2 : points) {
      auto p1 = state.automation.getCurrentPosition();
      f32 cx = p1.x + (p2.x - p1.x) * (p1.y < p2.y ? p2.c : 1.f - p2.c); 
      f32 cy = (p1.y < p2.y ? p1.y : p2.y) + std::abs(p2.y - p1.y) * (1.f - p2.c);
      state.automation.quadraticTo(cx, cy, p2.x, p2.y);
    }
  }

  if (automationLaneView) {
    auto& automation = automationLaneView->automation;
    automation = state.automation; 

    f32 zoom = state.zoom;
    automation.applyTransform(juce::AffineTransform::scale(zoom, kAutomationLaneHeight - Style::lineThickness));
    automation.applyTransform(juce::AffineTransform::translation(0, Style::lineThickness / 2));

    auto p = automation.getCurrentPosition();
    automation.quadraticTo(automationLaneView->getWidth(), p.y, automationLaneView->getWidth(), p.y);

    {
      auto& paths = state.paths;
      auto& pathViews = automationLaneView->pathViews;
      u32 numPaths = u32(paths.size());
      u32 numViews = u32(pathViews.size());

      if (numViews < numPaths) {
        u32 diff = numPaths - numViews;

        while (diff--) {
          pathViews.add(new PathView(automationLaneView->grid));
          automationLaneView->addAndMakeVisible(pathViews.getLast());
        }
      } else if (numViews > numPaths) {
        i32 diff = i32(numViews - numPaths);
        pathViews.removeLast(diff);
      }

      for (u32 i = 0; i < numPaths; ++i) {
        const auto& path = paths[i];
        auto* view = pathViews[i32(i)];
       
        view->id     = i;
        view->move   = [&path, this] (u32 id, f32 x, f32 y) { movePathDenorm(id, x, y, path.c); };
        view->remove = [this] (u32 id) { removePath(id); };

        i32 x = i32(path.x * zoom - PathView::posOffset);
        i32 y = i32(path.y * automationLaneView->getHeight() - PathView::posOffset);
           
        view->setBounds(x, y, PathView::size, PathView::size);
      }
    }

    automationLaneView->repaint();
  }
}

void StateManager::updateDebugView() {
  if (debugView) {
    debugView->resized(); 
  }
}

} // namespace atmt 
