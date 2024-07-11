#pragma once

#include "state_manager.hpp"

namespace atmt {

struct Track : juce::Component, juce::ValueTree::Listener, juce::DragAndDropTarget, juce::Timer {
  struct Clip : juce::Component, atmt::Clip {
    Clip(juce::ValueTree& vt, juce::UndoManager* um) : atmt::Clip(vt, um) {}

    void paint(juce::Graphics& g) {
      g.fillAll(juce::Colours::red);
      g.drawRect(getLocalBounds());
      g.drawText(name, getLocalBounds(), juce::Justification::centred);
    }

    void mouseDown(const juce::MouseEvent& e) {
      beginDrag(e);
    }

    void mouseUp(const juce::MouseEvent&) {
      endDrag();
    }

    void mouseDrag(const juce::MouseEvent& e) {
      auto parentLocalPoint = getParentComponent()->getLocalPoint(this, e.position);
      start.setValue((parentLocalPoint.x - mouseDownOffset) / zoom, undoManager);
    }

    void beginDrag(const juce::MouseEvent& e) {
      mouseDownOffset = e.position.x;
    }

    void endDrag() {
      isTrimDrag = false;
      mouseDownOffset = 0;
    }

    juce::ValueTree editValueTree { state.getParent() };

    // TODO(luca): create coordinate converters and move somewhere else
    juce::CachedValue<double> zoom { editValueTree, ID::zoom, undoManager };

    static constexpr int trimThreshold = 20;
    bool isTrimDrag = false;
    bool isLeftTrimDrag = false;
    double mouseDownOffset = 0;
  };

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
    g.setColour(juce::Colours::orange);
    g.strokePath(automation, juce::PathStrokeType { 1.f });
  }

  void resized() override {
    automation.clear();
    for (auto* clip : clips) {
      clip->setBounds(int(clip->start * zoom), clip->top ? 0 : getHeight() - 25, 25, 25);
      automation.lineTo(clip->getBounds().getCentre().toFloat());
    }
  }

  void rebuildClips() {
    clips.clear();
    for (auto child : editTree) {
      addClip(child);
    }
  }

  void addClip(juce::ValueTree& clipValueTree) {
    auto clip = new Clip(clipValueTree, undoManager);
    addAndMakeVisible(clip);
    clips.add(clip);
    resized();
  }

  void removeClip(juce::ValueTree& clipValueTree) {
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
    auto start = details.localPosition.x / zoom;
    auto top = details.localPosition.y < getHeight() / 2;
    manager.addClip(name, start, top);
  }

  void valueTreeChildAdded(juce::ValueTree&, juce::ValueTree& child) override {
    addClip(child);
  }

  void valueTreeChildRemoved(juce::ValueTree&, juce::ValueTree& child, int) override { 
    removeClip(child);
  }

  void valueTreePropertyChanged(juce::ValueTree& vt, const juce::Identifier& id) override {
    if (vt.hasType(ID::CLIP)) {
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
    resized();
  }

  StateManager& manager;
  UIBridge& uiBridge;
  juce::UndoManager* undoManager { manager.undoManager };
  juce::ValueTree editTree { manager.edit };
  juce::OwnedArray<Clip> clips;
  juce::CachedValue<double> zoom { editTree, ID::zoom, undoManager };
  static constexpr double zoomDeltaScale = 5.0;
  juce::Viewport viewport;
  juce::Path automation;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Track)
};

} // namespace atmt
