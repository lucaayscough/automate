#pragma once

#include "identifiers.hpp"
#include "state_manager.hpp"
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include "ui_bridge.hpp"

namespace atmt {

struct Engine : juce::ValueTree::Listener {
  Engine(StateManager& m, UIBridge& b) : manager(m), uiBridge(b) {
    editTree.addListener(this);
    presetsTree.addListener(this);
  }

  void prepare(double sampleRate, int blockSize) {
    if (instance) {
      instance->prepareToPlay(sampleRate, blockSize);
    }
  }

  void process(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiBuffer) {
    if (instance) {
      //auto playhead = apvts.processor.getPlayHead();
      //auto position = playhead->getPosition();

      //if (position.hasValue()) {
      //  auto time = position->getTimeInSeconds();
      //  if (time.hasValue()) {
      //    uiBridge.playheadPosition.store(*time, std::memory_order_relaxed);

      //    // TODO(luca): handle more than 2 overlapping clips and clips which have same start time
      //    auto clip1 = getFirstActiveClip(*time);
      //    auto clip2 = getSecondActiveClip(*time);

      //    if (clip1 && !clip2) {
      //      auto preset = getPresetForClip(clip1);
      //      setParameters(preset);
      //    } else if (clip1 && clip2) {
      //      double lerpPos = getClipInterpolationPosition(clip1, clip2, *time);
      //      interpolateParameters(getPresetForClip(clip1), getPresetForClip(clip2), lerpPos); 
      //    }
      //  }
      //}

      instance->processBlock(buffer, midiBuffer);
    }
  }

  //double getClipInterpolationPosition(Clip* clip1, Clip* clip2, double time) {
  //  double overlapLength;
  //  double offsetFromStart;
  //  if (clip1->start < clip2->start) {
  //    overlapLength = clip1->end - clip2->start;
  //    offsetFromStart = time - clip2->start;
  //  } else {
  //    overlapLength = clip2->end - clip1->start;
  //    offsetFromStart = time - clip1->start;
  //  }
  //  return offsetFromStart / overlapLength;
  //}

  void interpolateParameters(Preset* p1, Preset* p2, double position) {
    auto& beginParameters = p1->parameters;
    auto& endParameters   = p2->parameters;
    auto& parameters      = instance->getParameters();

    for (std::size_t i = 0; i < std::size_t(parameters.size()); ++i) {
      auto distance  = endParameters[i] - beginParameters[i] ;
      auto increment = distance * position; 
      auto newValue  = beginParameters[i] + increment;
      jassert(!(newValue > 1.f) && !(newValue < 0.f));
      parameters[int(i)]->setValue(float(newValue));
    }
  }
  
  //Clip* getFirstActiveClip(double time) {
  //  for (auto& clip : clips) {
  //    if (time >= clip->start && time <= clip->end) {
  //      return clip.get();
  //    }
  //  }
  //  return nullptr;
  //}

  //Clip* getSecondActiveClip(double time) {
  //  bool foundFirst = false;
  //  for (auto& clip : clips) {
  //    if (time >= clip->start && time <= clip->end) {
  //      if (foundFirst) {
  //        return clip.get();
  //      } else {
  //        foundFirst = true;
  //      }
  //    }
  //  }
  //  return nullptr;
  //}

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

  void setPluginInstance(std::unique_ptr<juce::AudioPluginInstance>& _instance) {
    jassert(_instance);

    instance = std::move(_instance);
    instanceBroadcaster.sendChangeMessage();
  }

  void getCurrentParameterValues(std::vector<float>& values) {
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
    //jassert(instance);

    //auto preset = presetsTree.getChildWithProperty(ID::name, name);
    //auto index = presetsTree.indexOf(preset);
    //auto parameters = instance->getParameters();
    //for (std::size_t i = 0; i < presetParameters[std::size_t(index)].size(); ++i) {
    //  parameters[int(i)]->setValue(presetParameters[std::size_t(index)][i]); 
    //}
  }

  void addPreset(juce::ValueTree& presetTree) {
    auto parametersVar = presetTree[ID::parameters];
    auto mb = parametersVar.getBinaryData();
    auto len = mb->getSize() / sizeof(float);
    auto data = (float*)mb->getData();

    auto preset = std::make_unique<Preset>(presetTree, undoManager);

    for (std::size_t i = 0; i < len; ++i) {
      preset->parameters.push_back(data[i]);
    }

    proc.suspendProcessing(true);
    presets.push_back(std::move(preset));
    proc.suspendProcessing(false);
  }

  void removePreset(int index) {
    proc.suspendProcessing(true);
    presets.erase(presets.begin() + index);
    proc.suspendProcessing(false);
  }

  void addClip(juce::ValueTree& clipTree) {
    auto clip = std::make_unique<Clip>(clipTree, undoManager);

    proc.suspendProcessing(true);
    clips.push_back(std::move(clip));
    proc.suspendProcessing(false);
  }

  void removeClip(int index) {
    proc.suspendProcessing(true);
    clips.erase(clips.begin() + index);
    proc.suspendProcessing(false);
  }

  void valueTreeChildAdded(juce::ValueTree&, juce::ValueTree& child) override {
    if (child.hasType(ID::PRESET)) {
      addPreset(child);
    } else if (child.hasType(ID::CLIP)) {
      addClip(child);
    }
  }

  void valueTreeChildRemoved(juce::ValueTree&, juce::ValueTree& child, int index) override {
    if (child.hasType(ID::PRESET)) {
      removePreset(index);
    } else if (child.hasType(ID::CLIP)) {
      removeClip(index);
    }
  }

  bool hasInstance() {
    if (instance)
      return true;
    else
      return false;
  }

  juce::AudioProcessorEditor* getEditor() {
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

  std::vector<std::unique_ptr<Preset>> presets;
  std::vector<std::unique_ptr<Clip>> clips;
};

} // namespace atmt
