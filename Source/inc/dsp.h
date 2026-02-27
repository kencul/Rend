#pragma once
#include <juce_dsp/juce_dsp.h>

class DSP {
public:
	DSP() = default;

	void prepare(double sampleRate, int samplesPerBlock) {
		juce::ignoreUnused(sampleRate, samplesPerBlock);
	}

	void process(juce::AudioBuffer<float> &buffer) {
		const auto numChannels = buffer.getNumChannels();
		const auto numSamples  = buffer.getNumSamples();

		for (int channel = 0; channel < numChannels; ++channel) {
			auto *channelData = buffer.getWritePointer(channel);

			for (int i = 0; i < numSamples; ++i) {
				float input    = channelData[i] * _drive;
				channelData[i] = _gain * interpolate(input);
			}
		}
	}

	void setDrive(float gain) { _drive = gain; }
	void setGain(float gain) { _gain = gain; }

	void updateCurve(const juce::String &pointString) {
		auto newPoints = parseString(pointString);
		std::sort(
		    newPoints.begin(), newPoints.end(), [](auto &a, auto &b) { return a.first < b.first; });

		controlPoints = newPoints;
	}

private:
	// Initialize with defaults to avoid garbage values
	float _gain {1.0f};
	float _drive {1.0f};

	std::array<std::pair<float, float>, 5> controlPoints {std::pair<float, float> {-1.0f, -1.0f},
	                                                      std::pair<float, float> {-0.5f, -0.5f},
	                                                      std::pair<float, float> {0.0f, 0.0f},
	                                                      std::pair<float, float> {0.5f, 0.5f},
	                                                      std::pair<float, float> {1.0f, 1.0f}};

	float interpolate(float x) const {
		// Ensure x is within the bounds of your control points
		x = juce::jlimit(controlPoints.front().first, controlPoints.back().first, x);

		for (size_t i = 0; i < controlPoints.size() - 1; ++i) {
			const auto &p1 = controlPoints[i];
			const auto &p2 = controlPoints[i + 1];

			if (x >= p1.first && x <= p2.first) {
				float range = p2.first - p1.first;

				// Division by zero safety
				if (range <= 0.00001f) return p1.second;

				float t = (x - p1.first) / range;
				return p1.second + t * (p2.second - p1.second);
			}
		}
		return x;
	}

	std::array<std::pair<float, float>, 5> parseString(const juce::String &s) {
		std::array<std::pair<float, float>, 5> pts;
		juce::StringArray tokens;
		tokens.addTokens(s, ",", "");

		// Ensure we don't read out of bounds if the string is malformed
		for (int i = 0; i < 5 && (i * 2 + 1) < tokens.size(); ++i) {
			pts[i] = {tokens[i * 2].getFloatValue(), tokens[i * 2 + 1].getFloatValue()};
		}
		return pts;
	}
};