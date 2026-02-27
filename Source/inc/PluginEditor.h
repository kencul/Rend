#pragma once

#include "PluginProcessor.h"
#include <juce_gui_extra/juce_gui_extra.h>

//==============================================================================
class AudioPluginAudioProcessorEditor final : public juce::AudioProcessorEditor {
public:
	explicit AudioPluginAudioProcessorEditor(AudioPluginAudioProcessor &);
	~AudioPluginAudioProcessorEditor() override;

	//==============================================================================
	void paint(juce::Graphics &) override;
	void resized() override;

	void updateCurveInUI(const juce::String &curveData);

private:
	// This reference is provided as a quick way for your editor to
	// access the processor object that created it.
	AudioPluginAudioProcessor &processorRef;

	juce::WebSliderRelay gainRelay;
	juce::WebSliderRelay driveRelay;
	juce::WebBrowserComponent webView;
	juce::WebSliderParameterAttachment gainAttachment;
	juce::WebSliderParameterAttachment driveAttachment;

	using Resource = juce::WebBrowserComponent::Resource;
	std::optional<Resource> getResource(const juce::String &url) const;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPluginAudioProcessorEditor)
};