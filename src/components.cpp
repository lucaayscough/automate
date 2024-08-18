#include "components.hpp"

namespace atmt {

DebugTools::DebugTools(StateManager& m) : manager(m) {
  addAndMakeVisible(printStateButton);
  addAndMakeVisible(killButton);
  addAndMakeVisible(playButton);
  addAndMakeVisible(stopButton);
  addAndMakeVisible(rewindButton);
  addAndMakeVisible(editModeButton);
  addAndMakeVisible(modulateDiscreteButton);
  addAndMakeVisible(undoButton);
  addAndMakeVisible(redoButton);
  addAndMakeVisible(randomiseButton);

  printStateButton        .onClick = [this] { DBG(manager.valueTreeToXmlString(manager.state)); };
  editModeButton          .onClick = [this] { editModeAttachment.setValue({ !editMode }); };
  killButton              .onClick = [this] { pluginID.setValue("", undoManager); };
  playButton              .onClick = [this] { static_cast<Plugin*>(&proc)->signalPlay(); };
  stopButton              .onClick = [this] { static_cast<Plugin*>(&proc)->signalStop(); };
  rewindButton            .onClick = [this] { static_cast<Plugin*>(&proc)->signalRewind(); };
  undoButton              .onClick = [this] { undoManager->undo(); };
  redoButton              .onClick = [this] { undoManager->redo(); };
  randomiseButton         .onClick = [this] { static_cast<Plugin*>(&proc)->engine.randomiseParameters(); };

  // TODO(luca): this discrete button seems to be broken
  modulateDiscreteButton  .onClick = [this] { modulateDiscreteAttachment.setValue({ !modulateDiscrete }); };
}

void DebugTools::resized() {
  auto r = getLocalBounds();
  auto width = getWidth() / getNumChildComponents();
  printStateButton.setBounds(r.removeFromLeft(width));
  editModeButton.setBounds(r.removeFromLeft(width));
  modulateDiscreteButton.setBounds(r.removeFromLeft(width));
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

void DescriptionBar::paint(juce::Graphics& g) {
  g.setColour(juce::Colours::white);
  g.setFont(getHeight());
  
  if (description) {
    g.drawText(description->name, getLocalBounds(), juce::Justification::left);
  }
}

void DescriptionBar::setDescription(std::unique_ptr<juce::PluginDescription>& desc) {
  description = std::move(desc);
  repaint();
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

PluginListComponent::PluginListComponent(StateManager& m, juce::AudioPluginFormatManager& fm, juce::KnownPluginList& kpl)
  : manager(m), knownPluginList(kpl), formatManager(fm) {
  for (auto& t : knownPluginList.getTypes()) {
    addPlugin(t);
  }
  updateSize();
  viewport.setScrollBarsShown(true, false);
  viewport.setViewedComponent(this, false);
}

void PluginListComponent::paint(juce::Graphics& g) {
  g.fillAll(juce::Colours::green);
}

void PluginListComponent::resized() {
  auto r = getLocalBounds();
  for (auto button : plugins) {
    button->setBounds(r.removeFromTop(buttonHeight));
  }
}

void PluginListComponent::addPlugin(juce::PluginDescription& pd) {
  auto button = new juce::TextButton(pd.pluginFormatName + " - " + pd.name);
  auto id = pd.createIdentifierString();
  addAndMakeVisible(button);
  button->onClick = [id, this] { pluginID.setValue(id, undoManager); };
  plugins.add(button);
}

void PluginListComponent::updateSize() {
  setSize(width, buttonHeight * plugins.size());
}

bool PluginListComponent::isInterestedInFileDrag(const juce::StringArray&){
  return true; 
}

void PluginListComponent::filesDropped(const juce::StringArray& files, i32, i32) {
  juce::OwnedArray<juce::PluginDescription> types;
  knownPluginList.scanAndAddDragAndDroppedFiles(formatManager, files, types);
  for (auto t : types) {
    addPlugin(*t);
  }
  updateSize();
}

} // namespace atmt
