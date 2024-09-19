#pragma once

#include "types.hpp"

namespace atmt {

struct UIBridge {
  std::atomic<bool> controlPlayhead = false;
  std::atomic<f32> playheadPosition = 0;
  std::atomic<f32> bpm = 120;
  std::atomic<u32> numerator = 4;
  std::atomic<u32> denominator = 4;
};

} // namespace atmt
