#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
AudioPluginAudioProcessor::AudioPluginAudioProcessor() :
    AudioProcessor(BusesProperties()
#if !JucePlugin_IsMidiEffect
	#if !JucePlugin_IsSynth
                       .withInput("Input", juce::AudioChannelSet::stereo(), true)
	#endif
                       .withOutput("Output", juce::AudioChannelSet::stereo(), true)
#endif
                       ),
    apvts(*this, nullptr, "Parameters", createParameterLayout()) {

	if (!apvts.state.hasProperty(IDs::curvePoints)) {
		apvts.state.setProperty(
		    IDs::curvePoints, "-1.0,-1.0, -0.5,-0.5, 0.0,0.0, 0.5,0.5, 1.0,1.0", nullptr);
	}

	apvts.state.addListener(this);

	presetManager = std::make_unique<PresetManager>(apvts);
}

AudioPluginAudioProcessor::~AudioPluginAudioProcessor() {}

//==============================================================================
const juce::String AudioPluginAudioProcessor::getName() const { return JucePlugin_Name; }

bool AudioPluginAudioProcessor::acceptsMidi() const {
#if JucePlugin_WantsMidiInput
	return true;
#else
	return false;
#endif
}

bool AudioPluginAudioProcessor::producesMidi() const {
#if JucePlugin_ProducesMidiOutput
	return true;
#else
	return false;
#endif
}

bool AudioPluginAudioProcessor::isMidiEffect() const {
#if JucePlugin_IsMidiEffect
	return true;
#else
	return false;
#endif
}

double AudioPluginAudioProcessor::getTailLengthSeconds() const { return 0.0; }

int AudioPluginAudioProcessor::getNumPrograms() {
	return 1; // NB: some hosts don't cope very well if you tell them there are
	          // 0 programs, so this should be at least 1, even if you're not
	          // really implementing programs.
}

int AudioPluginAudioProcessor::getCurrentProgram() { return 0; }

void AudioPluginAudioProcessor::setCurrentProgram(int index) { juce::ignoreUnused(index); }

const juce::String AudioPluginAudioProcessor::getProgramName(int index) {
	juce::ignoreUnused(index);
	return {};
}

void AudioPluginAudioProcessor::changeProgramName(int index, const juce::String &newName) {
	juce::ignoreUnused(index, newName);
}

//==============================================================================
void AudioPluginAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
	// Use this method as the place to do any pre-playback
	// initialisation that you need..
	juce::ignoreUnused(sampleRate, samplesPerBlock);
	dspUnit.prepare(sampleRate, samplesPerBlock, getTotalNumInputChannels());
	juce::String currentCurve = apvts.state.getProperty(IDs::curvePoints).toString();

	// Safety check: if for some reason the property is empty, use default
	if (currentCurve.isEmpty()) { currentCurve = "-1.0,-1.0,-0.5,-0.5,0.0,0.0,0.5,0.5,1.0,1.0"; }

	dspUnit.updateCurve(currentCurve);

	// Tell the DAW about the oversampling filter's latency
	setLatencySamples(static_cast<int>(dspUnit.getLatencySamples()));
}

void AudioPluginAudioProcessor::releaseResources() {
	// When playback stops, you can use this as an opportunity to free up any
	// spare memory, etc.
}

bool AudioPluginAudioProcessor::isBusesLayoutSupported(const BusesLayout &layouts) const {
#if JucePlugin_IsMidiEffect
	juce::ignoreUnused(layouts);
	return true;
#else
	// This is the place where you check if the layout is supported.
	// In this template code we only support mono or stereo.
	// Some plugin hosts, such as certain GarageBand versions, will only
	// load plugins that support stereo bus layouts.
	if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
	    && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
		return false;

	// This checks if the input layout matches the output layout
	#if !JucePlugin_IsSynth
	if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet()) return false;
	#endif

	return true;
#endif
}

void AudioPluginAudioProcessor::processBlock(juce::AudioBuffer<float> &buffer,
                                             juce::MidiBuffer &midiMessages) {
	juce::ignoreUnused(midiMessages);

	juce::ScopedNoDenormals noDenormals;
	auto totalNumInputChannels  = getTotalNumInputChannels();
	auto totalNumOutputChannels = getTotalNumOutputChannels();

	for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
		buffer.clear(i, 0, buffer.getNumSamples());

	// Get the gain parameter value from APVTS
	float gainValue  = apvts.getRawParameterValue(IDs::gain.getParamID())->load();
	float driveValue = apvts.getRawParameterValue(IDs::drive.getParamID())->load();
	dspUnit.setGain(gainValue);
	dspUnit.setDrive(driveValue);

	// swaps curve buffer if message thread wrote new data with atomic
	dspUnit.swapIfPending();
	dspUnit.process(buffer);
}

//==============================================================================
bool AudioPluginAudioProcessor::hasEditor() const {
	return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor *AudioPluginAudioProcessor::createEditor() {
	return new AudioPluginAudioProcessorEditor(*this);
}

//==============================================================================
void AudioPluginAudioProcessor::getStateInformation(juce::MemoryBlock &destData) {
	// Save the APVTS state to XML and copy it to the memory block
	auto state = apvts.copyState();
	std::unique_ptr<juce::XmlElement> xml(state.createXml());
	copyXmlToBinary(*xml, destData);
}

void AudioPluginAudioProcessor::setStateInformation(const void *data, int sizeInBytes) {
	// Restore the APVTS state from the memory block
	std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));

	if (xmlState.get() != nullptr)
		if (xmlState->hasTagName(apvts.state.getType()))
			apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout
AudioPluginAudioProcessor::createParameterLayout() {
	juce::AudioProcessorValueTreeState::ParameterLayout layout;

	auto attributes =
	    juce::AudioParameterFloatAttributes()
	        .withStringFromValueFunction([](auto v, int) { return juce::String(v, 2); })
	        .withValueFromStringFunction([](auto t) { return t.getFloatValue(); });

	// Add gain parameter: range from 0.0 to 1.0, default 0.5 (half gain)
	layout.add(std::make_unique<juce::AudioParameterFloat>(
	    IDs::gain, "Gain", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f, attributes));

	layout.add(std::make_unique<juce::AudioParameterFloat>(
	    IDs::drive, "Drive", juce::NormalisableRange<float>(0.0f, 10.0f, 0.01f), 0.5f, attributes));

	return layout;
}

void AudioPluginAudioProcessor::valueTreePropertyChanged(
    juce::ValueTree &treeWhosePropertyHasChanged, const juce::Identifier &property) {
	if (treeWhosePropertyHasChanged == apvts.state) {
		if (property == IDs::curvePoints) {
			auto curvePointsString = apvts.state.getProperty(IDs::curvePoints).toString();
			dspUnit.updateCurve(curvePointsString);
		}
	}
}

// When preset is loaded
void AudioPluginAudioProcessor::valueTreeRedirected(juce::ValueTree &treeWhichHasBeenChanged) {
	juce::String newCurve = treeWhichHasBeenChanged.getProperty(IDs::curvePoints).toString();

	if (newCurve.isEmpty()) return;

	dspUnit.updateCurve(newCurve);

	// Use callAsync because this callback might happen on a background thread,
	// but UI updates must happen on the Message Thread.
	juce::MessageManager::callAsync([this, newCurve]() {
		// Check if the editor is actually open
		if (auto *editor = dynamic_cast<AudioPluginAudioProcessorEditor *>(getActiveEditor())) {
			editor->updateCurveInUI(newCurve);
		}
	});
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter() { return new AudioPluginAudioProcessor(); }
