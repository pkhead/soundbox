import { BrowserWindow } from "electron";
import path from "path";
import { AudioDevice } from "../../audio";
import { SynthesizerBase, VoiceBase } from "./synthbase"
import { Parameter, Parameters } from "../util"

const PI = Math.PI;
const PI2 = PI * 2;
const mod = (a: number, b: number) => (a % b + b) % b;

export enum WaveformType {
    Sine = 0, Triangle = 1, Square = 2, Sawtooth = 3
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
                val = Math.sin(this.time);
                break;

            case WaveformType.Triangle: {
                let moda = this.time / (PI2 * this.freq) - period / 4.0;
                let modb = period;
                val = (4.0 / period) * Math.abs(mod(moda, modb) - period / 2.0) - 1.0;
                break;
            }

            case WaveformType.Sawtooth:
                val = 2.0 * mod(this.time / PI2 + 0.5, 1.0) - 1.0;
                break;

            case WaveformType.Square:
                val = Math.sin(this.time) >= 0.0 ? 1.0 : -1.0;
                break;
        }

        val *= this.volume;
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
    private waveformType: Parameter;

    constructor() {
        super();
        
        this.voices = [];
        this.voiceBuf = new Float32Array(2);

        this.parameters = new Parameters([
            ["waveform", WaveformType.Sine, false],
        ]);

        this.waveformType = this.parameters.get("waveform");
    }

    protected createVoice(key: number, freq: number, volume: number): VoiceBase {
        return new Voice(key, freq, volume, this.waveformType.value);
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

    private _curWindow: BrowserWindow | null = null;

    public openConfig(parent: BrowserWindow, id: string): void {
        if (this._curWindow) {
            this._curWindow.focus();
        } else {
            const win = new BrowserWindow({
                parent: parent,
                width: 300,
                height: 300,
                show: false,
                frame: false,
                webPreferences: {
                    preload: path.join(__dirname, "../module-preload.js")
                }
            });
            this._curWindow = win;
        
            win.once("ready-to-show", () => {
                win.show();
                win.webContents.send("moduleid", id);
            });

            win.on("closed", () => {
                this._curWindow = null;
            });

            win.setMenuBarVisibility(false);
            win.loadFile("app/modules/waveform.html");
        }
    }
}