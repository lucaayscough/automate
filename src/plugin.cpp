#include "plugin.hpp"
#include "editor.hpp"

#define DEFAULT_BUSES juce::AudioProcessor::BusesProperties() \
  .withInput("Input", juce::AudioChannelSet::stereo()) \
  .withOutput("Output", juce::AudioChannelSet::stereo())

namespace atmt {

Plugin::Plugin()
  : AudioProcessor(DEFAULT_BUSES) {
  FilePath::init();
  loadKnownPluginList(knownPluginList);
  apfm.addDefaultFormats();
}

Plugin::~Plugin() {
  saveKnownPluginList(knownPluginList);
}

void Plugin::pluginIDChangeCallback(const juce::var& v) {
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
    manager.clearEdit();
    manager.clearPresets();
  }

  undoManager.clearUndoHistory();
  suspendProcessing(false);
}

void Plugin::prepareToPlay(double sampleRate, int blockSize) {
  JUCE_ASSERT_MESSAGE_THREAD
  jassert(sampleRate > 0 && blockSize > 0);
  engine.prepare(sampleRate, blockSize);
}

void Plugin::releaseResources() {}

void Plugin::signalPlay() {
  play = true;
}

void Plugin::signalRewind() {
  rewind = true;
}

void Plugin::signalStop() {
  stop = true;
}

void Plugin::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiBuffer) {
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
      if (ppq.hasValue()) {
        uiBridge.playheadPosition.store(*ppq);
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

    if (play) {
      playhead->transportPlay(true);
      play = false;
    }
    if (stop) {
      playhead->transportPlay(false);
      stop = false;
    }
    if (rewind) {
      playhead->transportRewind();
      rewind = false;
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

bool Plugin::acceptsMidi()  const { return false; }
bool Plugin::producesMidi() const { return false; }
bool Plugin::isMidiEffect() const { return false; }
bool Plugin::hasEditor()    const { return true;  }

juce::AudioProcessorEditor* Plugin::createEditor() {
  return new Editor(*this);
}

void Plugin::getStateInformation(juce::MemoryBlock& destData) {
  auto state = manager.getState();
  std::unique_ptr<juce::XmlElement> xml(state.createXml());
  copyXmlToBinary(*xml, destData);
}

void Plugin::setStateInformation(const void* data, int sizeInBytes) {
  std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
  if (xmlState.get()) {
    if (xmlState->hasTagName(manager.state.getType())) {
      auto newState = juce::ValueTree::fromXml(*xmlState);
      manager.replace(newState);
    }
  }
}

double Plugin::getTailLengthSeconds() const      { return 0.0; }
int Plugin::getNumPrograms()                     { return 1;   }
int Plugin::getCurrentProgram()                  { return 0;   }
void Plugin::setCurrentProgram(int)              {}
const juce::String Plugin::getProgramName(int)   { return {};  }
void Plugin::changeProgramName(int, const juce::String&) {}

} // namespace atmt

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
  return new atmt::Plugin();
}

#undef DEFAULT_BUSES
#pragma endregion
