#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class PluginListComponent : public juce::Component
{
public:
  PluginListComponent(juce::AudioPluginFormatManager &formatManager,
                      juce::KnownPluginList &listToRepresent,
                      const juce::File &deadMansPedalFile,
                      juce::PropertiesFile* propertiesToUse)
    : pluginList {formatManager, listToRepresent, deadMansPedalFile, propertiesToUse}
  {
    addAndMakeVisible(pluginList);
    addAndMakeVisible(scanVST3Button);
    addAndMakeVisible(scanAUButton);

    scanVST3Button.onClick = [this] () -> void {
      pluginList.scanFor(vst3Format);
    };

    scanAUButton.onClick = [this] () -> void {
      pluginList.scanFor(auFormat);
    };
  }

  void resized() override
  {
    auto r = getLocalBounds();
    pluginList.setBounds(r.removeFromTop(400));
    scanVST3Button.setBounds(r.removeFromLeft(getWidth() / 2));
    scanAUButton.setBounds(r);
  }

private:
  juce::PluginListComponent pluginList;

  juce::TextButton scanVST3Button {"Scan VST3"};
  juce::TextButton scanAUButton   {"Scan AU"};

  juce::VST3PluginFormat vst3Format;
  juce::AudioUnitPluginFormat auFormat;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginListComponent)
};
