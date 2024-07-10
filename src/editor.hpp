#pragma once

#include "change_attachment.hpp"
#include "components.hpp"
#include "plugin.hpp"
#include "ui_bridge.hpp"
#include "timeline.hpp"

namespace atmt {

class PluginEditor : public juce::AudioProcessorEditor, public juce::DragAndDropContainer {
public:
  explicit PluginEditor(PluginProcessor&);

  void paint(juce::Graphics&) override;
  void resized() override;

private:
  void pluginIDChangeCallback(const juce::var&);
  void instanceChangeCallback();

  PluginProcessor& proc;
  StateManager& manager { proc.manager };
  UIBridge& uiBridge { proc.uiBridge };

  int width = 1280;
  int height = 720;

  int debugToolsHeight = 30;
  int descriptionBarHeight = 100;
  int statesPanelWidth = 150;
  int pluginListWidth = 150;
  int trackHeight = 150;

  DebugTools debugTools { manager };
  DescriptionBar descriptionBar;
  PresetsListPanel statesPanel { manager };
  PluginListComponent pluginList { proc.apfm, proc.knownPluginList, proc.deadMansPedalFile, &proc.propertiesFile };

  Track track { manager, uiBridge };

  ChangeAttachment instanceAttachment { proc.engine.instanceBroadcaster, CHANGE_CB(instanceChangeCallback) };
  StateAttachment pluginIDAttachment { manager.edit, ID::pluginID, STATE_CB(pluginIDChangeCallback), manager.undoManager };

  std::unique_ptr<juce::AudioProcessorEditor> instanceEditor;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditor)
};

} // namespace atmt
