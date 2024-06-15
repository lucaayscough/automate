#include "plugin.hpp"
#include "editor.hpp"

#define DEFAULT_BUSES juce::AudioProcessor::BusesProperties() \
  .withInput("Input", juce::AudioChannelSet::stereo()) \
  .withOutput("Output", juce::AudioChannelSet::stereo())

PluginProcessor::PluginProcessor() :
  AudioProcessor(DEFAULT_BUSES) {}

PluginProcessor::~PluginProcessor() {}

void PluginProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
  juce::ignoreUnused(sampleRate, samplesPerBlock);
}

void PluginProcessor::releaseResources() {}

void PluginProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
  juce::ScopedNoDenormals noDeNormals;

  for (auto i = getTotalNumInputChannels(); i < getTotalNumOutputChannels(); i++)
    buffer.clear(i, 0, buffer.getNumSamples());
}

// ================================================================================
#pragma region JUCE boilerplate

bool PluginProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
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

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
  return new PluginEditor(*this);
}

void PluginProcessor::getStateInformation(juce::MemoryBlock&) {}
void PluginProcessor::setStateInformation(const void*, int) {}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
  return new PluginProcessor();
}

double PluginProcessor::getTailLengthSeconds() const
{
  return 0.0;
}

int PluginProcessor::getNumPrograms()
{
  return 1;
}

int PluginProcessor::getCurrentProgram()
{
  return 0;
}

void PluginProcessor::setCurrentProgram(int) {}

const juce::String PluginProcessor::getProgramName(int)
{
  return {};
}

void PluginProcessor::changeProgramName(int, const juce::String&) {}

#undef DEFAULT_BUSES
#pragma endregion
