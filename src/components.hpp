#pragma once

#include "state_attachment.hpp"
#include <juce_gui_basics/juce_gui_basics.h>
#include "plugin.hpp"

namespace atmt {

struct Transport : juce::Component, juce::ValueTree::Listener, juce::DragAndDropTarget {
  Transport(const juce::ValueTree& vt, juce::UndoManager* um) : editTree(vt), undoManager(um) {
    rebuildClips();
    editTree.addListener(this);
  }

  ~Transport() override {
    editTree.removeListener(this);
  }

  void paint(juce::Graphics& g) override {
    g.fillAll(juce::Colours::grey);
  }

  void resized() override {
    for (auto* clip : clips) {
      clip->setBounds(clip->start, 0, clip->end - clip->start, getHeight()); 
    }
  }

  void rebuildClips() {
    clips.clear();
    for (const auto& child : editTree) {
      addClip(child);
    }
  }

  void addClip(const juce::ValueTree& clipTree) {
    auto clip = new Clip();
    clip->start = int(clipTree[ID::CLIP::start]);
    clip->end = int(clipTree[ID::CLIP::end]);
    clip->name = clipTree[ID::CLIP::name].toString();
    addAndMakeVisible(clip);
    clips.add(clip);
    resized();
  }

  void removeClip(const juce::ValueTree& clipTree) {
    auto name = clipTree[ID::CLIP::name].toString();
    for (int i = 0; i < clips.size(); ++i) {
      if (clips[i]->name == name) {
        clips.remove(i);
      }
    }
  }

  bool isInterestedInDragSource(const juce::DragAndDropTarget::SourceDetails&) override {
    return true;
  }

  void itemDropped(const juce::DragAndDropTarget::SourceDetails& details) override {
    jassert(!details.description.isVoid());
    auto name = details.description.toString();
    juce::ValueTree clip { ID::CLIP::type };
    clip.setProperty(ID::CLIP::start, details.localPosition.x, undoManager)
        .setProperty(ID::CLIP::end, details.localPosition.x + 20, undoManager)
        .setProperty(ID::CLIP::name, name, undoManager);
    editTree.addChild(clip, -1, undoManager);
  }

  void valueTreeChildAdded(juce::ValueTree&, juce::ValueTree& child) override {
    JUCE_ASSERT_MESSAGE_THREAD
    addClip(child);
  }

  void valueTreeChildRemoved(juce::ValueTree&, juce::ValueTree& child, int) override { 
    JUCE_ASSERT_MESSAGE_THREAD
    removeClip(child);
  }

  struct Clip : juce::Component {
    void paint(juce::Graphics& g) {
      g.fillAll(juce::Colours::red);
    }

    int start = 0;
    int end = 0;
    juce::String name;
  };

  juce::ValueTree editTree;
  juce::UndoManager* undoManager;
  juce::OwnedArray<Clip> clips;

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

struct PresetsListPanel : juce::Component, juce::ValueTree::Listener {
  PresetsListPanel(PluginProcessor& _proc)
    : proc(_proc) {
    addAndMakeVisible(title);
    addAndMakeVisible(presetNameInput);
    addAndMakeVisible(savePresetButton);

    savePresetButton.onClick = [this] () -> void {
      auto name = presetNameInput.getText();
      if (presetNameInput.getText() != "" && !manager.doesPresetNameExist(name)) {
        proc.engine.savePreset(name);
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
    auto nameVar = presetTree[ID::PRESET::name];
    jassert(!nameVar.isVoid());
    auto preset = new Preset(proc, nameVar.toString());
    presets.add(preset);
    addAndMakeVisible(preset);
    resized();
  }

  void removePreset(const juce::ValueTree& presetTree) {
    jassert(presetTree.isValid());
    auto nameVar = presetTree[ID::PRESET::name];
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
    JUCE_ASSERT_MESSAGE_THREAD
    addPreset(child);
  }

  void valueTreeChildRemoved(juce::ValueTree&, juce::ValueTree& child, int) override { 
    JUCE_ASSERT_MESSAGE_THREAD
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
    Preset(PluginProcessor& _proc, const juce::String& name) : proc(_proc) {
      setName(name);
      selectorButton.setButtonText(name);

      addAndMakeVisible(selectorButton);
      addAndMakeVisible(removeButton);

      selectorButton.onClick = [this] () -> void { proc.engine.restorePreset(getName()); };
      removeButton.onClick   = [this] () -> void { proc.engine.removePreset(getName()); };
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

    PluginProcessor& proc;
    juce::TextButton selectorButton;
    juce::TextButton removeButton { "X" };
  };

  PluginProcessor& proc;
  StateManager& manager { proc.manager };
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
