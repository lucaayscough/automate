#include "plugin.hpp"
#include "editor.hpp"

namespace atmt {

PluginEditor::PluginEditor(PluginProcessor& _proc)
  : AudioProcessorEditor(&_proc), proc(_proc), apvts(proc.apvts) {
  addAndMakeVisible(descriptionBar);
  addAndMakeVisible(statesPanel);
  addAndMakeVisible(pluginList);
  addAndMakeVisible(transport);

  setSize(width, height);
  setResizable(true, true);
  instanceChangeCallback();
}

void PluginEditor::paint(juce::Graphics& g) {
  g.fillAll(juce::Colours::black);
}

void PluginEditor::resized() {
  auto r = getLocalBounds();
  transport.setBounds(r.removeFromBottom(transportHeight));
  statesPanel.setBounds(r.removeFromLeft(statesPanelWidth));
  pluginList.setBounds(r.removeFromRight(pluginListWidth));
  descriptionBar.setBounds(r.removeFromTop(descriptionBarHeight));

  if (instanceEditor) {
    instanceEditor->setBounds(r.removeFromTop(instanceEditor->getHeight()));
  }
}

void PluginEditor::pluginIDChangeCallback(const juce::var& v) {
  auto description = proc.knownPluginList.getTypeForIdentifierString(v.toString());

  if (description) {
    descriptionBar.setDescription(description); 
  }

  statesPanel.reset();
}

void PluginEditor::instanceChangeCallback() {
  instanceEditor.reset(proc.engine.getEditor());
  if (instanceEditor) {
    addAndMakeVisible(instanceEditor.get());
    setSize(instanceEditor->getWidth()  + statesPanelWidth + pluginListWidth,
            instanceEditor->getHeight() + descriptionBarHeight + transportHeight);
  }
}

} // namespace atmt
