#pragma once

#include "utils.hpp"
#include "state_manager.hpp"
#include "engine.hpp"
#include "identifiers.hpp"
#include "state_attachment.hpp"
#include <juce_audio_processors/juce_audio_processors.h>

namespace atmt {

class PluginProcessor : public juce::AudioProcessor {
public:
  PluginProcessor();
  ~PluginProcessor() override;

  void knownPluginListChangeCallback();
  void pluginIDChangeCallback(const juce::var& v);

  void prepareToPlay(double sampleRate, int samplesPerBlock) override;
  void releaseResources() override;

  bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

  void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
  using AudioProcessor::processBlock;

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

  juce::UndoManager undoManager;
  juce::AudioProcessorValueTreeState apvts { *this, &undoManager, "Automate", {} };

  StateManager manager { apvts };
  Engine engine { apvts }; 

  juce::AudioPluginFormatManager apfm;
  juce::KnownPluginList knownPluginList;
  juce::PropertiesFile::Options optionsFile;
  juce::String searchPath                         { "~/Desktop/plugs/" };
  juce::File deadMansPedalFile                    { "~/Desktop/dmpf.txt" };
  juce::PropertiesFile propertiesFile             { { "~/Desktop/properties.txt" }, optionsFile};

private:
  ChangeAttachment knownPluginListAttachment { knownPluginList, CHANGE_CB(knownPluginListChangeCallback) };
  StateAttachment pluginIDAttachment { apvts.state, ID::pluginID, STATE_CB(pluginIDChangeCallback), apvts.undoManager };

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginProcessor)
};

} // namespace atmt
