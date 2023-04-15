class AudioProcessor extends AudioWorkletProcessor {
    private time: number;

    static get parameterDescriptors() {
        return [
            {
                name: "freq",
                defaultValue: 440,
                minValue: 0,
                maxValue: 100000,
                automationRate: "a-rate"
            }
        ]
    }

    constructor() {
        super();
        this.time = 0;
    }

    process(inputs: Float32Array[][], outputs: Float32Array[][], parameters: Record<string, Float32Array>): boolean {
        const output = outputs[0];

        for (let i = 0; i < output[0].length; i++) {
            let val = Math.sin(this.time) * 0.5;

            output[0][i] = val;
            output[1][i] = val;

            const freq = (parameters.freq.length > 1 ? parameters.freq[i] : parameters.freq[0]);
            this.time += (freq * Math.PI * 2) / sampleRate;
        }

        return true;
    }
}

registerProcessor("audio-processor", AudioProcessor);