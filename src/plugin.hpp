#pragma once

#include "engine.hpp"
#include "identifiers.hpp"
#include "state_attachment.hpp"
#include <juce_audio_processors/juce_audio_processors.h>

class PluginProcessor : public juce::AudioProcessor, public juce::ChangeListener
{
public:
  PluginProcessor();
  ~PluginProcessor() override;

  void changeListenerCallback(juce::ChangeBroadcaster*) override;
  void updatePluginInstance(juce::var v);

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

  atmt::Engine engine { apvts }; 

  atmt::StateAttachment instanceAttachment { apvts.state,
                                             IDENTIFIER_PLUGIN_INSTANCE,
                                             [this] (juce::var v) { updatePluginInstance(v); },
                                             apvts.undoManager };


  juce::AudioPluginFormatManager apfm;
  juce::KnownPluginList knownPluginList;
  juce::PropertiesFile::Options optionsFile;
  juce::String searchPath                         { "~/Desktop/plugs/" };
  juce::File deadMansPedalFile                    { "~/Desktop/dmpf.txt" };
  juce::PropertiesFile propertiesFile             { { "~/Desktop/properties.txt" }, optionsFile};

private:
  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginProcessor)
};
