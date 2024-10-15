#include "engine.hpp"
#include "scoped_timer.hpp"

namespace atmt {

Engine::Engine(StateManager& m) : manager(m) {}

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

void Engine::prepare(f32 sampleRate, i32 blockSize) {
  if (instance) {
    instance->prepareToPlay(sampleRate, blockSize);
    instance->addListener(this);
    proc.setLatencySamples(instance->getLatencySamples());
  }
}

void Engine::interpolate() {
  assert(instance);

  f32 time = manager.state.playheadPosition;
  f32 lerpPos = getYFromX(manager.state.automation, time);
  assert(!(lerpPos > 1.f) && !(lerpPos < 0.f));

  i32 a = -1;
  i32 b = -1;

  const auto& clips = manager.state.clips;

  if (!clips.empty()) {
    f32 closest = u32(-1);

    for (u32 i = 0; i < clips.size(); ++i) {
      if (time >= clips[i].x) {
        if (time - clips[i].x < closest) {
          a = i32(i);
          closest = time - clips[i].x;
        }
      }
    }

    assert(closest >= 0);
    closest = u32(-1);
    
    for (u32 i = 0; i < clips.size(); ++i) {
      if (i32(i) == a) {
        continue;
      }

      if (time <= clips[i].x) {
        if (clips[i].x - time < closest) {
          b = i32(i);  
          closest = clips[i].x - time;
        }
      }
    }

    assert(closest >= 0);

    if (a < 0 && b >= 0) {
      a = b;
      b = -1;
    }
  }

  if (a >= 0 && b < 0) {
    auto& presetParameters = clips[u32(a)].parameters;
    auto& parameters = manager.state.parameters;

    for (u32 i = 0; i < parameters.size(); ++i) {
      if (manager.shouldProcessParameter(&parameters[i])) {
        if (std::abs(parameters[i].parameter->getValue() - presetParameters[i]) > EPSILON) {
          parameters[i].parameter->setValue(presetParameters[i]);
        }
      }
    }
  } else if (a >= 0 && b >= 0) {
    f32 position = bool(clips[u32(a)].y) ? 1.f - lerpPos : lerpPos;

    auto& beginParameters = clips[u32(a)].parameters;
    auto& endParameters   = clips[u32(b)].parameters;
    auto& parameters      = manager.state.parameters;

    for (u32 i = 0; i < parameters.size(); ++i) {
      if (parameters[i].active) {
        if (manager.shouldProcessParameter(&parameters[i])) {
          assert(isNormalised(beginParameters[i]));
          assert(isNormalised(endParameters[i]));

          f32 increment = (endParameters[i] - beginParameters[i]) * position; 
          f32 newValue  = beginParameters[i] + increment;
          f32 distance  = std::abs(newValue - parameters[i].parameter->getValue());
          assert(isNormalised(newValue));

          if (distance > EPSILON) {
            parameters[i].parameter->setValue(f32(newValue));
          }
        }
      }
    }
  }
}

void Engine::process(juce::AudioBuffer<f32>& buffer, juce::MidiBuffer& midiBuffer) {
  if (instance) {
    if (!manager.state.editMode) {
      interpolate();
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
  handleParameterChange(&manager.state.parameters[u32(i)]);
}

void Engine::audioProcessorChanged(juce::AudioProcessor*, const juce::AudioProcessorListener::ChangeDetails&) {}

void Engine::audioProcessorParameterChangeGestureBegin(juce::AudioProcessor*, i32 i) {
  DBG("Engine::audioProcessorParameterChangeGestureBegin()");
  handleParameterChange(&manager.state.parameters[u32(i)]);
}

void Engine::handleParameterChange(Parameter* parameter) {
  juce::MessageManager::callAsync([parameter, this] {
    if (manager.state.captureParameterChanges) {
      manager.setParameterActive(parameter, true);
    } else if (manager.state.releaseParameterChanges) {
      manager.setParameterActive(parameter, false);
    }

    if (manager.shouldProcessParameter(parameter)) {
      if (!manager.state.editMode) {
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
