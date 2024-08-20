#include "components.hpp"
#include "editor.hpp"

namespace atmt {

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
  selectorButton.onClick = [this] () {
    static_cast<Plugin*>(&proc)->engine.restoreFromPreset(getName());
  };

  overwriteButton.onClick = [this] () { manager.overwritePreset(getName()); };
  removeButton.onClick    = [this] () { manager.removePreset(getName()); };
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

} // namespace atmt
