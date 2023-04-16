import { AudioModule, NoteEvent, NoteEventType } from "./audio";

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

    public compute(sampleRate: number, buf: Float32Array) {
        const val = Math.sin(this.time) * this.volume;

        buf[0] = val;
        buf[1] = val;

        this.time += (this.freq * 2 * Math.PI) / sampleRate;
    }
}

export class BasicSynthesizer extends AudioModule {
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
    }

    public event(event: NoteEvent) {
        switch (event.type) {
            case NoteEventType.NoteOn:
                const freq = 440 * 2 ** ((event.key - 69) / 12);

                this.voices.push(new Voice(event.key, freq, event.volume));
                break;

            case NoteEventType.NoteOff:
                for (let i = 0; i < this.voices.length; i++) {
                    if (this.voices[i].key === event.key) {
                        this.voices.splice(i, 1);
                        break;
                    }
                }
                break;
        }
    }

    public process(inputs: Float32Array[][], outputs: Float32Array[][], sampleRate: number) {
        const output = outputs[0];

        for (let i = 0; i < output[0].length; i++) {
            let valL = 0;
            let valR = 0;

            for (let voice of this.voices) {
                voice.compute(sampleRate, this.voiceBuf);
                valL += this.voiceBuf[0];
                valR += this.voiceBuf[1];
            }

            output[0][i] = valL;
            output[1][i] = valR;
        }

        return true;
    }
}