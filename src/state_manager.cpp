#include "state_manager.hpp"
#include "plugin.hpp"

namespace atmt {

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
  //editTree.setProperty(ID::pluginID, {}, undoManager);
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
}

void StateManager::addClip(std::int64_t id, const juce::String& name, double start, bool top) {
  JUCE_ASSERT_MESSAGE_THREAD

  juce::ValueTree clip { ID::CLIP };
  clip.setProperty(ID::id, id, undoManager)
      .setProperty(ID::name, name, undoManager)
      .setProperty(ID::start, start, undoManager)
      .setProperty(ID::top, top, undoManager);
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

void StateManager::savePreset(const juce::String& name) {
  JUCE_ASSERT_MESSAGE_THREAD

  auto& proc = *static_cast<PluginProcessor*>(&apvts.processor);
  std::vector<float> values;
  proc.engine.getCurrentParameterValues(values);

  std::int64_t id = 0;
  {
    bool foundValidID = false;
    while (!foundValidID) {
      id = rand.nextInt64();
      auto cond = [id] (const std::unique_ptr<Preset>& p) { return p->id == id; };
      auto it = std::find_if(presets.begin(), presets.end(), cond);
      if (it == presets.end()) {
        foundValidID = true; 
      }
    }
  }

  juce::ValueTree preset { ID::PRESET };
  preset.setProperty(ID::id, id, undoManager);
  preset.setProperty(ID::name, name, undoManager);
  preset.setProperty(ID::parameters, { values.data(), values.size() * sizeof(float) }, undoManager);
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
