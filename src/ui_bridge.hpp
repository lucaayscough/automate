#pragma once

namespace atmt {

struct UIBridge {
  std::atomic<int> playheadPosition = 0;
};

} // namespace atmt
