#include "timeline.hpp"

namespace atmt {

void Grid::reset(f64 z, f64 mw, TimeSignature _ts) {
  jassert(z > 0);

  if (zoom != z || maxWidth != mw || ts.numerator != _ts.numerator || ts.denominator != _ts.denominator) {
    zoom = z;
    maxWidth = mw;
    ts = _ts;
    reset();
  }
}

void Grid::reset() {
  DBG("Grid::reset())");

  jassert(zoom > 0 && maxWidth > 0 && ts.numerator > 0 && ts.denominator > 0);

  // TODO(luca): there is still some stuff to work out with the triplet grid
  lines.clear();
  beats.clear();
  
  u32 barCount = 0;
  u32 beatCount = 0;

  f64 pxInterval = zoom / (ts.denominator / 4.0);
  u32 beatInterval = 1;
  u32 barInterval = ts.numerator;

  while (pxInterval * beatInterval < intervalMin) {
    beatInterval *= 2;

    if (beatInterval > barInterval) {
      beatInterval -= beatInterval % barInterval;
    } else {
      beatInterval += barInterval - beatInterval;
    }
  }

  pxInterval *= beatInterval;

  f64 pxTripletInterval = pxInterval * 2 / 3;
  f64 x = 0;
  f64 tx = 0;
  u32 numSubIntervals = u32(std::abs(gridWidth)) * 2;
  
  f64 subInterval = 0;
  if (gridWidth < 0) {
    subInterval = tripletMode ? pxTripletInterval : pxInterval / f64(numSubIntervals);
  } else if (gridWidth > 0) {
    subInterval = tripletMode ? pxTripletInterval : pxInterval / (1.0 / f64(numSubIntervals));
  } else {
    subInterval = tripletMode ? pxTripletInterval : pxInterval; 
  }

  u32 count = 0;

  while (x < maxWidth || tx < maxWidth) {
    Beat b { barCount + 1, beatCount + 1, x };
    beats.push_back(b);

    if (gridWidth < 0) {
      for (u32 i = 0; i < numSubIntervals; ++i) {
        lines.push_back((tripletMode ? tx : x) + f64(i) * subInterval);
      }
      lines.push_back(tripletMode ? tx : x);
    } else if (gridWidth > 0) {
      if (count % numSubIntervals == 0) {
        lines.push_back(tripletMode ? tx : x);
      }
    } else {
      lines.push_back(tripletMode ? tx : x);
    }

    x += pxInterval;
    tx += pxTripletInterval;

    for (u32 i = 0; i < beatInterval; ++i) {
      beatCount = (beatCount + 1) % barInterval;

      if (!beatCount) {
        ++barCount;
      }
    }
    ++count;
  }

  snapInterval = subInterval;
}

f64 Grid::snap(f64 time) {
  i32 div = i32(time / snapInterval);
  f64 left  = div * snapInterval;
  return time - left < snapInterval / 2 ? left : left + snapInterval;
}

void Grid::narrow() {
  if (gridWidth > -2) {
    --gridWidth;
    reset();
  }
}

void Grid::widen() {
  if (gridWidth < 2) {
    ++gridWidth;
    reset();
  }
}

void Grid::triplet() {
  tripletMode = !tripletMode;
  reset();
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
  start.setValue(grid.snap(parentLocalPoint.x - mouseDownOffset) / zoom, undoManager);
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
  auto newX = f64(grid.snap(parentLocalPoint.x) / zoom);
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
    path->setBounds(i32(path->start * zoom) - PathView::posOffset,
                    i32(path->y * getHeight()) - PathView::posOffset,
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
  auto distance = hoverPoint.getDistanceFrom(e.position);
  if (distance < mouseOverDistance) {
    manager.addPath(hoverPoint.x / zoom, hoverPoint.y / getHeight());
  } 
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
    g.setFont(10);

    g.setColour(juce::Colours::black);

    for (auto& b : grid.beats) {
      auto beatText = juce::String(b.bar);
      if (b.beat > 1) {
        beatText << "." + juce::String(b.beat);
      }

      // TODO(luca): use a float rectangle here
      g.drawText(beatText, i32(b.x), r.getY(), 40, 20, juce::Justification::left);
    }

    g.setColour(juce::Colours::darkgrey);

    for (u32 i = 0; i < grid.lines.size(); ++i) {
      g.fillRect(f32(grid.lines[i]), f32(r.getY()), 0.75f, f32(getHeight()));
    }
  }

  g.setColour(juce::Colours::black);
  auto time = uiBridge.playheadPosition.load();
  g.fillRect(i32(time * zoom), r.getY(), 2, getHeight());
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
    clip->setBounds(i32(clip->start * zoom) - presetLaneHeight / 2,
                    clip->top ? presetLaneTopBounds.getY() : presetLaneBottomBounds.getY(),
                    presetLaneHeight, presetLaneHeight);
  }
}

void Track::timerCallback() {
  Grid::TimeSignature ts { uiBridge.numerator.load(), uiBridge.denominator.load() };
  zoom.forceUpdateOfCachedValue();
  grid.reset(zoom, getWidth(), ts);
  repaint();
}

i32 Track::getTrackWidth() {
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
  auto start = grid.snap(details.localPosition.x) / zoom;
  auto top = details.localPosition.y < getHeight() / 2;
  manager.addClip(id, name, start, top);
}

void Track::valueTreeChildAdded(juce::ValueTree&, juce::ValueTree&) {
  rebuildClips();
}

void Track::valueTreeChildRemoved(juce::ValueTree&, juce::ValueTree&, i32) { 
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
  viewport.setViewPosition(i32(viewport.getViewPositionX() + dx), 0);
  zoom.setValue(zoom + (amount * (zoom / zoomDeltaScale)), nullptr);
}

} // namespace atmt
