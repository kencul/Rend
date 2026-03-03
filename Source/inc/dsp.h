#pragma once
#include <juce_dsp/juce_dsp.h>

class DSP {
public:
	DSP() = default;

	void prepare(double sampleRate, int samplesPerBlock) {
		juce::ignoreUnused(sampleRate, samplesPerBlock);

		const double rampSeconds = 0.02;
		_gain.reset(sampleRate, rampSeconds);
		_drive.reset(sampleRate, rampSeconds);
	}

	void swapIfPending() {
		if (pendingCurveDirty.exchange(false)) { activeControlPoints = pendingControlPoints; }
	}

	void process(juce::AudioBuffer<float> &buffer) {
		const auto numChannels = buffer.getNumChannels();
		const auto numSamples  = buffer.getNumSamples();

		for (int i = 0; i < numSamples; ++i) {
			// Advance both smoothers once per sample, shared across channels
			const float gain  = _gain.getNextValue();
			const float drive = _drive.getNextValue();

			for (int channel = 0; channel < numChannels; ++channel) {
				auto *data = buffer.getWritePointer(channel);
				data[i]    = gain * interpolate(data[i] * drive);
			}
		}
	}

	void setGain(float gain) { _gain.setTargetValue(gain); }
	void setDrive(float drive) { _drive.setTargetValue(drive); }

	void updateCurve(const juce::String &pointString) {
		auto newPoints = parseString(pointString);
		std::sort(
		    newPoints.begin(), newPoints.end(), [](auto &a, auto &b) { return a.first < b.first; });

		pendingControlPoints = std::move(newPoints);
		pendingCurveDirty.store(true);
	}

private:
	// Initialize with defaults to avoid garbage values
	juce::SmoothedValue<float> _gain {1.0f};
	juce::SmoothedValue<float> _drive {1.0f};

	using PointArray = std::vector<std::pair<float, float>>;

	static PointArray makeDefaultPoints() {
		return {{-1.0f, -1.0f}, {-0.5f, -0.5f}, {0.0f, 0.0f}, {0.5f, 0.5f}, {1.0f, 1.0f}};
	}

	PointArray activeControlPoints {makeDefaultPoints()};
	PointArray pendingControlPoints {makeDefaultPoints()};
	std::atomic<bool> pendingCurveDirty {false};

	float interpolate(float x) const {
		if (activeControlPoints.size() < 2) return x;
		// Ensure x is within the bounds of your control points
		x = juce::jlimit(activeControlPoints.front().first, activeControlPoints.back().first, x);

		for (size_t i = 0; i < activeControlPoints.size() - 1; ++i) {
			const auto &p1 = activeControlPoints[i];
			const auto &p2 = activeControlPoints[i + 1];

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

	static PointArray parseString(const juce::String &s) {
		juce::StringArray tokens;
		tokens.addTokens(s.trim(), ",", "");

		// Drop trailing empty token from a trailing comma
		if (!tokens.isEmpty() && tokens.strings.getLast().trim().isEmpty())
			tokens.strings.removeLast();

		PointArray pts;
		pts.reserve(tokens.size() / 2);

		for (int i = 0; i + 1 < tokens.size(); i += 2)
			pts.push_back({tokens[i].getFloatValue(), tokens[i + 1].getFloatValue()});

		return pts;
	}
};