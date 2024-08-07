#pragma once

#include "types.h"
#include "state_manager.hpp"

namespace atmt {

struct Grid {
  struct Beat {
    u32 bar = 1;
    u32 beat = 1;
    f64 x = 0;
  };

  struct TimeSignature {
    u32 numerator = 4;
    u32 denominator = 4;
  };

  void reset(f64, f64);
  f64 snap(f64);

  void narrow();
  void widen();
  void triplet();

  TimeSignature ts;
  static constexpr f64 intervalMin = 40;
  f64 snapInterval = 0;
  bool tripletMode = false;
  i32 gridWidth = 0;

  std::vector<Beat> beats;
  std::vector<f64> lines;
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
  static constexpr i32 trimThreshold = 20;
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
  static constexpr i32 size = 10;
  static constexpr i32 posOffset = size / 2;
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
  static constexpr i32 mouseOverDistance = 10;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AutomationLane)
};

struct Track : juce::Component, juce::ValueTree::Listener, juce::DragAndDropTarget, juce::Timer {
  Track(StateManager&, UIBridge&);
  void paint(juce::Graphics&) override;
  void resized() override;
  void timerCallback() override;
  i32 getTrackWidth();
  void rebuildClips();
  bool isInterestedInDragSource(const juce::DragAndDropTarget::SourceDetails&) override;
  void itemDropped(const juce::DragAndDropTarget::SourceDetails&) override;
  void valueTreeChildAdded(juce::ValueTree&, juce::ValueTree&) override;
  void valueTreeChildRemoved(juce::ValueTree&, juce::ValueTree&, i32) override;
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

  static constexpr i32 timelineHeight = 20;
  static constexpr i32 presetLaneHeight = 25;
  static constexpr i32 height = 200;

  juce::Rectangle<i32> timelineBounds;
  juce::Rectangle<i32> presetLaneTopBounds;
  juce::Rectangle<i32> presetLaneBottomBounds;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Track)
};

} // namespace atmt
