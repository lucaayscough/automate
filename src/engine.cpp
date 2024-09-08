#include "engine.hpp"
#include "scoped_timer.hpp"

namespace atmt {

Engine::Engine(StateManager& m, UIBridge& b) : manager(m), uiBridge(b) {}

Engine::~Engine() {
  if (instance) {
    instance->removeListener(this);
  }
}

void Engine::kill() {
  proc.suspendProcessing(true);
  killInstanceBroadcaster.sendSynchronousChangeMessage();
  instance.reset();
  proc.suspendProcessing(false);
}

void Engine::prepare(double sampleRate, int blockSize) {
  if (instance) {
    instance->prepareToPlay(sampleRate, blockSize);
    instance->addListener(this);
    proc.setLatencySamples(instance->getLatencySamples());

    jassert(proc.getBusCount(true)  >= instance->getBusCount(true));
    jassert(proc.getBusCount(false) >= instance->getBusCount(false));

    if (proc.checkBusesLayoutSupported(instance->getBusesLayout())) {
      jassert(proc.setBusesLayout(instance->getBusesLayout()));
    } else {
      for (i32 i = 0; i < instance->getBusCount(true); ++i) {
        jassert(proc.setChannelLayoutOfBus(true, i, instance->getChannelLayoutOfBus(true, i)));  
      }

      for (i32 i = 0; i < instance->getBusCount(false); ++i) {
        jassert(proc.setChannelLayoutOfBus(false, i, instance->getChannelLayoutOfBus(false, i)));  
      }
    }

    if (instance->getMainBusNumInputChannels() > 0) {
      // TODO(luca): figure out appropriate protocol for plugins that don't have audio inputs
      jassert(proc.getMainBusNumInputChannels()  == instance->getMainBusNumInputChannels());
      jassert(proc.getMainBusNumOutputChannels() == instance->getMainBusNumOutputChannels());

      jassert(proc.getTotalNumInputChannels()  == instance->getTotalNumInputChannels());
      jassert(proc.getTotalNumOutputChannels() == instance->getTotalNumOutputChannels());
    }

    jassert(!instance->isUsingDoublePrecision());

    // TODO(luca): deal with precision

    // TODO(luca): check that this is redundant
    //for (i32 i = 0; i < proc.getBusCount(true); ++i) {
    //  jassert(proc.setChannelLayoutOfBus(true, i, instance->getChannelLayoutOfBus(true, i)));
    //}

    //for (i32 i = 0; i < proc.getBusCount(false); ++i) {
    //  jassert(proc.setChannelLayoutOfBus(false, i, instance->getChannelLayoutOfBus(false, i)));
    //}
    
    // TODO(luca): this stuff will probably not work the way we want it to
    // https://forum.juce.com/t/what-are-the-chances-of-dynamic-plugin-buses-ever-working/53020
  }
}

void Engine::process(juce::AudioBuffer<f32>& buffer, juce::MidiBuffer& midiBuffer) {
  if (instance) {
    if (!editMode) {
      auto time = double(uiBridge.playheadPosition);
      auto lerpPos = getYFromX(manager.automation, time);
      jassert(!(lerpPos > 1.f) && !(lerpPos < 0.f));

      ClipPair clipPair;

      {
        f64 closest = u32(-1);

        auto& clips = manager.clips;

        for (u32 i = 0; i < clips.size(); ++i) {
          if (time >= clips[i].x) {
            if (time - clips[i].x < closest) {
              clipPair.a = &clips[i];  
              closest = time - clips[i].x;
            }
          }
        }

        closest = u32(-1);
        
        for (u32 i = 0; i < clips.size(); ++i) {
          if (time <= clips[i].x) {
            if (clips[i].x - time < closest) {
              clipPair.b = &clips[i];  
              closest = time - clips[i].x;
            }
          }
        }

        if (!clipPair.a && clipPair.b) {
          clipPair.a = clipPair.b;
          clipPair.b = nullptr;
        }
      }

      if (clipPair.a && !clipPair.b) {
        auto& presetParameters = clipPair.a->parameters;
        auto& parameters = manager.parameters;

        for (u32 i = 0; i < parameters.size(); ++i) {
          if (manager.shouldProcessParameter(&parameters[i])) {
            if (std::abs(parameters[i].parameter->getValue() - presetParameters[i]) > EPSILON) {
              parameters[i].parameter->setValue(presetParameters[i]);
            }
          }
        }
      } else if (clipPair.a && clipPair.b) {
        f64 position = bool(clipPair.a->y) ? 1.0 - lerpPos : lerpPos;

        auto& beginParameters = clipPair.a->parameters;
        auto& endParameters   = clipPair.b->parameters;
        auto& parameters      = manager.parameters;

        for (u32 i = 0; i < parameters.size(); ++i) {

          if (parameters[i].active) {
            if (manager.shouldProcessParameter(&parameters[i])) {
              assert(isNormalised(beginParameters[i]));
              assert(isNormalised(endParameters[i]));

              auto distance  = endParameters[i] - beginParameters[i];
              auto increment = distance * position; 
              auto newValue  = beginParameters[i] + increment;
              assert(isNormalised(newValue));

              if (std::abs(distance) > EPSILON) {
                parameters[i].parameter->setValue(f32(newValue));
              }
            }
          }
        }
      }
    }

    if (buffer.getNumChannels() < instance->getTotalNumInputChannels()) {
      buffer.setSize(instance->getTotalNumInputChannels(), buffer.getNumSamples(), true, false, true);
    }

    instance->processBlock(buffer, midiBuffer);
  }
}

void Engine::setPluginInstance(std::unique_ptr<juce::AudioPluginInstance>& _instance) {
  jassert(_instance);

  instance = std::move(_instance);
  createInstanceBroadcaster.sendChangeMessage();
}

void Engine::audioProcessorParameterChanged(juce::AudioProcessor*, i32 i, f32) {
  DBG("Engine::audioProcessorParameterChanged()");
  handleParameterChange(&manager.parameters[u32(i)]);
}

void Engine::audioProcessorChanged(juce::AudioProcessor*, const juce::AudioProcessorListener::ChangeDetails&) {}

void Engine::audioProcessorParameterChangeGestureBegin(juce::AudioProcessor*, i32 i) {
  DBG("Engine::audioProcessorParameterChangeGestureBegin()");
  handleParameterChange(&manager.parameters[u32(i)]);
}

void Engine::handleParameterChange(Parameter* parameter) {
  juce::MessageManager::callAsync([parameter, this] {
    if (captureParameterChanges) {
      manager.setParameterActive(parameter, true);
    } else if (releaseParameterChanges) {
      manager.setParameterActive(parameter, false);
    }

    if (manager.shouldProcessParameter(parameter)) {
      if (!editMode) {
        manager.setEditMode(true);
      }
    }
  });
}

void Engine::audioProcessorParameterChangeGestureEnd(juce::AudioProcessor*, int) {
  // TODO(luca): implent this
}

bool Engine::hasInstance() {
  return instance ? true : false;
}

juce::AudioProcessorEditor* Engine::getEditor() {
  return hasInstance() && instance->hasEditor() ? instance->createEditor() : nullptr;
}

} // namespace atmt
