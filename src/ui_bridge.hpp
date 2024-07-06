#pragma once

namespace atmt {

struct UIBridge {
  std::atomic<double> playheadPosition = 0;
};

} // namespace atmt
