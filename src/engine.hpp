#pragma once

#include "identifiers.hpp"
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_devices/juce_audio_devices.h>

namespace atmt {

struct Engine
{
  Engine(juce::AudioProcessorValueTreeState& _apvts)
    : apvts(_apvts) {
    JUCE_ASSERT_MESSAGE_THREAD
  }

  void prepare(double sampleRate, int blockSize) {
    JUCE_ASSERT_MESSAGE_THREAD

    if (instance) {
      instance->prepareToPlay(sampleRate, blockSize);
    }
  }

  void process(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiBuffer) {
    if (instance) {
      auto playhead = apvts.processor.getPlayHead();
      auto position = playhead->getPosition();

      if (position.hasValue()) {
        auto time = position->getTimeInSamples();

        if (time.hasValue()) {
          auto interpolationValue = float(*time % 441000) / 441000.f;
          
          if (parameterStates.size() > 1) {
            interpolateStates(0, 1, interpolationValue); 
          }
        }
      }

      instance->processBlock(buffer, midiBuffer);
    }
  }

  void setPluginInstance(std::unique_ptr<juce::AudioPluginInstance>& _instance) {
    JUCE_ASSERT_MESSAGE_THREAD

    jassert(_instance);
    instance = std::move(_instance);
    parameterStates.clear();
    instanceBroadcaster.sendChangeMessage();
  }

  void saveParameterState() {
    JUCE_ASSERT_MESSAGE_THREAD

    jassert(instance);

    std::vector<float> parameterValues;

    auto parameters = instance->getParameters();
    
    for (auto* parameter : parameters) {
      parameterValues.push_back(parameter->getValue());
      DBG(parameter->getName(1024) + " - " + juce::String(parameter->getValue()));
    }

    parameterStates.push_back(parameterValues);
  }

  void restoreParameterState(int index) {
    JUCE_ASSERT_MESSAGE_THREAD

    jassert(instance);

    auto parameters = instance->getParameters();

    for (std::size_t i = 0; i < parameterStates[std::size_t(index)].size(); ++i) {
      parameters[int(i)]->setValue(parameterStates[std::size_t(index)][i]); 
    }
  }

  void interpolateStates(int stateBeginIndex, int stateEndIndex, float position) {
    auto& stateBegin = parameterStates[std::size_t(stateBeginIndex)];
    auto& stateEnd   = parameterStates[std::size_t(stateEndIndex)];
    auto parameters = instance->getParameters();

    for (int i = 0; i < parameters.size(); ++i) {
      float distance = stateEnd[std::size_t(i)] - stateBegin[std::size_t(i)] ;
      float increment = distance * position; 
      float newValue = stateBegin[std::size_t(i)] + increment;
      jassert(!(newValue > 1.f) && !(newValue < 0.f));
      parameters[i]->setValue(newValue);
    }
  }

  bool hasInstance() {
    JUCE_ASSERT_MESSAGE_THREAD

    if (instance)
      return true;
    else
      return false;
  }

  juce::AudioProcessorEditor* getEditor() {
    JUCE_ASSERT_MESSAGE_THREAD

    if (hasInstance() && instance->hasEditor())
      return instance->createEditor();
    else
      return nullptr;
  }

  juce::AudioProcessorValueTreeState& apvts;
  juce::ChangeBroadcaster instanceBroadcaster;
  std::unique_ptr<juce::AudioPluginInstance> instance; 
  std::vector<std::vector<float>> parameterStates;
};

} // namespace atmt
