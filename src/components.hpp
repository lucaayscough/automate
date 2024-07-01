#pragma once

#include "state_attachment.hpp"
#include <juce_gui_basics/juce_gui_basics.h>
#include "plugin.hpp"

namespace atmt {

struct Transport : juce::Component {
  Transport() {}

  void paint(juce::Graphics& g) override {
    g.fillAll(juce::Colours::grey);
  }
  
  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Transport)
};

struct DescriptionBar : juce::Component {
  void paint(juce::Graphics& g) override {
    g.setColour(juce::Colours::white);
    g.setFont(getHeight());
    
    if (description) {
      g.drawText(description->name, getLocalBounds(), juce::Justification::left);
    }
  }

  void setDescription(std::unique_ptr<juce::PluginDescription>& desc) {
    description = std::move(desc);
    repaint();
  }

  std::unique_ptr<juce::PluginDescription> description;
};

struct StatesListPanel : juce::Component, juce::ValueTree::Listener {
  StatesListPanel(PluginProcessor& _proc)
    : proc(_proc) {
    addAndMakeVisible(title);
    addAndMakeVisible(stateNameInput);
    addAndMakeVisible(saveStateButton);

    saveStateButton.onClick = [this] () -> void {
      if (stateNameInput.getText() != "") {
        proc.engine.saveParameterState(stateNameInput.getText());
      }
    };

    proc.manager.states.addListener(this);
  }

  ~StatesListPanel() override {
    proc.manager.states.removeListener(this);
  }

  void resized() override {
    auto r = getLocalBounds(); 
    title.setBounds(r.removeFromTop(40));
    for (auto* state : states) {
      state->setBounds(r.removeFromTop(30));
    }
    auto b = r.removeFromBottom(40);
    saveStateButton.setBounds(b.removeFromRight(getWidth() / 2));
    stateNameInput.setBounds(b);
  }

  void addState(const juce::String& name) {
    auto state = new State(proc, name);
    states.add(state);
    addAndMakeVisible(state);
    resized();
  }

  void removeState(const juce::String& name) {
    for (int i = 0; i < states.size(); ++i) {
      if (states[i]->getName() == name) {
        removeChildComponent(states[i]);
        states.remove(i);
      }
    }
    resized();
  }

  int getNumStates() {
    return states.size();
  }

  void valueTreeChildAdded(juce::ValueTree& parent, juce::ValueTree& child) override {
    JUCE_ASSERT_MESSAGE_THREAD
    jassert(parent.isValid() && child.isValid());
    auto nameVar = child[ID::STATE::name];
    jassert(!nameVar.isVoid());
    addState(nameVar.toString());
  }

  void valueTreeChildRemoved(juce::ValueTree& parent, juce::ValueTree& child, int) override { 
    JUCE_ASSERT_MESSAGE_THREAD
    jassert(parent.isValid() && child.isValid());
    auto nameVar = child[ID::STATE::name];
    jassert(!nameVar.isVoid());
    removeState(nameVar.toString());
  }

  struct Title : juce::Component {
    void paint(juce::Graphics& g) override
    {
      auto r = getLocalBounds();
      g.setColour(juce::Colours::white);
      g.setFont(getHeight());
      g.drawText(text, r, juce::Justification::centred);
    }

    juce::String text { "States" };
  };

  struct State : juce::Component {
    State(PluginProcessor& _proc, const juce::String& name) : proc(_proc) {
      setName(name);
      selectorButton.setButtonText(name);

      addAndMakeVisible(selectorButton);
      addAndMakeVisible(removeButton);

      selectorButton.onClick = [this] () -> void { proc.engine.restoreParameterState(getName()); };
      removeButton.onClick   = [this] () -> void { proc.engine.removeParameterState(getName()); };
    }

    void resized() {
      auto r = getLocalBounds();
      removeButton.setBounds(r.removeFromRight(getWidth() / 4));
      selectorButton.setBounds(r);
    }

    PluginProcessor& proc;
    juce::TextButton selectorButton;
    juce::TextButton removeButton { "X" };
  };

  PluginProcessor& proc;
  Title title;
  juce::OwnedArray<State> states;
  juce::TextEditor stateNameInput;
  juce::TextButton saveStateButton { "Save State" };

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StatesListPanel)
};

class PluginListComponent : public juce::Component {
public:
  PluginListComponent(juce::AudioPluginFormatManager &formatManager,
                      juce::KnownPluginList &listToRepresent,
                      const juce::File &deadMansPedalFile,
                      juce::PropertiesFile* propertiesToUse)
    : pluginList { formatManager, listToRepresent, deadMansPedalFile, propertiesToUse } {
    addAndMakeVisible(pluginList);
  }

  void resized() override {
    auto r = getLocalBounds();
    pluginList.setBounds(r.removeFromTop(400));
  }

private:
  juce::PluginListComponent pluginList;
  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginListComponent)
};

} // namespace atmt
