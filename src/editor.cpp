#include "plugin.hpp"
#include "editor.hpp"

namespace atmt {

Editor::Editor(Plugin& _proc) : AudioProcessorEditor(&_proc), proc(_proc) {
  if (proc.engine.hasInstance()) {
    showInstanceScreen(); 
  } else {
    showDefaultScreen();
  }

  setWantsKeyboardFocus(true);
  setResizable(false, false);
  setSize(width, height);
}

void Editor::paint(juce::Graphics& g) {
  g.fillAll(juce::Colours::black);
}

void Editor::resized() {
  auto r = getLocalBounds();
  debugTools.setBounds(r.removeFromTop(debugToolsHeight));
  debugInfo.setBounds(r.removeFromBottom(debugInfoHeight));

  if (instance) {
    track.viewport.setBounds(r.removeFromBottom(Track::height));
    track.resized();
    descriptionBar.setBounds(r.removeFromTop(descriptionBarHeight));
    statesPanel.setBounds(r.removeFromLeft(statesPanelWidth));
    instance->setBounds(r.removeFromTop(instance->getHeight()));
    parametersView.viewport.setBounds(instance->getBounds());
    parametersView.resized();
  } else {
    pluginList.viewport.setBounds(r);
    pluginList.resized();
  }
}

void Editor::showDefaultScreen() {
  removeAllChildren();
  instance.reset();
  addAndMakeVisible(debugTools);
  addAndMakeVisible(debugInfo);
  addAndMakeVisible(pluginList.viewport);
  setSize(width, height);
}

void Editor::showInstanceScreen() {
  removeAllChildren();
  parametersView.killInstance();
  instance.reset(proc.engine.getEditor());

  if (instance) {
    parametersView.setInstance(proc.engine.instance.get());
    addAndMakeVisible(debugTools);
    addAndMakeVisible(debugInfo);
    addAndMakeVisible(descriptionBar);
    addAndMakeVisible(statesPanel);
    addAndMakeVisible(track.viewport);
    showInstance ? addAndMakeVisible(instance.get()) : addAndMakeVisible(parametersView.viewport);
    setSize(instance->getWidth() + statesPanelWidth, instance->getHeight() + descriptionBarHeight + Track::height + debugToolsHeight + debugInfoHeight);
  }

  resized();
}

void Editor::showParametersView() {
  showInstance = false;
  showInstanceScreen(); 
}

void Editor::showInstanceView() {
  showInstance = true;
  showInstanceScreen(); 
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
      setSize(instance->getWidth() + statesPanelWidth, instance->getHeight() + descriptionBarHeight + Track::height + debugToolsHeight + debugInfoHeight);
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
  track.shiftKeyPressed = k.isShiftDown();
  track.cmdKeyPressed = k.isCommandDown();
}

} // namespace atmt
