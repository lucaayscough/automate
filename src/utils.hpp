#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#define EPSILON 1e-9f

namespace atmt {

struct FilePath {
  using File = juce::File;

  static void init();
  static const File data;
  static const File knownPluginList;
};

void loadKnownPluginList(juce::KnownPluginList&);
void saveKnownPluginList(juce::KnownPluginList&);
double secondsToPpq(double, double);

} // namespace atmt
