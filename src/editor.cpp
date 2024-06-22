#include "plugin.hpp"
#include "editor.hpp"

PluginEditor::PluginEditor(PluginProcessor& _proc) :
  AudioProcessorEditor(&_proc), proc(_proc), apvts(proc.apvts)
{
  addAndMakeVisible(pluginList);
  setSize(1000, 600);
  setResizable(true, true);
}

void PluginEditor::paint(juce::Graphics& g)
{
  g.fillAll(juce::Colours::black);
}

void PluginEditor::resized()
{
  auto r = getLocalBounds();
  if (pluginInstanceEditor)
  {
    pluginInstanceEditor->setBounds(r.removeFromTop(pluginInstanceEditor->getHeight()));
  }
  pluginList.setBounds(r.removeFromTop(pluginListHeight));
}

