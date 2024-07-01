#include "state_manager.hpp"

namespace atmt {

StateManager::StateManager(juce::AudioProcessorValueTreeState& _apvts)
  : apvts(_apvts) {
  undoManager = apvts.undoManager;
  state = apvts.state;
  init();
}

void StateManager::init() {
  JUCE_ASSERT_MESSAGE_THREAD
    
  undoable = state.getOrCreateChildWithName(ID::UNDOABLE, undoManager);
  states = undoable.getOrCreateChildWithName(ID::STATES, undoManager);
}

void StateManager::replace(const juce::ValueTree& newState) {
  JUCE_ASSERT_MESSAGE_THREAD

  juce::MessageManagerLock lk(juce::Thread::getCurrentThread());

  if (lk.lockWasGained())
  {
    state.copyPropertiesFrom(newState, apvts.undoManager);
    DBG(valueTreeToXmlString(apvts.state));
    undoManager->clearUndoHistory();
    validate();
  }
}

void StateManager::validate() {
  JUCE_ASSERT_MESSAGE_THREAD

  jassert(state.isValid());
  jassert(undoable.isValid());
  jassert(states.isValid());

  valueTreeToXmlString(state);
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