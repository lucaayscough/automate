#pragma once

#include "ui_components.hpp"
#include "plugin.hpp"

class PluginEditor : public juce::AudioProcessorEditor 
{
public:
  explicit PluginEditor(PluginProcessor&);

  void paint(juce::Graphics&) override;
  void resized() override;

private:
  PluginProcessor& proc;
  juce::AudioProcessorValueTreeState& apvts;

  int statesPanelWidth = 300;
  int pluginListHeight = 300;

  StatesListPanel statesPanel { proc };

  PluginListComponent pluginList { proc.apfm,
                                   proc.knownPluginList,
                                   proc.deadMansPedalFile,
                                   &proc.propertiesFile };

  std::unique_ptr<juce::AudioProcessorEditor> pluginInstanceEditor;
  atmt::StateAttachment pluginInstanceAttachment { apvts.state, IDENTIFIER_PLUGIN_INSTANCE, [this] (juce::var) {
    pluginInstanceEditor.reset(proc.engine.getEditor());
    
    if (pluginInstanceEditor)
    {
      addAndMakeVisible(pluginInstanceEditor.get());
      setSize(pluginInstanceEditor->getWidth() + statesPanelWidth, pluginInstanceEditor->getHeight() + pluginListHeight);
    }
  }, apvts.undoManager };

  juce::TextButton saveStateButton { "Save State" };

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditor)
};

