#pragma once

#include "ui_components.hpp"
#include "plugin.hpp"

class PluginEditor : public juce::AudioProcessorEditor, public juce::ChangeListener
{
public:
  explicit PluginEditor(PluginProcessor&);
  ~PluginEditor() override;

  void changeListenerCallback(juce::ChangeBroadcaster*) override;
  void paint(juce::Graphics&) override;
  void resized() override;

private:
  void instanceDescriptionUpdateCallback(juce::var);
  void instanceUpdateCallback();

  PluginProcessor& proc;
  juce::AudioProcessorValueTreeState& apvts;

  int width = 1280;
  int height = 720;

  int descriptionBarHeight = 100;
  int statesPanelWidth = 150;
  int pluginListWidth = 150;

  DescriptionBar descriptionBar;
  StatesListPanel statesPanel { proc };

  PluginListComponent pluginList { proc.apfm,
                                   proc.knownPluginList,
                                   proc.deadMansPedalFile,
                                   &proc.propertiesFile };

  std::unique_ptr<juce::AudioProcessorEditor> instanceEditor;
  atmt::StateAttachment instanceDescriptionAttachment { apvts.state,
                                                        IDENTIFIER_PLUGIN_INSTANCE,
                                                        [this] (juce::var v) { instanceDescriptionUpdateCallback(v); },
                                                        apvts.undoManager };

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditor)
};

