import { AudioDevice } from "../../audio";
import { SynthesizerBase, VoiceBase } from "./synthbase"

const PI = Math.PI;
const PI2 = PI * 2;
const mod = (a: number, b: number) => (a % b + b) % b;

export enum WaveformType {
    Sine, Triangle, Square, Sawtooth
}

class Voice extends VoiceBase {
    private type: WaveformType;

    constructor(key: number, freq: number, volume: number, type: WaveformType) {
        super(key, freq, volume);
        this.type = type;
    }

    public compute(sampleRate: number, buf: Float32Array) {
        let val = 0;
        const period = 1.0 / this.freq;

        switch (this.type) {
            case WaveformType.Sine:
                val = Math.sin(this.time) * this.volume;
                break;

            case WaveformType.Triangle: {
                let moda = this.time / (PI2 * this.freq) - period / 4.0;
                let modb = period;
                val = (4.0 / period) * Math.abs(mod(moda, modb) - period / 2.0) - 1.0;
            }
        }

        buf[0] = val;
        buf[1] = val;

        this.time += (this.freq * 2 * Math.PI) / sampleRate;
    }
}

/*
function voiceConstructor(key: number, freq: number, volume: number) {
    return new Voice(key, freq, volume);
}
*/

/**
 * A dummy synthesizer, which just emits a basic sine wave for each played note
 */
export class WaveformSynthesizer extends SynthesizerBase {
    public waveformType: WaveformType;

    constructor() {
        super();
        this.waveformType = WaveformType.Triangle;
        
        this.voices = [];
        this.voiceBuf = new Float32Array(2);
    }

    protected createVoice(key: number, freq: number, volume: number): VoiceBase {
        return new Voice(key, freq, volume, this.waveformType);
    }

    public process(_inputs: Float32Array[][], output: Float32Array[], device: AudioDevice) {
        const sampleRate = device.sampleRate;

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