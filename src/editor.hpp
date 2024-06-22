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

  int pluginListHeight = 400;
  PluginListComponent pluginList { proc.apfm,
                                   proc.knownPluginList,
                                   proc.deadMansPedalFile,
                                   &proc.propertiesFile };

  std::unique_ptr<juce::AudioProcessorEditor> pluginInstanceEditor;
  StateAttachment pluginInstanceAttachment {apvts.state, IDENTIFIER_PLUGIN_INSTANCE, [this] (juce::var v) {
    if (!v.isVoid() && proc.pluginInstance && proc.pluginInstance->hasEditor())
    {
      pluginInstanceEditor.reset(proc.pluginInstance->createEditor());
      addAndMakeVisible(pluginInstanceEditor.get());
      setSize(pluginInstanceEditor->getWidth(), pluginInstanceEditor->getHeight() + pluginListHeight);
    }
    else
    {
      pluginInstanceEditor.reset();
    }
  }, apvts.undoManager};

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditor)
};

