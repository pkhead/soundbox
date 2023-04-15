class Voice {
    public freq: number;
    public time: number;
    public volume: number;
    public key: number;

    constructor(key: number, freq: number, volume: number) {
        this.freq = freq;
        this.time = 0;
        this.volume = volume;
        this.key = key;
    }

    public compute(buf: Float32Array) {
        const val = Math.sin(this.time) * this.volume;

        buf[0] = val;
        buf[1] = val;

        this.time += (this.freq * 2 * Math.PI) / sampleRate;
    }
}

class AudioProcessor extends AudioWorkletProcessor {
    private time: number;
    private voices: Voice[];
    private voiceBuf: Float32Array; // buffer for voice output

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
        this.voices = [];
        this.voiceBuf = new Float32Array(2); 

        this.port.onmessage = (e) => {
            const data = e.data;

            switch (data.msg) {
                case "start":
                    this.voices.push(new Voice(data.key, data.freq, data.volume));
                    break;

                case "end":
                    for (let i = 0; i < this.voices.length; i++) {
                        if (this.voices[i].key === data.key) {
                            this.voices.splice(i, 1);
                            break;
                        }
                    }
                    break;
            }
        }
    }

    process(inputs: Float32Array[][], outputs: Float32Array[][], parameters: Record<string, Float32Array>): boolean {
        const output = outputs[0];

        for (let i = 0; i < output[0].length; i++) {
            let valL = 0;
            let valR = 0;

            for (let voice of this.voices) {
                voice.compute(this.voiceBuf);
                valL += this.voiceBuf[0];
                valR += this.voiceBuf[1];
            }

            output[0][i] = valL;
            output[1][i] = valR;
        }

        return true;
    }
}

registerProcessor("audio-processor", AudioProcessor);