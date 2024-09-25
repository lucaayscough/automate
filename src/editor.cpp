#include "plugin.hpp"
#include "editor.hpp"
#include <BinaryData.h>

namespace atmt {

const juce::Colour Colours::eerieBlack { 28, 28, 31 };
const juce::Colour Colours::jet { 43, 45, 49 };
const juce::Colour Colours::frenchGray { 182, 186, 192 };
const juce::Colour Colours::isabelline { 239, 233, 231 };
const juce::Colour Colours::glaucous { 118, 126, 206 };
const juce::Colour Colours::shamrockGreen { 45, 154, 84};
const juce::Colour Colours::auburn { 166, 48, 49 };
const juce::Colour Colours::outerSpace { 66, 70, 76 };
const juce::Colour Colours::atomicTangerine { 251, 146, 75 };

const juce::FontOptions Fonts::sofiaProRegular { juce::Typeface::createSystemTypefaceFor(BinaryData::sofia_pro_regular_otf, BinaryData::sofia_pro_regular_otfSize) };
const juce::FontOptions Fonts::sofiaProMedium { juce::Typeface::createSystemTypefaceFor(BinaryData::sofia_pro_medium_otf, BinaryData::sofia_pro_medium_otfSize) };

Button::Button(const juce::String& s, Type t) : juce::Button(s) {
  if (t == Type::toggle) {
    setClickingTogglesState(true);
  }

  setTriggeredOnMouseDown(true);
}

void Button::paintButton(juce::Graphics& g, bool highlighted, bool) {
  if (getToggleState()) {
    g.setColour(Colours::shamrockGreen);
  } else {
    g.setColour(Colours::glaucous);
  }

  g.setFont(font);
  g.drawText(getButtonText(), textBounds, juce::Justification::centred);
  g.drawRoundedRectangle(rectBounds, rectBounds.getHeight() / 2.f, highlighted ? Style::lineThicknessHighlighted : Style::lineThickness);
}

void Button::resized() {
  rectBounds = getLocalBounds().toFloat().reduced(Style::lineThicknessHighlighted, Style::lineThicknessHighlighted);
  f32 yTranslation = rectBounds.getHeight() * 0.025f;
  textBounds = rectBounds.translated(0, yTranslation);
}

Dial::Dial(const Parameter* p) : parameter(p) {
  setSliderStyle(juce::Slider::SliderStyle::RotaryVerticalDrag);
  setTextBoxStyle(juce::Slider::TextEntryBoxPosition::NoTextBox, true, 0, 0);
  setScrollWheelEnabled(false);

  setRange(0, 1);
  setValue(parameter->parameter->getValue());
  setDoubleClickReturnValue(true, parameter->parameter->getDefaultValue());
  onValueChange = [this] { parameter->parameter->setValue(f32(getValue())); };
}

void Dial::paint(juce::Graphics& g) {
  auto r = getLocalBounds().toFloat().reduced(Style::lineThicknessHighlighted, Style::lineThicknessHighlighted);

  if (parameter->active) {
    g.setColour(Colours::shamrockGreen);
  } else {
    g.setColour(Colours::auburn);
  }

  g.drawEllipse(r, Style::lineThicknessHighlighted);

  { // NOTE(luca) draw dot 
    f32 v = f32(getValue() * 0.75);
    f32 period = v * tau;

    f32 d = r.getWidth() * 0.5f;
    f32 centreOffset = (r.getWidth() - d) * 0.5f;

    f32 x = (std::cos(period - offset) + 1.f) * 0.5f * d + centreOffset + r.getX() - dotOffset;
    f32 y = (std::sin(period - offset) + 1.f) * 0.5f * d + centreOffset + r.getY() - dotOffset;
    
    g.fillEllipse(x, y, dotSize, dotSize);
  }
}

void Dial::resized() {
  assert(getWidth() ==  getHeight());
}

void Dial::mouseDown(const juce::MouseEvent& e) {
  setMouseCursor(juce::MouseCursor::NoCursor);
  juce::Slider::mouseDown(e);
}

void Dial::mouseUp(const juce::MouseEvent& e) {
  setMouseCursor(juce::MouseCursor::NormalCursor);
  juce::Slider::mouseUp(e);
  auto& desktop = juce::Desktop::getInstance();
  desktop.setMousePosition(localPointToGlobal(getLocalBounds().getCentre()));
}

void InfoView::paint(juce::Graphics& g) {
  auto r = getLocalBounds().withSizeKeepingCentre(i32(Style::minWidth * 0.75f), numCommands * commandHeight);
  auto left = r.removeFromLeft(r.getWidth() / 2);
  auto right = r;

  g.fillAll(Colours::eerieBlack);
  g.setFont(font);
  g.setColour(Colours::isabelline);

  for (u32 i = 0; i < numCommands; ++i) {
    g.drawText(commands[i].name, left.removeFromTop(commandHeight), juce::Justification::left);
    g.drawText(commands[i].binding, right.removeFromTop(commandHeight), juce::Justification::right);
  }
}

void InfoView::mouseDown(const juce::MouseEvent&) {
  mainViewUpdateCallback();
}

bool Grid::reset(f32 z, f32 mw, TimeSignature _ts) {
  if (std::abs(zoom - z) > EPSILON || std::abs(maxWidth - mw) > EPSILON || ts.numerator != _ts.numerator || ts.denominator != _ts.denominator) {
    maxWidth = mw;
    ts = _ts;
    zoom = z;
    reset();
    return true;
  }
  return false;
}

void Grid::reset() {
  assert(zoom > 0 && maxWidth > 0 && ts.numerator > 0 && ts.denominator > 0);

  // TODO(luca): there is still some stuff to work out with the triplet grid
  lines.clear();
  beats.clear();
  
  u32 barCount = 0;
  u32 beatCount = 0;

  f32 pxInterval = zoom / (ts.denominator / 4.f);
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

  f32 pxTripletInterval = pxInterval * 2 / 3;
  f32 x = 0;
  f32 tx = 0;
  u32 numSubIntervals = u32(std::abs(gridWidth)) * 2;
  
  f32 subInterval = 0;
  if (gridWidth < 0) {
    subInterval = tripletMode ? pxTripletInterval : pxInterval / f32(numSubIntervals);
  } else if (gridWidth > 0) {
    subInterval = tripletMode ? pxTripletInterval : pxInterval / (1.f / f32(numSubIntervals));
  } else {
    subInterval = tripletMode ? pxTripletInterval : pxInterval; 
  }

  u32 count = 0;

  while (x < maxWidth || tx < maxWidth) {
    Beat b { barCount + 1, beatCount + 1, x };
    beats.push_back(b);

    if (gridWidth < 0) {
      for (u32 i = 0; i < numSubIntervals; ++i) {
        lines.push_back((tripletMode ? tx : x) + f32(i) * subInterval);
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

f32 Grid::snap(f32 time) {
  if (snapOn) {
    i32 div = i32(time / snapInterval);
    f32 left  = div * snapInterval;
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

PathView::PathView(Grid& g) : grid(g) {}

void PathView::paint(juce::Graphics& g) {
  if (isMouseOverOrDragging()) {
    g.setColour(Colours::auburn);
  } else {
    g.setColour(Colours::atomicTangerine);
  }
  g.fillEllipse(getLocalBounds().toFloat().withSizeKeepingCentre(size / 2, size / 2));
}

void PathView::mouseDrag(const juce::MouseEvent& e) {
  assert(id);

  // TODO(luca): simplify this
  auto parentLocalPoint = getParentComponent()->getLocalPoint(this, e.position);
  auto newX = f32(grid.snap(parentLocalPoint.x));
  auto newY = f32(parentLocalPoint.y / getParentComponent()->getHeight());
  newY = std::clamp(newY, 0.f, 1.f);

  move(id, newX, newY);
}

void PathView::mouseDoubleClick(const juce::MouseEvent&) {
  assert(id);
  remove(id);
}

ClipView::ClipView(Grid& g) : grid(g) {}

void ClipView::paint(juce::Graphics& g) {
  g.setColour(Colours::auburn);
  g.fillEllipse(getLocalBounds().toFloat());
}

void ClipView::mouseDown(const juce::MouseEvent& e) {
  mouseDownOffset = e.position.x;
}

void ClipView::mouseDoubleClick(const juce::MouseEvent&) {
  remove(id);
}

void ClipView::mouseUp(const juce::MouseEvent&) {
  isTrimDrag = false;
  mouseDownOffset = 0;
}

void ClipView::mouseDrag(const juce::MouseEvent& e) {
  auto parentLocalPoint = getParentComponent()->getLocalPoint(this, e.position);
  f32 y = parentLocalPoint.y > (getParentComponent()->getHeight() / 2) ? 1 : 0;
  f32 x = grid.snap(parentLocalPoint.x - mouseDownOffset);
  move(id, x, y);
}

AutomationLane::AutomationLane(StateManager& m, Grid& g) : manager(m), grid(g) {
  manager.registerAutomationLaneView(this);
}

AutomationLane::~AutomationLane() {
  manager.deregisterAutomationLaneView();
}

void AutomationLane::paint(juce::Graphics& g) {
  scoped_timer t("AutomationLane::paint()");

  { // NOTE(luca): draw selection
    auto r = getLocalBounds();
    g.setColour(Colours::frenchGray);
    g.setOpacity(0.2f);
    g.fillRect(i32(selection.start < selection.end ? selection.start : selection.end), r.getY(), i32(std::abs(selection.end - selection.start)), r.getHeight());
    g.setOpacity(1);
  }

  { // NOTE(luca): draw automation line
    juce::Path::Iterator it(scaledAutomation);

    bool end = false;
    juce::Path tmp;
    it.next();

    auto x1 = it.x1;
    auto y1 = it.y1;

    do {
      end = !it.next();

      if (xHighlightedSegment > x1 && xHighlightedSegment < it.x2) {
        g.setColour(Colours::auburn);
      } else {
        g.setColour(Colours::atomicTangerine);
      }

      if (!end) {
        tmp.clear();
        tmp.startNewSubPath(x1, y1);
        tmp.quadraticTo(it.x1, it.y1, it.x2, it.y2);
        g.strokePath(tmp, juce::PathStrokeType { lineThickness });
      }

      x1 = it.x2;
      y1 = it.y2;
    } while (!end);
  }

  { // NOTE(luca): draw point
    auto cond = [] (PathView* p) { return p->isMouseButtonDown() || p->isMouseOver(); };
    auto it = std::find_if(pathViews.begin(), pathViews.end(), cond);
    if (it == pathViews.end()) {
      g.setColour(Colours::atomicTangerine);
      g.fillEllipse(hoverBounds);
    }
  }
}

void AutomationLane::resized() {
  scoped_timer t("AutomationLane::resized()");
  updateAutomation(automation);
}

auto AutomationLane::getAutomationPoint(juce::Point<f32> p) {
  juce::Point<f32> np;
  scaledAutomation.getNearestPoint(p, np);
  return np;
}

f32 AutomationLane::getDistanceFromPoint(juce::Point<f32> p) {
  return p.getDistanceFrom(getAutomationPoint(p));
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
    activeGesture = GestureType::bend;
    setMouseCursor(juce::MouseCursor::NoCursor);
  } else if (d < mouseIntersectDistance) {
    // TODO(luca): add gesture
    manager.addPath(grid.snap(p.x) / zoom, p.y / getHeight(), Path::defaultCurve);
  } else if (d < mouseOverDistance) {
    activeGesture = GestureType::drag;
    setMouseCursor(juce::MouseCursor::NoCursor);
  } else {
    activeGesture = GestureType::select;

    f32 start = grid.snap(e.position.x);
    f32 end = grid.snap(e.position.x); 

    selection = { start < 0 ? 0 : start, end < 0 ? 0 : end };
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
    auto p = manager.findAutomationPoint(xHighlightedSegment / zoom);
    assert(p->clip || p->path);

    auto offset = e.getOffsetFromDragStart();
    auto y = f32(lastMouseDragOffset.y - offset.y);
    auto increment = y / kDragIncrement;
    auto newValue = std::clamp(p->c + increment, 0.f, 1.f);
    assert(newValue >= 0 && newValue <= 1);

    if (p->clip) {
      manager.moveClip(p->clip->id, p->x, p->y, newValue);
    } else {
      manager.movePath(p->path->id, p->x, p->y, newValue);
    }

    lastMouseDragOffset = offset;
  } else if (activeGesture == GestureType::drag) {
    auto p = manager.findAutomationPoint(xHighlightedSegment / zoom);
    assert(p->clip || p->path);

    auto offset = e.getOffsetFromDragStart();
    auto y = f32(lastMouseDragOffset.y - offset.y);
    auto increment = y / kDragIncrement;
    auto newValue = std::clamp(p->y - increment, 0.f, 1.f);

    if (p->path) {
      manager.movePath(p->path->id, p->x, newValue, p->c);
    }

    if (p != manager.state.points.begin()) {
      auto prev = std::prev(p);
      if (prev->path) {
        newValue = std::clamp(prev->y - increment, 0.f, 1.f);
        manager.movePath(prev->path->id, prev->x, newValue, prev->c);
      }
    }

    lastMouseDragOffset = offset;
  } else if (activeGesture == GestureType::select) {
    f32 end = grid.snap(e.position.x); 
    selection.end = end < 0 ? 0 : end;
    repaint();
  } else {
    // TODO(luca): occurs when a path has just been added
    //jassertfalse;
  }
}

void AutomationLane::mouseDoubleClick(const juce::MouseEvent& e) {
  if (getDistanceFromPoint(e.position) < mouseOverDistance && optKeyPressed) {
    //auto p = automation.getClipPathForX(e.position.x / zoom);
    //jassert(p);

    //if (p) {
    //  undoManager->beginNewTransaction();
    //  p->setCurve(0.5);
    //}
  }
}

void AutomationLane::updateAutomation(juce::Path& a) {
  automation = a; 
  scaledAutomation = automation;

  scaledAutomation.applyTransform(juce::AffineTransform::scale(zoom, getHeight() - lineThickness));
  scaledAutomation.applyTransform(juce::AffineTransform::translation(0, lineThickness / 2));
}

void AutomationLane::update(const std::vector<Path>& paths, juce::Path& a, f32 z) {
  zoom = z;

  updateAutomation(a);

  {
    u32 numPaths = u32(paths.size());
    u32 numViews = u32(pathViews.size());

    if (numViews < numPaths) {
      u32 diff = numPaths - numViews;

      while (diff--) {
        pathViews.add(new PathView(grid));
        addAndMakeVisible(pathViews.getLast());
      }
    } else if (numViews > numPaths) {
      i32 diff = i32(numViews - numPaths);
      pathViews.removeLast(diff);
    }

    for (u32 i = 0; i < numPaths; ++i) {
      const auto& path = paths[i];
      auto* view = pathViews[i32(i)];
     
      view->id     = path.id;
      view->move   = [&path, this] (u32 id, f32 x, f32 y) { manager.movePathDenorm(id, x, y, path.c); };
      view->remove = [this] (u32 id) { manager.removePath(id); };

      i32 x = i32(path.x * zoom - PathView::posOffset);
      i32 y = i32(path.y * getHeight() - PathView::posOffset);
         
      view->setBounds(x, y, PathView::size, PathView::size);
    }
  }

  repaint();
}

Track::Track(StateManager& m, UIBridge& u) : manager(m), uiBridge(u) {
  addAndMakeVisible(automationLane);
  startTimerHz(60);
  setSize(getTrackWidth(), height);
  manager.registerTrackView(this);
}

Track::~Track() {
  manager.deregisterTrackView();
}

void Track::paint(juce::Graphics& g) {
  scoped_timer t("Track::paint()");

  auto r = getLocalBounds();
  g.fillAll(Colours::jet);

  g.setColour(Colours::eerieBlack);
  g.fillRect(b.timeline);

  g.setColour(Colours::eerieBlack);
  g.fillRect(b.presetLaneTop);
  g.fillRect(b.presetLaneBottom);

  { // NOTE(luca): beats
    g.setFont(font);
    g.setColour(Colours::frenchGray);
    for (auto& beat : grid.beats) {
      auto beatText = juce::String(beat.bar);
      if (beat.beat > 1) {
        beatText << "." + juce::String(beat.beat);
      }
      g.drawText(beatText, i32(beat.x + beatTextOffset), r.getY(), beatTextWidth, beatTextHeight, juce::Justification::left);
    }

    g.setColour(Colours::outerSpace);
    for (u32 i = 1; i < grid.lines.size(); ++i) {
      g.fillRect(f32(grid.lines[i]), f32(r.getY()), 0.75f, f32(getHeight()));
    }
  }

  g.setColour(Colours::isabelline);
  g.fillRect(playhead.x, f32(b.presetLaneTop.getY()), playhead.width, f32(getHeight() - b.timeline.getHeight()));
}

void Track::resized() {
  auto r = getLocalBounds();
  b.timeline = r.removeFromTop(timelineHeight);
  b.presetLaneTop = r.removeFromTop(presetLaneHeight);
  b.presetLaneBottom = r.removeFromBottom(presetLaneHeight);
  automationLane.setBounds(r);
}

void Track::timerCallback() {
  resetGrid();
}

void Track::resetGrid() {
  Grid::TimeSignature ts { uiBridge.numerator.load(), uiBridge.denominator.load() };
  auto ph = uiBridge.playheadPosition.load() * zoom;

  if (std::abs(playhead.x - ph) > EPSILON) {
    auto x = -viewportDeltaX;
    auto inBoundsOld = playhead.x > x && playhead.x < x; 
    auto inBoundsNew = ph > x && ph < (x + getWidth()); 

    playhead.x = ph;

    if (inBoundsOld || inBoundsNew) {
      repaint();
    }
  }

  if (getWidth() > 0 && grid.reset(zoom, getWidth(), ts)) {
    repaint();
  }
}

i32 Track::getTrackWidth() {
  f32 width = 0;

  for (auto& c : manager.state.clips) {
    if (c.x > width) {
      width = c.x;
    }
  }

  for (auto& p : manager.state.paths) {
    if (p.x > width) {
      width = p.x;
    }
  }

  width *= zoom;
  return width > getParentWidth() ? int(width) : getParentWidth();
}

void Track::zoomTrack(f32 amount) {
  //DBG("Track::zoomTrack()");

  Selection oldSelection = automationLane.selection;

  oldSelection.start /= zoom;
  oldSelection.end /= zoom;

  f32 mouseX = getMouseXYRelative().x;
  f32 z0 = zoom;
  f32 z1 = z0 + (amount * kZoomSpeed * (z0 / zoomDeltaScale)); 
  f32 x0 = -viewportDeltaX;
  f32 x1 = mouseX;
  f32 d = x1 - x0;
  f32 p = x1 / z0;
  f32 X1 = p * z1;
  f32 X0 = X1 - d;

  manager.setZoom(z1 <= 0 ? EPSILON : z1);

  f32 trackWidth = getTrackWidth();
  f32 parentWidth = getParentWidth();

  viewportDeltaX = std::clamp(-X0, -(trackWidth - parentWidth), 0.f);

  setBounds(i32(viewportDeltaX), getY(), i32(trackWidth), getHeight());

  assert(viewportDeltaX <= 0);
  assert(trackWidth + viewportDeltaX >= parentWidth);

  oldSelection.start *= zoom;
  oldSelection.end *= zoom;

  automationLane.selection = oldSelection;

  resetGrid();
  resized();
  repaint();
}

void Track::scroll(f32 amount) {
  viewportDeltaX += amount * Constants::kScrollSpeed;
  viewportDeltaX = std::clamp(viewportDeltaX, f32(-(getWidth() - getParentWidth())), 0.f);
  setTopLeftPosition(i32(viewportDeltaX), getY());
}

void Track::update(const std::vector<Clip>& clips, f32 z) {
  zoom = z;

  u32 numClips = u32(clips.size());
  u32 numViews = u32(clipViews.size());

  if (numViews < numClips) {
    u32 diff = numClips - numViews;

    while (diff--) {
      clipViews.add(new ClipView(grid));
      addAndMakeVisible(clipViews.getLast());
    }
  } else if (numViews > numClips) {
    i32 diff = i32(numViews - numClips);
    clipViews.removeLast(diff);
  }

  for (u32 i = 0; i < numClips; ++i) {
    const auto& clip = clips[i];
    auto* view = clipViews[i32(i)];
   
    view->id     = clip.id;
    view->move   = [&clip, this] (u32 id, f32 x, f32 y) { manager.moveClipDenorm(id, x, y, clip.c); };
    view->remove = [this] (u32 id) { manager.removeClip(id); };

    i32 h = presetLaneHeight;
    i32 w = presetLaneHeight;
    i32 x = i32((clip.x * zoom) - w * 0.5f);
    i32 y = !bool(clip.y) ? b.presetLaneTop.getY() : b.presetLaneBottom.getY();
       
    view->setBounds(x, y, w, h);
  }
}

void Track::mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& w) {
  if (cmdKeyPressed) {
    zoomTrack(w.deltaY);
  } else if (shiftKeyPressed) {
    scroll(w.deltaX);
  }
}

void Track::mouseDoubleClick(const juce::MouseEvent& e) {
  if (b.presetLaneTop.contains(e.position.toInt())) {
    manager.addClip(grid.snap(e.position.x) / zoom, 0, Path::defaultCurve);
  } else if (b.presetLaneBottom.contains(e.position.toInt())) {
    manager.addClip(grid.snap(e.position.x) / zoom, 1, Path::defaultCurve);
  }
}

ToolBar::InfoButton::InfoButton() : juce::Button({}) {
  setTriggeredOnMouseDown(true);
}

void ToolBar::InfoButton::paintButton(juce::Graphics& g, bool highlighted, bool) {
  g.setColour(Colours::shamrockGreen);

  if (highlighted) {
    g.drawEllipse(ellipseBounds, Style::lineThicknessHighlighted);
  } else {
    g.drawEllipse(ellipseBounds, Style::lineThickness);
  }

  g.setFont(font);
  g.drawText("i", iBounds, juce::Justification::centred);
}

void ToolBar::InfoButton::resized() {
  ellipseBounds = getLocalBounds().toFloat().reduced(Style::lineThicknessHighlighted, Style::lineThicknessHighlighted);
  f32 yTranslation = ellipseBounds.getHeight() * 0.05f;
  iBounds = ellipseBounds.translated(0, yTranslation); 
}

ToolBar::KillButton::KillButton() : juce::Button({}) {
  setTriggeredOnMouseDown(true);
}

void ToolBar::KillButton::paintButton(juce::Graphics& g, bool highlighted, bool) {
  auto r = getLocalBounds().toFloat().reduced(Style::lineThicknessHighlighted * 2, Style::lineThicknessHighlighted * 2);
  g.setColour(Colours::auburn);

  f32 thickness = highlighted ? Style::lineThicknessHighlighted * 2: Style::lineThickness * 2;

  g.drawLine(r.getX(), r.getY(), r.getX() + r.getWidth(), r.getY() + r.getHeight(), thickness);
  g.drawLine(r.getX() + r.getWidth(), r.getY(), r.getX(), r.getY() + r.getHeight(), thickness);
}

ToolBar::ToolBar(StateManager& m) : manager(m) {
  manager.debugView = this; 

  addAndMakeVisible(infoButton);
  addAndMakeVisible(editModeButton);
  addAndMakeVisible(modulateDiscreteButton);
  addAndMakeVisible(supportLinkButton);
  addAndMakeVisible(killButton);

  editModeButton.onClick = [this] { manager.setEditMode(!manager.state.editMode); };
  modulateDiscreteButton.onClick = [this] { manager.setModulateDiscrete(!manager.state.modulateDiscrete); };
  supportLinkButton.onClick = [] { supportURL.launchInDefaultBrowser(); };
  killButton.onClick = [this] { manager.setPluginID({}); };
}

ToolBar::~ToolBar() {
  manager.debugView = nullptr;
}

void ToolBar::resized() {
  auto r = getLocalBounds().reduced(padding, padding);
  i32 middleWidth = buttonWidth * 3 + buttonPadding * 2;
  auto middle = r.reduced((r.getWidth() - middleWidth) / 2, 0);

  infoButton.setBounds(r.removeFromLeft(r.getHeight()));
  editModeButton.setBounds(middle.removeFromLeft(buttonWidth));
  middle.removeFromLeft(buttonPadding);
  modulateDiscreteButton.setBounds(middle.removeFromLeft(buttonWidth));
  middle.removeFromLeft(buttonPadding);
  supportLinkButton.setBounds(middle.removeFromLeft(buttonWidth));
  killButton.setBounds(r.removeFromRight(r.getHeight()));

  editModeButton.setToggleState(manager.state.editMode, DONT_NOTIFY);
  modulateDiscreteButton.setToggleState(manager.state.modulateDiscrete, DONT_NOTIFY);
}

void ToolBar::paint(juce::Graphics& g) {
  g.fillAll(Colours::eerieBlack);
}

void DefaultView::PluginsPanel::paint(juce::Graphics& g) {
  g.fillAll(Colours::jet);

  {
    g.setColour(Colours::isabelline);
    g.setFont(Fonts::sofiaProRegular.withHeight(titleFontHeight));
    g.drawText("Plug-ins", titleBounds, juce::Justification::left);
  }
  
  g.setFont(Fonts::sofiaProRegular.withHeight(DefaultView::buttonFontHeight));

  for (u32 i = 0; i < plugins.size(); ++i) {
    const auto& p = plugins[i];

    if (p.visible) {
      if (p.r.contains(getMouseXYRelative())) {
        g.setColour(Colours::frenchGray);
        g.fillRect(p.r);
        g.setColour(Colours::eerieBlack);
        g.drawText(p.format, p.r, juce::Justification::left);
        g.drawText(p.description.name, p.r.withX(Button::namePadding), juce::Justification::left);
      } else {
        g.setColour(Colours::isabelline);
        g.drawText(p.format, p.r, juce::Justification::left);
        g.drawText(p.description.name, p.r.withX(Button::namePadding), juce::Justification::left);
      }
    }
  }
}

void DefaultView::PluginsPanel::mouseMove(const juce::MouseEvent& e) {
  auto p = e.position.toInt();
  auto it = std::find_if(plugins.begin(), plugins.end(), [p] (const auto& p_) { return p_.r.contains(p); });

  if (it != plugins.end()) {
    setMouseCursor(juce::MouseCursor::PointingHandCursor); 
  } else {
    setMouseCursor(juce::MouseCursor::NormalCursor); 
  }

  repaint();
}

void DefaultView::PluginsPanel::mouseDown(const juce::MouseEvent& e) {
  for (u32 i = 0; i < plugins.size(); ++i) {
    const auto& p = plugins[i];

    if (p.visible && p.r.contains(e.position.toInt())) {
      setPluginID(p.description.createIdentifierString());
      break;
    }
  }
}

void DefaultView::PluginsPanel::mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& w) {
  i32 v = getY() + i32(100.f * w.deltaY);
  v = std::clamp(v, - getHeight() + DefaultView::height, 0);
  setTopLeftPosition(getX(), v); 
}

void DefaultView::PluginsPanel::resized() {
  auto r = getLocalBounds();
  r.removeFromLeft(DefaultView::panelPadding);
  r.removeFromRight(DefaultView::panelPadding);
  titleBounds = r.removeFromTop(DefaultView::titleHeight);

  for (auto& p : plugins) {
    if (p.visible) {
      p.r = r.removeFromTop(DefaultView::buttonHeight);
    }
  }
}

void DefaultView::PluginsPanel::update(juce::Array<juce::PluginDescription> descriptions) {
  plugins.clear();

  for (const auto& t : descriptions) {
    plugins.push_back(Button { t }); 
    
    auto& p = plugins.back();
    p.format = p.description.pluginFormatName == "AudioUnit" ? "AU" : p.description.pluginFormatName;
  }

  updateManufacturerFilter(filter);
}

void DefaultView::PluginsPanel::updateManufacturerFilter(const juce::String& m) {
  filter = m; 

  i32 count = 0;
  for (auto& p : plugins) {
    if (p.description.manufacturerName == filter) {
      ++count;
      p.visible = true;
    } else {
      p.visible = false;
    }
  }

  i32 newHeight = DefaultView::buttonHeight * count + DefaultView::titleHeight;
  setTopLeftPosition(getX(), 0);
  setSize(DefaultView::width / 2, newHeight < height ? height : newHeight);
  resized();
  repaint();
}

DefaultView::ManufacturersPanel::ManufacturersPanel(PluginsPanel& p) : pluginsPanel(p) {}

void DefaultView::ManufacturersPanel::paint(juce::Graphics& g) {
  g.fillAll(Colours::eerieBlack);

  {
    g.setColour(Colours::isabelline);
    g.setFont(Fonts::sofiaProRegular.withHeight(titleFontHeight));
    g.drawText("Manufacturers", titleBounds, juce::Justification::left);
  }
  
  g.setFont(Fonts::sofiaProRegular.withHeight(DefaultView::buttonFontHeight));
  
  for (u32 i = 0; i < manufacturers.size(); ++i) {
    const auto& m = manufacturers[i];

    if (m.r.contains(getMouseXYRelative()) || i == activeButton) {
      g.setColour(Colours::frenchGray);
      g.fillRect(m.r);
      g.setColour(Colours::eerieBlack);
      g.drawText(m.name, m.r, juce::Justification::left);
    } else {
      g.setColour(Colours::isabelline);
      g.drawText(m.name, m.r, juce::Justification::left);
    }
  }
}

void DefaultView::ManufacturersPanel::mouseMove(const juce::MouseEvent& e) {
  auto p = e.position.toInt();
  auto it = std::find_if(manufacturers.begin(), manufacturers.end(), [p] (const auto& m) { return m.r.contains(p); });

  if (it != manufacturers.end()) {
    setMouseCursor(juce::MouseCursor::PointingHandCursor); 
  } else {
    setMouseCursor(juce::MouseCursor::NormalCursor); 
  }

  repaint();
}

void DefaultView::ManufacturersPanel::mouseDown(const juce::MouseEvent& e) {
  for (u32 i = 0; i < manufacturers.size(); ++i) {
    if (manufacturers[i].r.contains(e.position.toInt())) {
      activeButton = i;
      pluginsPanel.updateManufacturerFilter(manufacturers[i].name);
      break;
    }
  }
}

void DefaultView::ManufacturersPanel::mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& w) {
  i32 v = getY() + i32(100.f * w.deltaY);
  v = std::clamp(v, - getHeight() + DefaultView::height, 0);
  setTopLeftPosition(getX(), v); 
}

void DefaultView::ManufacturersPanel::resized() {
  auto r = getLocalBounds();
  r.removeFromLeft(DefaultView::panelPadding);
  r.removeFromRight(DefaultView::panelPadding);

  titleBounds = r.removeFromTop(DefaultView::titleHeight);

  for (auto& m : manufacturers) {
    m.r = r.removeFromTop(DefaultView::buttonHeight);
  }
}

void DefaultView::ManufacturersPanel::update(juce::Array<juce::PluginDescription> descriptions) {
  juce::StringArray manufacturerNames;

  for (const auto& t : descriptions) {
    manufacturerNames.add(t.manufacturerName);
  }

  manufacturerNames.removeDuplicates(false);
  manufacturerNames.sortNatural();

  juce::String currentManufacturer;

  if (activeButton < manufacturers.size()) {
    currentManufacturer = manufacturers[activeButton].name;
  }

  manufacturers.clear();

  for (const auto& n : manufacturerNames) {
    manufacturers.push_back(Button { n, {} });
  }
  
  activeButton = 0;
  for (u32 i = 0; i < manufacturers.size(); ++i) {
    if (manufacturers[i].name == currentManufacturer) {
      activeButton = i;
      break;
    }
  }

  if (!manufacturers.empty()) {
    pluginsPanel.updateManufacturerFilter(manufacturers.front().name);
  }

  i32 newHeight = DefaultView::buttonHeight * i32(manufacturers.size()) + DefaultView::titleHeight;
  setSize(DefaultView::width / 2, newHeight < height ? height : newHeight);
  resized();
}

DefaultView::DefaultView(StateManager& m, juce::AudioPluginFormatManager& fm, juce::KnownPluginList& kpl) : manager(m), knownPluginList(kpl), formatManager(fm) {
  pluginsPanel.setPluginID = [this] (const juce::String& id) { manager.setPluginID(id); };

  addAndMakeVisible(manufacturersPanel);
  addAndMakeVisible(pluginsPanel);

  setSize(width, height);
}

void DefaultView::resized() {
  auto r = getLocalBounds();
  auto l = r.removeFromLeft(r.getWidth() / 2);

  manufacturersPanel.setTopLeftPosition(l.getX(), l.getY()); 
  pluginsPanel.setTopLeftPosition(r.getX(), r.getY()); 

  manufacturersPanel.update(knownPluginList.getTypes());
  pluginsPanel.update(knownPluginList.getTypes());
}

bool DefaultView::isInterestedInFileDrag(const juce::StringArray&){
  return true; 
}

void DefaultView::filesDropped(const juce::StringArray& files, i32, i32) {
  juce::OwnedArray<juce::PluginDescription> types;
  knownPluginList.scanAndAddDragAndDroppedFiles(formatManager, files, types);
  resized();
}

ParametersView::ParameterView::ParameterView(StateManager& m, Parameter* p) : manager(m), parameter(p) {
  assert(parameter);
  assert(parameter->parameter);

  parameter->parameter->addListener(this);

  addAndMakeVisible(dial);
  addAndMakeVisible(activeToggle);

  activeToggle.setToggleState(parameter->active, DONT_NOTIFY);
  activeToggle.onClick = [this] { manager.setParameterActive(parameter, !parameter->active); };
}

ParametersView::ParameterView::~ParameterView() {
  parameter->parameter->removeListener(this);
}

void ParametersView::ParameterView::resized() {
  auto r = getLocalBounds().reduced(padding, padding);
  activeToggle.setBounds(r.removeFromTop(buttonSize).removeFromLeft(buttonSize));
  dial.setBounds(r.removeFromTop(dialSize).withSizeKeepingCentre(dialSize, dialSize));
  r.removeFromTop(padding);
  nameBounds = r.removeFromTop(nameHeight);
}

void ParametersView::ParameterView::paint(juce::Graphics& g) {
  g.setColour(Colours::isabelline); 
  g.setFont(Fonts::sofiaProRegular.withHeight(10));
  g.drawText(parameter->parameter->getName(1024), nameBounds, juce::Justification::centred);
}

void ParametersView::ParameterView::update() {
  activeToggle.setToggleState(parameter->active, DONT_NOTIFY);
  repaint();
}

void ParametersView::ParameterView::parameterValueChanged(i32, f32 v) {
  juce::MessageManager::callAsync([v, this] { dial.setValue(v); repaint(); });
}
 
void ParametersView::ParameterView::parameterGestureChanged(i32, bool) {}

ParametersView::ParametersView(StateManager& m) : manager(m) {
  DBG("ParametersView::ParametersView()");
  manager.parametersView = this;

  for (auto& p : manager.state.parameters) {
    if (p.parameter->isAutomatable()) {
      auto param = new ParameterView(manager, &p);
      addAndMakeVisible(param);
      parameters.add(param);
    }
  }
}

ParametersView::~ParametersView() {
  manager.parametersView = nullptr;
}

void ParametersView::resized() {
  // TODO(luca): clean this up
  auto r = getLocalBounds();
  r.removeFromTop(padding);
  r.removeFromLeft(padding);
  r.removeFromRight(padding);

  i32 numPerRow = r.getWidth() / ParameterView::height;
  i32 numRows = parameters.size() / numPerRow;

  if ( parameters.size() % numPerRow != 0) {
    ++numRows; 
  }

  i32 remainder = r.getWidth() - numPerRow * ParameterView::height;
  i32 offset = remainder / numPerRow;

  setSize(getWidth(), numRows * ParameterView::height);

  r = getLocalBounds();
  r.removeFromTop(padding);
  r.removeFromLeft(padding);
  r.removeFromRight(padding);

  i32 count = 0;
  juce::Rectangle<i32> row;

  for (auto p : parameters) {
    if (count % numPerRow == 0) {
      row = r.removeFromTop(ParameterView::height);    
    }
    p->setBounds(row.removeFromLeft(ParameterView::height + offset)); 
    ++count;
  }
}

void ParametersView::mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& w) {
  i32 y = getY() + i32(Constants::kScrollSpeed * w.deltaY);

  if (y > ToolBar::height) {
    y = ToolBar::height;
  } else if (y < - (getHeight() - viewportHeight - ToolBar::height)) {
    y = - (getHeight() - viewportHeight - ToolBar::height);
  }

  setTopLeftPosition(getX(), y);
}

void ParametersView::updateParameters() {
  for (auto& p : parameters) {
    p->update();
  }
}

MainView::MainView(StateManager& m, UIBridge& b, juce::AudioProcessorEditor* i) : manager(m), uiBridge(b), instance(i) {
  assert(i);
  addChildComponent(parametersView);
  addAndMakeVisible(toolBar);
  addAndMakeVisible(track);
  addAndMakeVisible(instance.get());
  addChildComponent(infoView);

  infoView.mainViewUpdateCallback = [this] { toggleInfoView(); };
  toolBar.infoButton.onClick = [this] { toggleInfoView(); };

  setSize(instance->getWidth() < Style::minWidth ? Style::minWidth : instance->getWidth(), instance->getHeight() + Track::height + ToolBar::height);
}

void MainView::paint(juce::Graphics& g) {
  g.fillAll(Colours::eerieBlack); 
}

void MainView::resized() {
  auto r = getLocalBounds();

  infoView.setBounds(r);
  toolBar.setBounds(r.removeFromTop(ToolBar::height));
  track.setTopLeftPosition(r.removeFromBottom(Track::height).getTopLeft());

  auto i = r.removeFromTop(instance->getHeight()); 
  instance->setBounds(i.withSizeKeepingCentre(instance->getWidth(), i.getHeight()));

  parametersView.setTopLeftPosition(i.getX(), i.getY());
  parametersView.setSize(i.getWidth(), parametersView.getHeight());
  parametersView.viewportHeight = i.getHeight();
}

void MainView::toggleParametersView() {
  if (!infoView.isVisible()) {
    instance->setVisible(!instance->isVisible()); 
    parametersView.setVisible(!parametersView.isVisible()); 
  }
}

void MainView::toggleInfoView() {
  infoView.setVisible(!infoView.isVisible());

  if (!parametersView.isVisible()) {
    instance->setVisible(!instance->isVisible());
  }
}

void MainView::childBoundsChanged(juce::Component*) {
  setSize(instance->getWidth() < Style::minWidth ? Style::minWidth : instance->getWidth(), instance->getHeight() + Track::height + ToolBar::height);
}

Editor::Editor(Plugin& p) : AudioProcessorEditor(&p), proc(p) {
  addAndMakeVisible(defaultView);
  
  useMainView = engine.hasInstance();

  if (useMainView) {
    showMainView();
  } else {
    showDefaultView();
  }

  setWantsKeyboardFocus(true);
  setResizable(false, false);
}

void Editor::paint(juce::Graphics& g) {
  g.fillAll(Colours::eerieBlack);
}

void Editor::resized() {
  auto r = getLocalBounds();
  if (useMainView) {
    mainView->setTopLeftPosition(r.getX(), r.getY());
  } else {
    defaultView.setBounds(r);
  }
}

void Editor::showMainView() {
  jassert(useMainView);
  if (!mainView) {
    mainView = std::make_unique<MainView>(manager, uiBridge, engine.getEditor());
    addAndMakeVisible(mainView.get());
  }
  defaultView.setVisible(false);
  setSize(mainView->getWidth(), mainView->getHeight());
}

void Editor::showDefaultView() {
  jassert(!useMainView && !mainView);
  defaultView.setVisible(true);
  setSize(DefaultView::width, DefaultView::height);
}

void Editor::createInstanceChangeCallback() {
  useMainView = true;
  showMainView();
}

void Editor::killInstanceChangeCallback() {
  useMainView = false;
  removeChildComponent(mainView.get());
  mainView.reset();
  showDefaultView();
}

void Editor::childBoundsChanged(juce::Component*) {
  if (useMainView) {
    showMainView();
  } else {
    showDefaultView();
  }
}

bool Editor::keyPressed(const juce::KeyPress& k) {
  auto modifier = k.getModifiers(); 
  auto code = k.getKeyCode();

  static constexpr i32 keyDelete = 127;

  static constexpr i32 keyNum1 = 49;
  static constexpr i32 keyNum2 = 50;
  static constexpr i32 keyNum3 = 51;
  static constexpr i32 keyNum4 = 52;

  static constexpr i32 keyPlus = 43;
  static constexpr i32 keyEquals = 61;
  static constexpr i32 keyMin = 45;

  static constexpr i32 keyCharD = 68;
  static constexpr i32 keyCharE = 69;
  static constexpr i32 keyCharI = 73;
  static constexpr i32 keyCharK = 75;
  static constexpr i32 keyCharR = 82;
  static constexpr i32 keyCharV = 86;
  static constexpr i32 keyCharZ = 90;

  auto& track = mainView->track;

  if (modifier.isCommandDown()) {
    switch (code) {
      case keyNum1: {
        track.grid.narrow(); 
        track.repaint();
        return true;
      } break;
      case keyNum2: {
        track.grid.widen(); 
        track.repaint();
        return true;
      } break;
      case keyNum3: {
        track.grid.triplet(); 
        track.repaint();
        return true;
      } break;
      case keyNum4: {
        track.grid.toggleSnap(); 
        track.repaint();
        return true;
      } break;
      case keyCharD: {
        manager.setAllParametersActive(false);
        return true;
      } break;
      case keyCharE: {
        manager.setAllParametersActive(true);
        return true;
      } break;
      case keyCharZ: {
        if (modifier.isShiftDown()) {
          // TODO(luca): implement redo
        } else {
          // TODO(luca): implement undo
        }
        return true;
      } break;
    };
  } else {
    switch (code) {
      case keyDelete: {
        Selection selection = track.automationLane.selection;
        selection.start /= track.zoom;
        selection.end /= track.zoom;
        manager.removeSelection(selection);
        return true;
      } break;
      case keyPlus: {
        track.zoomTrack(1);
        return true;
      } break;
      case keyEquals: {
        track.zoomTrack(1);
        return true;
      } break;
      case keyMin: {
        track.zoomTrack(-1);
        return true;
      } break;
      case keyCharE: {
        manager.setEditMode(!manager.state.editMode);
        return true;
      } break;
      case keyCharD: {
        manager.setModulateDiscrete(!manager.state.modulateDiscrete);
        return true;
      } break;
      case keyCharR: {
        manager.randomiseParameters();
        return true;
      } break;
      case keyCharV: {
        if (mainView) {
          mainView->toggleParametersView();
        }
        return true;
      } break;
      case keyCharK: {
        manager.setPluginID({}); 
        return true;
      } break;
      case keyCharI: {
        if (mainView) {
          mainView->toggleInfoView();
        }
        return true;
      } break;
    };
  } 

  DBG("Key code: " + juce::String(code));

  return false;
}

void Editor::modifierKeysChanged(const juce::ModifierKeys& k) {
  manager.state.captureParameterChanges = false;
  manager.state.releaseParameterChanges = false;
  
  if (k.isCommandDown() && k.isShiftDown()) {
    DBG("Releasing parameters");
    manager.state.releaseParameterChanges = true;
  } else if (k.isCommandDown()) {
    DBG("Capturing parameters");
    manager.state.captureParameterChanges = true; 
  }

  if (mainView) {
    auto& track = mainView->track;
    track.automationLane.optKeyPressed = k.isAltDown();
    track.shiftKeyPressed = k.isShiftDown();
    track.cmdKeyPressed = k.isCommandDown();
  }
}

} // namespace atmt
