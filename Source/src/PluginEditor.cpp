#include "PluginEditor.h"
#include "PluginProcessor.h"

//==============================================================================
AudioPluginAudioProcessorEditor::AudioPluginAudioProcessorEditor(AudioPluginAudioProcessor &p) :
    AudioProcessorEditor(&p), processorRef(p) {
	// Set up the gain slider
	gainSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
	gainSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
	addAndMakeVisible(gainSlider);

	// Set up the gain label
	gainLabel.setText("Gain", juce::dontSendNotification);
	gainLabel.setJustificationType(juce::Justification::centred);
	addAndMakeVisible(gainLabel);

	// Attach the slider to the APVTS parameter
	gainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
	    processorRef.apvts, IDs::gain.getParamID(), gainSlider);

	// Set the editor size
	setSize(400, 300);
}

AudioPluginAudioProcessorEditor::~AudioPluginAudioProcessorEditor() {}

//==============================================================================
void AudioPluginAudioProcessorEditor::paint(juce::Graphics &g) {
	// Fill background
	g.fillAll(juce::Colours::darkgrey);
}

void AudioPluginAudioProcessorEditor::resized() {
	// Layout the gain control in the center of the window
	auto bounds   = getLocalBounds();
	auto knobArea = bounds.reduced(50);

	// Position the label at the top
	gainLabel.setBounds(knobArea.removeFromTop(30));

	// Position the slider in the remaining space
	gainSlider.setBounds(knobArea);
}
