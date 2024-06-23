#pragma once

#include "state_attachment.hpp"
#include <juce_gui_basics/juce_gui_basics.h>
#include "plugin.hpp"

struct StatesListPanel : juce::Component
{
  StatesListPanel(PluginProcessor& _proc)
    : proc(_proc)
  {
    addAndMakeVisible(title);
  }

  void resized() override
  {
    auto r = getLocalBounds(); 
    title.setBounds(r.removeFromTop(40));
    for (auto* state : states)
    {
      state->setBounds(r.removeFromTop(30));
    }
  }

  void addState()
  {
    int index = states.size();
    auto state = new State(index);
    states.add(state);
    addAndMakeVisible(state);

    state->onClick = [this, index] () -> void {
      proc.engine.restoreParameterState(index); 
    };

    resized();
  }

  void removeState(int index)
  {
    removeChildComponent(index);
    states.remove(index); 
    resized();
  }

  struct Title : juce::Component
  {
    void paint(juce::Graphics& g) override
    {
      auto r = getLocalBounds();
      g.setFont(getHeight());
      g.drawText(text, r, juce::Justification::centred);
    }

    juce::String text { "States" };
  };

  struct State : juce::TextButton
  {
    State(int _index)
      : juce::TextButton(juce::String(_index)), index(_index) {}

    int index = 0;
  };

  PluginProcessor& proc;
  Title title;
  juce::OwnedArray<State> states;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StatesListPanel)
};

class PluginListComponent : public juce::Component
{
public:
  PluginListComponent(juce::AudioPluginFormatManager &formatManager,
                      juce::KnownPluginList &listToRepresent,
                      const juce::File &deadMansPedalFile,
                      juce::PropertiesFile* propertiesToUse)
    : pluginList { formatManager, listToRepresent, deadMansPedalFile, propertiesToUse }
  {
    addAndMakeVisible(pluginList);
  }

  void resized() override
  {
    auto r = getLocalBounds();
    pluginList.setBounds(r.removeFromTop(400));
  }

private:
  juce::PluginListComponent pluginList;
  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginListComponent)
};
