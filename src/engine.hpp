#pragma once

#include "identifiers.hpp"
#include "state_manager.hpp"
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_devices/juce_audio_devices.h>

namespace atmt {

struct Engine : juce::ValueTree::Listener {
  Engine(StateManager& _manager)
    : manager(_manager) {
    JUCE_ASSERT_MESSAGE_THREAD

    edit.addListener(this);
    presets.addListener(this);
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
      //    if (presets.size() > 1) {
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
    presetParameters.clear();
    instanceBroadcaster.sendChangeMessage();
  }

  void getCurrentParameterValues(std::vector<float>& values) {
    JUCE_ASSERT_MESSAGE_THREAD
    jassert(instance);
    auto parameters = instance->getParameters();
    values.reserve(std::size_t(parameters.size()));
    for (auto* parameter : parameters) {
      values.push_back(parameter->getValue());
    }
  }

  // TODO(luca): as noted elsewhere, this may not have a place in plugin, or
  // if it does, we should have a property in the vt which specifies which preset
  // is currently selected
  void restoreFromPreset(const juce::String& name) {
    JUCE_ASSERT_MESSAGE_THREAD
    jassert(instance);
    auto preset = presets.getChildWithProperty(ID::name, name);
    auto index = presets.indexOf(preset);
    auto parameters = instance->getParameters();
    for (std::size_t i = 0; i < presetParameters[std::size_t(index)].size(); ++i) {
      parameters[int(i)]->setValue(presetParameters[std::size_t(index)][i]); 
    }
  }

  void valueTreeChildAdded(juce::ValueTree& parent, juce::ValueTree& child) override {
    JUCE_ASSERT_MESSAGE_THREAD
    jassert(parent.isValid() && child.isValid());
    
    if (child.hasType(ID::PRESET)) {
      auto parametersVar = child[ID::parameters]; 
      jassert(!parametersVar.isVoid());
      auto mb = parametersVar.getBinaryData();
      auto len = mb->getSize() / sizeof(float);
      auto data = (float*)mb->getData();
      std::vector<float> parameterValues;
      for (std::size_t i = 0; i < len; ++i) {
        parameterValues.push_back(data[i]);
      }
      presetParameters.push_back(parameterValues);
    }
  }

  void valueTreeChildRemoved(juce::ValueTree& parent, juce::ValueTree& child, int index) override {
    JUCE_ASSERT_MESSAGE_THREAD
    jassert(parent.isValid() && child.isValid());

    if (child.hasType(ID::PRESET)) {
      presetParameters.erase(presetParameters.begin() + index);
    }
  }

  void interpolateStates(int stateBeginIndex, int stateEndIndex, float position) {
    auto& stateBegin = presetParameters[std::size_t(stateBeginIndex)];
    auto& stateEnd   = presetParameters[std::size_t(stateEndIndex)];
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

  StateManager& manager;
  juce::AudioProcessorValueTreeState& apvts { manager.apvts };
  juce::ValueTree edit { manager.edit };
  juce::ValueTree presets { manager.presets };
  juce::ChangeBroadcaster instanceBroadcaster;
  std::unique_ptr<juce::AudioPluginInstance> instance; 
  std::vector<std::vector<float>> presetParameters;
};

} // namespace atmt
