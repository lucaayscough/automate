#pragma once

#include "types.hpp"
#include <juce_gui_basics/juce_gui_basics.h>

static constexpr f32 kFlat = 0.000001f;
static constexpr f32 kExpand = 1000000.f;

static f64 getYFromX(juce::Path& p, f64 x) {
  return p.getPointAlongPath(f32(x), juce::AffineTransform::scale(1, kFlat)).y * kExpand;
}
