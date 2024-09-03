#include "state_manager.hpp"
#include "plugin.hpp"
#include <assert.h>

namespace atmt {

StateManager::StateManager(juce::AudioProcessor& a) : proc(a) {
  rand.setSeedRandomly();
}

void StateManager::replace(const juce::ValueTree& tree) {
  JUCE_ASSERT_MESSAGE_THREAD

  juce::MessageManagerLock lk(juce::Thread::getCurrentThread());

  if (lk.lockWasGained()) {
    clear();
    
    setZoom(tree["zoom"]);
    setEditMode(tree["editMode"]);
    setModulateDiscrete(tree["modulateDiscrete"]);
    
    auto presetsTree = tree.getChild(0);
    for (auto p : presetsTree) {
      presets.emplace_back();
      auto& preset = presets.back();
      preset.name = p["name"];
      auto mb = p["parameters"].getBinaryData(); 
      auto parameters = (f32*)mb->getData();
      auto numParameters = mb->getSize() / sizeof(f32);
      preset.parameters.reserve(numParameters); 
      for (u32 i = 0; i < numParameters; ++i) {
        preset.parameters.emplace_back(parameters[i]); 
      }
    }

    auto clipsTree = tree.getChild(1);
    for (auto c : clipsTree) {
      clips.emplace_back(std::make_unique<Clip>());
      auto& clip = clips.back();
      auto name = c["name"].toString();

      for (auto& p : presets) {
        if (name == p.name) {
          clip->preset = &p; 
        }
      }

      clip->x = c["x"];
      clip->y = c["y"];
      clip->c = c["c"];
    }

    auto pathsTree = tree.getChild(2);
    for (auto p : pathsTree) {
      paths.emplace_back(std::make_unique<Path_>());
      auto& path = paths.back();

      path->x = p["x"];
      path->y = p["y"];
      path->c = p["c"];
    }

    setPluginID(tree["pluginID"]);
    updateTrack(); 
    updatePresetList();
  }
}

void StateManager::addClip(Preset* p, f64 x, f64 y) {
  JUCE_ASSERT_MESSAGE_THREAD
  assert(x >= 0 && y >= 0 && y <= 1);

  proc.suspendProcessing(true);

  clips.emplace_back(std::make_unique<Clip>()); 

  auto& c = clips.back();
  c->preset = p;
  c->name = p->name;
  c->x = x;
  c->y = y;

  std::sort(clips.begin(), clips.end(), [] (ClipPtr& a, ClipPtr& b) { return a->x < b->x; });

  proc.suspendProcessing(false);

  updateTrack();
}

void StateManager::removeClip(Clip* c) {
  JUCE_ASSERT_MESSAGE_THREAD
  assert(c);

  proc.suspendProcessing(true);

  std::erase_if(clips, [c] (ClipPtr& o) { return c == o.get(); });

  proc.suspendProcessing(false);

  updateTrack();
}

void StateManager::moveClip(Clip* c, f64 x, f64 y, f64 curve) {
  JUCE_ASSERT_MESSAGE_THREAD
  assert(c);

  proc.suspendProcessing(true);

  c->x = x < 0 ? 0 : x;
  c->y = std::clamp(y, 0.0, 1.0);
  c->c = std::clamp(curve, 0.0, 1.0);

  std::sort(clips.begin(), clips.end(), [] (ClipPtr& a, ClipPtr& b) { return a->x < b->x; });

  proc.suspendProcessing(false);

  updateTrack();
}

void StateManager::overwritePreset(Preset* preset) {
  JUCE_ASSERT_MESSAGE_THREAD

  proc.suspendProcessing(true);

  auto& p = *static_cast<Plugin*>(&proc);
  p.engine.getCurrentParameterValues(preset->parameters);

  proc.suspendProcessing(false);
}

void StateManager::loadPreset(Preset* preset) {
  JUCE_ASSERT_MESSAGE_THREAD

  proc.suspendProcessing(true);

  auto& p = *static_cast<Plugin*>(&proc);
  p.engine.restoreFromPreset(preset);

  proc.suspendProcessing(false);
}

void StateManager::savePreset(const juce::String& name) {
  JUCE_ASSERT_MESSAGE_THREAD

  proc.suspendProcessing(true);

  presets.emplace_back();
  auto& preset = presets.back();

  preset.name = name;

  auto& p = *static_cast<Plugin*>(&proc);
  p.engine.getCurrentParameterValues(preset.parameters);

  proc.suspendProcessing(false);

  updatePresetList();
}

void StateManager::removePreset(Preset* preset) {
  JUCE_ASSERT_MESSAGE_THREAD

  proc.suspendProcessing(true);

  std::erase_if(clips, [preset] (ClipPtr& c) { return c->preset == preset; });
  std::erase_if(presets, [preset] (Preset& p) { return p.name == preset->name; });

  proc.suspendProcessing(false);

  updatePresetList();
  updateTrack();
}

bool StateManager::doesPresetNameExist(const juce::String& name) {
  JUCE_ASSERT_MESSAGE_THREAD
  for (const auto& preset : presets) {
    if (preset.name == name) {
      return true;
    }
  }
  return false;
}

void StateManager::addPath(double x, double y) {
  JUCE_ASSERT_MESSAGE_THREAD
  assert(x >= 0 && y >= 0 && y <= 1);

  proc.suspendProcessing(true);

  paths.emplace_back(std::make_unique<Path_>());

  auto& path = paths.back();
  path->x = x;
  path->y = y;

  std::sort(paths.begin(), paths.end(), [] (PathPtr& a, PathPtr& b) { return a->x < b->x; });

  proc.suspendProcessing(false);

  updateTrack();
}

void StateManager::removePath(Path_* p) {
  JUCE_ASSERT_MESSAGE_THREAD

  proc.suspendProcessing(true);

  std::erase_if(paths, [p] (PathPtr& o) { return p == o.get(); });

  proc.suspendProcessing(false);

  updateTrack();
}

void StateManager::movePath(Path_* p, f64 x, f64 y, f64 c) {
  JUCE_ASSERT_MESSAGE_THREAD

  proc.suspendProcessing(true);

  p->x = x < 0 ? 0 : x;
  p->y = std::clamp(y, 0.0, 1.0);
  p->c = std::clamp(c, 0.0, 1.0);

  std::sort(paths.begin(), paths.end(), [] (PathPtr& a, PathPtr& b) { return a->x < b->x; });

  proc.suspendProcessing(false);

  updateTrack(); 
}

void StateManager::setPluginID(const juce::String& id) {
  auto& p = *static_cast<Plugin*>(&proc);
  p.suspendProcessing(true);

  pluginID = id;
  juce::String errorMessage;

  if (id != "") {
    auto description = p.knownPluginList.getTypeForIdentifierString(id);
    if (description) {
      auto instance = p.apfm.createPluginInstance(*description, p.getSampleRate(), p.getBlockSize(), errorMessage);
      p.engine.setPluginInstance(instance);
      p.prepareToPlay(p.getSampleRate(), p.getBlockSize());
    }
  } else {
    p.engine.kill();
    clear();
  }

  p.suspendProcessing(false);

  // TODO(luca): update proc and editor
}

void StateManager::setZoom(f64 z) {
  zoom = z;
  updateTrack(); 
}

void StateManager::setEditMode(bool m) {
  editMode = m;

  if (debugView) {
    debugView->resized();
  }
}

void StateManager::setModulateDiscrete(bool m) {
  modulateDiscrete = m;

  if (debugView) {
    debugView->resized();
  }
}

void StateManager::clear() {
  proc.suspendProcessing(true);

  paths.clear();
  clips.clear();
  presets.clear();

  proc.suspendProcessing(false);

  updateTrack();
  updatePresetList();
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

void StateManager::updateAutomation() {
  DBG("StateManager::updateAutomation()");

  proc.suspendProcessing(true);

  automation_.clear();

  points.resize(clips.size() + paths.size());

  u32 n = 0;
  for (; n < clips.size(); ++n) {
    points[n].x = clips[n]->x; 
    points[n].y = clips[n]->y; 
    points[n].c = clips[n]->c; 
    points[n].clip = clips[n].get();
    points[n].path = nullptr;
  }
  for (u32 i = 0; i < paths.size(); ++i) {
    points[i + n].x = paths[i]->x; 
    points[i + n].y = paths[i]->y; 
    points[i + n].c = paths[i]->c; 
    points[i + n].clip = nullptr;
    points[i + n].path = paths[i].get();
  }

  std::sort(points.begin(), points.end(), [] (AutomationPoint& a, AutomationPoint& b) { return a.x < b.x; });

  // TODO(luca): tidy this 
  for (auto& p : points) {
    auto c = automation_.getCurrentPosition();
    f64 curve = p.c;
    f64 cx = c.x + (p.x - c.x) * (c.y < p.y ? curve : 1.0 - curve); 
    f64 cy = (c.y < p.y ? c.y : f64(p.y)) + std::abs(p.y - c.y) * (1.0 - curve);
    automation_.quadraticTo(f32(cx), f32(cy), f32(p.x), f32(p.y));
  }

  proc.suspendProcessing(false);

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

void StateManager::updatePresetList() {
  if (presetView) {
    presetView->resized();
  }
}

juce::String StateManager::valueTreeToXmlString(const juce::ValueTree& vt) {
  std::function<juce::String(const juce::ValueTree&, juce::String)> printTree = [&] (const juce::ValueTree& v, juce::String indentation) -> juce::String {
    auto printChildren = [&] (const juce::ValueTree& vRec, juce::String indentationRec) -> juce::String {
      juce::String contents;
      for (const auto& child : vRec)
        contents << printTree(child, indentationRec);
      return contents;
    };

    auto numChildren   = v.getNumChildren();
    auto numProperties = v.getNumProperties();
    juce::String contents;
    contents << indentation << "<" << v.getType();

    for (int i = 0; i < numProperties; ++i) {
      auto id = v.getPropertyName(i).toString();
      auto value = v[id].toString();
      contents << " " << id << "=\"" << value << "\"";
    }

    if (numChildren > 0) {
      contents << ">\n";
      contents << printChildren(v, indentation + "  ");
      contents << indentation + "<" + v.getType() + "/>\n";
    } else {
      contents << "/>\n";
    }

    return contents;
  };

  return printTree(vt, "");
}

} // namespace atmt 
