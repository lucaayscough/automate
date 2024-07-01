#include "utils.hpp"

namespace atmt {

using File = juce::File;

const File Path::data { File::getSpecialLocation(File::userApplicationDataDirectory).getChildFile("Automate") };
const File Path::knownPluginList { data.getChildFile("KnownPluginList.txt") };

void Path::init() {
  jassert(data.hasWriteAccess());
  jassert(knownPluginList.hasWriteAccess());
  data.createDirectory();  
  knownPluginList.create();
}

void loadKnownPluginList(juce::KnownPluginList& kpl) {
  auto xml = juce::XmlDocument::parse(Path::knownPluginList); 
  kpl.recreateFromXml(*xml.get());
}

void saveKnownPluginList(juce::KnownPluginList& kpl) {
  auto xml = kpl.createXml();
  xml->writeTo(Path::knownPluginList);
}

} // namespace atmt
