import * as Juce from "./juce/index.js";

console.log("JUCE frontend loaded");

// Global state
let controlPoints =
    [ {x : -1, y : -1}, {x : -0.5, y : -0.5}, {x : 0, y : 0}, {x : 0.5, y : 0.5}, {x : 1, y : 1} ];

/**
 * Parses a comma-separated string or JUCE event object to update controlPoints.
 * @returns {boolean} True if update was successful.
 */
function updateControlPointsFromString(inputData) {
	if (!inputData) return false;

	let pointsString = "";

	//  Handle JUCE event objects which wrap data in 'payload'
	if (Array.isArray(inputData)) {
		// JUCE sometimes wraps the string in a single-element array
		pointsString = inputData.length > 0 ? String(inputData[0]) : "";
	} else if (typeof inputData === 'object') {
		pointsString = inputData.payload || inputData.value || "";
	} else {
		pointsString = String(inputData);
	}

	if (!pointsString.includes(',')) return false;

	const tokens = pointsString.split(',');
	if (tokens.length !== controlPoints.length * 2) return false;

	for (let i = 0; i < controlPoints.length; i++) {
		controlPoints[i].x = parseFloat(tokens[i * 2]);
		controlPoints[i].y = parseFloat(tokens[i * 2 + 1]);
	}

	return true;
}

// Attempt to load initial state injected by C++ constructor
const initData = window.__JUCE__.initialisationData;
console.log(initData.initialCurveString);
if (initData && initData.initialCurveString) {
	if (updateControlPointsFromString(initData.initialCurveString)) {
		console.log("Loaded initial curve from constructor data.");
	}
}

document.addEventListener("DOMContentLoaded", () => {
	const gainSlider     = document.getElementById("gainSlider");
	const sliderState    = Juce.getSliderState("gain");
	const gainValueLabel = document.getElementById("gainLabel");

	// Enforce 0-1 range on the HTML slider to match normalised values
	gainSlider.min = 0;
	gainSlider.max = 1;

	// Helper to update gain UI
	const updateGainUI = () => {
		const val        = sliderState.getNormalisedValue();
		gainSlider.value = val;

		// Use .start and .end from JUCE properties
		const start = sliderState.properties.start !== undefined ? sliderState.properties.start : 0;
		const end   = sliderState.properties.end !== undefined ? sliderState.properties.end : 1;

		const displayVal = start + (val * (end - start));
		if (gainValueLabel) gainValueLabel.innerText = displayVal.toFixed(2);
	};

	gainSlider.oninput = function() {
		sliderState.setNormalisedValue(this.value);
		updateGainUI();
	};
	gainSlider.step = 1 / sliderState.properties.numSteps;
	sliderState.valueChangedEvent.addListener(updateGainUI);

	const driveSlider      = document.getElementById("driveSlider");
	const driveSliderState = Juce.getSliderState("drive");
	const driveValueLabel  = document.getElementById("driveLabel");

	// Enforce 0-1 range on the HTML slider to match normalised values
	driveSlider.min = 0;
	driveSlider.max = 1;

	// Helper to update drive UI
	const updateDriveUI = () => {
		const val         = driveSliderState.getNormalisedValue();
		driveSlider.value = val;

		// Use .start and .end from JUCE properties
		const start =
		    driveSliderState.properties.start !== undefined ? driveSliderState.properties.start : 0;
		const end =
		    driveSliderState.properties.end !== undefined ? driveSliderState.properties.end : 3;

		const displayVal = start + (val * (end - start));
		if (driveValueLabel) driveValueLabel.innerText = displayVal.toFixed(2);
	};

	driveSlider.oninput = function() {
		driveSliderState.setNormalisedValue(this.value);
		updateDriveUI();
	};
	driveSlider.step = 1 / driveSliderState.properties.numSteps;
	driveSliderState.valueChangedEvent.addListener(updateDriveUI);

	// --- Debug Button ---
	const myButton = document.getElementById("button");
	if (myButton) {
		myButton.addEventListener("click", () => {
			window.__JUCE__.backend.emitEvent("debugMessageFromUI", {message : "Button clicked!"});
			console.log("Sent 'debugMessageFromUI' event to C++");
		});
	}

	// --- Curve Editor Setup ---
	const canvas = document.getElementById('curveEditorCanvas');
	if (!canvas) {
		console.error("Canvas with ID 'curveEditorCanvas' not found!");
		return;
	}
	const ctx = canvas.getContext('2d');

	const canvasWidth  = canvas.width;
	const canvasHeight = canvas.height;
	const pointRadius  = 6;

	let draggingPointIndex   = -1;
	let lastSentPointsString = "";

	// Coordinate Conversion
	function normalizedToCanvasX(normX) { return (normX + 1) * 0.5 * canvasWidth; }
	function normalizedToCanvasY(normY) { return (1 - (normY + 1) * 0.5) * canvasHeight; }
	function canvasToNormalizedX(canvasX) { return (canvasX / canvasWidth) * 2 - 1; }
	function canvasToNormalizedY(canvasY) { return (1 - (canvasY / canvasHeight) * 2); }

	function drawCurve() {
		ctx.clearRect(0, 0, canvasWidth, canvasHeight);

		// Draw grid
		ctx.strokeStyle = '#888';
		ctx.lineWidth   = 1;
		ctx.beginPath();
		ctx.moveTo(0, normalizedToCanvasY(0));
		ctx.lineTo(canvasWidth, normalizedToCanvasY(0));
		ctx.moveTo(normalizedToCanvasX(0), 0);
		ctx.lineTo(normalizedToCanvasX(0), canvasHeight);
		ctx.stroke();

		// Draw segments
		ctx.strokeStyle = '#00FFFF';
		ctx.lineWidth   = 2;
		ctx.beginPath();
		if (controlPoints.length > 0) {
			const firstPoint = controlPoints[0];
			ctx.moveTo(normalizedToCanvasX(firstPoint.x), normalizedToCanvasY(firstPoint.y));
			for (let i = 1; i < controlPoints.length; i++) {
				const point = controlPoints[i];
				ctx.lineTo(normalizedToCanvasX(point.x), normalizedToCanvasY(point.y));
			}
		}
		ctx.stroke();

		// Draw points
		ctx.fillStyle   = '#FF4500';
		ctx.strokeStyle = '#FFFFFF';
		ctx.lineWidth   = 1.5;
		controlPoints.forEach(point => {
			const px = normalizedToCanvasX(point.x);
			const py = normalizedToCanvasY(point.y);
			ctx.beginPath();
			ctx.arc(px, py, pointRadius, 0, Math.PI * 2);
			ctx.fill();
			ctx.stroke();
		});
	}

	// --- Live Update Logic ---
	let updateIntervalTimer;
	const updateIntervalDelay = 20;

	function startLiveUpdates() {
		if (!updateIntervalTimer) {
			updateIntervalTimer = setInterval(() => {
				const currentPointsString = serializeControlPoints();
				if (currentPointsString !== lastSentPointsString) {
					sendCurveToBackend(currentPointsString);
					lastSentPointsString = currentPointsString;
				}
			}, updateIntervalDelay);
		}
	}

	function stopLiveUpdates() {
		clearInterval(updateIntervalTimer);
		updateIntervalTimer = null;
	}

	// --- Interaction Handlers ---
	function handleMouseMove(e) {
		if (draggingPointIndex !== -1) {
			const rect   = canvas.getBoundingClientRect();
			const mouseX = e.clientX - rect.left;
			const mouseY = e.clientY - rect.top;

			let normX = canvasToNormalizedX(mouseX);
			let normY = canvasToNormalizedY(mouseY);

			// Clamp values
			normX = Math.max(-1, Math.min(1, normX));
			normY = Math.max(-1, Math.min(1, normY));

			// Enforce X-axis ordering
			if (draggingPointIndex === 0) {
				normX = -1;
			} else if (draggingPointIndex === controlPoints.length - 1) {
				normX = 1;
			} else {
				const prevX = controlPoints[draggingPointIndex - 1].x;
				const nextX = controlPoints[draggingPointIndex + 1].x;
				normX       = Math.max(prevX, Math.min(nextX, normX));
			}

			const currentPoint = controlPoints[draggingPointIndex];
			if (currentPoint.x !== normX || currentPoint.y !== normY) {
				currentPoint.x = normX;
				currentPoint.y = normY;
				drawCurve();
			}
		}
	}

	function handleMouseUp() {
		if (draggingPointIndex !== -1) {
			draggingPointIndex = -1;
			stopLiveUpdates();

			// Send final update if changed since last interval
			const currentPointsString = serializeControlPoints();
			if (currentPointsString !== lastSentPointsString) {
				sendCurveToBackend(currentPointsString);
				lastSentPointsString = currentPointsString;
			}

			document.removeEventListener('mousemove', handleMouseMove);
			document.removeEventListener('mouseup', handleMouseUp);
		}
	}

	canvas.addEventListener('mousedown', (e) => {
		const mouseX = e.offsetX;
		const mouseY = e.offsetY;

		for (let i = 0; i < controlPoints.length; i++) {
			const point = controlPoints[i];
			const px    = normalizedToCanvasX(point.x);
			const py    = normalizedToCanvasY(point.y);

			const dist = Math.sqrt(Math.pow(mouseX - px, 2) + Math.pow(mouseY - py, 2));
			if (dist < pointRadius + 2) {
				draggingPointIndex = i;
				startLiveUpdates();

				document.addEventListener('mousemove', handleMouseMove);
				document.addEventListener('mouseup', handleMouseUp);
				break;
			}
		}
	});

	// --- Backend Communication ---
	function serializeControlPoints() {
		return controlPoints.map(p => `${p.x.toFixed(4)},${p.y.toFixed(4)}`).join(',');
	}

	function sendCurveToBackend(pointsString) {
		window.__JUCE__.backend.emitEvent("controlPointsUpdate", {payload : pointsString});
	}

	// --- Runtime Event Listener ---
	// Handles updates from C++ (e.g., preset loading)
	window.__JUCE__.backend.addEventListener("loadCurveFromBackend", (messageFromBackend) => {
		if (updateControlPointsFromString(messageFromBackend)) {
			drawCurve();
			// Sync lastSentString to prevent echoing back to C++
			if (messageFromBackend && messageFromBackend.payload) {
				lastSentPointsString = messageFromBackend.payload;
			}
			console.log("Loaded curve data from C++.");
		} else {
			console.warn("Malformed curve string from C++.");
		}
	});

	// --- Initialization ---
	lastSentPointsString = serializeControlPoints();

	// Initial UI state
	drawCurve();
	updateGainUI();
	updateDriveUI();
});