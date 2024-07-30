#pragma once

namespace atmt {

struct UIBridge {
  std::atomic<bool> controlPlayhead = false;
  std::atomic<double> playheadPosition = 0;
  std::atomic<double> bpm = 120;
};

} // namespace atmt
