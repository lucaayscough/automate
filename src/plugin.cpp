#include "plugin.hpp"
#include "editor.hpp"

#define DEFAULT_BUSES juce::AudioProcessor::BusesProperties() \
  .withInput("Input", juce::AudioChannelSet::stereo()) \
  .withOutput("Output", juce::AudioChannelSet::stereo())

namespace atmt {

PluginProcessor::PluginProcessor()
  : AudioProcessor(DEFAULT_BUSES) {
  Path::init();
  loadKnownPluginList(knownPluginList);
  apfm.addDefaultFormats();
}

PluginProcessor::~PluginProcessor() {
  saveKnownPluginList(knownPluginList);
}

void PluginProcessor::knownPluginListChangeCallback() {
  auto plugins = knownPluginList.getTypes();

  if (plugins.size() > 0) {
    auto id = plugins[0].createIdentifierString();
    pluginIDAttachment.setValue(id);
  } else {
    pluginIDAttachment.setValue("");
  }
}

void PluginProcessor::pluginIDChangeCallback(const juce::var& v) {
  suspendProcessing(true);
  juce::String errorMessage;
  auto id = v.toString();
  if (id != "") {
    auto description = knownPluginList.getTypeForIdentifierString(id);
    if (description) {
      auto instance = apfm.createPluginInstance(*description, getSampleRate(), getBlockSize(), errorMessage);
      engine.setPluginInstance(instance);
      prepareToPlay(getSampleRate(), getBlockSize());
    }
  } else {
    engine.kill();
  }
  suspendProcessing(false);
}

void PluginProcessor::prepareToPlay(double sampleRate, int blockSize) {
  JUCE_ASSERT_MESSAGE_THREAD
  jassert(sampleRate > 0 && blockSize > 0);
  engine.prepare(sampleRate, blockSize);
}

void PluginProcessor::releaseResources() {}

void PluginProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiBuffer) {
  juce::ScopedNoDenormals noDeNormals;

  for (auto i = getTotalNumInputChannels(); i < getTotalNumOutputChannels(); i++)
    buffer.clear(i, 0, buffer.getNumSamples());

  engine.process(buffer, midiBuffer);
}

// ================================================================================
#pragma region JUCE boilerplate

bool PluginProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
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

const juce::String PluginProcessor::getName() const { return JucePlugin_Name; }

bool PluginProcessor::acceptsMidi()  const { return false; }
bool PluginProcessor::producesMidi() const { return false; }
bool PluginProcessor::isMidiEffect() const { return false; }
bool PluginProcessor::hasEditor()    const { return true;  }

juce::AudioProcessorEditor* PluginProcessor::createEditor() {
  return new PluginEditor(*this);
}

void PluginProcessor::getStateInformation(juce::MemoryBlock& destData) {
  auto state = manager.getState();
  std::unique_ptr<juce::XmlElement> xml(state.createXml());
  copyXmlToBinary(*xml, destData);
}

void PluginProcessor::setStateInformation(const void* data, int sizeInBytes) {
  std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
  if (xmlState.get()) {
    if (xmlState->hasTagName(manager.state.getType())) {
      auto newState = juce::ValueTree::fromXml(*xmlState);
      manager.replace(newState);
    }
  }
}

double PluginProcessor::getTailLengthSeconds() const      { return 0.0; }
int PluginProcessor::getNumPrograms()                     { return 1;   }
int PluginProcessor::getCurrentProgram()                  { return 0;   }
void PluginProcessor::setCurrentProgram(int)              {}
const juce::String PluginProcessor::getProgramName(int)   { return {};  }
void PluginProcessor::changeProgramName(int, const juce::String&) {}

} // namespace atmt

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
  return new atmt::PluginProcessor();
}

#undef DEFAULT_BUSES
#pragma endregion
