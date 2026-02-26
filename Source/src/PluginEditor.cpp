#include "PluginEditor.h"
#include "PluginProcessor.h"
#include <BinaryData.h>

//==============================================================================
AudioPluginAudioProcessorEditor::AudioPluginAudioProcessorEditor(AudioPluginAudioProcessor &p) :
    AudioProcessorEditor(&p), processorRef(p), gainRelay {IDs::gain.getParamID()},
    webView(juce::WebBrowserComponent::Options {}
                .withBackend(juce::WebBrowserComponent::Options::Backend::webview2)
                .withWinWebView2Options(juce::WebBrowserComponent::Options::WinWebView2 {}.withUserDataFolder(
                    juce::File::getSpecialLocation(juce::File::SpecialLocationType::tempDirectory)))
                .withNativeIntegrationEnabled()
                .withResourceProvider([this](const juce::String &url) { return getResource(url); })
                .withOptionsFrom(gainRelay)),
    gainAttachment(*processorRef.apvts.getParameter(IDs::gain.getParamID()), gainRelay) {
	// // Set up the gain slider
	// gainSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
	// gainSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
	// addAndMakeVisible(gainSlider);

	// // Set up the gain label
	// gainLabel.setText("Gain", juce::dontSendNotification);
	// gainLabel.setJustificationType(juce::Justification::centred);
	// addAndMakeVisible(gainLabel);

	// // Attach the slider to the APVTS parameter
	// gainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
	//     processorRef.apvts, IDs::gain.getParamID(), gainSlider);

	addAndMakeVisible(webView);

	// Informs the browser to use the resource provider for the root page
	webView.goToURL(juce::WebBrowserComponent::getResourceProviderRoot());

	// Set the editor size
	setSize(400, 300);
}

AudioPluginAudioProcessorEditor::~AudioPluginAudioProcessorEditor() {}

//==============================================================================
void AudioPluginAudioProcessorEditor::paint(juce::Graphics &g) {
	juce::ignoreUnused(g);
	// Fill background
	// g.fillAll(juce::Colours::darkgrey);
}

void AudioPluginAudioProcessorEditor::resized() {
	webView.setBounds(getLocalBounds());
	// // Layout the gain control in the center of the window
	// auto bounds   = getLocalBounds();
	// auto knobArea = bounds.reduced(50);

	// // Position the label at the top
	// gainLabel.setBounds(knobArea.removeFromTop(30));

	// // Position the slider in the remaining space
	// gainSlider.setBounds(knobArea);
}

std::vector<std::byte> streamToVector(juce::InputStream &stream) {
	using namespace juce;
	const auto sizeInBytes = static_cast<size_t>(stream.getTotalLength());
	std::vector<std::byte> result(sizeInBytes);
	stream.setPosition(0);
	[[maybe_unused]] const auto bytesRead = stream.read(result.data(), result.size());
	jassert(bytesRead == static_cast<ssize_t>(sizeInBytes));
	return result;
}

static const char *getMimeForExtension(const juce::String &extension) {
	static const std::unordered_map<juce::String, const char *> mimeMap = {{{"htm"}, "text/html"},
	                                                                       {{"html"}, "text/html"},
	                                                                       {{"txt"}, "text/plain"},
	                                                                       {{"jpg"}, "image/jpeg"},
	                                                                       {{"jpeg"}, "image/jpeg"},
	                                                                       {{"svg"}, "image/svg+xml"},
	                                                                       {{"ico"}, "image/vnd.microsoft.icon"},
	                                                                       {{"json"}, "application/json"},
	                                                                       {{"png"}, "image/png"},
	                                                                       {{"css"}, "text/css"},
	                                                                       {{"map"}, "application/json"},
	                                                                       {{"js"}, "text/javascript"},
	                                                                       {{"woff2"}, "font/woff2"}};

	if (const auto it = mimeMap.find(extension.toLowerCase()); it != mimeMap.end()) return it->second;

	jassertfalse;
	return "";
}

auto AudioPluginAudioProcessorEditor::getResource(const juce::String &url) const -> std::optional<Resource> {
	std::cout << "ResourceProvider called with " << url << std::endl;

	static const auto resourceFileRoot = juce::File {R"(C:\Users\Ken\Desktop\JUCE\simpleGain\Source\ui)"};

	const auto resourceToRetrieve = url == "/" ? "index.html" : url.fromFirstOccurrenceOf("/", false, false);

	const auto resource = resourceFileRoot.getChildFile(resourceToRetrieve).createInputStream();

	if (resource) {
		const auto extension = resourceToRetrieve.fromLastOccurrenceOf(".", false, false);
		return Resource {streamToVector(*resource), getMimeForExtension(extension)};
	}

	return std::nullopt;
}