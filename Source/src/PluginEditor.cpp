#include "PluginEditor.h"
#include "PluginProcessor.h"
#include <BinaryData.h>

//==============================================================================
AudioPluginAudioProcessorEditor::AudioPluginAudioProcessorEditor(AudioPluginAudioProcessor &p) :
    AudioProcessorEditor(&p), processorRef(p), gainRelay {IDs::gain.getParamID()},
    driveRelay {IDs::drive.getParamID()},
    webView(
        juce::WebBrowserComponent::Options {}
            .withBackend(juce::WebBrowserComponent::Options::Backend::webview2)
            .withWinWebView2Options(
                juce::WebBrowserComponent::Options::WinWebView2 {}.withUserDataFolder(
                    juce::File::getSpecialLocation(juce::File::SpecialLocationType::tempDirectory)))
            .withNativeIntegrationEnabled()
            .withResourceProvider([this](const juce::String &url) { return getResource(url); })
            .withOptionsFrom(gainRelay)
            .withOptionsFrom(driveRelay)
            .withEventListener(
                "debugMessageFromUI",
                [this](const juce::var &message) {
	                juce::ignoreUnused(message);
	                std::cout << "Debug message from UI: "
	                          << processorRef.apvts.state.getProperty(IDs::curvePoints).toString()
	                          << std::endl;
                })
            .withEventListener(
                "controlPointsUpdate",
                [this](const juce::var &newCurvePoints) {
	                auto newCurvePointsString =
	                    newCurvePoints.getProperty("payload", juce::var("")).toString();
	                processorRef.apvts.state.setProperty(
	                    IDs::curvePoints, newCurvePointsString, nullptr);
                })
            .withInitialisationData(
                "initialCurveString",
                processorRef.apvts.state.getProperty(IDs::curvePoints).toString())),
    gainAttachment(*processorRef.apvts.getParameter(IDs::gain.getParamID()), gainRelay),
    driveAttachment(*processorRef.apvts.getParameter(IDs::drive.getParamID()), driveRelay) {
	addAndMakeVisible(webView);

	// Informs the browser to use the resource provider for the root page
	webView.goToURL(juce::WebBrowserComponent::getResourceProviderRoot());

	// Set the editor size
	setSize(800, 800);
}

AudioPluginAudioProcessorEditor::~AudioPluginAudioProcessorEditor() {}

//==============================================================================
void AudioPluginAudioProcessorEditor::paint(juce::Graphics &g) { juce::ignoreUnused(g); }

void AudioPluginAudioProcessorEditor::resized() { webView.setBounds(getLocalBounds()); }

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
	static const std::unordered_map<juce::String, const char *> mimeMap = {
	    {{"htm"}, "text/html"},
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

	if (const auto it = mimeMap.find(extension.toLowerCase()); it != mimeMap.end())
		return it->second;

	jassertfalse;
	return "";
}

auto AudioPluginAudioProcessorEditor::getResource(const juce::String &url) const
    -> std::optional<Resource> {
	std::cout << "ResourceProvider called with: " << url << std::endl;

	// Strip directory info (e.g., "juce/index.js" becomes "index.js")
	juce::String fileName = url == "/" ? "index.html" : url.fromLastOccurrenceOf("/", false, false);

	const juce::String extension = fileName.fromLastOccurrenceOf(".", false, false);
	const char *mimeType         = getMimeForExtension(extension);

	// Map the requested filename directly to your binary data arrays
	// Note: If CMake put these inside a namespace (e.g., MyAssets::index_html),
	// you will need to add the MyAssets:: prefix to the variables below.
	if (fileName == "index.html") {
		return Resource {std::vector<std::byte>((const std::byte *)BinaryData::index_html,
		                                        (const std::byte *)BinaryData::index_html
		                                            + BinaryData::index_htmlSize),
		                 mimeType};
	} else if (fileName == "main.js") {
		return Resource {std::vector<std::byte>((const std::byte *)BinaryData::main_js,
		                                        (const std::byte *)BinaryData::main_js
		                                            + BinaryData::main_jsSize),
		                 mimeType};
	} else if (fileName == "index.js") {
		return Resource {std::vector<std::byte>((const std::byte *)BinaryData::index_js,
		                                        (const std::byte *)BinaryData::index_js
		                                            + BinaryData::index_jsSize),
		                 mimeType};
	} else if (fileName == "check_native_interop.js") {
		return Resource {
		    std::vector<std::byte>((const std::byte *)BinaryData::check_native_interop_js,
		                           (const std::byte *)BinaryData::check_native_interop_js
		                               + BinaryData::check_native_interop_jsSize),
		    mimeType};
	}

	std::cout << "ResourceProvider error: File '" << fileName << "' not found in binaries."
	          << std::endl;
	return std::nullopt;
}

void AudioPluginAudioProcessorEditor::updateCurveInUI(const juce::String &curveData) {
	juce::DynamicObject::Ptr data = new juce::DynamicObject();
	data->setProperty("payload", curveData);

	webView.emitEventIfBrowserIsVisible("loadCurveFromBackend", data.get());
}