#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

class PresetManager {
public:
	// Pass the APVTS by reference
	PresetManager(juce::AudioProcessorValueTreeState &apvts);

	void savePreset(const juce::String &presetName);
	void loadPreset(const juce::String &presetName);
	void deletePreset(const juce::String &presetName);

	// Helper to list available files for a UI ComboBox
	juce::StringArray getAllPresets() const;

private:
	juce::AudioProcessorValueTreeState &valueTreeState;
	const juce::String extension = ".preset";

	juce::File getPresetDirectory() const;
};