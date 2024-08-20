#pragma once

#include "change_attachment.hpp"
#include "components.hpp"
#include "plugin.hpp"
#include "ui_bridge.hpp"

namespace atmt {

struct Editor : juce::AudioProcessorEditor, juce::DragAndDropContainer {
  explicit Editor(Plugin&);

  void paint(juce::Graphics&) override;
  void resized() override;

  void showDefaultView();
  void showMainView();

  void pluginIDChangeCallback(const juce::var&);
  void createInstanceChangeCallback();
  void killInstanceChangeCallback();
  void childBoundsChanged(juce::Component*) override;
  bool keyPressed(const juce::KeyPress&) override;
  void modifierKeysChanged(const juce::ModifierKeys&) override;

  Plugin& proc;
  StateManager& manager { proc.manager };
  UIBridge& uiBridge { proc.uiBridge };
  Engine& engine { proc.engine };

  juce::TooltipWindow tooltipWindow { this, 0 };

  int width  = 350;
  int height = 350;

  int debugToolsHeight = 30;
  int debugInfoHeight = 60;

  bool useMainView = false;

  DebugTools debugTools { manager };
  DebugInfo debugInfo { uiBridge };

  std::unique_ptr<MainView> mainView;
  DefaultView defaultView { manager, proc.knownPluginList, proc.apfm };

  ChangeAttachment createInstanceAttachment { proc.engine.createInstanceBroadcaster, CHANGE_CB(createInstanceChangeCallback) };
  ChangeAttachment killInstanceAttachment   { proc.engine.killInstanceBroadcaster, CHANGE_CB(killInstanceChangeCallback) };
  StateAttachment  pluginIDAttachment       { manager.editTree, ID::pluginID, STATE_CB(pluginIDChangeCallback), manager.undoManager };

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Editor)
};

} // namespace atmt
