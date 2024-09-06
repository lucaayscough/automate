#include "plugin.hpp"
#include "editor.hpp"

#define DEFAULT_BUSES juce::AudioProcessor::BusesProperties() \
  .withInput("Input", juce::AudioChannelSet::stereo()) \
  .withOutput("Output", juce::AudioChannelSet::stereo()) \
  .withInput("Sidechain", juce::AudioChannelSet::stereo())

namespace atmt {

Plugin::Plugin()
  : AudioProcessor(DEFAULT_BUSES) {
  FilePath::init();
  loadKnownPluginList(knownPluginList);
  apfm.addDefaultFormats();
  manager.init();
}

Plugin::~Plugin() {
  saveKnownPluginList(knownPluginList);
}

void Plugin::prepareToPlay(double sampleRate, int blockSize) {
  JUCE_ASSERT_MESSAGE_THREAD
  jassert(sampleRate > 0 && blockSize > 0);
  engine.prepare(sampleRate, blockSize);
}

void Plugin::releaseResources() {}

void Plugin::processBlock(juce::AudioBuffer<f32>& buffer, juce::MidiBuffer& midiBuffer) {
  juce::ScopedNoDenormals noDeNormals;

  for (auto i = getTotalNumInputChannels(); i < getTotalNumOutputChannels(); i++)
    buffer.clear(i, 0, buffer.getNumSamples());

  auto playhead = getPlayHead();
  if (playhead) {
    uiBridge.controlPlayhead = playhead->canControlTransport();

    // TODO(luca): do time conversion if necessary and ensure that all necessary information
    // including bpm etc is handled in hosts that do not provide them
    auto position = playhead->getPosition();
    if (position.hasValue()) {
      auto ppq = position->getPpqPosition();
      auto sec = position->getTimeInSeconds();
      if (ppq.hasValue()) {
        uiBridge.playheadPosition.store(*ppq);
      } else {
        // TODO(luca): we need a way of converting seconds to ppq
        uiBridge.playheadPosition.store(*sec);
      }

      auto bpm = position->getBpm();
      if (bpm.hasValue()) {
        uiBridge.bpm = *bpm; 
      }

      auto timeSignature = position->getTimeSignature();
      if (timeSignature.hasValue()) {
        uiBridge.numerator = u32(timeSignature->numerator);
        uiBridge.denominator = u32(timeSignature->denominator);
      }
    }
  }

  engine.process(buffer, midiBuffer);
}

// ================================================================================
#pragma region JUCE boilerplate

bool Plugin::isBusesLayoutSupported(const BusesLayout& layouts) const {
  auto outputLayout = layouts.getMainOutputChannelSet();

  std::vector<juce::AudioChannelSet> supportedLayouts = {
    juce::AudioChannelSet::stereo(),
  };

  for (auto& layout : supportedLayouts) {
    if (outputLayout == layout)
      return true;
  }

  return false;
}

const juce::String Plugin::getName() const { return JucePlugin_Name; }

bool Plugin::acceptsMidi()  const { return true; }
bool Plugin::producesMidi() const { return true; }
bool Plugin::isMidiEffect() const { return false; }
bool Plugin::hasEditor()    const { return true;  }

juce::AudioProcessorEditor* Plugin::createEditor() {
  return new Editor(*this);
}

void Plugin::getStateInformation(juce::MemoryBlock& mb) {
  std::unique_ptr<juce::XmlElement> xml(manager.getState().createXml());
  copyXmlToBinary(*xml, mb);
}

void Plugin::setStateInformation(const void* data, int size) {
  std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, size));
  if (xml.get() != nullptr) {
    auto tree = juce::ValueTree::fromXml(*xml);
    manager.replace(tree);
  }
}

f64   Plugin::getTailLengthSeconds() const      { return 0.0; }
int   Plugin::getNumPrograms()                  { return 1; }
int   Plugin::getCurrentProgram()               { return 0; }
void  Plugin::setCurrentProgram(int)            {}
const juce::String Plugin::getProgramName(int)  { return {}; }
void  Plugin::changeProgramName(int, const juce::String&) {}

} // namespace atmt

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
  return new atmt::Plugin();
}

#undef DEFAULT_BUSES
#pragma endregion
