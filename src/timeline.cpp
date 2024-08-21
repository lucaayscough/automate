#include "timeline.hpp"

namespace atmt {

bool Grid::reset(f64 z, f64 mw, TimeSignature _ts) {
  jassert(z > 0);

  if (std::abs(zoom - z) > EPSILON || std::abs(maxWidth - mw) > EPSILON || ts.numerator != _ts.numerator || ts.denominator != _ts.denominator) {
    zoom = z;
    maxWidth = mw;
    ts = _ts;
    reset();
    return true;
  }
  return false;
}

void Grid::reset() {
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
  if (snapOn) {
    i32 div = i32(time / snapInterval);
    f64 left  = div * snapInterval;
    return time - left < snapInterval / 2 ? left : left + snapInterval;
  } else {
    return time;
  }
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

void Grid::toggleSnap() {
  snapOn = !snapOn;
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
  undoManager->beginNewTransaction();
  manager.removeClip(state); 
}

void ClipView::mouseUp(const juce::MouseEvent&) {
  endDrag();
}

void ClipView::mouseDrag(const juce::MouseEvent& e) {
  auto parentLocalPoint = getParentComponent()->getLocalPoint(this, e.position);
  x.setValue(grid.snap(parentLocalPoint.x - mouseDownOffset) / zoom, undoManager);
  if (parentLocalPoint.y > getHeight() / 2) {
    y.setValue(1, undoManager);
  } else {
    y.setValue(0, undoManager);
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
  x.setValue(newX >= 0 ? newX : 0, undoManager);
  y.setValue(newY, undoManager);
}

void PathView::mouseDoubleClick(const juce::MouseEvent&) {
  undoManager->beginNewTransaction();
  manager.removePath(state);
}

AutomationLane::AutomationLane(StateManager& m, Grid& g) : manager(m), grid(g) {}

void AutomationLane::paint(juce::Graphics& g) {
  //DBG("AutomationLane::paint()");

  { // NOTE(luca): draw selection
    auto r = getLocalBounds();
    g.setColour(juce::Colours::aliceblue);
    g.setOpacity(0.5);
    g.fillRect(i32(selection.start < selection.end ? selection.start : selection.end), r.getY(), i32(std::abs(selection.end - selection.start)), r.getHeight());
    g.setOpacity(1);
  }

  { // NOTE(luca): draw automation line
    auto p = automation.get();
    p.applyTransform(juce::AffineTransform::scale(f32(zoom), getHeight()));

    juce::Path::Iterator it(p);

    bool end = false;
    juce::Path tmp;

    do {
      auto x1 = it.x2;
      auto y1 = it.y2;

      end = !it.next();

      if (xHighlightedSegment > x1 && xHighlightedSegment < it.x2) {
        g.setColour(juce::Colours::red);
      } else {
        g.setColour(juce::Colours::orange);
      }

      if (!end) {
        tmp.clear();
        tmp.startNewSubPath(x1, y1);
        tmp.quadraticTo(it.x1, it.y1, it.x2, it.y2);
        g.strokePath(tmp, juce::PathStrokeType { 2.f });
      }
    } while (!end);
  }

  { // NOTE(luca): draw point
    auto cond = [] (PathView* p) { return p->isMouseButtonDown() || p->isMouseOver(); };  
    auto it = std::find_if(paths.begin(), paths.end(), cond);
    if (it == paths.end()) {
      g.setColour(juce::Colours::orange);
      g.fillEllipse(hoverBounds);
    }
  }
}

void AutomationLane::resized() {
  zoom.forceUpdateOfCachedValue();

  for (auto& path : paths) {
    path->setBounds(i32(path->x * zoom) - PathView::posOffset,
                    i32(path->y * getHeight()) - PathView::posOffset,
                    PathView::size, PathView::size);
  }
}

auto AutomationLane::getAutomationPoint(juce::Point<f32> p) {
  juce::Point<f32> np;
  automation.get().getNearestPoint(p, np, juce::AffineTransform::scale(f32(zoom), getHeight()));
  return np;
}

f64 AutomationLane::getDistanceFromPoint(juce::Point<f32> p) {
  return p.getDistanceFrom(getAutomationPoint(p));
}

void AutomationLane::addPath(juce::ValueTree& v) {
  auto path = new PathView(manager, v, undoManager, grid);
  addAndMakeVisible(path);
  paths.add(path);
  resized();
}

void AutomationLane::mouseMove(const juce::MouseEvent& e) {
  auto p = getAutomationPoint(e.position);
  auto d = p.getDistanceFrom(e.position);

  if (d < mouseIntersectDistance && !optKeyPressed) {
    hoverBounds.setCentre(p);
    hoverBounds.setSize(10, 10);
  } else {
    hoverBounds = {};
  }

  if ((d < mouseOverDistance && mouseIntersectDistance < d) || (d < mouseOverDistance && optKeyPressed)) {
    xHighlightedSegment = p.x;
  } else {
    xHighlightedSegment = -1; 
  }

  repaint();
}

void AutomationLane::mouseExit(const juce::MouseEvent&) {
  hoverBounds = {};
  xHighlightedSegment = -1; 
  lastMouseDragOffset = {};
}

void AutomationLane::mouseDown(const juce::MouseEvent& e) {
  jassert(activeGesture == GestureType::none);

  auto p = getAutomationPoint(e.position);
  auto d = p.getDistanceFrom(e.position);

  if (d < mouseOverDistance && optKeyPressed) {
    undoManager->beginNewTransaction();
    activeGesture = GestureType::bend;
    setMouseCursor(juce::MouseCursor::NoCursor);
  } else if (d < mouseIntersectDistance) {
    undoManager->beginNewTransaction();
    manager.addPath(grid.snap(p.x) / zoom, p.y / getHeight());
  } else if (d < mouseOverDistance) {
    undoManager->beginNewTransaction();
    activeGesture = GestureType::drag;
    setMouseCursor(juce::MouseCursor::NoCursor);
  } else {
    activeGesture = GestureType::select;
    selection = { grid.snap(e.position.x), grid.snap(e.position.x) };
  }
}

void AutomationLane::mouseUp(const juce::MouseEvent&) {
  // TODO(luca): implement automatic mouse placing meachanism
  //if (activeGesture == GestureType::bend) {
  //  auto globalPoint = localPointToGlobal(juce::Point<i32> { 0, 0 });
  //  juce::Desktop::setMousePosition(globalPoint);
  //}

  xHighlightedSegment = -1; 
  hoverBounds = {};
  lastMouseDragOffset = {};

  activeGesture = GestureType::none;
  setMouseCursor(juce::MouseCursor::NormalCursor);
}

void AutomationLane::mouseDrag(const juce::MouseEvent& e) {
  if (activeGesture == GestureType::bend) {
    auto p = automation.getClipPathForX(xHighlightedSegment / zoom);
    jassert(p);
    if (p) {
      auto offset = e.getOffsetFromDragStart();
      auto y = f64(lastMouseDragOffset.y - offset.y);
      auto increment = y / kMoveIncrement;
      auto newValue = std::clamp(p->curve + increment, 0.0, 1.0);
      jassert(newValue >= 0 && newValue <= 1);
      p->curve.setValue(newValue, undoManager);
      lastMouseDragOffset = offset;
    }
  } else if (activeGesture == GestureType::drag) {
    // TODO(luca): 
  } else if (activeGesture == GestureType::select) {
    selection.end = grid.snap(e.position.x);
    repaint();
  } else {
    jassertfalse;
  }
}

void AutomationLane::mouseDoubleClick(const juce::MouseEvent& e) {
  if (getDistanceFromPoint(e.position) < mouseOverDistance && optKeyPressed) {
    auto p = automation.getClipPathForX(e.position.x / zoom);
    jassert(p);

    if (p) {
      undoManager->beginNewTransaction();
      p->curve.setValue(0.5, undoManager);
    }
  }
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
  //DBG("Track::paint()");

  auto r = getLocalBounds();
  g.fillAll(juce::Colours::grey);

  g.setColour(juce::Colours::coral);
  g.fillRect(timelineBounds);

  g.setColour(juce::Colours::aqua);
  g.fillRect(presetLaneTopBounds);
  g.fillRect(presetLaneBottomBounds);

  { // NOTE(luca): beats
    g.setFont(10);
    g.setColour(juce::Colours::black);
    for (auto& b : grid.beats) {
      auto beatText = juce::String(b.bar);
      if (b.beat > 1) {
        beatText << "." + juce::String(b.beat);
      }
      g.drawText(beatText, i32(b.x), r.getY(), 40, 20, juce::Justification::left);
    }
    g.setColour(juce::Colours::darkgrey);
    for (u32 i = 0; i < grid.lines.size(); ++i) {
      g.fillRect(f32(grid.lines[i]), f32(r.getY()), 0.75f, f32(getHeight()));
    }
  }

  g.setColour(juce::Colours::black);
  g.fillRect(i32(playheadPosition), r.getY(), 2, getHeight());
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
    clip->setBounds(i32(clip->x * zoom) - presetLaneHeight / 2,
                    !bool(i32(clip->y)) ? presetLaneTopBounds.getY() : presetLaneBottomBounds.getY(),
                    presetLaneHeight, presetLaneHeight);
  }
}

void Track::timerCallback() {
  Grid::TimeSignature ts { uiBridge.numerator.load(), uiBridge.denominator.load() };
  zoom.forceUpdateOfCachedValue();

  auto ph = uiBridge.playheadPosition.load() * zoom;

  if (std::abs(playheadPosition - ph) > EPSILON) {
    auto x = viewport.getViewPositionX();
    auto inBoundsOld = playheadPosition > x && playheadPosition < x; 
    auto inBoundsNew = ph > x && ph < (x + getWidth()); 

    playheadPosition = ph;

    if (inBoundsOld || inBoundsNew) {
      repaint();
    }
  }

  if (getWidth() > 0 && grid.reset(zoom, getWidth(), ts)) {
    repaint();
  }
}

i32 Track::getTrackWidth() {
  f64 width = 0;

  for (auto* c : clips) {
    if (c->x > width) {
      width = c->x;
    }
  }

  for (auto* p : automationLane.paths) {
    if (p->x > width) {
      width = p->x;
    }
  }

  width *= zoom;
  return width > getParentWidth() ? int(width) : getParentWidth();
}

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
  repaint();
}

bool Track::isInterestedInDragSource(const juce::DragAndDropTarget::SourceDetails&) {
  return true;
}

void Track::itemDropped(const juce::DragAndDropTarget::SourceDetails& details) {
  jassert(!details.description.isVoid());
  auto name = details.description.toString();
  auto preset = presets.getPresetFromName(name);
  auto id = preset->_id.load();
  auto x = grid.snap(details.localPosition.x) / zoom;
  auto y = details.localPosition.y > (getHeight() / 2);
  undoManager->beginNewTransaction();
  manager.addClip(id, name, x, y);
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
      repaint();
    }
  } else if (v.hasType(ID::CLIP) || v.hasType(ID::PATH)) {
    if (id == ID::x || id == ID::y || id == ID::curve) {
      resized();
      automationLane.resized();
      repaint();
    } 
  }
}

void Track::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w) {
  if (cmdKeyPressed) {
    zoomTrack(w.deltaY * kZoomSpeed, e.position.x);
  } else if (shiftKeyPressed) {
    viewport.setViewPosition(i32(viewport.getViewPositionX() - w.deltaX * kScrollSpeed), 0);
  }
}

void Track::zoomTrack(f64 amount, f64 mouseX) {
  f64 x0 = mouseX * zoom;
  auto newValue = zoom + (amount * (zoom / zoomDeltaScale)); 
  zoom.setValue(newValue <= 0 ? EPSILON : newValue, nullptr);
  zoom.forceUpdateOfCachedValue();
  f64 x1 = mouseX * zoom;
  f64 dx = (x1 - x0) / zoom;
  viewport.setViewPosition(i32(viewport.getViewPositionX() + dx), 0);
}

} // namespace atmt
