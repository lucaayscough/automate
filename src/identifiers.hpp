#pragma once

#include <juce_core/juce_core.h>

namespace atmt {

struct ID {
  static const juce::Identifier AUTOMATE;
  static const juce::Identifier PARAMETERS;
  static const juce::Identifier PRESETS;
  static const juce::Identifier PRESET;
  static const juce::Identifier EDIT;
  static const juce::Identifier CLIP;
  static const juce::Identifier PATH;

  static const juce::Identifier editMode;
  static const juce::Identifier modulateDiscrete;
  static const juce::Identifier pluginID;
  static const juce::Identifier name;
  static const juce::Identifier parameters;
  static const juce::Identifier zoom;
  static const juce::Identifier id;
  static const juce::Identifier x; 
  static const juce::Identifier y;
  static const juce::Identifier curve;
};

} // namespace atmt
