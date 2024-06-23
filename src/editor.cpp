#include "plugin.hpp"
#include "editor.hpp"

PluginEditor::PluginEditor(PluginProcessor& _proc) :
  AudioProcessorEditor(&_proc), proc(_proc), apvts(proc.apvts)
{
  addAndMakeVisible(statesPanel);
  addAndMakeVisible(pluginList);
  addAndMakeVisible(saveStateButton);

  saveStateButton.onClick = [this] () -> void {
    proc.engine.saveParameterState();
    statesPanel.addState();
  };

  setSize(1000, 600);
  setResizable(true, true);
}

void PluginEditor::paint(juce::Graphics& g)
{
  g.fillAll(juce::Colours::white);
}

void PluginEditor::resized()
{
  auto r = getLocalBounds();
  statesPanel.setBounds(r.removeFromLeft(300));

  pluginList.setBounds(r.removeFromBottom(pluginListHeight));
  saveStateButton.setBounds(r.removeFromTop(40).removeFromLeft(80));

  if (pluginInstanceEditor)
  {
    pluginInstanceEditor->setBounds(r.removeFromTop(pluginInstanceEditor->getHeight()));
  }
}

