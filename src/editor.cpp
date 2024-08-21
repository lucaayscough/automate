#include "plugin.hpp"
#include "editor.hpp"

namespace atmt {

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
  static constexpr i32 keyCharX = 88;
  static constexpr i32 keyCharZ = 90;

  auto& track = mainView->track;

  if (modifier.isCommandDown()) {
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
