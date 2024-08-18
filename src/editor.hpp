#pragma once

#include "change_attachment.hpp"
#include "components.hpp"
#include "plugin.hpp"
#include "ui_bridge.hpp"
#include "timeline.hpp"

namespace atmt {

struct Editor : juce::AudioProcessorEditor, juce::DragAndDropContainer {
  explicit Editor(Plugin&);

  void paint(juce::Graphics&) override;
  void resized() override;

  void showDefaultScreen();
  void showInstanceScreen();
  void showParametersView();
  void showInstanceView();

  void pluginIDChangeCallback(const juce::var&);
  void createInstanceChangeCallback();
  void killInstanceChangeCallback();
  void childBoundsChanged(juce::Component*) override;
  bool keyPressed(const juce::KeyPress&) override;
  void modifierKeysChanged(const juce::ModifierKeys&) override;

  Plugin& proc;
  StateManager& manager { proc.manager };
  UIBridge& uiBridge { proc.uiBridge };

  juce::TooltipWindow tooltipWindow { this, 0 };

  int width  = 350;
  int height = 350;

  int debugToolsHeight = 30;
  int descriptionBarHeight = 100;
  int statesPanelWidth = 150;
  int pluginListWidth = 150;
  int debugInfoHeight = 60;

  bool showInstance = true;

  DebugTools debugTools { manager };
  DescriptionBar descriptionBar;
  PresetsListPanel statesPanel { manager };
  PluginListComponent pluginList { manager, proc.apfm, proc.knownPluginList };
  DebugInfo debugInfo { uiBridge };

  ParametersView parametersView;
  Track track { manager, uiBridge };

  ChangeAttachment createInstanceAttachment { proc.engine.createInstanceBroadcaster, CHANGE_CB(createInstanceChangeCallback) };
  ChangeAttachment killInstanceAttachment   { proc.engine.killInstanceBroadcaster, CHANGE_CB(killInstanceChangeCallback) };
  StateAttachment pluginIDAttachment        { manager.editTree, ID::pluginID, STATE_CB(pluginIDChangeCallback), manager.undoManager };

  std::unique_ptr<juce::AudioProcessorEditor> instance;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Editor)
};

} // namespace atmt
