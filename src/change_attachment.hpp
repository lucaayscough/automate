#pragma once

#include <juce_events/juce_events.h>

#define CHANGE_CB(func) [this] () { func(); }

namespace atmt {

class ChangeAttachment : public juce::ChangeListener {
public:
  ChangeAttachment(juce::ChangeBroadcaster&, std::function<void()>);
  ~ChangeAttachment();

private:
  void changeListenerCallback(juce::ChangeBroadcaster*);

  juce::ChangeBroadcaster& changeBroadcaster;
  std::function<void()> callback; 
};

} // namespace atmt 
