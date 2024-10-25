#include "engine.hpp"
#include "scoped_timer.hpp"

namespace atmt {

Engine::Engine(StateManager& m) : manager(m) {}

void Engine::prepare(f32 sampleRate, i32 blockSize) {
  assert(instance);

  instance->prepareToPlay(sampleRate, blockSize);
  proc.setLatencySamples(instance->getLatencySamples());
}

void Engine::setParameters(const std::vector<f32>& preset, std::vector<Parameter>& parameters) {
  //scoped_timer t("Engine::setParameters()");

  for (u32 i = 0; i < parameters.size(); ++i) {
    if (manager.shouldProcessParameter(i)) {
      if (neqf32(parameters[i].parameter->getValue(), preset[i])) {
        parameters[i].parameter->setValue(preset[i]);
      }
    }
  }
}

void Engine::interpolate() {
  //scoped_timer t("Engine::interpolate()");

  assert(instance);

  f32 time = manager.playheadPosition;
  f32 lerpPos = getYFromX(manager.automation, time);
  assert(!(lerpPos > 1.f) && !(lerpPos < 0.f));

  const auto& clips = manager.clips;
  const auto& pairs = lerpPairs;

  assert(!clips.empty());

  if (clips.size() == 1) {
    if (lastVisitedPair != FRONT_PAIR) {
      lastVisitedPair = FRONT_PAIR;
      setParameters(clips.front().parameters, manager.parameters);
    }
  } else if (time < pairs.front().start) {
    if (lastVisitedPair != FRONT_PAIR) {
      lastVisitedPair = FRONT_PAIR;
      setParameters(clips[pairs.front().a].parameters, manager.parameters);
    }
  } else if (time > pairs.back().end) {
    assert(clips.size() == pairs.size() + 1);

    if (lastVisitedPair != BACK_PAIR) {
      lastVisitedPair = BACK_PAIR;
      setParameters(clips[pairs.back().b].parameters, manager.parameters);
    }
  } else {
    assert(clips.size() == pairs.size() + 1);

    u32 a, b;

    for (u32 pairIndex = 0; pairIndex < pairs.size(); ++pairIndex) {
      assert(pairIndex < pairs.size());

      if (time >= pairs[pairIndex].start && time <= pairs[pairIndex].end) {
        a = pairs[pairIndex].a;
        b = pairs[pairIndex].b;

        const auto& beginParameters = clips[a].parameters;
        const auto& endParameters   = clips[b].parameters;
        auto& parameters = manager.parameters;

        if (pairs[pairIndex].interpolate) {
          for (u32 parameterIndex = 0; parameterIndex < parameters.size(); ++parameterIndex) {
            if (parameters[parameterIndex].active && (pairs[pairIndex].parameters[parameterIndex] || lastVisitedPair != i32(pairIndex))) {
              if (manager.shouldProcessParameter(parameterIndex)) {
                assert(isNormalised(beginParameters[parameterIndex]));
                assert(isNormalised(endParameters[parameterIndex]));

                f32 position = bool(clips[u32(a)].y) ? 1.f - lerpPos : lerpPos;

                f32 increment = (endParameters[parameterIndex] - beginParameters[parameterIndex]) * position;
                f32 newValue  = beginParameters[parameterIndex] + increment;
                assert(isNormalised(newValue));

                parameters[parameterIndex].parameter->setValue(f32(newValue));

                if (manager.uiParameterSync.mode == UIParameterSync::EngineUpdate) {
                  manager.uiParameterSync.values[parameterIndex] = f32(newValue);
                  manager.uiParameterSync.updates[parameterIndex] = true;
                }
              }
            }
          }
        }

        lastVisitedPair = i32(pairIndex);
        break;
      }
    }
  }

  if (manager.uiParameterSync.mode == UIParameterSync::EngineUpdate) {
    manager.uiParameterSync.mode = UIParameterSync::UIUpdate;
  }
}

void Engine::process(juce::AudioBuffer<f32>& buffer, juce::MidiBuffer& midiBuffer) {
  assert(instance);

  if (!manager.editMode && !manager.clips.empty()) {
    interpolate();
  }

  if (buffer.getNumChannels() < instance->getTotalNumInputChannels()) {
    buffer.setSize(instance->getTotalNumInputChannels(), buffer.getNumSamples(), true, false, true);
  }

  instance->processBlock(buffer, midiBuffer);
}

} // namespace atmt
