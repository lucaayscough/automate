#include "change_attachment.hpp"

namespace atmt {

ChangeAttachment::ChangeAttachment(juce::ChangeBroadcaster& _changeBroadcaster, std::function<void()> cb)
  : changeBroadcaster(_changeBroadcaster), callback(std::move(cb)) {
  changeBroadcaster.addChangeListener(this);
}

ChangeAttachment::~ChangeAttachment() {
  changeBroadcaster.removeChangeListener(this);
}

void ChangeAttachment::changeListenerCallback(juce::ChangeBroadcaster*) {
  callback();
}

} // namespace atmt 
