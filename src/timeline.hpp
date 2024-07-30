#pragma once

#include "state_manager.hpp"

namespace atmt {

struct Grid {
  void reset();
  void calculateBeatInterval(double, double);
  double getNext();
  double getQuantized(double);

  double interval = 0;
  static constexpr double intervalMin = 20;
  double x = 0;
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
  juce::CachedValue<double> zoom { editValueTree, ID::zoom, nullptr };
  static constexpr int trimThreshold = 20;
  bool isTrimDrag = false;
  bool isLeftTrimDrag = false;
  double mouseDownOffset = 0;
};

struct PathView : juce::Component, Path {
  PathView(StateManager&, juce::ValueTree&, juce::UndoManager*, Grid&);
  void paint(juce::Graphics&) override;
  void mouseDown(const juce::MouseEvent&) override;
  void mouseDrag(const juce::MouseEvent&) override;
  void mouseUp(const juce::MouseEvent&) override;

  StateManager& manager;
  Grid& grid;
  juce::CachedValue<double> zoom { manager.editTree, ID::zoom, nullptr };
  double mouseDownOffset = 0;
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

  StateManager& manager;
  Grid& grid;
  juce::UndoManager* undoManager { manager.undoManager };
  juce::ValueTree editTree { manager.editTree };
  juce::CachedValue<double> zoom { editTree, ID::zoom, nullptr };
  Automation automation { editTree, undoManager };
  juce::Rectangle<float> hoverBounds;
  juce::OwnedArray<PathView> paths;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AutomationLane)
};

struct Track : juce::Component, juce::ValueTree::Listener, juce::DragAndDropTarget, juce::Timer {
  Track(StateManager&, UIBridge&);
  void paint(juce::Graphics&) override;
  void resized() override;
  void timerCallback() override;
  void rebuildClips();
  bool isInterestedInDragSource(const juce::DragAndDropTarget::SourceDetails&) override;
  void itemDropped(const juce::DragAndDropTarget::SourceDetails&) override;
  void valueTreeChildAdded(juce::ValueTree&, juce::ValueTree&) override;
  void valueTreeChildRemoved(juce::ValueTree&, juce::ValueTree&, int) override;
  void valueTreePropertyChanged(juce::ValueTree&, const juce::Identifier&) override;
  void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
  void zoomTrack(double, double);

  StateManager& manager;
  UIBridge& uiBridge;
  juce::UndoManager* undoManager { manager.undoManager };
  juce::ValueTree editTree { manager.editTree };
  juce::ValueTree presetsTree { manager.presetsTree };
  Presets presets { presetsTree, undoManager };

  Grid grid;

  AutomationLane automationLane { manager, grid };
  juce::OwnedArray<ClipView> clips;
  juce::CachedValue<double> zoom { editTree, ID::zoom, nullptr };
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
