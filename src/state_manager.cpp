#include "state_manager.hpp"
#include "plugin.hpp"
#include "editor.hpp"
#include <assert.h>
#include "scoped_timer.hpp"
#include "logger.hpp"

namespace atmt {

StateManager::StateManager(juce::AudioProcessor& a) : proc(a) {}

void StateManager::addClip(f32 x, f32 y, f32 curve) {
  JUCE_ASSERT_MESSAGE_THREAD
  scoped_timer t("StateManager::addClip()");

  assert(x >= 0 && y >= 0 && y <= 1);
  assert(instance);

  {
    ScopedProcLock lk(proc);

    clips.emplace_back();
    auto& clip = clips.back();

    clip.x = x;
    clip.y = y;
    clip.c = curve;

    clip.parameters.reserve(parameters.size());

    for (auto& parameter : parameters) {
      clip.parameters.push_back(parameter.parameter->getValue());
      assert(isNormalised(clip.parameters.back()));
    }

    if (selectedClipID != NONE) {
      selectClip(NONE);
    }

    updateTrack();
  }
}

void StateManager::addClipDenorm(f32 x, f32 y, f32 curve) {
  addClip(grid.snap(x) / zoom, y, curve);
}

void StateManager::duplicateClip(u32 id, f32 x, bool top) {
  JUCE_ASSERT_MESSAGE_THREAD
  assert(instance && editor);
  assert(id < clips.size());

  {
    ScopedProcLock lk(proc);

    clips.push_back(clips[id]);
    auto& newClip = clips.back();

    newClip.x = x;
    newClip.y = f32(!top);
    newClip.c = 0.5f;

    if (selectedClipID != NONE) {
      selectClip(NONE);
    }

    updateTrack();
  }

  if (editMode) {
    engine->interpolate();
  }
}

void StateManager::duplicateClipDenorm(u32 id, f32 x, bool top) {
  JUCE_ASSERT_MESSAGE_THREAD
  assert(instance && editor);
  assert(id < clips.size());

  duplicateClip(id, grid.snap(x) / zoom, top);
}

void StateManager::moveClip(u32 id, f32 x, f32 y, f32 curve) {
  JUCE_ASSERT_MESSAGE_THREAD
  assert(instance && editor);
  assert(id < clips.size());

  if (neqf32(clips[id].x, x) || neqf32(clips[id].y, y) || neqf32(clips[id].c, curve)) {
    {
      ScopedProcLock lk(proc);

      clips[id].x = x < 0 ? 0 : x;
      clips[id].y = std::clamp(y, 0.f, 1.f);
      clips[id].c = std::clamp(curve, 0.f, 1.f);

      updateTrack();
    }

    if (editMode) {
      engine->interpolate();
    }
  }
}

void StateManager::moveClipDenorm(u32 id, f32 x, f32 y, f32 curve) {
  assert(id < clips.size());
  moveClip(id, grid.snap(x) / zoom, y, curve);
}

void StateManager::selectClip(i32 id) {
  assert(instance && editor);
  assert(id < i32(clips.size()));
  selectedClipID = id;

  if (id != NONE) {
    engine->setParameters(clips[u32(id)].parameters, parameters);
  }

  updateTrackView();
}

void StateManager::removeClip(u32 id) {
  JUCE_ASSERT_MESSAGE_THREAD
  assert(instance && editor);
  assert(id < clips.size());

  {
    ScopedProcLock lk(proc);
    clips.erase(clips.begin() + id);

    if (selectedClipID != NONE) {
      selectClip(NONE);
    }

    updateTrack();
  }

  if (editMode && !clips.empty()) {
    engine->interpolate();
  }
}

u32 StateManager::addPath(f32 x, f32 y, f32 curve) {
  JUCE_ASSERT_MESSAGE_THREAD
  assert(instance);
  assert(x >= 0 && isNormalised(y) && isNormalised(curve));

  {
    ScopedProcLock lk(proc);
    paths.emplace_back();
    auto& path = paths.back();
    path.x = x;
    path.y = y;
    path.c = curve;

    if (selectedClipID != NONE) {
      selectClip(NONE);
    }

    updateTrack();
  }

  if (editMode && !clips.empty()) {
    engine->interpolate();
  }

  return u32(paths.size() - 1);
}

u32 StateManager::addPathDenorm(f32 x, f32 y, f32 curve) {
  JUCE_ASSERT_MESSAGE_THREAD
  assert(x >= 0 && rangeIncl(y, 0, kAutomationLaneHeight) && rangeIncl(curve, 0, 1));
  return addPath(grid.snap(x) / zoom, y / kAutomationLaneHeight, curve);
}

void StateManager::movePath(u32 id, f32 x, f32 y, f32 c) {
  JUCE_ASSERT_MESSAGE_THREAD
  assert(instance && editor);
  assert(id < paths.size());

  // TODO(luca): trackwidth
  x = x < 0 ? 0 : x;
  y = std::clamp(y, 0.f, 1.f);
  c = std::clamp(c, 0.f, 1.f);

  if (neqf32(x, paths[id].x) || neqf32(y, paths[id].y) || neqf32(c, paths[id].c)) {
    {
      ScopedProcLock lk(proc);
      paths[id].x = x;
      paths[id].y = y;
      paths[id].c = c;

      if (selectedClipID != NONE) {
        selectClip(NONE);
      }

      updateTrack(); 
    }

    if (editMode && clips.size() > 1) {
      engine->interpolate();
    }
  }
}

void StateManager::movePathDenorm(u32 id, f32 x, f32 y, f32 c) {
  JUCE_ASSERT_MESSAGE_THREAD
  assert(id < paths.size());
  movePath(id, grid.snap(x) / zoom, y / kAutomationLaneHeight, c);
}

void StateManager::removePath(u32 id) {
  JUCE_ASSERT_MESSAGE_THREAD
  assert(instance && editor);
  assert(id < paths.size());

  {
    ScopedProcLock lk(proc);
    paths.erase(paths.begin() + id);

    if (selectedClipID != NONE) {
      selectClip(NONE);
    }

    updateTrack();
  }

  if (editMode && !clips.empty()) {
    engine->interpolate();
  }
}

void StateManager::bendAutomation(f32 position, f32 amount) {
  assert(instance && editor);
  assert(position >= 0);

  i32 pointIndex = findAutomationPoint(position);

  assert(pointIndex != NONE);

  auto& point = points[u32(pointIndex)];

  assert(point.clip || point.path);

  f32 curve = std::clamp(point.c + amount, 0.f, 1.f);

  if (point.clip) {
    moveClip(point.id, point.x, point.y, curve);
  } else {
    movePath(point.id, point.x, point.y, curve);
  }
}

void StateManager::bendAutomationDenorm(f32 position, f32 amount) {
  bendAutomation(grid.snap(position) / zoom, amount);
}

void StateManager::flattenAutomationCurve(f32 position) {
  assert(instance && editor);
  assert(position >= 0);
 
  i32 pointIndex = findAutomationPoint(position);
  assert(u32(pointIndex) < points.size());

  if (pointIndex != NONE) {
    const auto& point = points[u32(pointIndex)];
    assert(point.clip || point.path);

    if (point.path) {
      movePath(point.id, point.x, point.y, 0.5f);
    } else {
      moveClip(point.id, point.x, point.y, 0.5f);
    }
  } 
}

void StateManager::flattenAutomationCurveDenorm(f32 position) {
  flattenAutomationCurve(grid.snap(position) / zoom);
}

void StateManager::dragAutomationSection(f32 position, f32 amount) {
  assert(instance && editor);

  i32 pointIndex_ = findAutomationPoint(position);

  assert(pointIndex_ != NONE);
  u32 pointIndex = u32(pointIndex_);

  const auto& point = points[pointIndex];
  assert(point.clip || point.path);

  f32 y = std::clamp(point.y - amount, 0.f, 1.f);

  if (point.path) {
    movePath(point.id, point.x, y, point.c);
  }

  if (pointIndex != 0 && pointIndex + 1 != points.size() - 1) {
    const auto& prev = points[pointIndex - 1];

    if (prev.path) {
      y = std::clamp(prev.y - amount, 0.f, 1.f);
      movePath(prev.id, prev.x, y, prev.c);
    }
  }
}

void StateManager::dragAutomationSectionDenorm(f32 position, f32 amount) {
  dragAutomationSection(grid.snap(position) / zoom, amount);
}

i32 StateManager::findAutomationPoint(f32 x) {
  assert(instance && editor);
  assert(x >= 0);

  for (u32 i = 0; i < points.size(); ++i) {
    if (i != 0) {
      if (x >= points[i - 1].x && x <= points[i].x) {
        return i32(i);
      } 
    } else if (x <= points[i].x) {
      return i32(i);
    }
  }

  if (points.size() > 0) {
    return i32(points.size() - 1);
  }

  return NONE;
}

i32 StateManager::findAutomationPointDenorm(f32 x) {
  return findAutomationPoint(grid.snap(x) / zoom);
}

void StateManager::doZoom(f32 amount, i32 position) {
  JUCE_ASSERT_MESSAGE_THREAD
  assert(instance && editor && trackView);

  {
    f32 z0 = zoom;
    f32 z1 = z0 + (amount * kZoomSpeed * (z0 / kZoomDeltaScale)); 

    z1 = std::clamp(z1, 0.001f, 10000.f);
    zoom = z1;

    f32 x0 = -viewportDeltaX;
    f32 x1 = position;
    f32 d = x1 - x0;
    f32 p = x1 / z0;
    f32 X1 = p * z1;
    f32 X0 = X1 - d;

    updateTrackWidth();
    assert(trackWidth >= kWidth);

    viewportDeltaX = std::clamp(i32(-X0), -(trackWidth - kWidth), 0);
    assert(viewportDeltaX <= 0);

    updateGrid();
    updateTrackView();
    updateAutomationView();
  }
  
  trackView->setTopLeftPosition(viewportDeltaX, trackView->getY());
}

void StateManager::doScroll(f32 amount) {
  JUCE_ASSERT_MESSAGE_THREAD
  assert(instance && editor && trackView);

  viewportDeltaX += i32(amount * kScrollSpeed);
  viewportDeltaX = std::clamp(viewportDeltaX, -(trackWidth - kWidth), 0);
  trackView->setTopLeftPosition(viewportDeltaX, trackView->getY());
}

void StateManager::setEditMode(bool m) {
  JUCE_ASSERT_MESSAGE_THREAD
  scoped_timer t("StateManager::setEditMode()");

  editMode = m;

  if (toolBarView) {
    updateToolBarView();
  }
}

void StateManager::setDiscreteMode(bool m) {
  JUCE_ASSERT_MESSAGE_THREAD

  discreteMode = m;

  if (toolBarView) {
    updateToolBarView();
  }
}

void StateManager::setSelection(f32 start, f32 end) {
  JUCE_ASSERT_MESSAGE_THREAD
  assert(instance && editor && trackView);
  assert(start >= 0 && end >= 0);

  selection.start = start; 
  selection.end = end;

  automationView->selection.start = selection.start * zoom;
  automationView->selection.end = selection.end * zoom;
  automationView->repaint();
}

void StateManager::setSelectionDenorm(f32 start, f32 end) {
  setSelection(grid.snap(start) / zoom, grid.snap(end) / zoom);
}

void StateManager::removeSelection() {
  JUCE_ASSERT_MESSAGE_THREAD
  assert(instance && editor && trackView);
  assert(selection.start >= 0 && selection.end >= 0);

  if (selection.start > selection.end) {
    std::swap(selection.start, selection.end);
  }

  if (std::abs(selection.start - selection.end) > EPSILON) {
    ScopedProcLock lk(proc);

    std::erase_if(clips, [this] (const Clip& c) { return c.x >= selection.start && c.x <= selection.end; }); 
    std::erase_if(paths, [this] (const Path& p) { return p.x >= selection.start && p.x <= selection.end; }); 
  }

  updateTrack();
}

void StateManager::setPlayheadPosition(f32 x) {
  JUCE_ASSERT_MESSAGE_THREAD
  assert(instance && editor && trackView && automationView);
  assert(x >= 0);

  if (editMode) {
    if (neqf32(playheadPosition, x)) {
      playheadPosition = x;

      if (trackView) {
        trackView->playhead.x = playheadPosition * zoom;
        trackView->repaint();
      }

      if (!clips.empty()) {
        engine->interpolate();
      }
    }
  }

  selectClip(NONE);
}

void StateManager::setPlayheadPositionDenorm(f32 x) {
  setPlayheadPosition(grid.snap(x) / zoom);
}

bool StateManager::shouldProcessParameter(u32 index) {
  if (parameters[index].active && parameters[index].parameter->isAutomatable()) {
    return parameters[index].parameter->isDiscrete() ? discreteMode.load() : true; 
  }

  return false;
}

void StateManager::randomiseParameters() {
  JUCE_ASSERT_MESSAGE_THREAD

  for (u32 i = 0; i < parameters.size(); ++i) {
    if (shouldProcessParameter(i)) {
      parameters[i].parameter->setValueNotifyingHost(random(randomSpread));
    }
  }
}

void StateManager::setAllParametersActive(bool v) {
  JUCE_ASSERT_MESSAGE_THREAD
  assert(parametersView);
  
  {
    ScopedProcLock lk(proc);

    for (u32 i = 0; i < parameters.size(); ++i) {
      setParameterActive(i, v);
    }
  }
}

void StateManager::setParameterActive(u32 index, bool a) {
  JUCE_ASSERT_MESSAGE_THREAD
  assert(parametersView);

  {
    ScopedProcLock lk(proc);
    parameters[index].active = a;
  }

  parametersView->parameterViews[i32(index)]->dial.active = a;
  parametersView->parameterViews[i32(index)]->activeToggle.setToggleState(a, DONT_NOTIFY);
  parametersView->parameterViews[i32(index)]->repaint();
}

void StateManager::parameterValueChanged(i32 i, f32 v) {
  JUCE_ASSERT_MESSAGE_THREAD
  DBG("Parameter " + parameters[u32(i)].parameter->getName(1024) + " at index " + juce::String(i) + " changed.");

  auto sendParameterUpdate = [this, i, v] {
    engine->lastVisitedPair = UNDEFINED_PAIR; 

    if (captureParameterChanges) {
      setParameterActive(u32(i), true);
    } else if (releaseParameterChanges) {
      setParameterActive(u32(i), false);
    }

    if (shouldProcessParameter(u32(i))) {
      if (!editMode) {
        setEditMode(true);
      } else if (selectedClipID != NONE) {
        clips[u32(selectedClipID)].parameters[u32(i)] = parameters[u32(i)].parameter->getValue();
        updateLerpPairs();
      }
    }

    if (parametersView) {
      parametersView->parameterViews[i]->dial.setValue(v, DONT_NOTIFY);
      parametersView->parameterViews[i]->dial.repaint();
    }
  };

  if (juce::MessageManager::existsAndIsLockedByCurrentThread() || juce::MessageManager::existsAndIsCurrentThread()) {
    sendParameterUpdate();
  } else {
    juce::MessageManager::callAsync([sendParameterUpdate] { sendParameterUpdate(); });
  }
}

void StateManager::parameterGestureChanged(i32, bool) {}

void StateManager::updateLerpPairs() {
  scoped_timer t("StateManager::updateLerpPair()");

  ScopedProcLock lk(proc);

  assert(engine);
  assert(clips.size() > 1);
  
  auto& pairs = engine->lerpPairs;

  pairs.resize(clips.size()); 
  
  for (u32 i = 0; i < clips.size(); ++i) {
    pairs[i].start = clips[i].x;
    pairs[i].a = i;
  }

  std::sort(pairs.begin(), pairs.end(), [] (LerpPair& a, LerpPair& b) { return a.start < b.start; });

  u32 numParameters = u32(clips[0].parameters.size());

  for (u32 i = 1; i < clips.size(); ++i) {
    pairs[i - 1].end = pairs[i].start;
    pairs[i - 1].b = pairs[i].a;

    u32 a = pairs[i - 1].a;
    u32 b = pairs[i - 1].b;

    if (i32(clips[a].y) != i32(clips[b].y)) {
      pairs[i - 1].interpolate = true;
      pairs[i - 1].parameters.resize(numParameters);

      for (u32 parameterIndex = 0; parameterIndex < numParameters; ++parameterIndex) {
        pairs[i - 1].parameters[parameterIndex] = neqf32(clips[a].parameters[parameterIndex], clips[b].parameters[parameterIndex]);
      }

      bool foundInterpolation = false;

      for (u32 parameterIndex = 0; parameterIndex < numParameters; ++parameterIndex) {
        if (pairs[i - 1].parameters[parameterIndex]) {
          foundInterpolation = true;
          break;
        }
      }

      if (!foundInterpolation) {
        pairs[i - 1].interpolate = false; 
      }
    } else {
      pairs[i - 1].interpolate = false;
    }
  }

  pairs.pop_back();
}

void StateManager::updateAutomation() {
  automation.clear();
  points.resize(clips.size() + paths.size());

  u32 n = 0;
  for (; n < clips.size(); ++n) {
    points[n].x = clips[n].x; 
    points[n].y = clips[n].y; 
    points[n].c = clips[n].c; 
    points[n].id = n;
    points[n].clip = &clips[n];
    points[n].path = nullptr;
  }
  for (u32 i = 0; i < paths.size(); ++i) {
    points[i + n].x = paths[i].x; 
    points[i + n].y = paths[i].y; 
    points[i + n].c = paths[i].c; 
    points[i + n].id = i; 
    points[i + n].clip = nullptr;
    points[i + n].path = &paths[i];
  }

  std::sort(points.begin(), points.end(), [] (const AutomationPoint& a, const AutomationPoint& b) { return a.x < b.x; });

  if (points.size() > 0) {
    automation.startNewSubPath(0, points[0].y);
  }

  for (auto& p2 : points) {
    auto p1 = automation.getCurrentPosition();
    f32 cx = p1.x + (p2.x - p1.x) * (p1.y < p2.y ? p2.c : 1.f - p2.c); 
    f32 cy = (p1.y < p2.y ? p1.y : p2.y) + std::abs(p2.y - p1.y) * (1.f - p2.c);
    automation.quadraticTo(cx, cy, p2.x, p2.y);
  }
}

void StateManager::updateAutomationView() {
  assert(automationView);

  automationView->selection.start = selection.start * zoom;
  automationView->selection.end = selection.end * zoom;
  automationView->automation = automation; 

  automationView->automation.applyTransform(juce::AffineTransform::scale(zoom, kAutomationLaneHeight - Style::lineThickness));
  automationView->automation.applyTransform(juce::AffineTransform::translation(0, Style::lineThickness / 2));

  auto p = automationView->automation.getCurrentPosition();
  automationView->automation.quadraticTo(trackWidth, p.y, trackWidth, p.y);

  {
    auto& pathViews = automationView->pathViews;
    u32 numPaths = u32(paths.size());
    u32 numViews = u32(pathViews.size());

    if (numViews < numPaths) {
      u32 diff = numPaths - numViews;

      while (diff--) {
        pathViews.add(new PathView());
        automationView->addAndMakeVisible(pathViews.getLast());
      }
    } else if (numViews > numPaths) {
      i32 diff = i32(numViews - numPaths);
      pathViews.removeLast(diff);
    }

    for (u32 i = 0; i < numPaths; ++i) {
      const auto& path = paths[i];
      auto* view = pathViews[i32(i)];
     
      view->id     = i;
      view->move   = [&path, this] (u32 id, f32 x, f32 y) { movePathDenorm(id, x, y, path.c); };
      view->remove = [this] (u32 id) { removePath(id); };

      i32 x = i32(path.x * zoom - PathView::posOffset);
      i32 y = i32(path.y * automationView->getHeight() - PathView::posOffset);
         
      view->setBounds(x, y, PathView::size, PathView::size);
    }
  }

  automationView->repaint();
}

void StateManager::updateTrackView() {
  scoped_timer t("StateManager::updateTrackView()");
  assert(trackView);

  auto& clipViews = trackView->clipViews;

  u32 numClips = u32(clips.size());
  u32 numViews = u32(clipViews.size());

  if (numViews < numClips) {
    u32 diff = numClips - numViews;

    while (diff--) {
      clipViews.add(new ClipView());
      trackView->addAndMakeVisible(clipViews.getLast());
    }
  } else if (numViews > numClips) {
    i32 diff = i32(numViews - numClips);
    clipViews.removeLast(diff);
  }

  for (u32 i = 0; i < numClips; ++i) {
    const auto& clip = clips[i];
    auto* view = clipViews[i32(i)];

    view->id = i;
    view->selected = selectedClipID == i32(i);

    view->move = [&clip, this] (u32 id, f32 x, f32 y) { moveClipDenorm(id, x, y, clip.c); };
    view->remove = [this] (u32 id) { removeClip(id); };
    view->select = [this] (i32 id) { selectClip(id); };

    i32 h = kPresetLaneHeight;
    i32 w = kPresetLaneHeight;
    i32 x = i32((clip.x * zoom) - w * 0.5f);
    i32 y = !bool(clip.y) ? trackView->b.presetLaneTop.getY() : trackView->b.presetLaneBottom.getY();

    view->setBounds(x, y, w, h);
  }

  trackView->setSize(trackWidth, kTrackHeight);
  trackView->repaint();
}

void StateManager::updateGrid() {
  assert(editor && automationView);

  if (neqf32(grid.zoom, zoom) || neqf32(grid.maxWidth, trackWidth)) {
    grid.zoom = zoom;
    grid.maxWidth = trackWidth;
    grid.reset();
    automationView->repaint();
  }
}

void StateManager::updateTrack() {
  assert(instance);

  updateAutomation(); 

  if (instanceEditor) {
    assert(editor && trackView && automationView);

    updateTrackWidth();
    updateGrid();
    updateTrackView();
    updateAutomationView();
  }

  if (clips.size() > 1) {
    updateLerpPairs();
  }
}

void StateManager::updateToolBarView() {
  assert(toolBarView);
  toolBarView->editModeButton.setToggleState(editMode, DONT_NOTIFY);
  toolBarView->discreteModeButton.setToggleState(discreteMode, DONT_NOTIFY);
  toolBarView->repaint();
}

void StateManager::updateTrackWidth() {
  f32 width = 0;

  for (const auto& point : points) {
    if (point.x > width) {
      width = point.x;
    }
  }

  width *= zoom;
  width += kTrackWidthRightPadding;

  if (width > kWidth) {
    trackWidth = i32(width);
  } else {
    trackWidth = kWidth;
  }
}

void StateManager::showDefaultView() {
  assert(editor);
  stopTimer();
  editor->mainView.setVisible(false);
  editor->defaultView.setVisible(true);
  editor->setSize(kDefaultViewWidth, kDefaultViewHeight);
}

void StateManager::showMainView() {
  assert(instance);

  instanceEditor.reset(instance->createEditor());
  editor->instanceWindow = std::make_unique<InstanceWindow>(instanceEditor.get());
  assert(editor->instanceWindow);
  editor->mainView.setVisible(true);
  editor->defaultView.setVisible(false);
  editor->setSize(kWidth, kHeight);

  trackView = &editor->mainView.track;
  trackView->grid = &grid;
  trackView->addClip = [this] (f32 x, f32 y, f32 c) { addClipDenorm(x, y, c); };
  trackView->duplicateClip = [this] (u32 id, f32 x, bool top) { duplicateClipDenorm(id, x, top); };
  trackView->doZoom = [this] (f32 amount, i32 position) { doZoom(amount, position); };
  trackView->doScroll = [this] (f32 amount) { doScroll(amount); };

  automationView = &trackView->automationLane;
  automationView->setSelection = [this] (f32 start, f32 end) { setSelectionDenorm(start, end); };
  automationView->setPlayheadPosition = [this] (f32 x) { setPlayheadPositionDenorm(x); };
  automationView->addPath = [this] (f32 x, f32 y, f32 curve) { return addPathDenorm(x, y, curve); };
  automationView->bendAutomation = [this] (f32 x, f32 amount) { return bendAutomationDenorm(x, amount); };
  automationView->flattenAutomationCurve = [this] (f32 x) { flattenAutomationCurveDenorm(x); };
  automationView->dragAutomationSection = [this] (f32 x, f32 amount) { dragAutomationSectionDenorm(x, amount); };
  automationView->movePath = [this] (u32 id, f32 x, f32 y) { movePathDenorm(id, x, y, kDefaultPathCurve); };

  parametersView = &editor->mainView.parametersView;

  {
    auto& views = parametersView->parameterViews;

    u32 numParameters = u32(parameters.size());
    u32 numViews = u32(views.size());

    assert(numViews == 0);

    for (u32 i = 0; i < numParameters; ++i) {
      views.add(new ParametersView::ParameterView());
      parametersView->addAndMakeVisible(views.getLast());

      auto& parameter = parameters[i];
      auto* view = views[i32(i)];
      auto* ptr = &parameter;

      view->dial.setValue(parameter.parameter->getValue(), DONT_NOTIFY);
      view->dial.setDoubleClickReturnValue(true, parameter.parameter->getDefaultValue());
      view->activeToggle.setToggleState(parameter.active, DONT_NOTIFY);
      view->dial.active = parameter.active;
      view->name = parameter.parameter->getName(1024);

      view->activeToggle.onClick = [this, i, ptr] { setParameterActive(i, !ptr->active); };
      view->dial.onValueChange = [parameter, view] {
        parameter.parameter->setValueNotifyingHost(f32(view->dial.getValue()));
      };
    }
  }

  toolBarView = &editor->mainView.toolBar;
      
  toolBarView->editModeButton.onClick = [this] { setEditMode(!editMode); };
  toolBarView->discreteModeButton.onClick = [this] { setDiscreteMode(!discreteMode); };
  toolBarView->supportLinkButton.onClick = [] { supportURL.launchInDefaultBrowser(); };
  toolBarView->killButton.onClick = [this] { loadPlugin({}); };

  updateTrack();

  {
    parametersView->setSize(trackWidth, trackView->getHeight());
    parametersView->resized();
    parametersView->repaint();
  }

  updateToolBarView();

  startTimerHz(60);
}

bool StateManager::loadPlugin(const juce::String& id) {
  JUCE_ASSERT_MESSAGE_THREAD

  scoped_timer t("StateManager::loadPlugin()");

  bool result = false;

  {
    ScopedProcLock lk(proc);

    { // NOTE(luca): clear everything
      trackView = nullptr;
      automationView = nullptr;
      parametersView = nullptr;
      toolBarView = nullptr;

      pluginID = "";
      editMode = true; 
      zoom = 100;
      discreteMode = false;
      discreteMode = false;
      captureParameterChanges = false;
      releaseParameterChanges = false;

      playheadPosition = 0;
      bpm = 120;
      numerator = 4;
      denominator = 4;

      automation = {};
      selection = {};
      selectedClipID = NONE;

      paths.clear();
      clips.clear();
      parameters.clear();
      points.clear();

      // TODO(luca): rethink this
      if (editor) {
        editor->instanceWindow.reset();
      }

      assert(engine);
      engine->instance = nullptr;

      instanceEditor.reset();
      instance.reset();

      if (editor) {
        showDefaultView();
      }

      paths.reserve(1024);
      clips.reserve(1024);
      parameters.reserve(1024);
    }

    {
      pluginID = id;
      juce::String errorMessage;

      if (id != "") {
        auto description = plugin->knownPluginList.getTypeForIdentifierString(id);

        if (description) {
          instance = plugin->apfm.createPluginInstance(*description, proc.getSampleRate(), proc.getBlockSize(), errorMessage);

          if (instance) {
            auto processorParameters = instance->getParameters();
            u32 numParameters = u32(processorParameters.size());
            parameters.reserve(numParameters);
            uiParameterSync.values.resize(numParameters);
            uiParameterSync.updates.resize(numParameters);

            for (u32 i = 0; i < numParameters; ++i) {
              parameters.emplace_back();
              parameters.back().parameter = processorParameters[i32(i)];
              parameters.back().parameter->addListener(this);
            }

            engine->instance = instance.get();
            proc.prepareToPlay(proc.getSampleRate(), proc.getBlockSize());

            if (editor) {
              showMainView();
            }
            
            result = true;
          }
        }
      }
    }
  }

  return result;
}

void StateManager::registerEditor(Editor* view) {
  assert(view);
  assert(!editor);

  editor = view;

  if (instance) {
    showMainView(); 
  } else {
    showDefaultView();
  }
}

void StateManager::deregisterEditor(Editor* view) {
  assert(view == editor);
  assert(editor);

  stopTimer();
  editor = nullptr;
}

void StateManager::init() {
  plugin = static_cast<Plugin*>(&proc); 
  engine = &plugin->engine;
}

void StateManager::replace(const juce::ValueTree& tree) {
  JUCE_ASSERT_MESSAGE_THREAD

  juce::MessageManagerLock lk(juce::Thread::getCurrentThread());

  if (lk.lockWasGained()) {
    if (!loadPlugin(tree["pluginID"])) {
      return;
    }

    assert(instance);

    {
      auto mb = tree["pluginData"].getBinaryData();
      instance->setStateInformation(mb->getData(), i32(mb->getSize()));
    }
    
    setEditMode(tree["editMode"]);
    setDiscreteMode(tree["discreteMode"]);
    
    auto clipsTree = tree.getChild(0);
    for (auto c : clipsTree) {
      addClip(c["x"], c["y"], c["c"]);
      auto& clip = clips.back();
      clip.parameters.clear();

      auto mb = c["parameters"].getBinaryData(); 
      auto parameters_ = (f32*)mb->getData();
      auto numParameters = mb->getSize() / sizeof(f32);
      clip.parameters.reserve(numParameters); 

      for (u32 i = 0; i < numParameters; ++i) {
        clip.parameters.emplace_back(parameters_[i]); 
      }

      assert(clip.parameters.size() == numParameters);
    }

    auto pathsTree = tree.getChild(1);
    for (const auto& p : pathsTree) {
      addPath(p["x"], p["y"], p["c"]);
    }

    {
      auto parametersTree = tree.getChild(2);

      u32 i = 0;
      for (auto v : parametersTree) {
        juce::String name = v["name"];

        if (name != parameters[i].parameter->getName(1024)) {
          assert(false);

          for (u32 j = 0; j < parameters.size(); ++j) {
            if (name == parameters[j].parameter->getName(1024)) {
              parameters[j].active = v["active"];
              break; 
            }
          }
        } else {
          parameters[i].active = v["active"];
        }

        ++i;
      }
    }

    if (clips.size() > 1) {
      assert(engine);
      updateLerpPairs();
    }

    updateTrack(); 
  }
}

juce::ValueTree StateManager::getState() {
  JUCE_ASSERT_MESSAGE_THREAD

  juce::MessageManagerLock lk(juce::Thread::getCurrentThread());

  if (lk.lockWasGained()) {
    juce::ValueTree tree("tree");

    juce::MemoryBlock mb;

    if (instance) {
      instance->getStateInformation(mb);
    }

    tree.setProperty("zoom", zoom, nullptr)
        .setProperty("editMode", editMode.load(), nullptr)
        .setProperty("discreteMode", discreteMode.load(), nullptr)
        .setProperty("pluginID", pluginID, nullptr)
        .setProperty("pluginData", mb, nullptr);

    juce::ValueTree clipsTree("clips");
    for (const auto& c : clips) {
      juce::ValueTree clip("clip");
      clip.setProperty("x", c.x, nullptr)
          .setProperty("y", c.y, nullptr)
          .setProperty("c", c.c, nullptr)
          .setProperty("parameters", { c.parameters.data(), c.parameters.size() * sizeof(f32) }, nullptr);
      clipsTree.appendChild(clip, nullptr);
    }

    juce::ValueTree pathsTree("paths");
    for (const auto& p : paths) {
      juce::ValueTree path("path");
      path.setProperty("x", p.x, nullptr)
          .setProperty("y", p.y, nullptr)
          .setProperty("c", p.c, nullptr);
      pathsTree.appendChild(path, nullptr);
    }

    juce::ValueTree parametersTree("parameters");
    for (const auto& p : parameters) {
      juce::ValueTree parameter("parameter");
      parameter.setProperty("name", p.parameter->getName(1024), nullptr)
               .setProperty("active", p.active, nullptr);
      parametersTree.appendChild(parameter, nullptr);
    }

    tree.appendChild(clipsTree, nullptr);
    tree.appendChild(pathsTree, nullptr);
    tree.appendChild(parametersTree, nullptr);

    return tree;
  }

  return {};
}

void StateManager::timerCallback() {
  TimeSignature ts { numerator.load(), denominator.load() };

  if (ts.numerator != grid.ts.numerator || ts.denominator != grid.ts.denominator) {
    grid.ts = ts;
    updateGrid();        
  }

  if (trackView) {
    {
      auto ph = playheadPosition.load() * zoom;
      auto x = -viewportDeltaX;
      auto inBoundsOld = trackView->playhead.x >= x && trackView->playhead.x <= x + trackWidth; 
      auto inBoundsNew = ph >= x && ph <= x + trackWidth; 

      if (std::abs(trackView->playhead.x - ph) > EPSILON) {
        trackView->playhead.x = ph;

        if (inBoundsOld || inBoundsNew) {
          trackView->repaint();
        }
      }
    }

    {
      if (uiParameterSync.mode == UIParameterSync::UIUpdate) {
        for (u32 i = 0; i < parameters.size(); ++i) {
          auto& views = parametersView->parameterViews;

          if (uiParameterSync.updates[i]) {
            uiParameterSync.updates[i] = false; 
            views[i32(i)]->dial.setValue(uiParameterSync.values[i], DONT_NOTIFY); 
            views[i32(i)]->repaint();
          }
        }

        uiParameterSync.mode = UIParameterSync::EngineUpdate;
      }
    }
  }
}

} // namespace atmt 
