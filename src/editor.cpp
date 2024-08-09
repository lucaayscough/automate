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

  setWantsKeyboardFocus(true);
}

void Editor::paint(juce::Graphics& g) {
  g.fillAll(juce::Colours::black);
}

void Editor::resized() {
  auto r = getLocalBounds();
  debugTools.setBounds(r.removeFromTop(debugToolsHeight));

  if (instance) {
    track.viewport.setBounds(r.removeFromBottom(Track::height));
    track.resized(); // NOTE(luca): need viewport to conform
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
    setSize(instance->getWidth() + statesPanelWidth, instance->getHeight() + descriptionBarHeight + Track::height + debugToolsHeight);
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
      setSize(instance->getWidth() + statesPanelWidth, instance->getHeight() + descriptionBarHeight + Track::height + debugToolsHeight);
    }
  }
}

bool Editor::keyPressed(const juce::KeyPress& k) {
  auto modifier = k.getModifiers(); 
  auto code = k.getKeyCode();

  static constexpr i32 keyNum1 = 49;
  static constexpr i32 keyNum2 = 50;
  static constexpr i32 keyNum3 = 51;
  static constexpr i32 keyNum4 = 52;

  if (modifier == juce::ModifierKeys::commandModifier) {
    switch (code) {
      case keyNum1: {
        track.grid.narrow(); 
        return true;
      } break;
      case keyNum2: {
        track.grid.widen(); 
        return true;
      } break;
      case keyNum3: {
        track.grid.triplet(); 
        return true;
      } break;
      case keyNum4: {
        track.grid.toggleSnap(); 
        return true;
      } break;
    };
  }

  return false;
}

void Editor::modifierKeysChanged(const juce::ModifierKeys& k) {
  track.automationLane.optKeyPressed = k.isAltDown();
}

} // namespace atmt
