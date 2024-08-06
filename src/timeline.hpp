#pragma once

#include "types.h"
#include "state_manager.hpp"

namespace atmt {

struct Grid {
  struct BeatIndicator {
    int bar = 1;
    int beat = 1;
    int x = 0;
  };

  struct TimeSignature {
    u32 numerator = 4;
    u32 denominator = 4;
  };

  void reset(f64);
  BeatIndicator getNext();
  f64 getQuantized(f64);

  TimeSignature ts;
  static constexpr f64 intervalMin = 40;

  f64 x = 0;
  f64 interval = 0;

  u32 beatInterval = 0;
  u32 barInterval = 0;
  u32 barCount  = 0;
  u32 beatCount = 0;
};

struct ClipView : juce::Component, Clip, juce::SettableTooltipClient {
  ClipView(StateManager&, juce::ValueTree&, juce::UndoManager*, Grid&);
  void paint(juce::Graphics&) override;
  void mouseDown(const juce::MouseEvent&) override;
  void mouseDoubleClick(const juce::MouseEvent&) override;
  void mouseUp(const juce::MouseEvent&) override;
  void mouseDrag(const juce::MouseEvent&) override;
  void beginDrag(const juce::MouseEvent&);
  void endDrag();

  StateManager& manager;
  juce::ValueTree editValueTree { manager.editTree };
  Grid& grid;
  juce::CachedValue<f64> zoom { editValueTree, ID::zoom, nullptr };
  static constexpr int trimThreshold = 20;
  bool isTrimDrag = false;
  bool isLeftTrimDrag = false;
  f64 mouseDownOffset = 0;
};

struct PathView : juce::Component, Path {
  PathView(StateManager&, juce::ValueTree&, juce::UndoManager*, Grid&);
  void paint(juce::Graphics&) override;
  void mouseDown(const juce::MouseEvent&) override;
  void mouseDrag(const juce::MouseEvent&) override;
  void mouseDoubleClick(const juce::MouseEvent&) override;

  StateManager& manager;
  Grid& grid;
  juce::CachedValue<f64> zoom { manager.editTree, ID::zoom, nullptr };
  static constexpr int size = 10;
  static constexpr int posOffset = size / 2;
};

struct AutomationLane : juce::Component {
  AutomationLane(StateManager&, Grid&);
  void paint(juce::Graphics&) override;
  void resized() override;
  void addPath(juce::ValueTree&);
  void mouseMove(const juce::MouseEvent&) override;
  void mouseDown(const juce::MouseEvent&) override;
  void mouseExit(const juce::MouseEvent&) override;

  StateManager& manager;
  Grid& grid;
  juce::UndoManager* undoManager { manager.undoManager };
  juce::ValueTree editTree { manager.editTree };
  juce::CachedValue<f64> zoom { editTree, ID::zoom, nullptr };
  Automation automation { editTree, undoManager };
  juce::Rectangle<f32> hoverBounds;
  juce::OwnedArray<PathView> paths;
  static constexpr int mouseOverDistance = 10;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AutomationLane)
};

struct Track : juce::Component, juce::ValueTree::Listener, juce::DragAndDropTarget, juce::Timer {
  Track(StateManager&, UIBridge&);
  void paint(juce::Graphics&) override;
  void resized() override;
  void timerCallback() override;
  int getTrackWidth();
  void rebuildClips();
  bool isInterestedInDragSource(const juce::DragAndDropTarget::SourceDetails&) override;
  void itemDropped(const juce::DragAndDropTarget::SourceDetails&) override;
  void valueTreeChildAdded(juce::ValueTree&, juce::ValueTree&) override;
  void valueTreeChildRemoved(juce::ValueTree&, juce::ValueTree&, int) override;
  void valueTreePropertyChanged(juce::ValueTree&, const juce::Identifier&) override;
  void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
  void zoomTrack(f64, f64);

  StateManager& manager;
  UIBridge& uiBridge;
  juce::UndoManager* undoManager { manager.undoManager };
  juce::ValueTree editTree { manager.editTree };
  juce::ValueTree presetsTree { manager.presetsTree };
  Presets presets { presetsTree, undoManager };

  Grid grid;

  AutomationLane automationLane { manager, grid };
  juce::OwnedArray<ClipView> clips;
  juce::CachedValue<f64> zoom { editTree, ID::zoom, nullptr };
  static constexpr f64 zoomDeltaScale = 5.0;

  juce::Viewport viewport;

  static constexpr int timelineHeight = 20;
  static constexpr int presetLaneHeight = 25;
  static constexpr int height = 200;

  juce::Rectangle<int> timelineBounds;
  juce::Rectangle<int> presetLaneTopBounds;
  juce::Rectangle<int> presetLaneBottomBounds;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Track)
};

} // namespace atmt
