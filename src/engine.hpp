#pragma once

#include "state_attachment.hpp"
#include "identifiers.hpp"
#include "state_manager.hpp"
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include "ui_bridge.hpp"

namespace atmt {

struct Engine : juce::ValueTree::Listener, juce::AudioProcessorListener {
  struct ClipPair {
    Clip* a = nullptr;
    Clip* b = nullptr;
  };

  Engine(StateManager& m, UIBridge& b) : manager(m), uiBridge(b) {
    editTree.addListener(this);
    presetsTree.addListener(this);
  }

  ~Engine() override {
    if (instance) {
      instance->removeListener(this);
    }
  }

  void prepare(double sampleRate, int blockSize) {
    if (instance) {
      instance->prepareToPlay(sampleRate, blockSize);
      instance->addListener(this);
    }
  }

  void process(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiBuffer) {
    if (instance) {
      auto playhead = apvts.processor.getPlayHead();
      auto position = playhead->getPosition();

      if (position.hasValue()) {
        auto time = position->getTimeInSeconds();
        if (time.hasValue()) {
          uiBridge.playheadPosition.store(*time, std::memory_order_relaxed);

          if (!editMode) {
            // TODO(luca): find nicer way of doing this scaling
            auto lerpPos = automation.getPointAlongPath(float(*time), juce::AffineTransform::scale(1, 0.000001f)).y * 1000000.0;
            jassert(!(lerpPos > 1.f) && !(lerpPos < 0.f));
            auto clipPair = getClipPair(*time);

            if (clipPair.a && !clipPair.b) {
              auto preset = getPresetForClip(clipPair.a);
              setParameters(preset);
            } else if (clipPair.a && clipPair.b) {
              auto p1 = getPresetForClip(clipPair.a);
              auto p2 = getPresetForClip(clipPair.b);
              interpolateParameters(p1, p2, clipPair.a->top ? lerpPos : 1.0 - lerpPos); 
            }
          }
        }
      }

      instance->processBlock(buffer, midiBuffer);
    }
  }

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
  
  ClipPair getClipPair(double time) {
    ClipPair clipPair;
    
    auto cond = [time] (const std::unique_ptr<Clip>& c) { return time > c->start; }; 
    auto it = std::find_if(clips.begin(), clips.end(), cond);

    if (it != clips.end()) {
      clipPair.a = it->get();
      if (std::next(it) != clips.end()) {
        clipPair.b = std::next(it)->get();
      }
    } else if (!clips.empty()) {
      clipPair.a = clips.front().get();
    }

    return clipPair;
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
    proc.suspendProcessing(true);
    clips.emplace_back(std::make_unique<Clip>(clipTree, undoManager));
    proc.suspendProcessing(false);
  }

  void removeClip(juce::ValueTree& clipTree) {
    auto name = clipTree[ID::name].toString();
    auto cond = [name] (const std::unique_ptr<Clip>& c) { return name == c->name; };
    auto it = std::find_if(clips.begin(), clips.end(), cond);

    if (it != clips.end()) {
      proc.suspendProcessing(true);
      clips.erase(it);
      proc.suspendProcessing(false);
    } else {
      jassertfalse;
    }
  }

  void sortClipsByStartTime() {
    using Clip = const std::unique_ptr<Clip>&;
    auto cmp = [] (Clip a, Clip b) { return a->start < b->start; };

    proc.suspendProcessing(true);
    std::sort(clips.begin(), clips.end(), cmp);
    proc.suspendProcessing(false);
  }

  void updateAutomation(const juce::var& v) {
    proc.suspendProcessing(true);
    automation.restoreFromString(v.toString());
    proc.suspendProcessing(false);
  }

  void valueTreeChildAdded(juce::ValueTree&, juce::ValueTree& child) override {
    if (child.hasType(ID::PRESET)) {
      addPreset(child);
    } else if (child.hasType(ID::CLIP)) {
      addClip(child);
      sortClipsByStartTime();
    }
  }

  void valueTreeChildRemoved(juce::ValueTree&, juce::ValueTree& child, int index) override {
    if (child.hasType(ID::PRESET)) {
      removePreset(index);
    } else if (child.hasType(ID::CLIP)) {
      removeClip(child);
      sortClipsByStartTime();
    }
  }

  void valueTreePropertyChanged(juce::ValueTree& vt, const juce::Identifier& id) override {
    if (vt.hasType(ID::EDIT) && id == ID::automation) {
      updateAutomation(vt[id]);
    } else if (vt.hasType(ID::CLIP)) {
      if (id == ID::start) {
        sortClipsByStartTime();
      }
    }
  }

  void audioProcessorParameterChanged(juce::AudioProcessor*, int, float) override {}
  void audioProcessorChanged(juce::AudioProcessor*, const juce::AudioProcessorListener::ChangeDetails&) override {}

  void audioProcessorParameterChangeGestureBegin(juce::AudioProcessor*, int) override {
    if (!editMode) {
      editModeAttachment.setValue({ true });
    }
  }

  void audioProcessorParameterChangeGestureEnd(juce::AudioProcessor*, int) override {
    // TODO(luca): implent this
  }

  void editModeChangeCallback(const juce::var& v) {
    editMode = bool(v); 
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

  StateAttachment editModeAttachment { editTree, ID::editMode, STATE_CB(editModeChangeCallback), nullptr};
  std::atomic<bool> editMode = false;

  juce::Path automation;
};

} // namespace atmt
