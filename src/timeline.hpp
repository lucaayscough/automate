#pragma once

#include "state_manager.hpp"

namespace atmt {

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
    setClipOverlaps();
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
    manager.addClip(name, start);
  }

  void valueTreeChildAdded(juce::ValueTree&, juce::ValueTree& child) override {
    addClip(child);
  }

  void valueTreeChildRemoved(juce::ValueTree&, juce::ValueTree& child, int) override { 
    removeClip(child);
  }

  void valueTreePropertyChanged(juce::ValueTree& vt, const juce::Identifier& id) override {
    if (vt.hasType(ID::CLIP)) {
      if (id == ID::start || id == ID::end) {
        resized();
      }
    }
  }

  struct Overlap {
    double start = 0;
    double end = 0;
  };

  void setClipOverlaps() {
    for (auto clip : clips) {
      clip->clearOverlaps();
      for (auto other : clips) {
        if (clip == other) {
          continue;
        } else {
          if (clip->start <= other->start && clip->end >= other->start) {
            if (clip->end >= other->end) {
              clip->addOverlap(other->start - clip->start, other->end - clip->start);
            } else {
              clip->addOverlap(other->start - clip->start, clip->end - clip->start);
            }
          } else if (clip->start >= other->start && other->end >= clip->start) {
            clip->addOverlap(0, other->end - clip->start);
          }
        }
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

  struct Clip : juce::Component, atmt::Clip {
    Clip(juce::ValueTree& vt, juce::UndoManager* um) : atmt::Clip(vt, um) {}

    void paint(juce::Graphics& g) {
      g.fillAll(juce::Colours::red);
      g.drawRect(getLocalBounds());
      g.drawText(name, getLocalBounds(), juce::Justification::centred);
      
      if (isOverlapping()) {
        for (auto& overlap : overlaps) {
          auto r = getLocalBounds().toFloat();
          r.removeFromLeft(float(overlap.start));
          r.removeFromRight(float(getWidth() - overlap.end));
          g.fillCheckerBoard(r, 5, 5, juce::Colours::blue, juce::Colours::yellow);
        }
      }
    }

    bool isOverlapping() {
      return overlaps.size() > 0 ? true : false;
    }

    void mouseDown(const juce::MouseEvent& e) {
      beginDrag(e);
    }

    void mouseUp(const juce::MouseEvent&) {
      endDrag();
    }

    void mouseDrag(const juce::MouseEvent& e) {
      auto parentLocalPoint = getParentComponent()->getLocalPoint(this, e.position);
      if (isTrimDrag) {
        if (isLeftTrimDrag) {
          start.setValue((parentLocalPoint.x - mouseDownOffset) / zoom, undoManager);
        } else {
          end.setValue((parentLocalPoint.x + mouseDownOffset) / zoom, undoManager);
        }
      } else {
        auto length = getLength();
        start.setValue((parentLocalPoint.x - mouseDownOffset) / zoom, undoManager);
        end.setValue(start + length, undoManager);
      }
    }

    void beginDrag(const juce::MouseEvent& e) {
      mouseDownOffset = e.position.x;
      if (e.position.x < trimThreshold) {
        isTrimDrag = true;
        isLeftTrimDrag = true;
      } else if (e.position.x > (getLength() * zoom) - trimThreshold) {
        mouseDownOffset = (getLength() * zoom) - e.position.x;
        isTrimDrag = true;
        isLeftTrimDrag = false;
      }
    }

    void endDrag() {
      isTrimDrag = false;
      mouseDownOffset = 0;
    }

    double getLength() {
      return end - start;
    }

    juce::ValueTree editValueTree { state.getParent() };

    // TODO(luca): create coordinate converters and move somewhere else
    juce::CachedValue<double> zoom { editValueTree, ID::zoom, undoManager };

    static constexpr int trimThreshold = 20;
    bool isTrimDrag = false;
    bool isLeftTrimDrag = false;
    double mouseDownOffset = 0;

    void clearOverlaps() {
      overlaps.clear();
    }

    void addOverlap(double overlapStart, double overlapEnd) {
      overlaps.push_back({ overlapStart * zoom, overlapEnd * zoom }); 
    }

    std::vector<Overlap> overlaps;
  };

  StateManager& manager;
  UIBridge& uiBridge;
  juce::UndoManager* undoManager { manager.undoManager };
  juce::ValueTree editTree { manager.edit };
  juce::OwnedArray<Clip> clips;
  juce::CachedValue<double> zoom { editTree, ID::zoom, undoManager };
  static constexpr double zoomDeltaScale = 5.0;
  juce::Viewport viewport;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Track)
};

} // namespace atmt
