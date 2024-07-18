#pragma once

#include "state_attachment.hpp"
#include <juce_gui_basics/juce_gui_basics.h>
#include "plugin.hpp"

namespace atmt {

struct DebugTools : juce::Component {
  DebugTools(StateManager& m) : manager(m) {
    addAndMakeVisible(printStateButton);
    addAndMakeVisible(editModeButton);

    printStateButton.onClick = [this] { DBG(manager.valueTreeToXmlString(manager.state)); };
    editModeButton  .onClick = [this] { editModeAttachment.setValue({ !editMode }); };
  }
  
  void resized() override {
    auto r = getLocalBounds();
    printStateButton.setBounds(r.removeFromLeft(getWidth() / 2));
    editModeButton.setBounds(r);
  }

  void editModeChangeCallback(const juce::var& v) {
    editModeButton.setToggleState(bool(v), juce::NotificationType::dontSendNotification);
  }

  StateManager& manager;
  juce::ValueTree editTree { manager.edit };
  juce::CachedValue<bool> editMode { editTree, ID::editMode, nullptr};
  juce::TextButton printStateButton { "Print State" };
  juce::ToggleButton editModeButton { "Edit Mode" };

  StateAttachment editModeAttachment { editTree, ID::editMode, STATE_CB(editModeChangeCallback), nullptr };
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

struct PresetsListPanel : juce::Component, juce::ValueTree::Listener {
  PresetsListPanel(StateManager& m) : manager(m) {
    addAndMakeVisible(title);
    addAndMakeVisible(presetNameInput);
    addAndMakeVisible(savePresetButton);

    savePresetButton.onClick = [this] () -> void {
      auto name = presetNameInput.getText();
      if (presetNameInput.getText() != "" && !manager.doesPresetNameExist(name)) {
        manager.savePreset(name);
      } else {
        juce::MessageBoxOptions options;
        juce::AlertWindow::showAsync(options.withTitle("Error").withMessage("Preset name unavailable."), nullptr);
      }
    };

    presetsTree.addListener(this);

    for (const auto& child : presetsTree) {
      addPreset(child);
    }
  }

  void resized() override {
    auto r = getLocalBounds(); 
    title.setBounds(r.removeFromTop(40));
    for (auto* preset : presets) {
      preset->setBounds(r.removeFromTop(30));
    }
    auto b = r.removeFromBottom(40);
    savePresetButton.setBounds(b.removeFromRight(getWidth() / 2));
    presetNameInput.setBounds(b);
  }

  void addPreset(const juce::ValueTree& presetTree) {
    jassert(presetTree.isValid());
    auto nameVar = presetTree[ID::name];
    jassert(!nameVar.isVoid());
    auto preset = new Preset(manager, nameVar.toString());
    presets.add(preset);
    addAndMakeVisible(preset);
    resized();
  }

  void removePreset(const juce::ValueTree& presetTree) {
    jassert(presetTree.isValid());
    auto nameVar = presetTree[ID::name];
    jassert(!nameVar.isVoid());
    auto name = nameVar.toString();
    for (int i = 0; i < presets.size(); ++i) {
      if (presets[i]->getName() == name) {
        removeChildComponent(presets[i]);
        presets.remove(i);
      }
    }
    resized();
  }

  int getNumPresets() {
    return presets.size();
  }

  void valueTreeChildAdded(juce::ValueTree&, juce::ValueTree& child) override {
    addPreset(child);
  }

  void valueTreeChildRemoved(juce::ValueTree&, juce::ValueTree& child, int) override { 
    removePreset(child);
  }

  struct Title : juce::Component {
    void paint(juce::Graphics& g) override {
      auto r = getLocalBounds();
      g.setColour(juce::Colours::white);
      g.setFont(getHeight());
      g.drawText(text, r, juce::Justification::centred);
    }

    juce::String text { "Presets" };
  };

  struct Preset : juce::Component {
    Preset(StateManager& m, const juce::String& name) : manager(m) {
      setName(name);
      selectorButton.setButtonText(name);

      addAndMakeVisible(selectorButton);
      addAndMakeVisible(removeButton);

      // TODO(luca): this may not be needed because it may be incompatible with our interface
      //selectorButton.onClick = [this] () -> void { proc.engine.restorePreset(getName()); };
      removeButton.onClick   = [this] () -> void { manager.removePreset(getName()); };
    }

    void resized() {
      auto r = getLocalBounds();
      r.removeFromLeft(getWidth() / 4);
      removeButton.setBounds(r.removeFromRight(getWidth() / 4));
      selectorButton.setBounds(r);
    }

    void mouseDown(const juce::MouseEvent&) {
      auto container = juce::DragAndDropContainer::findParentDragContainerFor(this);
      if (container) {
        container->startDragging(getName(), this);
      }
    }

    StateManager& manager;
    juce::TextButton selectorButton;
    juce::TextButton removeButton { "X" };
  };

  StateManager& manager;
  juce::ValueTree presetsTree { manager.presets };
  Title title;
  juce::OwnedArray<Preset> presets;
  juce::TextEditor presetNameInput;
  juce::TextButton savePresetButton { "Save Preset" };

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetsListPanel)
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
