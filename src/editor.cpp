#include "plugin.hpp"
#include "editor.hpp"

namespace atmt {

bool Grid::reset(f64 mw, TimeSignature _ts) {
  if (std::abs(zoom - zoom_) > EPSILON || std::abs(maxWidth - mw) > EPSILON || ts.numerator != _ts.numerator || ts.denominator != _ts.denominator) {
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

PathView::PathView(StateManager& m, Grid& g, Path* p) : manager(m), grid(g), path(p) {}

void PathView::paint(juce::Graphics& g) {
  if (isMouseOverOrDragging()) {
    g.setColour(juce::Colours::red);
  } else {
    g.setColour(juce::Colours::orange);
  }
  g.fillEllipse(getLocalBounds().toFloat());
}

void PathView::mouseDrag(const juce::MouseEvent& e) {
  // TODO(luca): simplify this
  auto parentLocalPoint = getParentComponent()->getLocalPoint(this, e.position);
  auto newX = f64(grid.snap(parentLocalPoint.x) / zoom);
  auto newY = f64(parentLocalPoint.y / getParentComponent()->getHeight());
  newY = std::clamp(newY, 0.0, 1.0);
  manager.movePath(path, newX >= 0 ? newX : 0, newY, path->c);
}

void PathView::mouseDoubleClick(const juce::MouseEvent&) {
  manager.removePath(path);
}

ClipView::ClipView(StateManager& m, Grid& g, Clip* c) : manager(m), grid(g), clip(c) {
  setTooltip(clip->name);
}

void ClipView::paint(juce::Graphics& g) {
  g.setColour(juce::Colours::red);
  g.fillEllipse(getLocalBounds().toFloat());
}

void ClipView::mouseDown(const juce::MouseEvent& e) {
  mouseDownOffset = e.position.x;
}

void ClipView::mouseDoubleClick(const juce::MouseEvent&) {
  manager.removeClip(clip); 
}

void ClipView::mouseUp(const juce::MouseEvent&) {
  isTrimDrag = false;
  mouseDownOffset = 0;
}

void ClipView::mouseDrag(const juce::MouseEvent& e) {
  auto parentLocalPoint = getParentComponent()->getLocalPoint(this, e.position);
  f64 y = parentLocalPoint.y > (getParentComponent()->getHeight() / 2) ? 1 : 0;
  f64 x = grid.snap(parentLocalPoint.x - mouseDownOffset) / zoom;
  manager.moveClip(clip, x, y, clip->c);
}

AutomationLane::AutomationLane(StateManager& m, Grid& g) : manager(m), grid(g) {
  manager.automationView = this;
}

AutomationLane::~AutomationLane() {
  manager.automationView = nullptr;
}

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
    juce::Path::Iterator it(automation);

    bool end = false;
    bool start = true;
    juce::Path tmp;
    it.next();

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
        if (start) {
          tmp.startNewSubPath(0, it.y2);
          tmp.lineTo(it.x2, it.y2);
          start = false;
        } else {
          tmp.startNewSubPath(x1, y1);
          tmp.quadraticTo(it.x1, it.y1, it.x2, it.y2);
        }
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
  DBG("AutomationLane::resized()");

  // TODO(luca): maybe not a good idea
  if (u64(paths.size()) != manager.paths.size()) {
    paths.clear();
    for (auto& p : manager.paths) {
      auto path = new PathView(manager, grid, &p);
      path->setBounds(i32(p.x * zoom) - PathView::posOffset, i32(p.y * getHeight()) - PathView::posOffset, PathView::size, PathView::size);
      addAndMakeVisible(path);
      paths.add(path); 
    }
  } else {
    for (auto p : paths) {
      p->setBounds(i32(p->path->x * zoom) - PathView::posOffset, i32(p->path->y * getHeight()) - PathView::posOffset, PathView::size, PathView::size);
    }
  }

  automation = manager.automation;
  automation.applyTransform(juce::AffineTransform::scale(f32(zoom), getHeight()));
}

auto AutomationLane::getAutomationPoint(juce::Point<f32> p) {
  juce::Point<f32> np;
  automation.getNearestPoint(p, np);
  return np;
}

f64 AutomationLane::getDistanceFromPoint(juce::Point<f32> p) {
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
    manager.addPath(grid.snap(p.x) / zoom, p.y / getHeight());
  } else if (d < mouseOverDistance) {
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
    auto p = manager.findAutomationPoint(xHighlightedSegment / zoom);
    assert(p->clip || p->path);

    auto offset = e.getOffsetFromDragStart();
    auto y = f64(lastMouseDragOffset.y - offset.y);
    auto increment = y / kDragIncrement;
    auto newValue = std::clamp(p->c + increment, 0.0, 1.0);
    assert(newValue >= 0 && newValue <= 1);

    if (p->clip) {
      manager.moveClip(p->clip, p->x, p->y, newValue);
    } else {
      manager.movePath(p->path, p->x, p->y, newValue);
    }

    lastMouseDragOffset = offset;
  } else if (activeGesture == GestureType::drag) {
    auto p = manager.findAutomationPoint(xHighlightedSegment / zoom);
    assert(p->clip || p->path);

    auto offset = e.getOffsetFromDragStart();
    auto y = f64(lastMouseDragOffset.y - offset.y);
    auto increment = y / kDragIncrement;
    auto newValue = std::clamp(p->y - increment, 0.0, 1.0);

    if (p->path) {
      manager.movePath(p->path, p->x, newValue, p->c);
    }

    if (p != manager.points.begin()) {
      auto prev = std::prev(p);
      if (prev->path) {
        newValue = std::clamp(prev->y - increment, 0.0, 1.0);
        manager.movePath(prev->path, prev->x, newValue, prev->c);
      }
    }

    lastMouseDragOffset = offset;
  } else if (activeGesture == GestureType::select) {
    selection.end = grid.snap(e.position.x);
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

Track::Track(StateManager& m, UIBridge& u) : manager(m), uiBridge(u) {
  addAndMakeVisible(automationLane);
  startTimerHz(60);
  setSize(getTrackWidth(), height);
  manager.trackView = this;
}

Track::~Track() {
  manager.trackView = nullptr;
}

void Track::paint(juce::Graphics& g) {
  //DBG("Track::paint()");

  auto r = getLocalBounds();
  g.fillAll(juce::Colours::grey);

  g.setColour(juce::Colours::coral);
  g.fillRect(b.timeline);

  g.setColour(juce::Colours::aqua);
  g.fillRect(b.presetLaneTop);
  g.fillRect(b.presetLaneBottom);

  { // NOTE(luca): beats
    g.setFont(10);
    g.setColour(juce::Colours::black);
    for (auto& beat : grid.beats) {
      auto beatText = juce::String(beat.bar);
      if (beat.beat > 1) {
        beatText << "." + juce::String(beat.beat);
      }
      g.drawText(beatText, i32(beat.x), r.getY(), 40, 20, juce::Justification::left);
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
  //DBG("Track::resized()");

  auto r = getLocalBounds();
  b.timeline = r.removeFromTop(timelineHeight);
  b.presetLaneTop = r.removeFromTop(presetLaneHeight);
  b.presetLaneBottom = r.removeFromBottom(presetLaneHeight);
  automationLane.setBounds(r);

  // TODO(luca): maybe not a good idea
  if (u64(clips.size()) != manager.clips.size()) {
    clips.clear();
    for (auto& c : manager.clips) {
      auto clip = new ClipView(manager, grid, &c);
      clip->setBounds(i32(c.x * zoom) - presetLaneHeight / 2,
                      !bool(i32(c.y)) ? b.presetLaneTop.getY() : b.presetLaneBottom.getY(),
                      presetLaneHeight, presetLaneHeight);
      addAndMakeVisible(clip);
      clips.add(clip); 
    }
  } else {
    for (auto c : clips) {
      c->setBounds(i32(c->clip->x * zoom) - presetLaneHeight / 2,
                   !bool(i32(c->clip->y)) ? b.presetLaneTop.getY() : b.presetLaneBottom.getY(),
                   presetLaneHeight, presetLaneHeight);
    }
  }
}

void Track::timerCallback() {
  resetGrid();
}

void Track::resetGrid() {
  Grid::TimeSignature ts { uiBridge.numerator.load(), uiBridge.denominator.load() };
  auto ph = uiBridge.playheadPosition.load() * zoom;

  if (std::abs(playheadPosition - ph) > EPSILON) {
    auto x = -viewportDeltaX;
    auto inBoundsOld = playheadPosition > x && playheadPosition < x; 
    auto inBoundsNew = ph > x && ph < (x + getWidth()); 

    playheadPosition = ph;

    if (inBoundsOld || inBoundsNew) {
      repaint();
    }
  }

  if (getWidth() > 0 && grid.reset(getWidth(), ts)) {
    repaint();
  }
}

i32 Track::getTrackWidth() {
  f64 width = 0;

  for (auto& c : manager.clips) {
    if (c.x > width) {
      width = c.x;
    }
  }

  for (auto& p : manager.paths) {
    if (p.x > width) {
      width = p.x;
    }
  }

  width *= zoom;
  return width > getParentWidth() ? int(width) : getParentWidth();
}

bool Track::isInterestedInDragSource(const juce::DragAndDropTarget::SourceDetails&) {
  return true;
}

void Track::itemDropped(const juce::DragAndDropTarget::SourceDetails& details) {
  jassert(!details.description.isVoid());
  auto name = details.description.toString();

  Preset* preset = nullptr;
  for (auto& p : manager.presets) {
    if (p.name == name) {
      preset = &p;
    }
  }

  assert(preset);
  if (preset) {
    auto x = grid.snap(details.localPosition.x) / zoom;
    auto y = details.localPosition.y > (getHeight() / 2);
    manager.addClip(preset, x, y);
  }
}

void Track::zoomTrack(f64 amount) {
  //DBG("Track::zoomTrack()");
  f64 mouseX = getMouseXYRelative().x;
  f64 z0 = zoom;
  f64 z1 = z0 + (amount * kZoomSpeed * (z0 / zoomDeltaScale)); 
  f64 x0 = -viewportDeltaX;
  f64 x1 = mouseX;
  f64 d = x1 - x0;
  f64 p = x1 / z0;
  f64 X1 = p * z1;
  f64 X0 = X1 - d;
  viewportDeltaX = -X0;
  viewportDeltaX = std::clamp(-X0, f64(-(getTrackWidth() - getParentWidth())), 0.0);
  manager.setZoom(z1 <= 0 ? EPSILON : z1);
  setBounds(i32(viewportDeltaX), getY(), getTrackWidth(), getHeight());
  jassert(std::abs(viewportDeltaX) + getParentWidth() <= getTrackWidth());
  resetGrid();
  resized();
  repaint();
}

void Track::scroll(f64 amount) {
  viewportDeltaX += amount * kScrollSpeed;
  viewportDeltaX = std::clamp(viewportDeltaX, f64(-(getWidth() - getParentWidth())), 0.0);
  setTopLeftPosition(i32(viewportDeltaX), getY());
}

void Track::mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& w) {
  if (cmdKeyPressed) {
    zoomTrack(w.deltaY);
  } else if (shiftKeyPressed) {
    scroll(w.deltaY);
  }
}

Editor::Editor(Plugin& p) : AudioProcessorEditor(&p), proc(p) {
  addAndMakeVisible(debugTools);
  addAndMakeVisible(debugInfo);
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

DebugTools::DebugTools(StateManager& m) : manager(m) {
  manager.debugView = this; 

  addAndMakeVisible(printStateButton);
  addAndMakeVisible(killButton);
  addAndMakeVisible(parametersToggleButton);
  addAndMakeVisible(playButton);
  addAndMakeVisible(stopButton);
  addAndMakeVisible(rewindButton);
  addAndMakeVisible(editModeButton);
  addAndMakeVisible(modulateDiscreteButton);
  addAndMakeVisible(undoButton);
  addAndMakeVisible(redoButton);
  addAndMakeVisible(randomiseButton);
  addAndMakeVisible(captureParameterChangesButton);

  auto plugin = static_cast<Plugin*>(&proc);
  jassert(plugin);

  printStateButton.onClick = [this] { manager.getState(); };
  editModeButton.onClick = [this] { manager.setEditMode(!editMode); };
  killButton.onClick = [this] { manager.setPluginID(""); };
  playButton.onClick = [plugin] { plugin->signalPlay(); };
  stopButton.onClick = [plugin] { plugin->signalStop(); };
  rewindButton.onClick = [plugin] { plugin->signalRewind(); };
  undoButton.onClick = {};
  redoButton.onClick = {};
  randomiseButton.onClick = [this] { manager.randomiseParameters(); };
  modulateDiscreteButton.onClick = [this] { manager.setModulateDiscrete(!modulateDiscrete); };
  captureParameterChangesButton.onClick = [this] { manager.setCaptureParameterChanges(!captureParameterChanges); };

  parametersToggleButton.onClick = [this] { 
    auto editor = static_cast<Editor*>(getParentComponent());
    if (editor->useMainView) editor->mainView->toggleParametersView();
  };
}

DebugTools::~DebugTools() {
  manager.debugView = nullptr;
}

void DebugTools::resized() {
  auto r = getLocalBounds();
  auto width = getWidth() / getNumChildComponents();
  printStateButton.setBounds(r.removeFromLeft(width));
  editModeButton.setBounds(r.removeFromLeft(width));
  modulateDiscreteButton.setBounds(r.removeFromLeft(width));
  parametersToggleButton.setBounds(r.removeFromLeft(width));
  killButton.setBounds(r.removeFromLeft(width));
  playButton.setBounds(r.removeFromLeft(width));
  stopButton.setBounds(r.removeFromLeft(width));
  rewindButton.setBounds(r.removeFromLeft(width));
  undoButton.setBounds(r.removeFromLeft(width));
  redoButton.setBounds(r.removeFromLeft(width));
  captureParameterChangesButton.setBounds(r.removeFromLeft(width));
  randomiseButton.setBounds(r);

  editModeButton.setToggleState(editMode, juce::NotificationType::dontSendNotification);
  modulateDiscreteButton.setToggleState(modulateDiscrete, juce::NotificationType::dontSendNotification);
  captureParameterChangesButton.setToggleState(captureParameterChanges, juce::NotificationType::dontSendNotification);
}

DebugInfo::DebugInfo(UIBridge& b) : uiBridge(b) {
  addAndMakeVisible(info);
  info.setText("some info");
  info.setFont(juce::FontOptions { 12 }, false);
  info.setColour(juce::Colours::white);
}

void DebugInfo::resized() {
  info.setBounds(getLocalBounds());
}

void PresetsListPanel::Title::paint(juce::Graphics& g) {
  auto r = getLocalBounds();
  g.setColour(juce::Colours::white);
  g.setFont(getHeight());
  g.drawText(text, r, juce::Justification::centred);
}

PresetsListPanel::PresetView::PresetView(StateManager& m, Preset* p) : manager(m), preset(p) {
  setName(preset->name);
  selectorButton.setButtonText(preset->name);

  addAndMakeVisible(selectorButton);
  addAndMakeVisible(overwriteButton);
  addAndMakeVisible(removeButton);

  selectorButton.onClick  = [this] { manager.loadPreset(preset); };
  overwriteButton.onClick = [this] { manager.overwritePreset(preset); };
  removeButton.onClick    = [this] { manager.removePreset(preset); };
}

void PresetsListPanel::PresetView::resized() {
  auto r = getLocalBounds();
  r.removeFromLeft(getWidth() / 8);
  removeButton.setBounds(r.removeFromRight(getWidth() / 4));
  overwriteButton.setBounds(r.removeFromLeft(getWidth() / 6));
  selectorButton.setBounds(r);
}

void PresetsListPanel::PresetView::mouseDown(const juce::MouseEvent&) {
  auto container = juce::DragAndDropContainer::findParentDragContainerFor(this);
  if (container) {
    container->startDragging(getName(), this);
  }
}

PresetsListPanel::PresetsListPanel(StateManager& m) : manager(m) {
  manager.presetView = this;

  addAndMakeVisible(title);
  addAndMakeVisible(presetNameInput);
  addAndMakeVisible(savePresetButton);

  savePresetButton.onClick = [this] () -> void {
    auto name = presetNameInput.getText();
    if (presetNameInput.getText() != "" && !manager.doesPresetNameExist(name)) {
      manager.savePreset(name);
    } else {
      juce::MessageBoxOptions options;
      juce::AlertWindow::showAsync(options.withTitle("Error").withMessage("Preset name unavailable."), nullptr);
    }
  };
}

PresetsListPanel::~PresetsListPanel() {
  manager.presetView = nullptr;
}

void PresetsListPanel::resized() {
  auto r = getLocalBounds(); 
  title.setBounds(r.removeFromTop(40));

  presets.clear();

  for (auto& preset : manager.presets) {
    auto p = new PresetView(manager, &preset);
    addAndMakeVisible(p);
    p->setBounds(r.removeFromTop(30));
    presets.add(p); 
  }
  auto b = r.removeFromBottom(40);
  savePresetButton.setBounds(b.removeFromRight(getWidth() / 2));
  presetNameInput.setBounds(b);
}

PluginListView::Contents::Contents(StateManager& m, juce::AudioPluginFormatManager& fm, juce::KnownPluginList& kpl)
  : manager(m), knownPluginList(kpl), formatManager(fm) {
  for (auto& t : knownPluginList.getTypes()) {
    addPlugin(t);
  }
  resized();
}

void PluginListView::Contents::paint(juce::Graphics& g) {
  g.fillAll(juce::Colours::green);
}

void PluginListView::Contents::resized() {
  setSize(getWidth(), buttonHeight * plugins.size());
  auto r = getLocalBounds();
  for (auto button : plugins) {
    button->setBounds(r.removeFromTop(buttonHeight));
  }
}

void PluginListView::Contents::addPlugin(juce::PluginDescription& pd) {
  auto button = new juce::TextButton(pd.pluginFormatName + " - " + pd.name);
  auto id = pd.createIdentifierString();
  addAndMakeVisible(button);
  button->onClick = [id, this] { manager.setPluginID(id); };
  plugins.add(button);
}

bool PluginListView::Contents::isInterestedInFileDrag(const juce::StringArray&){
  return true; 
}

void PluginListView::Contents::filesDropped(const juce::StringArray& files, i32, i32) {
  juce::OwnedArray<juce::PluginDescription> types;
  knownPluginList.scanAndAddDragAndDroppedFiles(formatManager, files, types);
  for (auto t : types) {
    addPlugin(*t);
  }
  resized();
}

PluginListView::PluginListView(StateManager& m, juce::AudioPluginFormatManager& fm, juce::KnownPluginList& kpl) : c(m, fm, kpl) {
  setScrollBarsShown(true, false);
  setViewedComponent(&c, false);
}

void PluginListView::resized() {
  c.setSize(getWidth(), c.getHeight()); 
}

DefaultView::DefaultView(StateManager& m, juce::KnownPluginList& kpl, juce::AudioPluginFormatManager& fm) : list(m, fm, kpl) {
  addAndMakeVisible(list);
}

void DefaultView::resized() {
  list.setBounds(getLocalBounds());
}

ParametersView::ParameterView::ParameterView(StateManager& m, Parameter* p) : manager(m), parameter(p) {
  assert(parameter);
  assert(parameter->parameter);

  parameter->parameter->addListener(this);

  name.setText(parameter->parameter->getName(150), juce::NotificationType::dontSendNotification);

  slider.setRange(0, 1);
  slider.setValue(parameter->parameter->getValue());

  addAndMakeVisible(name);
  addAndMakeVisible(slider);
  addAndMakeVisible(activeToggle);

  slider.onValueChange = [this] { parameter->parameter->setValue(f32(slider.getValue())); };
  activeToggle.setToggleState(parameter->active, juce::NotificationType::dontSendNotification);

  activeToggle.onClick = [this] { manager.setParameterActive(parameter, !parameter->active); };
}

ParametersView::ParameterView::~ParameterView() {
  parameter->parameter->removeListener(this);
}

void ParametersView::ParameterView::paint(juce::Graphics& g) {
  g.fillAll(juce::Colours::green);
}

void ParametersView::ParameterView::resized() {
  auto r = getLocalBounds();
  activeToggle.setBounds(r.removeFromLeft(50));
  name.setBounds(r.removeFromLeft(160));
  slider.setBounds(r);
}

void ParametersView::ParameterView::update() {
  activeToggle.setToggleState(parameter->active, juce::NotificationType::dontSendNotification);
}

void ParametersView::ParameterView::parameterValueChanged(i32, f32 v) {
  juce::MessageManager::callAsync([v, this] { slider.setValue(v); repaint(); });
}
 
void ParametersView::ParameterView::parameterGestureChanged(i32, bool) {}

ParametersView::Contents::Contents(StateManager& m) : manager(m) {
  for (auto& p : manager.parameters) {
    auto param = new ParameterView(manager, &p);
    addAndMakeVisible(param);
    parameters.add(param);
  }
  setSize(getWidth(), i32(parameters.size()) * 20);
}

void ParametersView::Contents::resized() {
  auto r = getLocalBounds();
  for (auto p : parameters) {
    p->setBounds(r.removeFromTop(20)); 
  }
}

ParametersView::ParametersView(StateManager& m) : manager(m), c(m) {
  DBG("ParametersView::ParametersView()");
  setViewedComponent(&c, false);
  manager.parametersView = this;
}

ParametersView::~ParametersView() {
  manager.parametersView = nullptr;
}

void ParametersView::resized() {
  c.setSize(getWidth(), c.getHeight());
}

void ParametersView::updateParameters() {
  for (auto& p : c.parameters) {
    p->update();
  }
}

MainView::MainView(StateManager& m, UIBridge& b, juce::AudioProcessorEditor* i) : manager(m), uiBridge(b), instance(i) {
  jassert(i);
  addAndMakeVisible(statesPanel);
  addAndMakeVisible(track);
  addAndMakeVisible(instance.get());
  addChildComponent(parametersView);
  setSize(instance->getWidth() + statesPanelWidth, instance->getHeight() + Track::height);
}

void MainView::resized() {
  auto r = getLocalBounds();
  track.setTopLeftPosition(r.removeFromBottom(Track::height).getTopLeft());
  statesPanel.setBounds(r.removeFromLeft(statesPanelWidth));
  instance->setBounds(r.removeFromTop(instance->getHeight()));
  parametersView.setBounds(instance->getBounds());
}

void MainView::toggleParametersView() {
  instance->setVisible(!instance->isVisible()); 
  parametersView.setVisible(!parametersView.isVisible()); 
}

void MainView::childBoundsChanged(juce::Component*) {
  setSize(instance->getWidth() + statesPanelWidth, instance->getHeight() + Track::height);
}

void Editor::paint(juce::Graphics& g) {
  g.fillAll(juce::Colours::black);
}

void Editor::resized() {
  auto r = getLocalBounds();
  debugTools.setBounds(r.removeFromTop(debugToolsHeight));
  debugInfo.setBounds(r.removeFromBottom(debugInfoHeight));
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
  setSize(mainView->getWidth(), mainView->getHeight() + debugToolsHeight + debugInfoHeight);
}

void Editor::showDefaultView() {
  jassert(!useMainView && !mainView);
  defaultView.setVisible(true);
  setSize(width, height);
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
  //static constexpr i32 keyCharX = 88;
  static constexpr i32 keyCharZ = 90;
  static constexpr i32 keyPlus = 43;
  static constexpr i32 keyEquals = 61;
  static constexpr i32 keyMin = 45;

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
      case keyCharZ: {
        if (modifier.isShiftDown()) {
          // TODO(luca): implment 
        } else {
          // TODO(luca): implment 
        }
        return true;
      } break;
    };
  } else if (code == keyDelete) {
    //std::vector<juce::ValueTree> toRemove;
    //for (auto p : track.automationLane.paths) {
    //  auto zoom = f64(manager.editTree[ID::zoom]);
    //  if (p->x * zoom >= track.automationLane.selection.start && p->x * zoom <= track.automationLane.selection.end) {
    //    toRemove.push_back(p->state);
    //  }
    //}
    //undoManager->beginNewTransaction();
    //for (auto& v : toRemove) {
    //  // TODO(luca): fix
    //  //manager.removePath(v);
    //}
    return true;
  } else if (code == keyPlus || code == keyEquals) {
    track.zoomTrack(1);
    return true;
  } else if (code == keyMin) {
    track.zoomTrack(-1);
    return true;
  }
  //DBG(code);
  return false;
}

void Editor::modifierKeysChanged(const juce::ModifierKeys& k) {
  auto& track = mainView->track;
  track.automationLane.optKeyPressed = k.isAltDown();
  track.shiftKeyPressed = k.isShiftDown();
  track.cmdKeyPressed = k.isCommandDown();
}

} // namespace atmt
