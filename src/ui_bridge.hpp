#pragma once

#include "types.h"

namespace atmt {

struct UIBridge {
  std::atomic<bool> controlPlayhead = false;
  std::atomic<double> playheadPosition = 0;
  std::atomic<double> bpm = 120;
  std::atomic<u32> numerator = 4;
  std::atomic<u32> denominator = 4;
};

} // namespace atmt
