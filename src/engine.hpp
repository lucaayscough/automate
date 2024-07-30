#pragma once

#include "state_attachment.hpp"
#include "identifiers.hpp"
#include "state_manager.hpp"
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include "ui_bridge.hpp"

namespace atmt {

struct ClipPair {
  Clip* a = nullptr;
  Clip* b = nullptr;
};

struct Engine : juce::AudioProcessorListener {
  Engine(StateManager&, UIBridge&);
  ~Engine() override;

  void kill();
  void prepare(double, int);
  void process(juce::AudioBuffer<float>&, juce::MidiBuffer&);
  void interpolateParameters(Preset*, Preset*, double);
  
  ClipPair getClipPair(double);
  void setParameters(Preset*);
  void setPluginInstance(std::unique_ptr<juce::AudioPluginInstance>&);
  void getCurrentParameterValues(std::vector<float>&);
  void restoreFromPreset(const juce::String&);

  void audioProcessorParameterChanged(juce::AudioProcessor*, int, float) override;
  void audioProcessorChanged(juce::AudioProcessor*, const juce::AudioProcessorListener::ChangeDetails&) override;
  void audioProcessorParameterChangeGestureBegin(juce::AudioProcessor*, int) override;
  void audioProcessorParameterChangeGestureEnd(juce::AudioProcessor*, int) override;

  void editModeChangeCallback(const juce::var&);

  bool hasInstance();
  juce::AudioProcessorEditor* getEditor();

  StateManager& manager;
  UIBridge& uiBridge;
  juce::UndoManager* undoManager { manager.undoManager };
  juce::AudioProcessorValueTreeState& apvts { manager.apvts };
  juce::AudioProcessor& proc { apvts.processor };
  juce::ValueTree editTree { manager.editTree };
  juce::ValueTree presetsTree { manager.presetsTree };
  juce::ChangeBroadcaster createInstanceBroadcaster;
  juce::ChangeBroadcaster killInstanceBroadcaster;
  std::unique_ptr<juce::AudioPluginInstance> instance; 

  Presets presets { presetsTree, undoManager, &proc };
  Clips clips { editTree, undoManager, &proc };
  Automation automation { editTree, undoManager, &proc };

  StateAttachment editModeAttachment { editTree, ID::editMode, STATE_CB(editModeChangeCallback), nullptr};
  std::atomic<bool> editMode = false;
};

} // namespace atmt
