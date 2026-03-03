#include "PluginEditor.h"
#include "PluginProcessor.h"
#include <BinaryData.h>

const AudioPluginAudioProcessorEditor::ResourceMap AudioPluginAudioProcessorEditor::resourceMap = {
    {"index.html", {BinaryData::index_html, BinaryData::index_htmlSize}},
    {"main.js", {BinaryData::main_js, BinaryData::main_jsSize}},
    {"index.js", {BinaryData::index_js, BinaryData::index_jsSize}},
    {"check_native_interop.js",
     {BinaryData::check_native_interop_js, BinaryData::check_native_interop_jsSize}},
    {"knob.js", {BinaryData::knob_js, BinaryData::knob_jsSize}},
};

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
            .withEventListener(
                "webViewReady",
                [this](const juce::var &) {
	                auto curve = processorRef.apvts.state.getProperty(IDs::curvePoints).toString();
	                if (curve.isNotEmpty()) updateCurveInUI(curve);
                })

            .withInitialisationData(
                "initialCurveString",
                processorRef.apvts.state.getProperty(IDs::curvePoints).toString())

            .withNativeFunction("getPresetList",
                                [this](const juce::Array<juce::var> &args, auto completion) {
	                                juce::ignoreUnused(
	                                    args); // No arguments expected for this function
	                                // Get array from Manager
	                                auto presets = processorRef.presetManager->getAllPresets();

	                                // Convert StringArray to a JS-compatible var array
	                                juce::var presetJsArray;
	                                for (auto &p : presets) presetJsArray.append(p);

	                                // Send back to JS
	                                completion(presetJsArray);
                                })
            .withNativeFunction("loadPreset",
                                [this](const juce::Array<juce::var> &args, auto completion) {
	                                if (args.size() > 0 && args[0].isString()) {
		                                juce::String name = args[0].toString();
		                                processorRef.presetManager->loadPreset(name);
	                                }
	                                completion({}); // completion must be called even if void
                                })
            .withNativeFunction("savePreset",
                                [this](const juce::Array<juce::var> &args, auto completion) {
	                                if (args.size() > 0 && args[0].isString()) {
		                                juce::String name = args[0].toString();
		                                processorRef.presetManager->savePreset(name);

		                                // Return the updated list immediately so UI refreshes
		                                auto presets = processorRef.presetManager->getAllPresets();
		                                juce::var presetJsArray;
		                                for (auto &p : presets) presetJsArray.append(p);
		                                completion(presetJsArray);
	                                } else {
		                                completion({});
	                                }
                                })
            .withNativeFunction("deletePreset",
                                [this](const juce::Array<juce::var> &args, auto completion) {
	                                if (args.size() > 0 && args[0].isString()) {
		                                processorRef.presetManager->deletePreset(
		                                    args[0].toString());
	                                }

	                                completion({});
                                })),
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
	juce::String fileName = url == "/" ? "index.html" : url.fromLastOccurrenceOf("/", false, false);
	juce::String extension = fileName.fromLastOccurrenceOf(".", false, false);

	// Silently ignore requests we don't handle (favicon, browser internals, etc.)
	if (extension.isEmpty()) return std::nullopt;

	const char *mimeType = getMimeForExtension(extension);

	auto it = resourceMap.find(fileName);
	if (it == resourceMap.end()) return std::nullopt;

	auto [data, size] = it->second;
	return Resource {std::vector<std::byte>(reinterpret_cast<const std::byte *>(data),
	                                        reinterpret_cast<const std::byte *>(data) + size),
	                 mimeType};
}

void AudioPluginAudioProcessorEditor::updateCurveInUI(const juce::String &curveData) {
	juce::DynamicObject::Ptr data = new juce::DynamicObject();
	data->setProperty("payload", curveData);

	webView.emitEventIfBrowserIsVisible("loadCurveFromBackend", data.get());
}