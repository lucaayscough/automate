#pragma once

#include "identifiers.hpp"
#include "state_manager.hpp"
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include "ui_bridge.hpp"

namespace atmt {

struct Engine : juce::ValueTree::Listener {
  Engine(StateManager& m, UIBridge& b) : manager(m), uiBridge(b) {
    JUCE_ASSERT_MESSAGE_THREAD

    editTree.addListener(this);
    presetsTree.addListener(this);
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
        auto time = position->getTimeInSeconds();
        if (time.hasValue()) {
          // NOTE(luca): a regular int will give us ≈25 days of audio
          auto ms = int(*time * 1000.0); 
          uiBridge.playheadPosition.store(ms, std::memory_order_relaxed);
          auto clip = getFirstActiveClip(ms);
          if (clip) {
            auto preset = getPresetForClip(clip);
            setParameters(preset);
          }
        }
      }

      instance->processBlock(buffer, midiBuffer);
    }
  }

  void interpolateParameters(int beginIndex, int endIndex, float position) {
    auto& beginParameters = presets[std::size_t(beginIndex)]->parameters;
    auto& endParameters   = presets[std::size_t(endIndex)]->parameters;
    auto& parameters      = instance->getParameters();

    for (int i = 0; i < parameters.size(); ++i) {
      float distance  = endParameters[std::size_t(i)] - beginParameters[std::size_t(i)] ;
      float increment = distance * position; 
      float newValue  = beginParameters[std::size_t(i)] + increment;
      jassert(!(newValue > 1.f) && !(newValue < 0.f));
      parameters[i]->setValue(newValue);
    }
  }

  void setPluginInstance(std::unique_ptr<juce::AudioPluginInstance>& _instance) {
    JUCE_ASSERT_MESSAGE_THREAD
    jassert(_instance);

    instance = std::move(_instance);
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
  void restoreFromPreset(const juce::String&) {
    //JUCE_ASSERT_MESSAGE_THREAD
    //jassert(instance);

    //auto preset = presetsTree.getChildWithProperty(ID::name, name);
    //auto index = presetsTree.indexOf(preset);
    //auto parameters = instance->getParameters();
    //for (std::size_t i = 0; i < presetParameters[std::size_t(index)].size(); ++i) {
    //  parameters[int(i)]->setValue(presetParameters[std::size_t(index)][i]); 
    //}
  }

  void addPresetParameters(juce::ValueTree& presetTree) {
    JUCE_ASSERT_MESSAGE_THREAD
    jassert(presetTree.isValid());

    auto parametersVar = presetTree[ID::parameters];
    auto mb = parametersVar.getBinaryData();
    auto len = mb->getSize() / sizeof(float);
    auto data = (float*)mb->getData();
    auto preset = std::make_unique<Preset>();
    for (std::size_t i = 0; i < len; ++i) {
      preset->parameters.push_back(data[i]);
    }
    preset->name.referTo(presetTree, ID::name, undoManager);

    proc.suspendProcessing(true);
    presets.push_back(std::move(preset));
    proc.suspendProcessing(false);
  }

  void removePresetParameters(int index) {
    JUCE_ASSERT_MESSAGE_THREAD
    
    proc.suspendProcessing(true);
    presets.erase(presets.begin() + index);
    proc.suspendProcessing(false);
  }

  void addClip(juce::ValueTree& clipTree) {
    JUCE_ASSERT_MESSAGE_THREAD
    jassert(clipTree.isValid());
    
    auto clip = std::make_unique<Clip>();
    clip->name.referTo(clipTree, ID::name, undoManager);
    clip->start.referTo(clipTree, ID::start, undoManager);
    clip->end.referTo(clipTree, ID::end, undoManager);

    proc.suspendProcessing(true);
    clips.push_back(std::move(clip));
    proc.suspendProcessing(false);
  }

  void removeClip(int) {
    //
  }

  void valueTreeChildAdded(juce::ValueTree& parent, juce::ValueTree& child) override {
    JUCE_ASSERT_MESSAGE_THREAD
    jassert(parent.isValid() && child.isValid());
    
    if (child.hasType(ID::PRESET)) {
      addPresetParameters(child);
    } else if (child.hasType(ID::CLIP)) {
      addClip(child);
    }
  }

  void valueTreeChildRemoved(juce::ValueTree& parent, juce::ValueTree& child, int index) override {
    JUCE_ASSERT_MESSAGE_THREAD
    jassert(parent.isValid() && child.isValid());

    if (child.hasType(ID::PRESET)) {
      removePresetParameters(index);
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
  UIBridge& uiBridge;
  juce::UndoManager* undoManager { manager.undoManager };
  juce::AudioProcessorValueTreeState& apvts { manager.apvts };
  juce::AudioProcessor& proc { apvts.processor };
  juce::ValueTree editTree { manager.edit };
  juce::ValueTree presetsTree { manager.presets };
  juce::ChangeBroadcaster instanceBroadcaster;
  std::unique_ptr<juce::AudioPluginInstance> instance; 

  struct Preset {
    juce::CachedValue<juce::String> name;
    std::vector<float> parameters;
  };

  struct Clip {
    juce::CachedValue<juce::String> name;
    juce::CachedValue<int> start;
    juce::CachedValue<int> end;
  };
  
  Clip* getFirstActiveClip(int time) {
    for (auto& clip : clips) {
      if (time >= clip->start && time <= clip->end) {
        return clip.get();
      }
    }
    return nullptr;
  }

  Preset* getPresetForClip(Clip* clip) {
    for (auto& preset : presets) {
      if (preset->name == clip->name) {
        return preset.get();
      }
    }
    jassertfalse;
    return nullptr;
  }

  void setParameters(Preset* preset) {
    auto& presetParameters = preset->parameters;
    auto& parameters = instance->getParameters();

    for (int i = 0; i < parameters.size(); ++i) {
      parameters[i]->setValue(presetParameters[std::size_t(i)]);
    }
  }

  std::vector<std::unique_ptr<Preset>> presets;
  std::vector<std::unique_ptr<Clip>> clips;
};

} // namespace atmt
