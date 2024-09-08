#include "utils.hpp"

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

void loadKnownPluginList(juce::KnownPluginList& kpl) {
  auto xml = juce::XmlDocument::parse(FilePath::knownPluginList); 
  if (xml) {
    kpl.recreateFromXml(*xml.get());
  }
}

void saveKnownPluginList(juce::KnownPluginList& kpl) {
  auto xml = kpl.createXml();
  if (xml) {
    xml->writeTo(FilePath::knownPluginList);
  }
}

double secondsToPpq(double bpm, double seconds) {
  // TODO(luca): this is incorrect
  return 60.0 * seconds / bpm;
}

} // namespace atmt
