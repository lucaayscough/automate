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

const juce::FontOptions Fonts::sofiaProLight { juce::Typeface::createSystemTypefaceFor(BinaryData::sofia_pro_light_otf, BinaryData::sofia_pro_light_otfSize) };
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

Dial::Dial() {
  setSliderStyle(juce::Slider::SliderStyle::RotaryVerticalDrag);
  setTextBoxStyle(juce::Slider::TextEntryBoxPosition::NoTextBox, true, 0, 0);
  setScrollWheelEnabled(false);
  setRange(0, 1);
}

void Dial::paint(juce::Graphics& g) {
  auto r = getLocalBounds().toFloat().reduced(Style::lineThicknessHighlighted, Style::lineThicknessHighlighted);

  if (active) {
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
  auto r = getLocalBounds().withSizeKeepingCentre(i32(kWidth * 0.75f), numCommands * commandHeight);
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

PathView::PathView() {
  setRepaintsOnMouseActivity(true);
}

void PathView::paint(juce::Graphics& g) {
  if (isMouseOverOrDragging()) {
    g.setColour(Colours::auburn);
  } else {
    g.setColour(Colours::atomicTangerine);
  }
  g.fillEllipse(getLocalBounds().toFloat().withSizeKeepingCentre(size / 2, size / 2));
}

void PathView::mouseDrag(const juce::MouseEvent& e) {
  auto parent = getParentComponent();
  assert(parent);
  auto parentLocalPoint = parent->getLocalPoint(this, e.position);
  move(id, parentLocalPoint.x, parentLocalPoint.y);
}

void PathView::mouseDoubleClick(const juce::MouseEvent&) {
  remove(id);
}

ClipView::ClipView() {
  setRepaintsOnMouseActivity(true);
}

void ClipView::paint(juce::Graphics& g) {
  g.setColour(Colours::auburn);
  g.fillEllipse(getLocalBounds().toFloat());

  if (selected) {
    g.setColour(Colours::isabelline);
    g.fillEllipse(getLocalBounds().toFloat());
  }

  if (isMouseOver()) {
    g.setColour(Colours::shamrockGreen);
    g.drawEllipse(getLocalBounds().toFloat().reduced(Style::lineThicknessHighlighted / 2), Style::lineThicknessHighlighted);
  }
}

void ClipView::mouseDown(const juce::MouseEvent& e) {
  mouseDownOffset = getWidth() / 2 - e.position.x;

  select(i32(id));

  if (gOptKeyPressed) {
    auto container = juce::DragAndDropContainer::findParentDragContainerFor(this);
    assert(container);
    container->startDragging({ i32(id) }, this);
  }
}

void ClipView::mouseDoubleClick(const juce::MouseEvent&) {
  remove(id);
}

void ClipView::mouseUp(const juce::MouseEvent&) {
  isTrimDrag = false;
  mouseDownOffset = 0;
}

void ClipView::mouseDrag(const juce::MouseEvent& e) {
  if (!gOptKeyPressed) {
    auto parentLocalPoint = getParentComponent()->getLocalPoint(this, e.position);
    f32 y = parentLocalPoint.y > (getParentComponent()->getHeight() / 2) ? 1 : 0;
    f32 x = parentLocalPoint.x + mouseDownOffset;
    move(id, x, y);
  }
}

void AutomationLane::paint(juce::Graphics& g) {

  { // NOTE(luca): draw selection
    auto r = getLocalBounds();
    g.setColour(Colours::frenchGray);
    g.setOpacity(0.2f);
    g.fillRect(i32(selection.start < selection.end ? selection.start : selection.end), r.getY(), i32(std::abs(selection.end - selection.start)), r.getHeight());
    g.setOpacity(1);
  }

  { // NOTE(luca): draw automation line
    juce::Path::Iterator it(automation);

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

    if (it == pathViews.end() && paintHoverPoint) {
      g.setColour(Colours::atomicTangerine);
      g.fillEllipse(hoverBounds);
    }
  }
}

auto AutomationLane::getAutomationPoint(juce::Point<f32> p) {
  juce::Point<f32> np;
  automation.getNearestPoint(p, np);
  return np;
}

f32 AutomationLane::getDistanceFromPoint(juce::Point<f32> p) {
  return p.getDistanceFrom(getAutomationPoint(p));
}

void AutomationLane::mouseMove(const juce::MouseEvent& e) {
  auto p = getAutomationPoint(e.position);
  auto d = p.getDistanceFrom(e.position);

  if (d < mouseIntersectDistance && !gOptKeyPressed && hoverBounds.getCentre() != p) {
    hoverBounds.setCentre(p);
    hoverBounds.setSize(10, 10);
    paintHoverPoint = true;
    repaint();
  } else if (paintHoverPoint) {
    paintHoverPoint = false;
    repaint();
  }

  if ((d < mouseOverDistance && mouseIntersectDistance < d) || (d < mouseOverDistance && gOptKeyPressed)) {
    xHighlightedSegment = p.x;
    repaint();
  } else if (i32(xHighlightedSegment) != NONE) {
    xHighlightedSegment = NONE; 
    repaint();
  }
}

void AutomationLane::mouseExit(const juce::MouseEvent&) {
  paintHoverPoint = false;
  xHighlightedSegment = NONE; 
  lastMouseDragOffset = {};
  repaint();
}

void AutomationLane::mouseDown(const juce::MouseEvent& e) {
  assert(activeGesture == GestureType::none);

  auto point = getAutomationPoint(e.position);
  auto distance = point.getDistanceFrom(e.position);

  if (distance < mouseOverDistance && gOptKeyPressed) {
    activeGesture = GestureType::bend;
    setMouseCursor(juce::MouseCursor::NoCursor);
  } else if (distance < mouseIntersectDistance) {
    activeGesture = GestureType::addPath;
    lastPathAddedID = addPath(point.x, point.y, kDefaultPathCurve);
    paintHoverPoint = false;
  } else if (distance < mouseOverDistance) {
    activeGesture = GestureType::drag;
    setMouseCursor(juce::MouseCursor::NoCursor);
  } else {
    activeGesture = GestureType::select;

    f32 start = e.position.x;
    f32 end = e.position.x; 

    setSelection(start < 0 ? 0 : start, end < 0 ? 0 : end);
    setPlayheadPosition(selection.start);
  }
}

void AutomationLane::mouseUp(const juce::MouseEvent&) {
  // TODO(luca): implement automatic mouse placing meachanism
  //if (activeGesture == GestureType::bend) {
  //  auto globalPoint = localPointToGlobal(juce::Point<i32> { 0, 0 });
  //  juce::Desktop::setMousePosition(globalPoint);
  //}

  xHighlightedSegment = NONE; 
  paintHoverPoint = false;
  lastMouseDragOffset = {};

  activeGesture = GestureType::none;
  setMouseCursor(juce::MouseCursor::NormalCursor);
}

void AutomationLane::mouseDrag(const juce::MouseEvent& e) {
  if (activeGesture == GestureType::bend) {
    auto offset = e.getOffsetFromDragStart();
    f32 y = f32(lastMouseDragOffset.y - offset.y);
    f32 increment = y / kDragIncrement;

    lastMouseDragOffset = offset;
    bendAutomation(e.position.x, increment);
  } else if (activeGesture == GestureType::drag) {
    auto offset = e.getOffsetFromDragStart();
    f32 y = f32(lastMouseDragOffset.y - offset.y);
    f32 increment = y / kDragIncrement;

    lastMouseDragOffset = offset;
    dragAutomationSection(e.position.x, increment);
  } else if (activeGesture == GestureType::select) {
    f32 end = e.position.x; 

    setSelection(selection.start, end < 0 ? 0 : end);
    setPlayheadPosition(selection.end);

    repaint();
  } else if (activeGesture == GestureType::addPath) {
    movePath(lastPathAddedID, e.position.x, e.position.y);
  } else {
    assert(false);
  }
}

void AutomationLane::mouseDoubleClick(const juce::MouseEvent& e) {
  if (getDistanceFromPoint(e.position) < mouseOverDistance && gOptKeyPressed) {
    flattenAutomationCurve(e.position.x);
  }
}

TrackView::TrackView() {
  addAndMakeVisible(automationLane);
}

void TrackView::paint(juce::Graphics& g) {
  //scoped_timer t("TrackView::paint()");

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
    for (auto& beat : grid->beats) {
      auto beatText = juce::String(beat.bar);
      if (beat.beat > 1) {
        beatText << "." + juce::String(beat.beat);
      }
      g.drawText(beatText, i32(beat.x + beatTextOffset), r.getY(), beatTextWidth, beatTextHeight, juce::Justification::left);
    }

    g.setColour(Colours::outerSpace);
    for (u32 i = 1; i < grid->lines.size(); ++i) {
      g.fillRect(f32(grid->lines[i]), f32(r.getY()), 0.75f, f32(getHeight()));
    }
  }

  g.setColour(Colours::isabelline);
  g.fillRect(playhead.x, f32(automationLane.getY()), playhead.width, f32(automationLane.getHeight()));
}

void TrackView::resized() {
  auto r = getLocalBounds();
  b.timeline = r.removeFromTop(kTimelineHeight);
  b.presetLaneTop = r.removeFromTop(kPresetLaneHeight);
  b.presetLaneBottom = r.removeFromBottom(kPresetLaneHeight);
  automationLane.setBounds(r);
}

void TrackView::mouseMagnify(const juce::MouseEvent&, f32 scale) {
  if (scale < 1) {
    doZoom(-0.1f * (1.f / scale), getMouseXYRelative().x);
  } else {
    doZoom(0.1f * scale, getMouseXYRelative().x);
  }
}

void TrackView::mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& w) {
  if (gCmdKeyPressed) {
    doZoom(w.deltaY, getMouseXYRelative().x);
  } else if (gShiftKeyPressed) {
    doScroll(w.deltaX + w.deltaY);
  } else {
    doScroll(w.deltaX);
  }
}

void TrackView::mouseDoubleClick(const juce::MouseEvent& e) {
  if (b.presetLaneTop.contains(e.position.toInt())) {
    addClip(e.position.x, 0, kDefaultPathCurve);
  } else if (b.presetLaneBottom.contains(e.position.toInt())) {
    addClip(e.position.x, 1, kDefaultPathCurve);
  }
}

bool TrackView::isInterestedInDragSource(const Details&) {
  return true;
}

void TrackView::itemDropped(const Details& d) {
  u32 id = u32(i32(d.description));

  if (b.presetLaneTop.contains(d.localPosition.toInt())) {
    duplicateClip(id, d.localPosition.x, true);
  } else if (b.presetLaneBottom.contains(d.localPosition.toInt())) {
    duplicateClip(id, d.localPosition.x, false);
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

ToolBar::ToolBar() {
  addAndMakeVisible(infoButton);
  addAndMakeVisible(editModeButton);
  addAndMakeVisible(discreteModeButton);
  addAndMakeVisible(supportLinkButton);
  addAndMakeVisible(killButton);
}

void ToolBar::resized() {
  auto r = getLocalBounds().reduced(padding, padding);
  i32 middleWidth = buttonWidth * 3 + buttonPadding * 2;
  auto middle = r.reduced((r.getWidth() - middleWidth) / 2, 0);

  infoButton.setBounds(r.removeFromLeft(r.getHeight()));
  editModeButton.setBounds(middle.removeFromLeft(buttonWidth));
  middle.removeFromLeft(buttonPadding);
  discreteModeButton.setBounds(middle.removeFromLeft(buttonWidth));
  middle.removeFromLeft(buttonPadding);
  supportLinkButton.setBounds(middle.removeFromLeft(buttonWidth));
  killButton.setBounds(r.removeFromRight(r.getHeight()));
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
      loadPlugin(p.description.createIdentifierString());
      break;
    }
  }
}

void DefaultView::PluginsPanel::mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& w) {
  i32 v = getY() + i32(100.f * w.deltaY);
  v = std::clamp(v, - getHeight() + kDefaultViewHeight, 0);
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
  setSize(kDefaultViewWidth / 2, newHeight < kDefaultViewHeight ? kDefaultViewHeight : newHeight);
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

  if (manufacturers.empty()) {
    g.drawText("Drag and drop a VST3/AU to start", getLocalBounds(), juce::Justification::centred); 

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

  repaint();
}

void DefaultView::ManufacturersPanel::mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& w) {
  i32 v = getY() + i32(100.f * w.deltaY);
  v = std::clamp(v, - getHeight() + kDefaultViewHeight, 0);
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

  if (currentManufacturer == "" && !manufacturers.empty()) {
    pluginsPanel.updateManufacturerFilter(manufacturers.front().name);
    activeButton = 0;
  } else {
    pluginsPanel.updateManufacturerFilter(currentManufacturer);
  }

  i32 newHeight = DefaultView::buttonHeight * i32(manufacturers.size()) + DefaultView::titleHeight;
  setSize(kDefaultViewWidth / 2, newHeight < kDefaultViewHeight ? kDefaultViewHeight : newHeight);
  resized();
}

DefaultView::DefaultView(StateManager& m, juce::AudioPluginFormatManager& fm, juce::KnownPluginList& kpl) : manager(m), knownPluginList(kpl), formatManager(fm) {
  pluginsPanel.loadPlugin = [this] (const juce::String& id) { manager.loadPlugin(id); };

  addAndMakeVisible(manufacturersPanel);
  addAndMakeVisible(pluginsPanel);
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
  saveKnownPluginList(knownPluginList); 
  resized();
}

ParametersView::ParameterView::ParameterView() {
  addAndMakeVisible(dial);
  addAndMakeVisible(activeToggle);
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
  g.drawText(name, nameBounds, juce::Justification::centred);
}

void ParametersView::resized() {
  // TODO(luca): clean this up
  auto r = getLocalBounds();
  r.removeFromTop(padding);
  r.removeFromLeft(padding);
  r.removeFromRight(padding);

  i32 numPerRow = r.getWidth() / ParameterView::height;
  i32 numRows = parameterViews.size() / numPerRow;

  if (parameterViews.size() % numPerRow != 0) {
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

  for (auto p : parameterViews) {
    if (count % numPerRow == 0) {
      row = r.removeFromTop(ParameterView::height);    
    }
    p->setBounds(row.removeFromLeft(ParameterView::height + offset)); 
    ++count;
  }
}

void ParametersView::mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& w) {
  i32 y = getY() + i32(kScrollSpeed * w.deltaY);

  if (y > kToolBarHeight) {
    y = kToolBarHeight;
  } else if (y < - (getHeight() - viewportHeight - kToolBarHeight)) {
    y = - (getHeight() - viewportHeight - kToolBarHeight);
  }

  setTopLeftPosition(getX(), y);
}

MainView::MainView() {
  addAndMakeVisible(toolBar);
  addAndMakeVisible(track);
  addChildComponent(infoView);

  infoView.mainViewUpdateCallback = [this] { toggleInfoView(); };
  toolBar.infoButton.onClick = [this] { toggleInfoView(); };

  setSize(kWidth, kHeight);
}

void MainView::paint(juce::Graphics& g) {
  g.fillAll(Colours::eerieBlack); 
}

void MainView::resized() {
  auto r = getLocalBounds();

  infoView.setBounds(r);
  toolBar.setBounds(r.removeFromTop(kToolBarHeight));

  track.setTopLeftPosition(r.getTopLeft());
  track.setSize(kWidth, kTrackHeight);
}

void MainView::toggleInfoView() {
  infoView.setVisible(!infoView.isVisible());
}

Editor::Editor(Plugin& p) : AudioProcessorEditor(&p), proc(p) {
  addChildComponent(defaultView);
  addChildComponent(mainView);
  setWantsKeyboardFocus(true);
  setResizable(false, false);
  manager.registerEditor(this);
}

Editor::~Editor() {
  manager.deregisterEditor(this);
}

void Editor::paint(juce::Graphics& g) {
  g.fillAll(Colours::eerieBlack);
}

void Editor::resized() {
  auto r = getLocalBounds();
  defaultView.setBounds(r);
  mainView.setBounds(r);
}

bool Editor::keyPressed(const juce::KeyPress& k) {
  auto modifier = k.getModifiers(); 
  auto code = k.getKeyCode();

  static constexpr i32 space = 32;

  static constexpr i32 keyNum1 = 49;
  static constexpr i32 keyNum2 = 50;
  static constexpr i32 keyNum3 = 51;
  static constexpr i32 keyNum4 = 52;

  static constexpr i32 keyPlus = 43;
  static constexpr i32 keyMin = 45;
  static constexpr i32 keyEquals = 61;

  static constexpr i32 keyCharD = 68;
  static constexpr i32 keyCharE = 69;
  static constexpr i32 keyCharI = 73;
  static constexpr i32 keyCharK = 75;
  static constexpr i32 keyCharR = 82;

  static constexpr i32 keyLeft  = 63234;
  static constexpr i32 keyRight = 63235;

  static constexpr i32 keyDelete = 127;

  if (code == space) {
    return false;
  }

  auto& track = mainView.track;

  if (modifier.isCommandDown()) {
    switch (code) {
      case keyNum1: {
        manager.grid.narrow(); 
        track.repaint();
      } break;
      case keyNum2: {
        manager.grid.widen(); 
        track.repaint();
      } break;
      case keyNum3: {
        manager.grid.triplet(); 
        track.repaint();
      } break;
      case keyNum4: {
        manager.grid.toggleSnap(); 
        track.repaint();
      } break;
      case keyCharD: {
        manager.setAllParametersActive(false);
      } break;
      case keyCharE: {
        manager.setAllParametersActive(true);
      } break;
    };
  } else {
    switch (code) {
      case keyDelete: {
        manager.removeSelection();
      } break;
      case keyPlus: {
        manager.doZoom(1, getMouseXYRelative().x);
      } break;
      case keyEquals: {
        manager.doZoom(1, getMouseXYRelative().x);
      } break;
      case keyMin: {
        manager.doZoom(-1, getMouseXYRelative().x);
      } break;
      case keyCharE: {
        manager.setEditMode(!manager.editMode);
      } break;
      case keyCharD: {
        manager.setDiscreteMode(!manager.discreteMode);
      } break;
      case keyCharR: {
        manager.randomiseParameters();
      } break;
      case keyCharK: {
        manager.loadPlugin({}); 
      } break;
      case keyCharI: {
        if (instanceWindow) {
          mainView.toggleInfoView();
        }
      } break;
      case keyLeft: {
        manager.movePlayheadBack();
      } break;
      case keyRight: {
        manager.movePlayheadForward();
      } break;
    };
  } 

  DBG("Key code: " + juce::String(code));

  return true;
}

void Editor::modifierKeysChanged(const juce::ModifierKeys& k) {
  manager.captureParameterChanges = false;
  manager.releaseParameterChanges = false;

  if (k.isCommandDown() && k.isShiftDown()) {
    DBG("Releasing parameters");
    manager.releaseParameterChanges = true;
  } else if (k.isCommandDown()) {
    DBG("Capturing parameters");
    manager.captureParameterChanges = true; 
  }

  gOptKeyPressed = k.isAltDown();
  gShiftKeyPressed = k.isShiftDown();
  gCmdKeyPressed = k.isCommandDown();
}

} // namespace atmt
