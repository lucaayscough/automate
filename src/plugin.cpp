#include "plugin.hpp"
#include "editor.hpp"

#define DEFAULT_BUSES juce::AudioProcessor::BusesProperties() \
  .withInput("Input", juce::AudioChannelSet::stereo()) \
  .withOutput("Output", juce::AudioChannelSet::stereo())

PluginProcessor::PluginProcessor()
  : AudioProcessor(DEFAULT_BUSES)
{
  apfm.addDefaultFormats();  
  knownPluginList.addChangeListener(this); 
}

PluginProcessor::~PluginProcessor()
{
  knownPluginList.removeChangeListener(this);
}

void PluginProcessor::changeListenerCallback(juce::ChangeBroadcaster*)
{
  juce::String errorMessage;
  auto plugins = knownPluginList.getTypes();

  if (plugins.size() > 0)
  {
    DBG("Loading plugin");

    suspendProcessing(true);
    pluginInstance = apfm.createPluginInstance(plugins[0],
                                               getSampleRate(),
                                               getBlockSize(),
                                               errorMessage);

    jassert(pluginInstance);

    pluginInstanceAttachment.setValue(plugins[0].createIdentifierString());
    suspendProcessing(false);
  }
  else
  {
    pluginInstance.reset();
    pluginInstanceAttachment.setValue({});
  }
}

void PluginProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
  if (pluginInstance)
  {
    pluginInstance->prepareToPlay(sampleRate, samplesPerBlock);
  }
}

void PluginProcessor::releaseResources() {}

void PluginProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiBuffer)
{
  juce::ScopedNoDenormals noDeNormals;

  for (auto i = getTotalNumInputChannels(); i < getTotalNumOutputChannels(); i++)
    buffer.clear(i, 0, buffer.getNumSamples());

  if (pluginInstance)
    pluginInstance->processBlock(buffer, midiBuffer);
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

void PluginProcessor::getStateInformation(juce::MemoryBlock& destData)
{
  auto state = apvts.copyState();
  std::unique_ptr<juce::XmlElement> xml(state.createXml());
  copyXmlToBinary(*xml, destData);
}

void PluginProcessor::setStateInformation(const void* data, int sizeInBytes)
{
  std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));

  if (xmlState.get())
  {
    if (xmlState->hasTagName(apvts.state.getType()))
    {
      //auto newState = juce::ValueTree::fromXml(*xmlState);
      //apvts.replaceState(newState);
    }
  }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
  return new PluginProcessor();
}

double PluginProcessor::getTailLengthSeconds() const      { return 0.0; }
int PluginProcessor::getNumPrograms()                     { return 1;   }
int PluginProcessor::getCurrentProgram()                  { return 0;   }
void PluginProcessor::setCurrentProgram(int)              {}
const juce::String PluginProcessor::getProgramName(int)   { return {};  }
void PluginProcessor::changeProgramName(int, const juce::String&) {}

#undef DEFAULT_BUSES
#pragma endregion
