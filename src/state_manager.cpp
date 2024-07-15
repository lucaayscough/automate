#include "state_manager.hpp"
#include "plugin.hpp"

namespace atmt {

StateManager::StateManager(juce::AudioProcessorValueTreeState& _apvts)
  : apvts(_apvts) {
  undoManager = apvts.undoManager;
  init();
}

void StateManager::init() {
  JUCE_ASSERT_MESSAGE_THREAD
    
  state.appendChild(parameters, undoManager);
  state.appendChild(edit, undoManager);
  state.appendChild(presets, undoManager);

  edit.setProperty(ID::automation, {}, undoManager);
  edit.setProperty(ID::zoom, defaultZoomValue, undoManager);

  undoManager->clearUndoHistory();
  validate();

  state.addListener(this);
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
}

void StateManager::removeClip(const juce::ValueTree& vt) {
  edit.removeChild(vt, undoManager);
}

void StateManager::removeClipsIfInvalid(const juce::var& preset) {
  JUCE_ASSERT_MESSAGE_THREAD

  for (int i = 0; i < edit.getNumChildren(); ++i) {
    const auto& clip = edit.getChild(i);
    if (clip[ID::name].toString() == preset.toString()) {
      edit.removeChild(i, undoManager);
      --i;
    }
  }
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
  auto cmp = [] (const std::unique_ptr<Clip>& a, const std::unique_ptr<Clip>& b) { return a->start < b->start; };
  std::sort(clips.begin(), clips.end(), cmp);

  juce::Path automation;
  for (auto& clip : clips) {
    automation.lineTo(float(clip->start), clip->top ? 0 : 1);
  }

  edit.setProperty(ID::automation, automation.toString(), undoManager);
}

void StateManager::valueTreeChildAdded(juce::ValueTree& parent, juce::ValueTree& child) {
  JUCE_ASSERT_MESSAGE_THREAD

  if (parent.hasType(ID::EDIT)) {
    if (child.hasType(ID::CLIP)) {
      clips.emplace_back(std::make_unique<Clip>(child, undoManager)); 
    }
    updateAutomation();
  }
}

void StateManager::valueTreeChildRemoved(juce::ValueTree& parent, juce::ValueTree& child, int) {
  JUCE_ASSERT_MESSAGE_THREAD

  if (child.hasType(ID::PRESET)) {
    removeClipsIfInvalid(child[ID::name]);
  } else if (parent.hasType(ID::EDIT)) {
    if (child.hasType(ID::CLIP)) {
      // TODO: don't really know where this should go
      auto cond = [&] (const std::unique_ptr<Clip>& c) { return child == c->state; };
      auto it = std::find_if(clips.begin(), clips.end(), cond);

      if (it != clips.end()) {
        clips.erase(it);
      }
    }

    updateAutomation();
  }
}

void StateManager::valueTreePropertyChanged(juce::ValueTree& vt, const juce::Identifier& id) {
  JUCE_ASSERT_MESSAGE_THREAD

  if (vt.hasType(ID::CLIP)) {
    if (id == ID::start || id == ID::top) {
      updateAutomation();
    }
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
