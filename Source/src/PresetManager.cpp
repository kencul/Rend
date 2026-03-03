#include "PresetManager.h"

PresetManager::PresetManager(juce::AudioProcessorValueTreeState &vts) : valueTreeState(vts) {
	// Create the directory if it doesn't exist
	const auto presetDir = getPresetDirectory();
	if (!presetDir.exists()) presetDir.createDirectory();
}

void PresetManager::savePreset(const juce::String &presetName) {
	const auto file = getPresetDirectory().getChildFile(presetName + extension);

	// Get the underlying ValueTree (includes params + custom properties)
	auto state = valueTreeState.copyState();
	std::unique_ptr<juce::XmlElement> xml(state.createXml());

	// Write to disk
	xml->writeTo(file);
}

void PresetManager::loadPreset(const juce::String &presetName) {
	const auto file = getPresetDirectory().getChildFile(presetName + extension);

	if (file.existsAsFile()) {
		std::unique_ptr<juce::XmlElement> xml(juce::XmlDocument::parse(file));

		if (xml != nullptr) { valueTreeState.replaceState(juce::ValueTree::fromXml(*xml)); }
	}
}

juce::StringArray PresetManager::getAllPresets() const {
	juce::StringArray presets;
	const auto fileArray = getPresetDirectory().findChildFiles(
	    juce::File::TypesOfFileToFind::findFiles, false, "*" + extension);

	for (const auto &f : fileArray) presets.add(f.getFileNameWithoutExtension());

	return presets;
}

juce::File PresetManager::getPresetDirectory() const {
	// ~/Library/Application Support/ (macOS) or %AppData% (Windows)
	auto root = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);

	return root.getChildFile(JUCE_COMPANY_NAME)
	    .getChildFile(JUCE_PRODUCT_NAME)
	    .getChildFile("Presets");
}

void PresetManager::deletePreset(const juce::String &presetName) {
	const auto file = getPresetDirectory().getChildFile(presetName + extension);

	// Prevent attempting to delete missing files or directories
	if (file.existsAsFile()) file.deleteFile();
}