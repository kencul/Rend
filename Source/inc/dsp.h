#pragma once
#include <juce_dsp/juce_dsp.h>

class DSP {
public:
	DSP() = default;

	void prepare(double sampleRate, int samplesPerBlock, int numChannels) {
		const double rampSeconds = 0.02;
		_gain.reset(sampleRate, rampSeconds);
		_drive.reset(sampleRate, rampSeconds);

		oversampling = std::make_unique<juce::dsp::Oversampling<float>>(
		    numChannels,
		    3,
		    juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
		    true, // max quality
		    true  // integer latency for DAW compensation
		);

		oversampling->initProcessing(static_cast<size_t>(samplesPerBlock));
		oversampling->reset();

		// One filter instance per channel as IIR filters are stateful
		dcBlockers.clear();
		dcBlockers.resize(numChannels);
		auto coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, 20.0f);
		for (auto &filter : dcBlockers) filter.coefficients = coefficients;
	}

	void swapIfPending() {
		if (pendingCurveDirty.exchange(false)) { activeControlPoints = pendingControlPoints; }
	}

	void process(juce::AudioBuffer<float> &buffer) {
		juce::dsp::AudioBlock<float> inputBlock(buffer);
		// returns a block at 2x sample rate
		auto oversampledBlock = oversampling->processSamplesUp(inputBlock);

		const auto osNumSamples  = oversampledBlock.getNumSamples();
		const auto osNumChannels = oversampledBlock.getNumChannels();

		for (size_t i = 0; i < osNumSamples; ++i) {
			// Advance smoothers at the oversampled rate
			const float gain  = _gain.getNextValue();
			const float drive = _drive.getNextValue();

			for (size_t ch = 0; ch < osNumChannels; ++ch) {
				auto *data = oversampledBlock.getChannelPointer(ch);
				data[i]    = gain * interpolate(data[i] * drive);
			}
		}

		// Downsample back to original rate
		oversampling->processSamplesDown(inputBlock);

		// Remove DC offset
		for (int ch = 0; ch < buffer.getNumChannels(); ++ch) {
			auto *data = buffer.getWritePointer(ch);
			for (int i = 0; i < buffer.getNumSamples(); ++i)
				data[i] = dcBlockers[ch].processSample(data[i]);
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

	// Returns latency in samples for DAW compensation
	float getLatencySamples() const {
		return oversampling ? oversampling->getLatencyInSamples() : 0.0f;
	}

private:
	// Initialize with defaults to avoid garbage values
	juce::SmoothedValue<float> _gain {1.0f};
	juce::SmoothedValue<float> _drive {1.0f};

	std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
	std::vector<juce::dsp::IIR::Filter<float>> dcBlockers;

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