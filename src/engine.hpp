#pragma once

#include "geometry.hpp"
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
  void prepare(f64, i32);
  void process(juce::AudioBuffer<f32>&, juce::MidiBuffer&);

  void setPluginInstance(std::unique_ptr<juce::AudioPluginInstance>&);

  void audioProcessorParameterChanged(juce::AudioProcessor*, i32, f32) override;
  void audioProcessorChanged(juce::AudioProcessor*, const juce::AudioProcessorListener::ChangeDetails&) override;
  void audioProcessorParameterChangeGestureBegin(juce::AudioProcessor*, i32) override;
  void audioProcessorParameterChangeGestureEnd(juce::AudioProcessor*, i32) override;

  bool hasInstance();
  juce::AudioProcessorEditor* getEditor();

  StateManager& manager;
  UIBridge& uiBridge;
  juce::AudioProcessor& proc { manager.proc };
  
  // TODO(luca): decide if we should keep these
  juce::ChangeBroadcaster createInstanceBroadcaster;
  juce::ChangeBroadcaster killInstanceBroadcaster;

  std::unique_ptr<juce::AudioPluginInstance> instance; 
};

} // namespace atmt
