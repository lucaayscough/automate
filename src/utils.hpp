#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "types.hpp"

#define EPSILON 1e-9f

namespace atmt {

struct ScopedProcLock {
  ScopedProcLock(juce::AudioProcessor& p) : proc(p) {
    wasSuspended = proc.isSuspended();

    if (wasSuspended) {
      return;
    }

    proc.suspendProcessing(true);
  }

  ~ScopedProcLock() {
    if (wasSuspended) {
      return;
    }
 
    proc.suspendProcessing(false);
  }

  bool wasSuspended;
  juce::AudioProcessor& proc;
};

struct FilePath {
  using File = juce::File;

  static void init();
  static const File data;
  static const File knownPluginList;
};

} // namespace atmt
