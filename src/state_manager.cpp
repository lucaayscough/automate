#include "state_manager.hpp"
#include "plugin.hpp"

namespace atmt {

StateManager::StateManager(juce::AudioProcessorValueTreeState& a) : apvts(a) {
  init();
}

void StateManager::init() {
  JUCE_ASSERT_MESSAGE_THREAD
    
  state.appendChild(parameters, undoManager);
  state.appendChild(edit, undoManager);
  state.appendChild(presets, undoManager);

  edit.setProperty(ID::editMode, false, nullptr);
  //edit.setProperty(ID::pluginID, {}, undoManager);
  edit.setProperty(ID::automation, {}, undoManager);
  edit.setProperty(ID::zoom, defaultZoomValue, undoManager);

  undoManager->clearUndoHistory();
  validate();
}

void StateManager::replace(const juce::ValueTree& newState) {
  JUCE_ASSERT_MESSAGE_THREAD

  juce::MessageManagerLock lk(juce::Thread::getCurrentThread());

  if (lk.lockWasGained()) {
    parameters.copyPropertiesAndChildrenFrom(newState.getChildWithName(ID::PARAMETERS), undoManager);
    edit.copyPropertiesAndChildrenFrom(newState.getChildWithName(ID::EDIT), undoManager);
    presets.copyPropertiesAndChildrenFrom(newState.getChildWithName(ID::PRESETS), undoManager);

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
  jassert(parameters.isValid());
  jassert(edit.isValid());
  jassert(presets.isValid());
}

void StateManager::addClip(const juce::String& name, double start, bool top) {
  JUCE_ASSERT_MESSAGE_THREAD

  juce::ValueTree clip { ID::CLIP };
  clip.setProperty(ID::start, start, undoManager)
      .setProperty(ID::top, top, undoManager)
      .setProperty(ID::name, name, undoManager);
  edit.addChild(clip, -1, undoManager);
  updateAutomation();
}

void StateManager::removeClip(const juce::ValueTree& vt) {
  edit.removeChild(vt, undoManager);
  updateAutomation();
}

void StateManager::removeClipsIfInvalid(const juce::var& preset) {
  for (int i = 0; i < edit.getNumChildren(); ++i) {
    const auto& clip = edit.getChild(i);
    if (clip[ID::name].toString() == preset.toString()) {
      edit.removeChild(i, undoManager);
      --i;
    }
  }

  updateAutomation();
}

void StateManager::savePreset(const juce::String& name) {
  JUCE_ASSERT_MESSAGE_THREAD

  auto& proc = *static_cast<PluginProcessor*>(&apvts.processor);
  std::vector<float> values;
  proc.engine.getCurrentParameterValues(values);
  juce::ValueTree preset { ID::PRESET };
  preset.setProperty(ID::name, name, undoManager);
  preset.setProperty(ID::parameters, { values.data(), values.size() * sizeof(float) }, undoManager);
  presets.addChild(preset, -1, undoManager);
}

void StateManager::removePreset(const juce::String& name) {
  JUCE_ASSERT_MESSAGE_THREAD

  auto preset = presets.getChildWithProperty(ID::name, name);
  presets.removeChild(preset, undoManager);
  removeClipsIfInvalid(preset[ID::name]);
}

bool StateManager::doesPresetNameExist(const juce::String& name) {
  JUCE_ASSERT_MESSAGE_THREAD

  for (const auto& preset : presets) {
    if (preset[ID::name].toString() == name) {
      return true;
    }
  }
  return false;
}

void StateManager::updateAutomation() {
  juce::Path automation;
  for (auto& clip : clips.clips) {
    automation.lineTo(float(clip->start), clip->top ? 0 : 1);
  }

  edit.setProperty(ID::automation, automation.toString(), undoManager);
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
