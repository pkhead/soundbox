import { AudioDevice } from "../../audio";
import { SynthesizerBase, VoiceBase } from "./synthbase"

class Voice extends VoiceBase {
    public compute(sampleRate: number, buf: Float32Array) {
        const val = Math.sin(this.time) * this.volume;

        buf[0] = val * 0.1;
        buf[1] = val * 0.1;

        this.time += (this.freq * 2 * Math.PI) / sampleRate;
    }
}

function voiceConstructor(key: number, freq: number, volume: number) {
    return new Voice(key, freq, volume);
}

/**
 * A dummy synthesizer, which just emits a basic sine wave for each played note
 */
export class DummySynthesizer extends SynthesizerBase {
    constructor() {
        super(voiceConstructor);
        
        this.voices = [];
        this.voiceBuf = new Float32Array(2);
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