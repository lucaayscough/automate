#include "timeline.hpp"

namespace atmt {

void Grid::reset(f64 zoom) {
  jassert(zoom > 0);
  
  x = 0;
  barCount = 0;
  beatCount = 0;

  interval = zoom / (ts.denominator / 4.0);
  beatInterval = 1;
  barInterval = ts.numerator;

  while (interval * beatInterval < intervalMin) {
    beatInterval *= 2;

    if (beatInterval > barInterval) {
      beatInterval -= beatInterval % barInterval;
    } else {
      beatInterval += barInterval - beatInterval;
    }
  }

  interval *= beatInterval;
}

Grid::BeatIndicator Grid::getNext() {
  BeatIndicator b { int(barCount + 1), int(beatCount + 1), int(x) };

  x += interval;

  for (u32 i = 0; i < beatInterval; ++i) {
    beatCount = (beatCount + 1) % barInterval;

    if (!beatCount) {
      ++barCount;
    }
  }
  
  return b;
}

f64 Grid::getQuantized(f64 time) {
  int div = int(time / interval);
  f64 left  = div * interval;
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
  auto newX = f64(grid.getQuantized(parentLocalPoint.x) / zoom);
  auto newY = f64(parentLocalPoint.y / getParentComponent()->getHeight());
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
    p.lineTo(f32(getWidth() / zoom), pos.y);
    g.strokePath(p, juce::PathStrokeType { 2.f }, juce::AffineTransform::scale(f32(zoom), getHeight()));
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
  juce::Point<f32> hoverPoint;
  automation.get().getNearestPoint(e.position, hoverPoint, juce::AffineTransform::scale(f32(zoom), getHeight()));
  auto distance = hoverPoint.getDistanceFrom(e.position);
  if (distance < mouseOverDistance) {
    hoverBounds.setCentre(hoverPoint);
    hoverBounds.setSize(10, 10);
  } else {
    hoverBounds = juce::Rectangle<f32>();
  }
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
  hoverBounds = juce::Rectangle<f32>();
}

Track::Track(StateManager& m, UIBridge& b) : manager(m), uiBridge(b) {
  addAndMakeVisible(automationLane);
  rebuildClips();
  editTree.addListener(this);
  startTimerHz(60);
  viewport.setViewedComponent(this, false);
  viewport.setScrollBarPosition(false, false);
  viewport.setScrollBarsShown(false, true, true, true);
  setSize(getTrackWidth(), height);
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
    grid.ts.numerator = uiBridge.numerator;
    grid.ts.denominator = uiBridge.denominator;
    zoom.forceUpdateOfCachedValue();
    grid.reset(zoom);

    g.setFont(10);

    auto b = grid.getNext();
    while (b.x < getWidth()) {
      auto beatText = juce::String(b.bar);
      if (b.beat > 1) {
        beatText << "." + juce::String(b.beat);
      }

      g.setColour(juce::Colours::black);
      g.drawText(beatText, b.x, r.getY(), 40, 20, juce::Justification::left);

      g.setColour(juce::Colours::darkgrey);
      g.fillRect(f32(b.x), f32(r.getY()), 1.f, f32(getHeight()));
      b = grid.getNext();
    }
  }

  g.setColour(juce::Colours::black);
  auto time = uiBridge.playheadPosition.load();
  g.fillRect(int(time * zoom), r.getY(), 2, getHeight());
}

void Track::resized() {
  // TODO(luca): this will often run resized() twice
  setSize(getTrackWidth(), height);

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

int Track::getTrackWidth() {
  f64 width = 0;

  for (auto* c : clips) {
    if (c->start > width) {
      width = c->start;
    }
  }

  for (auto* p : automationLane.paths) {
    if (p->start > width) {
      width = p->start;
    }
  }

  width *= zoom;
  return width > getParentWidth() ? int(width) : getParentWidth();
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

  resized();
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
}

void Track::valueTreeChildRemoved(juce::ValueTree&, juce::ValueTree&, int) { 
  rebuildClips();
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
      resized();
      automationLane.resized();
    }
  }
}

void Track::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w) {
  zoomTrack(w.deltaY, e.position.x);
}

void Track::zoomTrack(f64 amount, f64 mouseX) {
  f64 x0 = mouseX * zoom;
  f64 x1 = mouseX * zoom;
  f64 dx = (x1 - x0) / zoom;
  viewport.setViewPosition(int(viewport.getViewPositionX() + dx), 0);
  zoom.setValue(zoom + (amount * (zoom / zoomDeltaScale)), nullptr);
}

} // namespace atmt
