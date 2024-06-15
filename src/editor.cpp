#include "plugin.hpp"
#include "editor.hpp"

PluginEditor::PluginEditor(PluginProcessor& p) :
  AudioProcessorEditor(&p)
{
  setSize(400, 300);
}

PluginEditor::~PluginEditor()
{
}

void PluginEditor::paint(juce::Graphics& g)
{
  g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));

  g.setColour(juce::Colours::white);
  g.setFont(15.0f);
  g.drawFittedText("Hello World!", getLocalBounds(), juce::Justification::centred, 1);
}

void PluginEditor::resized()
{
}
