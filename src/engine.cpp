#include "engine.hpp"

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

void Engine::process(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiBuffer) {
  if (instance) {
    if (!editMode) {
      auto time = double(uiBridge.playheadPosition);
      auto lerpPos = automation.getYFromX(time);
      jassert(!(lerpPos > 1.f) && !(lerpPos < 0.f));
      auto clipPair = getClipPair(time);

      if (clipPair.a && !clipPair.b) {
        auto preset = presets.getPresetForClip(clipPair.a);
        setParameters(preset);
      } else if (clipPair.a && clipPair.b) {
        auto p1 = presets.getPresetForClip(clipPair.a);
        auto p2 = presets.getPresetForClip(clipPair.b);

        if (p1 && p2) {
          interpolateParameters(p1, p2, bool(clipPair.a->_y) ? 1.0 - lerpPos : lerpPos); 
        } 
      }
    }

    if (buffer.getNumChannels() < instance->getTotalNumInputChannels()) {
      buffer.setSize(instance->getTotalNumInputChannels(), buffer.getNumSamples(), true, false, true);
    }

    instance->processBlock(buffer, midiBuffer);
  }
}

void Engine::interpolateParameters(Preset* p1, Preset* p2, double position) {
  auto& beginParameters = p1->_parameters;
  auto& endParameters   = p2->_parameters;
  auto& parameters      = instance->getParameters();

  auto numParameters = p1->_numParameters < p2->_numParameters ? p1->_numParameters.load() : p2->_numParameters.load();

  for (std::size_t i = 0; i < numParameters; ++i) {
    auto distance  = endParameters[i] - beginParameters[i];
    auto increment = distance * position; 
    auto newValue  = beginParameters[i] + increment;
    jassert(!(newValue > 1.f) && !(newValue < 0.f));

    if (!modulateDiscrete && parameters[int(i)]->isDiscrete()) {
      continue; 
    }

    parameters[int(i)]->setValue(float(newValue));
  }
}

ClipPair Engine::getClipPair(double time) {
  ClipPair clipPair;
  
  auto cond = [time] (const std::unique_ptr<Clip>& c) { return time > c->_x; }; 
  auto it = std::find_if(clips.rbegin(), clips.rend(), cond);

  if (it != clips.rend()) {
    if (it == clips.rbegin()) {
      clipPair.a = it->get();
    } else {
      clipPair.a = it->get();
      clipPair.b = std::prev(it)->get();
    }
  } else if (!clips.empty()) {
    clipPair.a = clips.front().get();
  }

  return clipPair;
}

void Engine::setParameters(Preset* preset) {
  auto& presetParameters = preset->_parameters;
  auto& parameters = instance->getParameters();

  for (int i = 0; i < parameters.size(); ++i) {
    if (!modulateDiscrete && parameters[i]->isDiscrete()) {
      continue; 
    }

    parameters[i]->setValue(presetParameters[std::size_t(i)]);
  }
}

void Engine::setPluginInstance(std::unique_ptr<juce::AudioPluginInstance>& _instance) {
  jassert(_instance);

  instance = std::move(_instance);
  createInstanceBroadcaster.sendChangeMessage();
}

void Engine::getCurrentParameterValues(std::vector<float>& values) {
  jassert(instance);

  auto parameters = instance->getParameters();
  values.reserve(std::size_t(parameters.size()));
  for (auto* parameter : parameters) {
    values.push_back(parameter->getValue());
  }
}

void Engine::restoreFromPreset(const juce::String& name) {
  jassert(instance);

  editModeAttachment.setValue({ true });

  proc.suspendProcessing(true);
  auto preset = presets.getPresetFromName(name);
  auto& presetParameters = preset->_parameters;
  auto parameters = instance->getParameters();
  for (std::size_t i = 0; i < std::size_t(parameters.size()); ++i) {
    if (!modulateDiscrete && parameters[int(i)]->isDiscrete()) {
      continue; 
    }
    parameters[int(i)]->setValue(presetParameters[i]); 
  }
  proc.suspendProcessing(false);
}

void Engine::randomiseParameters() {
  proc.suspendProcessing(true);  

  for (auto p : instance->getParameters()) {
    p->setValue(rand.nextFloat());  
  }

  proc.suspendProcessing(false);
}

void Engine::audioProcessorParameterChanged(juce::AudioProcessor*, int, float) {}
void Engine::audioProcessorChanged(juce::AudioProcessor*, const juce::AudioProcessorListener::ChangeDetails&) {}

void Engine::audioProcessorParameterChangeGestureBegin(juce::AudioProcessor*, int) {
  if (!editMode) {
    editModeAttachment.setValue({ true });
  }
}

void Engine::audioProcessorParameterChangeGestureEnd(juce::AudioProcessor*, int) {
  // TODO(luca): implent this
}

void Engine::editModeChangeCallback(const juce::var& v) {
  editMode = bool(v); 
}

void Engine::modulateDiscreteChangeCallback(const juce::var& v) {
  modulateDiscrete = bool(v);
}

bool Engine::hasInstance() {
  return instance ? true : false;
}

juce::AudioProcessorEditor* Engine::getEditor() {
  return hasInstance() && instance->hasEditor() ? instance->createEditor() : nullptr;
}

} // namespace atmt
