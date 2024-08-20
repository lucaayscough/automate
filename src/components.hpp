#pragma once

#include "timeline.hpp"
#include <juce_gui_basics/juce_gui_basics.h>
#include "types.h"
#include "plugin.hpp"

namespace atmt {

struct DebugTools : juce::Component {
  DebugTools(StateManager&);
  void resized() override;
  void editModeChangeCallback(const juce::var&);
  void modulateDiscreteChangeCallback(const juce::var&);

  StateManager& manager;
  juce::AudioProcessor& proc { manager.apvts.processor };
  juce::UndoManager* undoManager { manager.undoManager };
  juce::ValueTree editTree { manager.editTree };
  juce::CachedValue<bool> editMode { editTree, ID::editMode, nullptr };
  juce::CachedValue<bool> modulateDiscrete { editTree, ID::modulateDiscrete, undoManager };
  juce::CachedValue<juce::String> pluginID { editTree, ID::pluginID, undoManager };
  juce::TextButton printStateButton { "Print State" };
  juce::TextButton killButton { "Kill" };
  juce::TextButton playButton { "Play" };
  juce::TextButton stopButton { "Stop" };
  juce::TextButton rewindButton { "Rewind" };
  juce::TextButton undoButton { "Undo" };
  juce::TextButton redoButton { "Redo" };
  juce::TextButton randomiseButton { "Randomise" };
  juce::ToggleButton parametersToggleButton { "Show Parameters" };
  juce::ToggleButton editModeButton { "Edit Mode" };
  juce::ToggleButton modulateDiscreteButton { "Modulate Discrete" };

  StateAttachment editModeAttachment { editTree, ID::editMode, STATE_CB(editModeChangeCallback), nullptr };
  StateAttachment modulateDiscreteAttachment { editTree, ID::modulateDiscrete, STATE_CB(modulateDiscreteChangeCallback), undoManager };
};

struct DebugInfo : juce::Component {
  DebugInfo(UIBridge&);
  void resized();

  UIBridge& uiBridge;
  juce::DrawableText info;
};

struct PresetsListPanel : juce::Component, juce::ValueTree::Listener {
  struct Title : juce::Component {
    void paint(juce::Graphics&) override;

    juce::String text { "Presets" };
  };

  struct Preset : juce::Component {
    Preset(StateManager&, const juce::String&);
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;

    StateManager& manager;
    juce::AudioProcessor& proc { manager.proc };
    juce::TextButton selectorButton;
    juce::TextButton removeButton { "X" };
    juce::TextButton overwriteButton { "Overwrite" };
  };

  PresetsListPanel(StateManager&);
  void resized() override;
  void addPreset(const juce::ValueTree&);
  void removePreset(const juce::ValueTree&);
  i32 getNumPresets();
  void valueTreeChildAdded(juce::ValueTree&, juce::ValueTree&) override;
  void valueTreeChildRemoved(juce::ValueTree&, juce::ValueTree&, i32) override;

  StateManager& manager;
  juce::ValueTree presetsTree { manager.presetsTree };
  Title title;
  juce::OwnedArray<Preset> presets;
  juce::TextEditor presetNameInput;
  juce::TextButton savePresetButton { "Save Preset" };

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetsListPanel)
};

struct PluginListView : juce::Viewport {
  struct Contents : juce::Component, juce::FileDragAndDropTarget {
    Contents(StateManager&, juce::AudioPluginFormatManager&, juce::KnownPluginList&);
    void paint(juce::Graphics&) override;
    void resized() override;
    void addPlugin(juce::PluginDescription&);
    bool isInterestedInFileDrag(const juce::StringArray&)	override;
    void filesDropped(const juce::StringArray&, i32, i32) override;

    StateManager& manager;
    juce::UndoManager* undoManager { manager.undoManager };
    juce::ValueTree editTree { manager.editTree };
    juce::CachedValue<juce::String> pluginID { editTree, ID::pluginID, undoManager };

    juce::KnownPluginList& knownPluginList;
    juce::AudioPluginFormatManager& formatManager;

    juce::OwnedArray<juce::TextButton> plugins;
    
    static constexpr i32 buttonHeight = 25;
  };
  
  Contents c;

  PluginListView(StateManager&, juce::AudioPluginFormatManager&, juce::KnownPluginList&);
  void resized();
};

struct DefaultView : juce::Component {
  DefaultView(StateManager& m, juce::KnownPluginList& kpl, juce::AudioPluginFormatManager& fm) : list(m, fm, kpl) {
    addAndMakeVisible(list);
  }

  void resized() override {
    list.setBounds(getLocalBounds());
  }

  PluginListView list;
};

struct ParametersView : juce::Viewport {
  struct Parameter : juce::Component, juce::AudioProcessorParameter::Listener {
    Parameter(juce::AudioProcessorParameter* p) : parameter(p) {
      jassert(parameter);
      parameter->addListener(this);
      name.setText(parameter->getName(150), juce::NotificationType::dontSendNotification);
      slider.setRange(0, 1);
      slider.setValue(parameter->getValue());
      addAndMakeVisible(name);
      addAndMakeVisible(slider);
    }

    ~Parameter() override {
      parameter->removeListener(this);
    }

    void paint(juce::Graphics& g) override {
      g.fillAll(juce::Colours::green);
    }

    void resized() override {
      auto r = getLocalBounds();
      name.setBounds(r.removeFromLeft(160));
      slider.setBounds(r);
    }

    void parameterValueChanged(i32, f32 v) override {
      juce::MessageManager::callAsync([v, this] {
        slider.setValue(v);
        repaint();
      });
    }
 
    void parameterGestureChanged(i32, bool) override {}

    juce::AudioProcessorParameter* parameter;
    juce::Slider slider;
    juce::Label name;
  };

  struct Contents : juce::Component {
    Contents(const juce::Array<juce::AudioProcessorParameter*>& i) {
      for (auto p : i) {
        jassert(p);
        auto param = new Parameter(p);
        addAndMakeVisible(param);
        parameters.add(param);
      }
      setSize(getWidth(), parameters.size() * 20);
    }

    void resized() override {
      auto r = getLocalBounds();
      for (auto p : parameters) {
        p->setBounds(r.removeFromTop(20)); 
      }
    }

    juce::OwnedArray<Parameter> parameters;
  };

  Contents c;

  ParametersView(const juce::Array<juce::AudioProcessorParameter*>& i) : c(i) {
    setViewedComponent(&c, false);
  }

  void resized() override {
    c.setSize(getWidth(), c.getHeight());
  }
};

struct MainView : juce::Component {
  MainView(StateManager& m, UIBridge& b, juce::AudioProcessorEditor* i, const juce::Array<juce::AudioProcessorParameter*>& p) : manager(m), uiBridge(b), instance(i), parametersView(p) {
    jassert(i);
    addAndMakeVisible(statesPanel);
    addAndMakeVisible(track.viewport);
    addAndMakeVisible(instance.get());
    addChildComponent(parametersView);
    setSize(instance->getWidth() + statesPanelWidth, instance->getHeight() + Track::height);
  }
  
  void resized() {
    // TODO(luca): clean up viewport stuff
    auto r = getLocalBounds();
    track.viewport.setBounds(r.removeFromBottom(Track::height));
    track.resized();
    statesPanel.setBounds(r.removeFromLeft(statesPanelWidth));
    instance->setBounds(r.removeFromTop(instance->getHeight()));
    parametersView.setBounds(instance->getBounds());
  }

  void toggleParametersView() {
    instance->setVisible(!instance->isVisible()); 
    parametersView.setVisible(!parametersView.isVisible()); 
  }

  void childBoundsChanged(juce::Component*) {
    setSize(instance->getWidth() + statesPanelWidth, instance->getHeight() + Track::height);
  }

  StateManager& manager;
  UIBridge& uiBridge;
  PresetsListPanel statesPanel { manager };
  Track track { manager, uiBridge };
  std::unique_ptr<juce::AudioProcessorEditor> instance;
  ParametersView parametersView;
  i32 statesPanelWidth = 150;
};

} // namespace atmt
