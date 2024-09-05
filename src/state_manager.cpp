#include "state_manager.hpp"
#include "plugin.hpp"
#include "editor.hpp"
#include <assert.h>

namespace atmt {

StateManager::StateManager(juce::AudioProcessor& a) : proc(a) {
  rand.setSeedRandomly();
}

void StateManager::init() {
  plugin = static_cast<Plugin*>(&proc); 
  engine = &plugin->engine;
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
      auto parameters_ = (f32*)mb->getData();
      auto numParameters = mb->getSize() / sizeof(f32);
      preset.parameters.reserve(numParameters); 
      for (u32 i = 0; i < numParameters; ++i) {
        preset.parameters.emplace_back(parameters_[i]); 
      }
    }

    auto clipsTree = tree.getChild(1);
    for (auto c : clipsTree) {
      clips.emplace_back();
      auto& clip = clips.back();
      auto name = c["name"].toString();

      for (auto& p : presets) {
        if (name == p.name) {
          clip.preset = &p; 
        }
      }

      clip.x = c["x"];
      clip.y = c["y"];
      clip.c = c["c"];
    }

    auto pathsTree = tree.getChild(2);
    for (auto p : pathsTree) {
      paths.emplace_back();
      auto& path = paths.back();

      path.x = p["x"];
      path.y = p["y"];
      path.c = p["c"];
    }

    DBG(valueTreeToXmlString(tree));

    setPluginID(tree["pluginID"]);
    updateTrack(); 
    updatePresetList();
  }
}

juce::ValueTree StateManager::getState() {
  JUCE_ASSERT_MESSAGE_THREAD

  juce::MessageManagerLock lk(juce::Thread::getCurrentThread());

  if (lk.lockWasGained()) {
    juce::ValueTree tree("tree");

    tree.setProperty("zoom", zoom, nullptr)
        .setProperty("editMode", editMode.load(), nullptr)
        .setProperty("modulateDiscrete", modulateDiscrete.load(), nullptr)
        .setProperty("pluginID", pluginID, nullptr);

    juce::ValueTree presetsTree("presets");

    for (auto& p : presets) {
      juce::ValueTree preset("preset");
      preset.setProperty("name", p.name, nullptr)
            .setProperty("parameters", { p.parameters.data(), p.parameters.size() * sizeof(f32) }, nullptr);
      presetsTree.appendChild(preset, nullptr);
    }

    juce::ValueTree clipsTree("clips");
    for (auto& c : clips) {
      juce::ValueTree clip("clip");
      clip.setProperty("x", c.x, nullptr)
          .setProperty("y", c.y, nullptr)
          .setProperty("c", c.c, nullptr)
          .setProperty("name", c.preset->name, nullptr);
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

    tree.appendChild(presetsTree, nullptr);
    tree.appendChild(clipsTree, nullptr);
    tree.appendChild(pathsTree, nullptr);

    DBG(valueTreeToXmlString(tree));
    return tree;
  }

  return {};
}

void StateManager::addClip(Preset* p, f64 x, f64 y) {
  JUCE_ASSERT_MESSAGE_THREAD
  assert(x >= 0 && y >= 0 && y <= 1);

  proc.suspendProcessing(true);

  clips.emplace_back();
  auto& c = clips.back();
  c.preset = p;
  c.name = p->name;
  c.x = x;
  c.y = y;

  proc.suspendProcessing(false);

  updateTrack();
}

void StateManager::removeClip(Clip* c) {
  JUCE_ASSERT_MESSAGE_THREAD
  assert(c);

  proc.suspendProcessing(true);

  std::erase_if(clips, [c] (Clip& o) { return c == &o; });

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

  proc.suspendProcessing(false);

  updateTrack();
}

void StateManager::overwritePreset(Preset* preset) {
  JUCE_ASSERT_MESSAGE_THREAD

  proc.suspendProcessing(true);

  preset->parameters.clear();
  preset->parameters.reserve(parameters.size());

  for (auto& parameter : parameters) {
    preset->parameters.push_back(parameter.parameter->getValue());
  }

  proc.suspendProcessing(false);
}

void StateManager::loadPreset(Preset* preset) {
  JUCE_ASSERT_MESSAGE_THREAD

  proc.suspendProcessing(true);

  if (engine->hasInstance()) {
    if (!editMode) {
      setEditMode(true);
    }

    auto& presetParameters = preset->parameters;

    for (u32 i = 0; i < parameters.size(); ++i) {
      auto parameter = parameters[i]; 
      assert(presetParameters[i] >= 0.f && presetParameters[i] <= 1.f);

      if (std::abs(parameter.parameter->getValue() - presetParameters[i]) > EPSILON) {
        if (captureParameterChanges) {
          setParameterActive(i, true);
        }

        if (shouldProcessParameter(&parameter)) {
          parameter.parameter->setValue(presetParameters[i]);
        }
      }
    }
  }

  proc.suspendProcessing(false);
}

void StateManager::randomiseParameters() {
  JUCE_ASSERT_MESSAGE_THREAD

  proc.suspendProcessing(true);  

  setEditMode(true);

  for (auto& p : parameters) {
    if (shouldProcessParameter(&p)) {
      p.parameter->setValue(rand.nextFloat());
    }
  }

  proc.suspendProcessing(false);
}

bool StateManager::shouldProcessParameter(Parameter* p) {
  // TODO(luca): make active responsible for discrete and bool parameters too
  if (p->active) {
    return p->parameter->isDiscrete() ? modulateDiscrete.load() : true; 
  }
  return false;
}

void StateManager::savePreset(const juce::String& name) {
  JUCE_ASSERT_MESSAGE_THREAD

  proc.suspendProcessing(true);

  assert(engine->hasInstance());

  presets.emplace_back();
  auto& preset = presets.back();
  preset.name = name;
  overwritePreset(&presets.back());

  proc.suspendProcessing(false);

  updatePresetList();
}

void StateManager::removePreset(Preset* preset) {
  JUCE_ASSERT_MESSAGE_THREAD

  proc.suspendProcessing(true);

  std::erase_if(clips, [preset] (Clip& c) { return c.preset == preset; });
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

  paths.emplace_back();

  auto& path = paths.back();
  path.x = x;
  path.y = y;

  proc.suspendProcessing(false);

  updateTrack();
}

void StateManager::removePath(Path* p) {
  JUCE_ASSERT_MESSAGE_THREAD

  proc.suspendProcessing(true);

  std::erase_if(paths, [p] (Path& o) { return p == &o; });

  proc.suspendProcessing(false);

  updateTrack();
}

void StateManager::movePath(Path* p, f64 x, f64 y, f64 c) {
  JUCE_ASSERT_MESSAGE_THREAD

  proc.suspendProcessing(true);

  p->x = x < 0 ? 0 : x;
  p->y = std::clamp(y, 0.0, 1.0);
  p->c = std::clamp(c, 0.0, 1.0);

  proc.suspendProcessing(false);

  updateTrack(); 
}

void StateManager::setPluginID(const juce::String& id) {
  DBG("StateManager::setPluginID");

  proc.suspendProcessing(true);

  pluginID = id;
  juce::String errorMessage;

  if (id != "") {
    auto description = plugin->knownPluginList.getTypeForIdentifierString(id);

    if (description) {
      auto instance = plugin->apfm.createPluginInstance(*description, proc.getSampleRate(), proc.getBlockSize(), errorMessage);
      
      auto processorParameters = instance->getParameters();
      u32 numParameters = u32(processorParameters.size());
      parameters.clear();
      parameters.reserve(numParameters);

      for (u32 i = 0; i < numParameters; ++i) {
        parameters.emplace_back();
        parameters.back().parameter = processorParameters[i32(i)];
      }

      engine->setPluginInstance(instance);
      proc.prepareToPlay(proc.getSampleRate(), proc.getBlockSize());
    }
  } else {
    engine->kill();
    clear();
  }

  proc.suspendProcessing(false);
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

void StateManager::setCaptureParameterChanges(bool v) {
  JUCE_ASSERT_MESSAGE_THREAD
  
  captureParameterChanges = v;

  if (v) {
    for (auto& p : parameters) {
      p.active = false;
    }
  }

  updateParametersView(); 
}

void StateManager::setParameterActive(u32 i, bool a) {
  JUCE_ASSERT_MESSAGE_THREAD

  parameters[i].active = a;
  updateParametersView();
}

void StateManager::clear() {
  proc.suspendProcessing(true);

  paths.clear();
  clips.clear();
  presets.clear();
  parameters.clear();

  // TODO(luca): this is temporary hack to avoid the vector reallocating
  // its memory when adding new element invalidating all pointers
  paths.reserve(1000);
  clips.reserve(1000);
  presets.reserve(1000);
  parameters.reserve(1000);

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

void StateManager::updateParametersView() {
  if (parametersView) {
    parametersView->updateParameters();
  }
}

void StateManager::updateAutomation() {
  DBG("StateManager::updateAutomation()");

  proc.suspendProcessing(true);

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
