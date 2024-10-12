# Compatibility

## DAWs

- Ableton: full (look under Audio FX)
- Logic: full (load as AU MIDI-controlled effect for hosting instruments)
- Bitwig: full (look under Audio FX)

## Plugins

### Vital 1.5.5 VST3 MacOS Ableton

- Wavetable isn't a parameter 

- UI doesn't respond to discrete parameter changes (including Ableton's modulation)
- Voices don't respond to discrete changes until voices are reset

### Native Instruments - Raum

- JUCE Assertion is triggered when setting a discrete value which causes a plug-in reset on the audio thread

### U-He

- Everything U-He causes JUCE leak detector to go off when exiting the application
