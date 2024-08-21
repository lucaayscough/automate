#include "state_manager.hpp"
#include "plugin.hpp"

namespace atmt {

StateAttachment::StateAttachment(juce::ValueTree& s, const juce::Identifier& i, std::function<void(juce::var)> cb, juce::UndoManager* um)
  : state(s), identifier(i), callback(std::move(cb)), undoManager(um) {
  JUCE_ASSERT_MESSAGE_THREAD
  state.addListener(this);
  performUpdate();
}

StateAttachment::~StateAttachment() {
  state.removeListener(this);
}

void StateAttachment::setValue(const juce::var& v) {
  JUCE_ASSERT_MESSAGE_THREAD
  state.setProperty(identifier, v, undoManager);
}

juce::var StateAttachment::getValue() {
  JUCE_ASSERT_MESSAGE_THREAD
  return state[identifier];
}

void StateAttachment::performUpdate() {
  callback(getValue());
}

void StateAttachment::valueTreePropertyChanged(juce::ValueTree&, const juce::Identifier& i) {
  JUCE_ASSERT_MESSAGE_THREAD

  if (identifier == i) {
    performUpdate();
  }
}

Automation::Automation(juce::ValueTree& v, juce::UndoManager* um, juce::AudioProcessor* p=nullptr) : TreeWrapper(v, um), proc(p) {
  jassert(v.hasType(ID::EDIT));
  rebuild();
}

auto Automation::getPointFromXIntersection(f64 x) {
  return automation.getPointAlongPath(f32(x), juce::AffineTransform::scale(1, kFlat));
}

f64 Automation::getYFromX(f64 x) {
  return getPointFromXIntersection(x).y * kExpand;
}

ClipPath* Automation::getClipPathForX(f64 x) {
  for (auto it = clipPaths.begin(); it != clipPaths.end(); ++it) {
    if (it != clipPaths.begin()) {
      auto& a = *std::prev(it);
      auto& b = *it;

      if (x >= a->x && x <= b->x) {
        return b.get();
      }
    }
  }
  return nullptr;
}

void Automation::rebuild() {
  if (proc) proc->suspendProcessing(true);

  automation.clear();
  clipPaths.rebuild();
 
  for (auto& p : clipPaths) {
    auto c = automation.getCurrentPosition();
    
    auto curve = *p->curve;
    auto cx = c.x + (p->x - c.x) * (c.y < p->y ? curve : 1.0 - curve); 
    auto cy = (c.y < p->y ? c.y : p->y) + std::abs(p->y - c.y) * (1.0 - curve);
    
    automation.quadraticTo(f32(cx), f32(cy), f32(p->x), f32(p->y));
  }

  if (proc) proc->suspendProcessing(false);
}

juce::Path& Automation::get() { return automation; }

StateManager::StateManager(juce::AudioProcessorValueTreeState& a) : apvts(a) {
  rand.setSeedRandomly();
  init();
}

void StateManager::init() {
  JUCE_ASSERT_MESSAGE_THREAD
    
  state.appendChild(parametersTree, undoManager);
  state.appendChild(editTree, undoManager);
  state.appendChild(presetsTree, undoManager);

  editTree.setProperty(ID::editMode, false, nullptr);
  editTree.setProperty(ID::modulateDiscrete, true, nullptr);
  editTree.setProperty(ID::pluginID, {}, undoManager);
  editTree.setProperty(ID::zoom, defaultZoomValue, undoManager);

  undoManager->clearUndoHistory();
  validate();
}

void StateManager::replace(const juce::ValueTree& newState) {
  JUCE_ASSERT_MESSAGE_THREAD

  juce::MessageManagerLock lk(juce::Thread::getCurrentThread());

  if (lk.lockWasGained()) {
    parametersTree.copyPropertiesAndChildrenFrom(newState.getChildWithName(ID::PARAMETERS), undoManager);
    editTree.copyPropertiesAndChildrenFrom(newState.getChildWithName(ID::EDIT), undoManager);
    presetsTree.copyPropertiesAndChildrenFrom(newState.getChildWithName(ID::PRESETS), undoManager);

    undoManager->clearUndoHistory();
    validate();
  }
}

juce::ValueTree StateManager::getState() {
  JUCE_ASSERT_MESSAGE_THREAD

  juce::MessageManagerLock lk(juce::Thread::getCurrentThread());

  juce::ValueTree copy;

  if (lk.lockWasGained()) {
    copy = state.createCopy();
  }

  return copy;
}

void StateManager::validate() {
  JUCE_ASSERT_MESSAGE_THREAD

  jassert(state.isValid());
  jassert(parametersTree.isValid());
  jassert(editTree.isValid());
  jassert(presetsTree.isValid());

  jassert(editTree.hasProperty(ID::pluginID));
  jassert(editTree.hasProperty(ID::editMode));
  jassert(editTree.hasProperty(ID::modulateDiscrete));
  jassert(editTree.hasProperty(ID::zoom));
}

void StateManager::addClip(std::int64_t id, const juce::String& name, double x, double y) {
  JUCE_ASSERT_MESSAGE_THREAD
  juce::ValueTree clip { ID::CLIP };
  clip.setProperty(ID::id, id, undoManager)
      .setProperty(ID::name, name, undoManager)
      .setProperty(ID::x, x, undoManager)
      .setProperty(ID::y, y, undoManager)
      .setProperty(ID::curve, 0.5, undoManager);
  editTree.addChild(clip, -1, undoManager);
}

void StateManager::removeClip(const juce::ValueTree& vt) {
  editTree.removeChild(vt, undoManager);
}

void StateManager::removeClipsIfInvalid(const juce::var& preset) {
  for (int i = 0; i < editTree.getNumChildren(); ++i) {
    const auto& clip = editTree.getChild(i);
    if (clip[ID::name].toString() == preset.toString()) {
      editTree.removeChild(i, undoManager);
      --i;
    }
  }
}

void StateManager::clearEdit() {
  editTree.removeAllChildren(undoManager);
  undoManager->clearUndoHistory();
}

void StateManager::overwritePreset(const juce::String& name) {
  JUCE_ASSERT_MESSAGE_THREAD
  auto& _proc = *static_cast<Plugin*>(&proc);
  std::vector<f32> values;
  _proc.engine.getCurrentParameterValues(values);
  auto preset = presets.getPresetFromName(name); 
  preset->parameters.setValue(values, undoManager);
}

void StateManager::savePreset(const juce::String& name) {
  JUCE_ASSERT_MESSAGE_THREAD
  auto& _proc = *static_cast<Plugin*>(&proc);
  std::vector<f32> values;
  _proc.engine.getCurrentParameterValues(values);

  std::int64_t id = 0;
  {
    bool foundValidID = false;
    while (!foundValidID) {
      id = rand.nextInt64();
      auto cond = [id] (const std::unique_ptr<Preset>& p) { return p->_id == id; };
      auto it = std::find_if(presets.begin(), presets.end(), cond);
      if (it == presets.end()) {
        foundValidID = true; 
      }
    }
  }

  juce::ValueTree preset { ID::PRESET };
  preset.setProperty(ID::id, id, undoManager);
  preset.setProperty(ID::name, name, undoManager);
  preset.setProperty(ID::parameters, { values.data(), values.size() * sizeof(f32) }, undoManager);
  presetsTree.addChild(preset, -1, undoManager);
}

void StateManager::removePreset(const juce::String& name) {
  JUCE_ASSERT_MESSAGE_THREAD
  auto preset = presetsTree.getChildWithProperty(ID::name, name);
  presetsTree.removeChild(preset, undoManager);
  removeClipsIfInvalid(preset[ID::name]);
}

bool StateManager::doesPresetNameExist(const juce::String& name) {
  JUCE_ASSERT_MESSAGE_THREAD

  for (const auto& preset : presetsTree) {
    if (preset[ID::name].toString() == name) {
      return true;
    }
  }
  return false;
}

void StateManager::clearPresets() {
  presetsTree.removeAllChildren(undoManager);
  undoManager->clearUndoHistory();
}

void StateManager::addPath(double x, double y) {
  JUCE_ASSERT_MESSAGE_THREAD
  jassert(x >= 0 && y >= 0 && y <= 1);

  juce::ValueTree path(ID::PATH);
  path.setProperty(ID::x, x, undoManager)
      .setProperty(ID::y, y, undoManager)
      .setProperty(ID::curve, 0.5, undoManager);

  editTree.appendChild(path, undoManager);
}

void StateManager::removePath(const juce::ValueTree& v) {
  JUCE_ASSERT_MESSAGE_THREAD
  editTree.removeChild(v, undoManager);
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
