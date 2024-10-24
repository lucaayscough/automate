#pragma once

#include "geometry.hpp"
#include "state_manager.hpp"
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_devices/juce_audio_devices.h>

#define UNDEFINED_PAIR -3
#define FRONT_PAIR -2
#define BACK_PAIR -1

namespace atmt {

struct Engine  {
  Engine(StateManager&);

  void prepare(f32, i32);
  void setParameters(const std::vector<f32>&, std::vector<Parameter>&);
  void interpolate();
  void process(juce::AudioBuffer<f32>&, juce::MidiBuffer&);

  StateManager& manager;
  juce::AudioProcessor& proc { manager.proc };
  juce::AudioProcessor* instance = nullptr;

  std::vector<LerpPair> lerpPairs;
  i32 lastVisitedPair = UNDEFINED_PAIR;
};

} // namespace atmt
