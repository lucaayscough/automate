#pragma once

#include "geometry.hpp"
#include "state_manager.hpp"
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_devices/juce_audio_devices.h>

namespace atmt {

struct Engine : juce::AudioProcessorListener {
  Engine(StateManager&);
  ~Engine() override;

  void kill();
  void prepare(f32, i32);
  void process(juce::AudioBuffer<f32>&, juce::MidiBuffer&);

  void setPluginInstance(std::unique_ptr<juce::AudioPluginInstance>&);

  void audioProcessorParameterChanged(juce::AudioProcessor*, i32, f32) override;
  void audioProcessorChanged(juce::AudioProcessor*, const juce::AudioProcessorListener::ChangeDetails&) override;
  void audioProcessorParameterChangeGestureBegin(juce::AudioProcessor*, i32) override;
  void audioProcessorParameterChangeGestureEnd(juce::AudioProcessor*, i32) override;
  void handleParameterChange(Parameter*);

  bool hasInstance();
  juce::AudioProcessorEditor* getEditor();

  StateManager& manager;
  juce::AudioProcessor& proc { manager.proc };
  
  // TODO(luca): get rid of these 
  juce::ChangeBroadcaster createInstanceBroadcaster;
  juce::ChangeBroadcaster killInstanceBroadcaster;

  std::unique_ptr<juce::AudioPluginInstance> instance; 
};

} // namespace atmt
