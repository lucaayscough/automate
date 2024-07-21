#pragma once

#include "state_attachment.hpp"
#include "identifiers.hpp"
#include "state_manager.hpp"
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include "ui_bridge.hpp"

namespace atmt {

struct Engine : juce::AudioProcessorListener {
  struct ClipPair {
    Clip* a = nullptr;
    Clip* b = nullptr;
  };

  Engine(StateManager& m, UIBridge& b) : manager(m), uiBridge(b) {}

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
            auto lerpPos = automation->getPointAlongPath(float(*time), juce::AffineTransform::scale(1, 0.000001f)).y * 1000000.0;
            jassert(!(lerpPos > 1.f) && !(lerpPos < 0.f));
            auto clipPair = getClipPair(*time);

            if (clipPair.a && !clipPair.b) {
              auto preset = presets.getPresetForClip(clipPair.a);
              setParameters(preset);
            } else if (clipPair.a && clipPair.b) {
              auto p1 = presets.getPresetForClip(clipPair.a);
              auto p2 = presets.getPresetForClip(clipPair.b);
              interpolateParameters(p1, p2, clipPair.a->top ? lerpPos : 1.0 - lerpPos); 
            }
          }
        }
      }

      instance->processBlock(buffer, midiBuffer);
    }
  }

  void interpolateParameters(Preset* p1, Preset* p2, double position) {
    auto beginParameters = *p1->parameters;
    auto endParameters   = *p2->parameters;
    auto& parameters      = instance->getParameters();

    for (std::size_t i = 0; i < std::size_t(parameters.size()); ++i) {
      auto distance  = endParameters[i] - beginParameters[i];
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

  void setParameters(Preset* preset) {
    auto presetParameters = *preset->parameters;
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

  void restoreFromPreset(const juce::String& name) {
    jassert(instance);

    editModeAttachment.setValue({ true });

    proc.suspendProcessing(true);
    auto preset = presets.getPresetFromName(name);
    auto presetParameters = *(preset->parameters);
    auto parameters = instance->getParameters();
    for (std::size_t i = 0; i < std::size_t(parameters.size()); ++i) {
      parameters[int(i)]->setValue(presetParameters[i]); 
    }
    proc.suspendProcessing(false);
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

  Presets presets { presetsTree, undoManager, &proc };
  Clips clips { editTree, undoManager, &proc };

  juce::CachedValue<juce::Path> automation { editTree, ID::automation, undoManager };

  StateAttachment editModeAttachment { editTree, ID::editMode, STATE_CB(editModeChangeCallback), nullptr};
  std::atomic<bool> editMode = false;
};

} // namespace atmt
