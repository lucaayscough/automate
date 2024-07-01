#pragma once

#include "identifiers.hpp"
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_devices/juce_audio_devices.h>

namespace atmt {

struct Engine : juce::ValueTree::Listener {
  Engine(StateManager& _manager)
    : manager(_manager) {
    JUCE_ASSERT_MESSAGE_THREAD

    states.addListener(this);
  }

  ~Engine() {
    states.removeListener(this);
  }

  void prepare(double sampleRate, int blockSize) {
    JUCE_ASSERT_MESSAGE_THREAD

    if (instance) {
      instance->prepareToPlay(sampleRate, blockSize);
    }
  }

  void process(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiBuffer) {
    if (instance) {
      //auto playhead = apvts.processor.getPlayHead();
      //auto position = playhead->getPosition();

      //if (position.hasValue()) {
      //  auto time = position->getTimeInSamples();

      //  if (time.hasValue()) {
      //    auto interpolationValue = float(*time % 441000) / 441000.f;
      //    
      //    if (parameterStates.size() > 1) {
      //      interpolateStates(0, 1, interpolationValue); 
      //    }
      //  }
      //}

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

  void saveParameterState(const juce::String& name) {
    JUCE_ASSERT_MESSAGE_THREAD
    jassert(instance);
    std::vector<float> parameterValues;
    auto parameters = instance->getParameters();
    for (auto* parameter : parameters) {
      parameterValues.push_back(parameter->getValue());
    }
    juce::ValueTree newState { ID::STATE::type };
    newState.setProperty(ID::STATE::name, name, apvts.undoManager);
    newState.setProperty(ID::STATE::parameters, { parameterValues.data(), parameterValues.size() * sizeof(float) }, apvts.undoManager);
    states.addChild(newState, -1, apvts.undoManager);
    DBG(manager.valueTreeToXmlString(manager.state));
  }

  void removeParameterState(const juce::String& name) {
    auto child = states.getChildWithProperty(ID::STATE::name, name);
    states.removeChild(child, apvts.undoManager);
  }

  void restoreParameterState(const juce::String& name) {
    JUCE_ASSERT_MESSAGE_THREAD
    jassert(instance);
    auto child = states.getChildWithProperty(ID::STATE::name, name);
    auto index = states.indexOf(child);
    auto parameters = instance->getParameters();
    for (std::size_t i = 0; i < parameterStates[std::size_t(index)].size(); ++i) {
      parameters[int(i)]->setValue(parameterStates[std::size_t(index)][i]); 
    }
  }

  void valueTreeChildAdded(juce::ValueTree& parent, juce::ValueTree& child) {
    JUCE_ASSERT_MESSAGE_THREAD
    jassert(parent.isValid() && child.isValid());
    auto parametersVar = child[ID::STATE::parameters]; 
    jassert(!parametersVar.isVoid());
    auto mb = parametersVar.getBinaryData();
    auto len = mb->getSize() / sizeof(float);
    auto data = (float*)mb->getData();
    std::vector<float> parameterValues;
    for (size_t i = 0; i < len; ++i) {
      parameterValues.push_back(data[i]);
    }
    parameterStates.push_back(parameterValues);
  }

  void valueTreeChildRemoved(juce::ValueTree& parent, juce::ValueTree& child, int index) {
    JUCE_ASSERT_MESSAGE_THREAD
    jassert(parent.isValid() && child.isValid());
    parameterStates.erase(parameterStates.begin() + index);
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

  // TODO(luca): rename states variable to avoid confusion
  StateManager& manager;
  juce::AudioProcessorValueTreeState& apvts { manager.apvts };
  juce::ValueTree states { manager.states };
  juce::ChangeBroadcaster instanceBroadcaster;
  std::unique_ptr<juce::AudioPluginInstance> instance; 
  std::vector<StateAttachment> parameterStateAttachments;
  std::vector<std::vector<float>> parameterStates;
};

} // namespace atmt
