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

  void signalPlay();
  void signalRewind();
  void signalStop();
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

  std::atomic<bool> play = false;
  std::atomic<bool> stop = false;
  std::atomic<bool> rewind = false;

  // TODO(luca): some of these may not be needed and need appropriate dirs for these files
  juce::AudioPluginFormatManager apfm;
  juce::KnownPluginList knownPluginList;
  juce::PropertiesFile::Options optionsFile;
  juce::File deadMansPedalFile                    { "~/Desktop/dmpf.txt" };
  juce::PropertiesFile propertiesFile             { { "~/Desktop/properties.txt" }, optionsFile};
};

} // namespace atmt
