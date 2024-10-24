#pragma once

#include "types.hpp"
#include "assert.h"

namespace atmt {

struct Beat {
  u32 bar = 1;
  u32 beat = 1;
  f32 x = 0;
};

struct TimeSignature {
  u32 numerator = 4;
  u32 denominator = 4;
};

struct Grid {
  void reset() {
    assert(zoom > 0 && maxWidth > 0 && ts.numerator > 0 && ts.denominator > 0);

    // TODO(luca): there is still some stuff to work out with the triplet grid
    lines.clear();
    beats.clear();
    
    u32 barCount = 0;
    u32 beatCount = 0;

    f32 pxInterval = zoom / (ts.denominator / 4.f);
    u32 beatInterval = 1;
    u32 barInterval = ts.numerator;

    while (pxInterval * beatInterval < intervalMin) {
      beatInterval *= 2;

      if (beatInterval > barInterval) {
        beatInterval -= beatInterval % barInterval;
      } else {
        beatInterval += barInterval - beatInterval;
      }
    }

    pxInterval *= beatInterval;

    f32 pxTripletInterval = pxInterval * 2 / 3;
    f32 x = 0;
    f32 tx = 0;
    u32 numSubIntervals = u32(std::abs(gridWidth)) * 2;
    
    f32 subInterval = 0;
    if (gridWidth < 0) {
      subInterval = tripletMode ? pxTripletInterval : pxInterval / f32(numSubIntervals);
    } else if (gridWidth > 0) {
      subInterval = tripletMode ? pxTripletInterval : pxInterval / (1.f / f32(numSubIntervals));
    } else {
      subInterval = tripletMode ? pxTripletInterval : pxInterval; 
    }

    u32 count = 0;

    while (x < maxWidth || tx < maxWidth) {
      Beat b { barCount + 1, beatCount + 1, x };
      beats.push_back(b);

      if (gridWidth < 0) {
        for (u32 i = 0; i < numSubIntervals; ++i) {
          lines.push_back((tripletMode ? tx : x) + f32(i) * subInterval);
        }
        lines.push_back(tripletMode ? tx : x);
      } else if (gridWidth > 0) {
        if (count % numSubIntervals == 0) {
          lines.push_back(tripletMode ? tx : x);
        }
      } else {
        lines.push_back(tripletMode ? tx : x);
      }

      x += pxInterval;
      tx += pxTripletInterval;

      for (u32 i = 0; i < beatInterval; ++i) {
        beatCount = (beatCount + 1) % barInterval;

        if (!beatCount) {
          ++barCount;
        }
      }
      ++count;
    }

    snapInterval = subInterval;
  }

  f32 snap(f32 time) {
    if (snapOn) {
      i32 div = i32(time / snapInterval);
      f32 left  = div * snapInterval;
      return time - left < snapInterval / 2 ? left : left + snapInterval;
    } else {
      return time;
    }
  }

  void narrow() {
    if (gridWidth > -2) {
      --gridWidth;
      reset();
    }
  }

  void widen() {
    if (gridWidth < 2) {
      ++gridWidth;
      reset();
    }
  }

  void triplet() {
    tripletMode = !tripletMode;
    reset();
  }

  void toggleSnap() {
    snapOn = !snapOn;
  }

  // Cache
  TimeSignature ts;
  f32 maxWidth = 0;
  f32 zoom = 0;

  static constexpr f32 intervalMin = 40;
  f32 snapInterval = 0;
  bool tripletMode = false;
  i32 gridWidth = -2;
  bool snapOn = true;

  std::vector<Beat> beats;
  std::vector<f32> lines;
};

} // namespace atmt
