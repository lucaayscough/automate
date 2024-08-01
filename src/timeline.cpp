#include "timeline.hpp"

namespace atmt {

void Grid::reset() {
  interval = 0;
  x = 0;
}

void Grid::calculateBeatInterval(double bpm, double zoom) {
  jassert(bpm > 0 && zoom > 0);
  reset();
  double mul = 1;
  while (interval < intervalMin) {
    interval = mul / bpm * zoom;
    mul += 1;
  }
}

double Grid::getNext() {
  x += interval;
  return x;
}

double Grid::getQuantized(double time) {
  int div = int(time / interval);
  double left  = div * interval;
  return time - left < interval / 2 ? left : left + interval;
}

ClipView::ClipView(StateManager& m, juce::ValueTree& vt, juce::UndoManager* um, Grid& g) : Clip(vt, um), manager(m), grid(g) {
  setTooltip(name);
}

void ClipView::paint(juce::Graphics& g) {
  g.setColour(juce::Colours::red);
  g.fillEllipse(getLocalBounds().toFloat());
}

void ClipView::mouseDown(const juce::MouseEvent& e) {
  beginDrag(e);
}

void ClipView::mouseDoubleClick(const juce::MouseEvent&) {
  manager.removeClip(state); 
}

void ClipView::mouseUp(const juce::MouseEvent&) {
  endDrag();
}

void ClipView::mouseDrag(const juce::MouseEvent& e) {
  auto parentLocalPoint = getParentComponent()->getLocalPoint(this, e.position);
  start.setValue(grid.getQuantized(parentLocalPoint.x - mouseDownOffset) / zoom, undoManager);
  if (parentLocalPoint.y > getHeight() / 2) {
    top.setValue(false, undoManager);
  } else {
    top.setValue(true, undoManager);
  }
}

void ClipView::beginDrag(const juce::MouseEvent& e) {
  undoManager->beginNewTransaction();
  mouseDownOffset = e.position.x;
}

void ClipView::endDrag() {
  isTrimDrag = false;
  mouseDownOffset = 0;
}

PathView::PathView(StateManager& m, juce::ValueTree& v, juce::UndoManager* um, Grid& g) : Path(v, um), manager(m), grid(g) {}

void PathView::paint(juce::Graphics& g) {
  if (isMouseOverOrDragging()) {
    g.setColour(juce::Colours::red);
  } else {
    g.setColour(juce::Colours::orange);
  }
  g.fillEllipse(getLocalBounds().toFloat());
}

void PathView::mouseDown(const juce::MouseEvent&) {
  undoManager->beginNewTransaction();
}

void PathView::mouseDrag(const juce::MouseEvent& e) {
  auto parentLocalPoint = getParentComponent()->getLocalPoint(this, e.position);
  auto newX = double(grid.getQuantized(parentLocalPoint.x) / zoom);
  auto newY = double(parentLocalPoint.y / getParentComponent()->getHeight());
  newY = std::clamp(newY, 0.0, 1.0);
  start.setValue(newX, undoManager);
  y.setValue(newY, undoManager);
}

void PathView::mouseDoubleClick(const juce::MouseEvent&) {
  manager.removePath(state);
}

AutomationLane::AutomationLane(StateManager& m, Grid& g) : manager(m), grid(g) {}

void AutomationLane::paint(juce::Graphics& g) {
  g.setColour(juce::Colours::orange);
  
  {
    auto p = automation.get();
    auto pos = p.getCurrentPosition();
    p.lineTo(float(getWidth() / zoom), pos.y);
    g.strokePath(p, juce::PathStrokeType { 2.f }, juce::AffineTransform::scale(float(zoom), getHeight()));
  }

  auto cond = [] (PathView* p) { return p->isMouseButtonDown() || p->isMouseOver(); };  
  auto it = std::find_if(paths.begin(), paths.end(), cond);
  if (it == paths.end()) {
    g.fillEllipse(hoverBounds);
  }
}

void AutomationLane::resized() {
  zoom.forceUpdateOfCachedValue();

  for (auto& path : paths) {
    path->setBounds(int(path->start * zoom) - PathView::posOffset,
                    int(path->y * getHeight()) - PathView::posOffset,
                    PathView::size, PathView::size);
  }
}

void AutomationLane::addPath(juce::ValueTree& v) {
  auto path = new PathView(manager, v, undoManager, grid);
  addAndMakeVisible(path);
  paths.add(path);
  resized();
}

// TODO(luca): clean up
void AutomationLane::mouseMove(const juce::MouseEvent& e) {
  auto x = e.position.x / zoom;
  auto hoverPoint = automation.getPointFromXIntersection(x); 
  hoverPoint.x = e.position.x;
  hoverPoint.y *= getHeight() * Automation::kExpand;
  hoverBounds.setCentre(hoverPoint);
  hoverBounds.setSize(10, 10);
}

// TODO(luca): clean up
void AutomationLane::mouseDown(const juce::MouseEvent& e) {
  auto x = e.position.x / zoom;
  auto hoverPoint = automation.getPointFromXIntersection(x); 
  hoverPoint.x = e.position.x;
  hoverPoint.y *= getHeight() * Automation::kExpand;
  manager.addPath(hoverPoint.x / zoom, hoverPoint.y / getHeight());
}

void AutomationLane::mouseExit(const juce::MouseEvent&) {
  hoverBounds = juce::Rectangle<float>();
}

Track::Track(StateManager& m, UIBridge& b) : manager(m), uiBridge(b) {
  addAndMakeVisible(automationLane);
  rebuildClips();
  editTree.addListener(this);
  startTimerHz(60);
  viewport.setViewedComponent(this, false);
  viewport.setScrollBarPosition(false, false);
  viewport.setScrollBarsShown(false, true, true, true);
}

void Track::paint(juce::Graphics& g) {
  auto r = getLocalBounds();
  g.fillAll(juce::Colours::grey);

  g.setColour(juce::Colours::coral);
  g.fillRect(timelineBounds);

  g.setColour(juce::Colours::aqua);
  g.fillRect(presetLaneTopBounds);
  g.fillRect(presetLaneBottomBounds);

  {
    auto bpm = uiBridge.bpm.load();
    grid.calculateBeatInterval(bpm, zoom);

    if (bpm > 0) {
      g.setColour(juce::Colours::darkgrey);

      auto x = grid.getNext();
      while (x < getWidth()) {
        g.fillRect(float(x), float(r.getY()), 1.f, float(getHeight()));
        x = grid.getNext();
      }
    }
  }

  g.setColour(juce::Colours::black);
  auto time = int(uiBridge.playheadPosition.load(std::memory_order_relaxed) * zoom);
  g.fillRect(time, r.getY(), 2, getHeight());
}

void Track::resized() {
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

void Track::timerCallback() {
  repaint();
}

// TODO(luca): tidy up 
void Track::rebuildClips() {
  clips.clear();
  automationLane.paths.clear();

  for (auto child : editTree) {
    if (child.hasType(ID::CLIP)) {
      auto clip = new ClipView(manager, child, undoManager, grid);
      addAndMakeVisible(clip);
      clips.add(clip);
    } else if (child.hasType(ID::PATH)) {
      automationLane.addPath(child);
    }
  }
}

bool Track::isInterestedInDragSource(const juce::DragAndDropTarget::SourceDetails&) {
  return true;
}

void Track::itemDropped(const juce::DragAndDropTarget::SourceDetails& details) {
  jassert(!details.description.isVoid());
  auto name = details.description.toString();
  auto preset = presets.getPresetFromName(name);
  auto id = preset->_id.load();
  auto start = grid.getQuantized(details.localPosition.x) / zoom;
  auto top = details.localPosition.y < getHeight() / 2;
  manager.addClip(id, name, start, top);
}

void Track::valueTreeChildAdded(juce::ValueTree&, juce::ValueTree&) {
  rebuildClips();
  resized();
}

void Track::valueTreeChildRemoved(juce::ValueTree&, juce::ValueTree&, int) { 
  rebuildClips();
  resized();
}

void Track::valueTreePropertyChanged(juce::ValueTree& v, const juce::Identifier& id) {
  if (v.hasType(ID::EDIT)) {
    if (id == ID::zoom) {
      resized();
      automationLane.resized();
    }
  } else if (v.hasType(ID::CLIP)) {
    if (id == ID::start || id == ID::top) {
      resized();
    } 
  } else if (v.hasType(ID::PATH)) {
    if (id == ID::start || id == ID::y) {
      automationLane.resized();
    }
  }
}

void Track::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w) {
  zoomTrack(w.deltaY, e.position.x);
}

void Track::zoomTrack(double amount, double mouseX) {
  double x0 = mouseX * zoom;
  double x1 = mouseX * zoom;
  double dx = (x1 - x0) / zoom;
  viewport.setViewPosition(int(viewport.getViewPositionX() + dx), 0);
  zoom.setValue(zoom + (amount * (zoom / zoomDeltaScale)), nullptr);
}

} // namespace atmt
