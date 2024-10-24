#include "utils.hpp"
#include <random>

namespace atmt {

using File = juce::File;

const File FilePath::data { File::getSpecialLocation(File::userApplicationDataDirectory).getChildFile("Automate") };
const File FilePath::knownPluginList { data.getChildFile("KnownPluginList.txt") };

void FilePath::init() {
  jassert(data.hasWriteAccess());
  jassert(knownPluginList.hasWriteAccess());
  data.createDirectory();  
  knownPluginList.create();
}

static void loadKnownPluginList(juce::KnownPluginList& kpl) {
  auto xml = juce::XmlDocument::parse(FilePath::knownPluginList); 
  if (xml) {
    kpl.recreateFromXml(*xml.get());
  }
}

static void saveKnownPluginList(juce::KnownPluginList& kpl) {
  auto xml = kpl.createXml();
  if (xml) {
    xml->writeTo(FilePath::knownPluginList);
  }
}

inline double secondsToPpq(double bpm, double seconds) {
  // TODO(luca): this is incorrect
  return 60.0 * seconds / bpm;
}

inline bool neqf32(f32 a, f32 b) {
  return std::abs(a - b) > EPSILON;
}

inline bool isNormalised(f32 v) {
  return v >= 0.0 && v <= 1.0;
}

inline bool rangeIncl(f32 v, f32 lo, f32 hi) {
  return v >= lo && v <= hi;
}

static std::random_device gRandDevice;
static std::mt19937 gRandGen { gRandDevice() };
static std::normal_distribution<f32> gRand { 0.0, 1.0 };
static std::atomic<u32> gUniqueCounter = 0;

inline f32 random(f32 randomSpread) {
  f32 v = gRand(gRandGen) / randomSpread; // NOTE(luca): parameter that tightens random distribution
  v = std::clamp(v, -1.f, 1.f);
  v = (v * 0.5f) + 0.5f;
  return v;
}

} // namespace atmt
