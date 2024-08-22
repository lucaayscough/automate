#include "plugin.hpp"
#include "editor.hpp"

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

  zoom.forceUpdateOfCachedValue();
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

void Track::zoomTrack(f64 amount, f64 mouseX) {
  //DBG("Track::zoomTrack()");

  // TODO(luca): the component jiggles because of floats being cast to ints in setBounds()
  // need to hack JUCE or find another way

  zoom.forceUpdateOfCachedValue();

  f64 z0 = zoom;
  f64 z1 = z0 + (amount * (z0 / zoomDeltaScale)); 

  zoom.setValue(z1 <= 0 ? EPSILON : z1, nullptr);

  f64 x0 = viewport.x;
  f64 x1 = mouseX;
  f64 d = x1 - x0;
  f64 p = x1 / z0;
  f64 X1 = p * z1;
  f64 X0 = X1 - d;

  viewport.setViewPosition(i32(X0), 0);
  viewport.x = X0;
}

void Track::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w) {
  if (cmdKeyPressed) {
    zoomTrack(w.deltaY * kZoomSpeed, e.position.x);
  } else if (shiftKeyPressed) {
    viewport.setViewPosition(i32(viewport.getViewPositionX() - w.deltaX * kScrollSpeed), 0);
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

  auto plugin = static_cast<Plugin*>(&proc);
  jassert(plugin);

  printStateButton        .onClick = [this] { DBG(manager.valueTreeToXmlString(manager.state)); };
  editModeButton          .onClick = [this] { editModeAttachment.setValue({ !editMode }); };
  killButton              .onClick = [this] { pluginID.setValue("", undoManager); };
  playButton              .onClick = [plugin] { plugin->signalPlay(); };
  stopButton              .onClick = [plugin] { plugin->signalStop(); };
  rewindButton            .onClick = [plugin] { plugin->signalRewind(); };
  undoButton              .onClick = [this] { undoManager->undo(); };
  redoButton              .onClick = [this] { undoManager->redo(); };
  randomiseButton         .onClick = [plugin] { plugin->engine.randomiseParameters(); };
  modulateDiscreteButton  .onClick = [this] { modulateDiscreteAttachment.setValue({ !modulateDiscrete }); };

  parametersToggleButton.onClick = [this] { 
    auto editor = static_cast<Editor*>(getParentComponent());
    if (editor->useMainView) editor->mainView->toggleParametersView();
  };
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
  randomiseButton.setBounds(r);
}

void DebugTools::editModeChangeCallback(const juce::var& v) {
  editModeButton.setToggleState(bool(v), juce::NotificationType::dontSendNotification);
}

void DebugTools::modulateDiscreteChangeCallback(const juce::var& v) {
  modulateDiscreteButton.setToggleState(bool(v), juce::NotificationType::dontSendNotification);
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

PresetsListPanel::Preset::Preset(StateManager& m, const juce::String& n) : manager(m) {
  setName(n);
  selectorButton.setButtonText(n);

  addAndMakeVisible(selectorButton);
  addAndMakeVisible(overwriteButton);
  addAndMakeVisible(removeButton);

  // TODO(luca): find more appropriate way of doing this 
  selectorButton.onClick = [this] {
    static_cast<Plugin*>(&proc)->engine.restoreFromPreset(getName());
  };

  overwriteButton.onClick = [this] {
    undoManager->beginNewTransaction();
    manager.overwritePreset(getName());
  };

  removeButton.onClick = [this] {
    undoManager->beginNewTransaction();
    manager.removePreset(getName());
  };
}

void PresetsListPanel::Preset::resized() {
  auto r = getLocalBounds();
  r.removeFromLeft(getWidth() / 8);
  removeButton.setBounds(r.removeFromRight(getWidth() / 4));
  overwriteButton.setBounds(r.removeFromLeft(getWidth() / 6));
  selectorButton.setBounds(r);
}

void PresetsListPanel::Preset::mouseDown(const juce::MouseEvent&) {
  auto container = juce::DragAndDropContainer::findParentDragContainerFor(this);
  if (container) {
    container->startDragging(getName(), this);
  }
}

PresetsListPanel::PresetsListPanel(StateManager& m) : manager(m) {
  addAndMakeVisible(title);
  addAndMakeVisible(presetNameInput);
  addAndMakeVisible(savePresetButton);

  // TODO(luca): the vt listeners can probably go now

  savePresetButton.onClick = [this] () -> void {
    auto name = presetNameInput.getText();
    if (presetNameInput.getText() != "" && !manager.doesPresetNameExist(name)) {
      manager.savePreset(name);
    } else {
      juce::MessageBoxOptions options;
      juce::AlertWindow::showAsync(options.withTitle("Error").withMessage("Preset name unavailable."), nullptr);
    }
  };

  presetsTree.addListener(this);

  for (const auto& child : presetsTree) {
    addPreset(child);
  }
}

void PresetsListPanel::resized() {
  auto r = getLocalBounds(); 
  title.setBounds(r.removeFromTop(40));
  for (auto* preset : presets) {
    preset->setBounds(r.removeFromTop(30));
  }
  auto b = r.removeFromBottom(40);
  savePresetButton.setBounds(b.removeFromRight(getWidth() / 2));
  presetNameInput.setBounds(b);
}

void PresetsListPanel::addPreset(const juce::ValueTree& presetTree) {
  jassert(presetTree.isValid());
  auto nameVar = presetTree[ID::name];
  jassert(!nameVar.isVoid());
  auto preset = new Preset(manager, nameVar.toString());
  presets.add(preset);
  addAndMakeVisible(preset);
  resized();
}

void PresetsListPanel::removePreset(const juce::ValueTree& presetTree) {
  jassert(presetTree.isValid());
  auto nameVar = presetTree[ID::name];
  jassert(!nameVar.isVoid());
  auto name = nameVar.toString();
  for (i32 i = 0; i < presets.size(); ++i) {
    if (presets[i]->getName() == name) {
      removeChildComponent(presets[i]);
      presets.remove(i);
    }
  }
  resized();
}

i32 PresetsListPanel::getNumPresets() {
  return presets.size();
}

void PresetsListPanel::valueTreeChildAdded(juce::ValueTree&, juce::ValueTree& child) {
  addPreset(child);
}

void PresetsListPanel::valueTreeChildRemoved(juce::ValueTree&, juce::ValueTree& child, i32) { 
  removePreset(child);
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
  button->onClick = [id, this] { pluginID.setValue(id, undoManager); };
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

ParametersView::Parameter::Parameter(juce::AudioProcessorParameter* p) : parameter(p) {
  jassert(parameter);
  parameter->addListener(this);
  name.setText(parameter->getName(150), juce::NotificationType::dontSendNotification);
  slider.setRange(0, 1);
  slider.setValue(parameter->getValue());
  addAndMakeVisible(name);
  addAndMakeVisible(slider);
  slider.onValueChange = [this] { parameter->setValue(f32(slider.getValue())); };
}

ParametersView::Parameter::~Parameter() {
  parameter->removeListener(this);
}

void ParametersView::Parameter::paint(juce::Graphics& g) {
  g.fillAll(juce::Colours::green);
}

void ParametersView::Parameter::resized() {
  auto r = getLocalBounds();
  name.setBounds(r.removeFromLeft(160));
  slider.setBounds(r);
}

void ParametersView::Parameter::parameterValueChanged(i32, f32 v) {
  juce::MessageManager::callAsync([v, this] {
    slider.setValue(v);
    repaint();
  });
}
 
void ParametersView::Parameter::parameterGestureChanged(i32, bool) {}

ParametersView::Contents::Contents(const juce::Array<juce::AudioProcessorParameter*>& i) {
  for (auto p : i) {
    jassert(p);
    auto param = new Parameter(p);
    addAndMakeVisible(param);
    parameters.add(param);
  }
  setSize(getWidth(), parameters.size() * 20);
}

void ParametersView::Contents::resized() {
  auto r = getLocalBounds();
  for (auto p : parameters) {
    p->setBounds(r.removeFromTop(20)); 
  }
}

ParametersView::ParametersView(const juce::Array<juce::AudioProcessorParameter*>& i) : c(i) {
  setViewedComponent(&c, false);
}

void ParametersView::resized() {
  c.setSize(getWidth(), c.getHeight());
}

MainView::MainView(StateManager& m, UIBridge& b, juce::AudioProcessorEditor* i, const juce::Array<juce::AudioProcessorParameter*>& p) : manager(m), uiBridge(b), instance(i), parametersView(p) {
  jassert(i);
  addAndMakeVisible(statesPanel);
  addAndMakeVisible(track.viewport);
  addAndMakeVisible(instance.get());
  addChildComponent(parametersView);
  setSize(instance->getWidth() + statesPanelWidth, instance->getHeight() + Track::height);
}

void MainView::resized() {
  // TODO(luca): clean up viewport stuff
  auto r = getLocalBounds();
  track.viewport.setBounds(r.removeFromBottom(Track::height));
  track.resized();
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
    mainView = std::make_unique<MainView>(manager, uiBridge, engine.getEditor(), engine.instance->getParameters());
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

void Editor::pluginIDChangeCallback(const juce::var&) {
  // TODO(luca):
  //auto description = proc.knownPluginList.getTypeForIdentifierString(v.toString());
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
          undoManager->redo();
        } else {
          undoManager->undo(); 
        }
        return true;
      } break;
    };
  } else if (code == keyDelete) {
    std::vector<juce::ValueTree> toRemove;
    for (auto p : track.automationLane.paths) {
      auto zoom = f64(manager.editTree[ID::zoom]);
      if (p->x * zoom >= track.automationLane.selection.start && p->x * zoom <= track.automationLane.selection.end) {
        toRemove.push_back(p->state);
      }
    }
    undoManager->beginNewTransaction();
    for (auto& v : toRemove) {
      manager.removePath(v);
    }
    return true;
  } 

  return false;
}

void Editor::modifierKeysChanged(const juce::ModifierKeys& k) {
  auto& track = mainView->track;
  track.automationLane.optKeyPressed = k.isAltDown();
  track.shiftKeyPressed = k.isShiftDown();
  track.cmdKeyPressed = k.isCommandDown();
}

} // namespace atmt
