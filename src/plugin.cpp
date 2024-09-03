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

void Plugin::signalPlay() {
  play = true;
}

void Plugin::signalRewind() {
  rewind = true;
}

void Plugin::signalStop() {
  stop = true;
}

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

bool Plugin::acceptsMidi()  const { return true; }
bool Plugin::producesMidi() const { return true; }
bool Plugin::isMidiEffect() const { return false; }
bool Plugin::hasEditor()    const { return true;  }

juce::AudioProcessorEditor* Plugin::createEditor() {
  return new Editor(*this);
}

void Plugin::getStateInformation(juce::MemoryBlock& mb) {
  juce::ValueTree tree("tree");

  tree.setProperty("zoom", zoom, nullptr)
      .setProperty("editMode", editMode.load(), nullptr)
      .setProperty("modulateDiscrete", modulateDiscrete.load(), nullptr)
      .setProperty("pluginID", pluginID, nullptr);

  juce::ValueTree presets("presets");

  for (auto& p : manager.presets) {
    juce::ValueTree preset("preset");
    preset.setProperty("name", p.name, nullptr)
          .setProperty("parameters", { p.parameters.data(), p.parameters.size() * sizeof(f32) }, nullptr);
    presets.appendChild(preset, nullptr);
  }

  juce::ValueTree clips("clips");
  for (auto& c : manager.clips) {
    juce::ValueTree clip("clip");
    clip.setProperty("x", c->x, nullptr)
        .setProperty("y", c->y, nullptr)
        .setProperty("c", c->c, nullptr)
        .setProperty("name", c->preset->name, nullptr);
    clips.appendChild(clip, nullptr);
  }

  juce::ValueTree paths("paths");
  for (auto& p : manager.paths) {
    juce::ValueTree path("path");
    path.setProperty("x", p->x, nullptr)
        .setProperty("y", p->y, nullptr)
        .setProperty("c", p->c, nullptr);
    paths.appendChild(path, nullptr);
  }

  tree.appendChild(presets, nullptr);
  tree.appendChild(clips, nullptr);
  tree.appendChild(paths, nullptr);

  DBG(manager.valueTreeToXmlString(tree));

  std::unique_ptr<juce::XmlElement> xml(tree.createXml());
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
