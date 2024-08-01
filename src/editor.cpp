#include "plugin.hpp"
#include "editor.hpp"

namespace atmt {

Editor::Editor(Plugin& _proc)
  : AudioProcessorEditor(&_proc), proc(_proc) {
  setSize(width, height);
  setResizable(false, false);

  if (proc.engine.hasInstance()) {
    showInstanceScreen(); 
  } else {
    showDefaultScreen();
  }
}

void Editor::paint(juce::Graphics& g) {
  g.fillAll(juce::Colours::black);
}

void Editor::resized() {
  auto r = getLocalBounds();
  debugTools.setBounds(r.removeFromTop(debugToolsHeight));

  if (instance) {
    // TODO(luca): tidy up
    track.setSize(10000, trackHeight);
    track.viewport.setBounds(r.removeFromBottom(trackHeight));

    descriptionBar.setBounds(r.removeFromTop(descriptionBarHeight));
    statesPanel.setBounds(r.removeFromLeft(statesPanelWidth));
    instance->setBounds(r.removeFromTop(instance->getHeight()));
  } else {
    pluginList.setBounds(r);
  }
}

void Editor::showDefaultScreen() {
  removeAllChildren();
  instance.reset();
  addAndMakeVisible(pluginList);
  setSize(width, height);
}

void Editor::showInstanceScreen() {
  removeAllChildren();
  instance.reset(proc.engine.getEditor());

  if (instance) {
    addAndMakeVisible(debugTools);
    addAndMakeVisible(descriptionBar);
    addAndMakeVisible(statesPanel);
    addAndMakeVisible(track.viewport);
    addAndMakeVisible(instance.get());
    setSize(instance->getWidth() + statesPanelWidth, instance->getHeight() + descriptionBarHeight + trackHeight + debugToolsHeight);
  }
}

void Editor::pluginIDChangeCallback(const juce::var& v) {
  auto description = proc.knownPluginList.getTypeForIdentifierString(v.toString());

  if (description) {
    descriptionBar.setDescription(description); 
  }
}

void Editor::createInstanceChangeCallback() {
  showInstanceScreen();
}

void Editor::killInstanceChangeCallback() {
  showDefaultScreen();
}

void Editor::childBoundsChanged(juce::Component* c) {
  if (c == instance.get()) {
    if (instance) {
      setSize(instance->getWidth() + statesPanelWidth, instance->getHeight() + descriptionBarHeight + trackHeight + debugToolsHeight);
    }
  }
}

} // namespace atmt
