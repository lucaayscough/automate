#pragma once

#include "utils.hpp"
#include "state_manager.hpp"
#include "engine.hpp"
#include <juce_audio_processors/juce_audio_processors.h>
#include "ui_bridge.hpp"
#include "types.hpp"

namespace atmt {

struct Plugin : juce::AudioProcessor {
  Plugin();
  ~Plugin() override;

  void prepareToPlay(double sampleRate, int samplesPerBlock) override;
  void releaseResources() override;
  bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
  void processBlock(juce::AudioBuffer<f32>&, juce::MidiBuffer&) override;

  juce::AudioProcessorEditor* createEditor() override;
  bool hasEditor() const override;

  const juce::String getName() const override;

  bool acceptsMidi() const override;
  bool producesMidi() const override;
  bool isMidiEffect() const override;
  double getTailLengthSeconds() const override;

  int getNumPrograms() override;
  int getCurrentProgram() override;
  void setCurrentProgram(int index) override;
  const juce::String getProgramName(int index) override;
  void changeProgramName(int index, const juce::String& newName) override;

  void getStateInformation(juce::MemoryBlock& destData) override;
  void setStateInformation(const void* data, int sizeInBytes) override;

  StateManager manager { *this };
  UIBridge uiBridge;
  Engine engine { manager, uiBridge }; 

  juce::AudioPluginFormatManager apfm;
  juce::KnownPluginList knownPluginList;
};

} // namespace atmt
