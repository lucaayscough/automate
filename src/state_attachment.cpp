#include "state_attachment.hpp"

namespace atmt {

StateAttachment::StateAttachment(juce::ValueTree& _state,
                const juce::Identifier& _identifier,
                std::function<void(juce::var)> _callback,
                juce::UndoManager* _undoManager)
  : state(_state),
    identifier(_identifier),
    callback(std::move(_callback)),
    undoManager(_undoManager) {
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

void StateAttachment::valueTreeRedirected(juce::ValueTree&) {
  jassertfalse;
}

} // namespace atmt
