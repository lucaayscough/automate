#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace atmt {

struct Path {
  using File = juce::File;

  static void init();
  static const File data;
  static const File knownPluginList;
};

void loadKnownPluginList(juce::KnownPluginList&);
void saveKnownPluginList(juce::KnownPluginList&);

} // namespace atmt
