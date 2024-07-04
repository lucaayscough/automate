#pragma once

#include "state_attachment.hpp"
#include <juce_gui_basics/juce_gui_basics.h>
#include "plugin.hpp"

namespace atmt {

struct DebugTools : juce::Component {
  DebugTools(StateManager& m) : manager(m) {
    addAndMakeVisible(printStateButton);
    printStateButton.onClick = [this] {
      DBG(manager.valueTreeToXmlString(manager.state));
    };
  }
  
  void resized() override {
    printStateButton.setBounds(getLocalBounds());
  }

  StateManager& manager;
  juce::TextButton printStateButton { "Print State" };
};

struct Track : juce::Component, juce::ValueTree::Listener, juce::DragAndDropTarget, juce::Timer {
  Track(StateManager& m, UIBridge& b) : manager(m), uiBridge(b) {
    rebuildClips();
    editTree.addListener(this);
    startTimerHz(60);
    viewport.setViewedComponent(this, false);
    viewport.setScrollBarPosition(false, false);
    viewport.setScrollBarsShown(false, true, true, true);
  }

  void timerCallback() override {
    repaint();
  }

  void paint(juce::Graphics& g) override {
    auto r = getLocalBounds();
    g.fillAll(juce::Colours::grey);
    g.setColour(juce::Colours::black);
    auto time = int(uiBridge.playheadPosition.load(std::memory_order_relaxed) * zoom);
    g.fillRect(time, r.getY(), 2, getHeight());
  }

  void resized() override {
    for (auto* clip : clips) {
      clip->setBounds(int(clip->start * zoom), 0, int(clip->getLength() * zoom), getHeight());
    }
  }

  void rebuildClips() {
    clips.clear();
    for (const auto& child : editTree) {
      addClip(child);
    }
  }

  void addClip(const juce::ValueTree& clipValueTree) {
    auto clip = new Clip(clipValueTree, undoManager);
    clip->start = int(clipValueTree[ID::start]);
    clip->end = int(clipValueTree[ID::end]);
    clip->name = clipValueTree[ID::name].toString();
    addAndMakeVisible(clip);
    clips.add(clip);
    resized();
  }

  void removeClip(const juce::ValueTree& clipValueTree) {
    auto name = clipValueTree[ID::name].toString();
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
    auto start = int(details.localPosition.x / zoom);
    manager.addEditClip(name, start);
  }

  void valueTreeChildAdded(juce::ValueTree&, juce::ValueTree& child) override {
    JUCE_ASSERT_MESSAGE_THREAD
    addClip(child);
  }

  void valueTreeChildRemoved(juce::ValueTree&, juce::ValueTree& child, int) override { 
    JUCE_ASSERT_MESSAGE_THREAD
    removeClip(child);
  }

  void valueTreePropertyChanged(juce::ValueTree& vt, const juce::Identifier& id) override {
    if (vt.hasType(ID::CLIP)) {
      if (id == ID::start || id == ID::end) {
        resized();
      }
    }
  }

  void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& w) override {
    zoom.setValue(zoom + w.deltaY, undoManager);
    resized();
  }

  struct Clip : juce::Component {
    Clip(const juce::ValueTree& vt, juce::UndoManager* um) : undoManager(um), clipValueTree(vt) {}

    void paint(juce::Graphics& g) {
      g.fillAll(juce::Colours::red);
      g.drawText(name, getLocalBounds(), juce::Justification::centred);
    }

    void mouseDown(const juce::MouseEvent& e) {
      beginDrag(e);
    }

    void mouseUp(const juce::MouseEvent&) {
      endDrag();
    }

    void mouseDrag(const juce::MouseEvent& e) {
      auto parentLocalPoint = getParentComponent()->getLocalPoint(this, e.position).toInt();
      if (isTrimDrag) {
        if (isLeftTrimDrag) {
          start.setValue(int((parentLocalPoint.x - mouseDownOffset) / zoom), undoManager);
        } else {
          end.setValue(int((parentLocalPoint.x + mouseDownOffset) / zoom), undoManager);
        }
      } else {
        int length = getLength();
        start.setValue(int((parentLocalPoint.x - mouseDownOffset) / zoom), undoManager);
        end.setValue(start + length, undoManager);
      }
    }

    void beginDrag(const juce::MouseEvent& e) {
      mouseDownOffset = e.position.x;
      if (e.position.x < trimThreashold * zoom) {
        isTrimDrag = true;
        isLeftTrimDrag = true;
      } else if (e.position.x > (getLength() - trimThreashold) * zoom) {
        mouseDownOffset = (getLength() * zoom) - e.position.x;
        isTrimDrag = true;
        isLeftTrimDrag = false;
      }
    }

    void endDrag() {
      isTrimDrag = false;
      mouseDownOffset = 0;
    }

    int getLength() {
      return end - start;
    }

    juce::UndoManager* undoManager;
    juce::ValueTree clipValueTree; 
    juce::ValueTree editValueTree   { clipValueTree.getParent() };
    juce::CachedValue<int> start    { clipValueTree, ID::start, undoManager };
    juce::CachedValue<int> end      { clipValueTree, ID::end, undoManager };
    juce::CachedValue<float> zoom   { editValueTree, ID::zoom, undoManager };
    juce::String name;

    static constexpr int trimThreashold = 20;
    bool isTrimDrag = false;
    bool isLeftTrimDrag = false;
    float mouseDownOffset = 0;
  };

  StateManager& manager;
  UIBridge& uiBridge;
  juce::UndoManager* undoManager { manager.undoManager };
  juce::ValueTree editTree { manager.edit };
  juce::OwnedArray<Clip> clips;
  juce::CachedValue<float> zoom { editTree, ID::zoom, undoManager };
  juce::Viewport viewport;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Track)
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
