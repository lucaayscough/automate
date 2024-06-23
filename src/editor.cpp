#include "plugin.hpp"
#include "editor.hpp"

PluginEditor::PluginEditor(PluginProcessor& _proc) :
  AudioProcessorEditor(&_proc), proc(_proc), apvts(proc.apvts)
{
  addAndMakeVisible(descriptionBar);
  addAndMakeVisible(statesPanel);
  addAndMakeVisible(pluginList);

  proc.engine.instanceBroadcaster.addChangeListener(this);

  setSize(width, height);
  setResizable(true, true);
}

PluginEditor::~PluginEditor()
{
  proc.engine.instanceBroadcaster.removeChangeListener(this);
}

void PluginEditor::changeListenerCallback(juce::ChangeBroadcaster*)
{
  instanceUpdateCallback();
}

void PluginEditor::paint(juce::Graphics& g)
{
  g.fillAll(juce::Colours::black);
}

void PluginEditor::resized()
{
  auto r = getLocalBounds();
  statesPanel.setBounds(r.removeFromLeft(statesPanelWidth));
  pluginList.setBounds(r.removeFromRight(pluginListWidth));
  descriptionBar.setBounds(r.removeFromTop(descriptionBarHeight));

  if (instanceEditor)
  {
    instanceEditor->setBounds(r.removeFromTop(instanceEditor->getHeight()));
  }
}

void PluginEditor::instanceDescriptionUpdateCallback(juce::var v)
{
  auto description = proc.knownPluginList.getTypeForIdentifierString(v.toString());

  if (description)
  {
    descriptionBar.setDescription(description); 
  }

  statesPanel.reset();
}

void PluginEditor::instanceUpdateCallback()
{
  instanceEditor.reset(proc.engine.getEditor());
  if (instanceEditor)
  {
    addAndMakeVisible(instanceEditor.get());
    setSize(instanceEditor->getWidth()  + statesPanelWidth + pluginListWidth,
            instanceEditor->getHeight() + descriptionBarHeight);
  }
}
