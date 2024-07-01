#pragma once

#include <juce_data_structures/juce_data_structures.h>

#define STATE_CB(func) [this] (juce::var v) { func(v); }

namespace atmt {

class StateAttachment : public juce::ValueTree::Listener
{
public:
  StateAttachment(juce::ValueTree&, const juce::String&, std::function<void(juce::var)>, juce::UndoManager*);
  ~StateAttachment() override;
  void setValue(const juce::var& v);

private:
  juce::var getValue();
  void performUpdate();
  void valueTreePropertyChanged(juce::ValueTree&, const juce::Identifier& i) override;
  void valueTreeRedirected(juce::ValueTree&) override;

  juce::ValueTree state;
  juce::Identifier identifier;
  std::function<void(juce::var)> callback; 
  juce::UndoManager* undoManager;
};

} // namespace atmt
