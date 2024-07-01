#pragma once

#include "change_attachment.hpp"
#include "components.hpp"
#include "plugin.hpp"

namespace atmt {

class PluginEditor : public juce::AudioProcessorEditor {
public:
  explicit PluginEditor(PluginProcessor&);

  void paint(juce::Graphics&) override;
  void resized() override;

private:
  void pluginIDChangeCallback(const juce::var&);
  void instanceChangeCallback();

  PluginProcessor& proc;
  juce::AudioProcessorValueTreeState& apvts;

  int width = 1280;
  int height = 720;

  int descriptionBarHeight = 100;
  int statesPanelWidth = 150;
  int pluginListWidth = 150;

  DescriptionBar descriptionBar;
  StatesListPanel statesPanel { proc };
  PluginListComponent pluginList { proc.apfm, proc.knownPluginList, proc.deadMansPedalFile, &proc.propertiesFile };

  ChangeAttachment instanceAttachment { proc.engine.instanceBroadcaster, CHANGE_CB(instanceChangeCallback) };
  StateAttachment pluginIDAttachment { apvts.state, ID::pluginID, STATE_CB(pluginIDChangeCallback), apvts.undoManager };

  std::unique_ptr<juce::AudioProcessorEditor> instanceEditor;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditor)
};

} // namespace atmt
