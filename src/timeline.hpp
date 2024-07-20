#pragma once

#include "state_manager.hpp"

namespace atmt {

struct Track : juce::Component, juce::ValueTree::Listener, juce::DragAndDropTarget, juce::Timer {
  struct Clip : juce::Component, atmt::Clip, juce::SettableTooltipClient {
    Clip(StateManager& m, juce::ValueTree& vt, juce::UndoManager* um)
      : atmt::Clip(vt, um), manager(m) {
      setTooltip(name);
    }

    void paint(juce::Graphics& g) override {
      g.setColour(juce::Colours::red);
      g.fillEllipse(getLocalBounds().toFloat());
    }

    void mouseDown(const juce::MouseEvent& e) override {
      beginDrag(e);
    }

    void mouseDoubleClick(const juce::MouseEvent&) override {
      manager.removeClip(state); 
    }

    void mouseUp(const juce::MouseEvent&) override {
      endDrag();
    }

    void mouseDrag(const juce::MouseEvent& e) override {
      auto parentLocalPoint = getParentComponent()->getLocalPoint(this, e.position);
      start.setValue((parentLocalPoint.x - mouseDownOffset) / zoom, undoManager);

      if (parentLocalPoint.y > getHeight() / 2) {
        top.setValue(false, undoManager);
      } else {
        top.setValue(true, undoManager);
      }
    }

    void beginDrag(const juce::MouseEvent& e) {
      mouseDownOffset = e.position.x;
    }

    void endDrag() {
      isTrimDrag = false;
      mouseDownOffset = 0;
    }

    StateManager& manager;
    juce::ValueTree editValueTree { state.getParent() };

    // TODO(luca): create coordinate converters and move somewhere else
    juce::CachedValue<double> zoom { editValueTree, ID::zoom, undoManager };

    static constexpr int trimThreshold = 20;
    bool isTrimDrag = false;
    bool isLeftTrimDrag = false;
    double mouseDownOffset = 0;
  };

  struct AutomationLane : juce::Component {
    AutomationLane(StateManager& m) : manager(m) {}

    void paint(juce::Graphics& g) override {
      g.setColour(juce::Colours::orange);
      g.strokePath(automation, juce::PathStrokeType { 2.f }, juce::AffineTransform::scale(float(zoom), getHeight()));
    }

    StateManager& manager;
    juce::UndoManager* undoManager { manager.undoManager };
    juce::ValueTree editTree { manager.edit };
    juce::CachedValue<double> zoom { editTree, ID::zoom, undoManager };
    juce::CachedValue<juce::Path> automation { editTree, ID::automation, undoManager };
  };

  Track(StateManager& m, UIBridge& b) : manager(m), uiBridge(b) {
    addAndMakeVisible(automationLane);
    rebuildClips();
    editTree.addListener(this);
    startTimerHz(60);
    viewport.setViewedComponent(this, false);
    viewport.setScrollBarPosition(false, false);
    viewport.setScrollBarsShown(false, true, true, true);
  }

  void timerCallback() override {
    repaint(); // NOTE(luca): used for the playead
  }

  void paint(juce::Graphics& g) override {
    auto r = getLocalBounds();
    g.fillAll(juce::Colours::grey);

    g.setColour(juce::Colours::coral);
    g.fillRect(timelineBounds);

    g.setColour(juce::Colours::aqua);
    g.fillRect(presetLaneTopBounds);
    g.fillRect(presetLaneBottomBounds);

    g.setColour(juce::Colours::black);
    auto time = int(uiBridge.playheadPosition.load(std::memory_order_relaxed) * zoom);
    g.fillRect(time, r.getY(), 2, getHeight());
  }

  void resized() override {
    auto r = getLocalBounds();

    timelineBounds          = r.removeFromTop(timelineHeight);
    presetLaneTopBounds     = r.removeFromTop(presetLaneHeight);
    presetLaneBottomBounds  = r.removeFromBottom(presetLaneHeight);
    automationLane.setBounds(r);

    for (auto* clip : clips) {
      clip->setBounds(int(clip->start * zoom) - presetLaneHeight / 2,
                      clip->top ? presetLaneTopBounds.getY() : presetLaneBottomBounds.getY(),
                      presetLaneHeight, presetLaneHeight);
    }
  }

  void rebuildClips() {
    clips.clear();
    for (auto child : editTree) {
      addClip(child);
    }
  }

  void addClip(juce::ValueTree& clipValueTree) {
    auto clip = new Clip(manager, clipValueTree, undoManager);
    addAndMakeVisible(clip);
    clips.add(clip);
  }

  void removeClip(juce::ValueTree& clipValueTree) {
    auto name = clipValueTree[ID::name].toString();
    for (int i = 0; i < clips.size(); ++i) {
      if (clips[i]->state == clipValueTree) {
        clips.remove(i);
        break;
      }
    }
  }

  bool isInterestedInDragSource(const juce::DragAndDropTarget::SourceDetails&) override {
    return true;
  }

  void itemDropped(const juce::DragAndDropTarget::SourceDetails& details) override {
    jassert(!details.description.isVoid());
    auto name = details.description.toString();
    auto start = details.localPosition.x / zoom;
    auto top = details.localPosition.y < getHeight() / 2;
    manager.addClip(name, start, top);
  }

  void valueTreeChildAdded(juce::ValueTree&, juce::ValueTree& child) override {
    addClip(child);
    resized();
  }

  void valueTreeChildRemoved(juce::ValueTree&, juce::ValueTree& child, int) override { 
    removeClip(child);
    resized();
  }

  void valueTreePropertyChanged(juce::ValueTree& vt, const juce::Identifier& id) override {
    if (vt.hasType(ID::EDIT)) {
      if (id == ID::zoom) {
        resized();
      }
    } else if (vt.hasType(ID::CLIP)) {
      if (id == ID::start || id == ID::top) {
        resized();
      }
    }
  }

  void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override {
    zoomTrack(w.deltaY, e.position.x);
  }

  void zoomTrack(double amount, double mouseX) {
    double x0 = mouseX * zoom;
    zoom.setValue(zoom + (amount * (zoom / zoomDeltaScale)), undoManager);
    double x1 = mouseX * zoom;
    double dx = (x1 - x0) / zoom;
    viewport.setViewPosition(int(viewport.getViewPositionX() + dx), 0);
  }

  StateManager& manager;
  UIBridge& uiBridge;
  juce::UndoManager* undoManager { manager.undoManager };
  juce::ValueTree editTree { manager.edit };
  AutomationLane automationLane { manager };
  juce::OwnedArray<Clip> clips;
  juce::CachedValue<double> zoom { editTree, ID::zoom, undoManager };
  static constexpr double zoomDeltaScale = 5.0;
  juce::Viewport viewport;

  static constexpr int timelineHeight = 20;
  static constexpr int presetLaneHeight = 25;

  juce::Rectangle<int> timelineBounds;
  juce::Rectangle<int> presetLaneTopBounds;
  juce::Rectangle<int> presetLaneBottomBounds;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Track)
};

} // namespace atmt
