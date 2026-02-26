#pragma once
#include <juce_audio_basics/juce_audio_basics.h>

class DSP {
public:
	void prepare(double sampleRate) { currentSampleRate = sampleRate; }

	void process(juce::AudioBuffer<float> &buffer, float gain) {
		for (int channel = 0; channel < buffer.getNumChannels(); ++channel) {
			auto *channelData = buffer.getWritePointer(channel);
			for (int sample = 0; sample < buffer.getNumSamples(); ++sample) { channelData[sample] *= gain; }
		}
	}

private:
	double currentSampleRate = 0.0;
};