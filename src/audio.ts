//import Speaker from "speaker";
import { AudioOutputDevice } from "soundlib";
import { Readable } from "stream";

interface AudioDeviceOptions {
    bitDepth?: number,
    channels?: number,
    sampleRate?: number,
    samplesPerFrame?: number
}

const BUFFER_SIZE = 512;

export class AudioDevice {
    private _device: AudioOutputDevice
    private _inputs: AudioModule[]

    public get inputs() { return this._inputs }
    public get channelCount() { return this._device.channelCount; }
    public get sampleRate() { return this._device.sampleRate; }
    public get bufferSize() { return BUFFER_SIZE; }
    
    //public process: (channels: Float32Array[]) => void;

    private timeout: NodeJS.Timeout | null

    constructor() {
        this._device = new AudioOutputDevice();
        this._inputs = [];
        //this.process = function(channels: Float32Array[]) { }
        
        let audioBuf = new Float32Array(BUFFER_SIZE * this._device.channelCount);
        let channelL = new Float32Array(BUFFER_SIZE);
        let channelR = new Float32Array(BUFFER_SIZE);

        let timeoutCallback = () => {
            while (this._device.numQueuedFrames < BUFFER_SIZE * 5) {
                // set buffers to zero
                for (let i = 0; i < BUFFER_SIZE; i++) {
                    channelL[i] = 0;
                    channelR[i] = 0;
                }

                this.process([channelL, channelR]);

                for (let i = 0; i < BUFFER_SIZE; i++) {
                    audioBuf[i * 2] = channelL[i];
                    audioBuf[i * 2 + 1] = channelR[i];
                }

                this._device.queue(audioBuf.buffer);
            }
            
            this.timeout = setTimeout(timeoutCallback, 10);
        };

        this.timeout = setTimeout(timeoutCallback, 10);
    }

    public close() {
        if (this.timeout) {
            clearTimeout(this.timeout);
            this.timeout = null;
        }

        return this._device.close();
    }

    private process(channels: Float32Array[]) {
        // zero out buffers
        for (let ch = 0; ch < channels.length; ch++) {
            channels[ch].fill(0);
        }

        // get inputs
        let inputArray: Float32Array[][] = [];

        for (let input of this._inputs) {
            inputArray.push(input.getAudio(this));
        }

        // add all inputs together to put in output buffer
        for (let arr of inputArray) {
            for (let ch = 0; ch < channels.length; ch++) {
                for (let i = 0; i < channels[ch].length; i++) {
                    channels[ch][i] += arr[ch][i];
                }
            }
        }
    }

    public removeInput(mod: AudioModule) {
        let idx = this._inputs.indexOf(mod);
        if (idx >= 0) this._inputs.splice(idx, 1);
        return idx >= 0;
    }
}

export const enum NoteEventType {
    NoteOn = 0,
    NoteOff = 1
};

interface NoteOnEvent {
    type: NoteEventType.NoteOn,
    key: number,
    volume: number
}

interface NoteOffEvent {
    type: NoteEventType.NoteOff,
    key: number
}

export type NoteEvent = NoteOffEvent | NoteOnEvent;

export abstract class AudioModule {
    private _inputs: AudioModule[]
    private _output: AudioModule | AudioDevice | null

    private _outputBuffer: Float32Array[] | null

    public constructor() {
        this._inputs = [];
        this._output = null;
        this._outputBuffer = null;
    }

    public get output() { return this._output; }
    public get inputs() { return this._inputs; }

    public removeInput(mod: AudioModule) {
        let idx = this._inputs.indexOf(mod);
        if (idx >= 0) this._inputs.splice(idx, 1);
        return idx >= 0;
    }

    public connect(dest: AudioModule | AudioDevice) {
        this.disconnect();
        dest.inputs.push(this);
    }

    public disconnect() {
        if (this._output) this._output.removeInput(this);
        this._output = null;
    }

    public removeAllConnections() {
        this.disconnect();

        for (let input of this._inputs) {
            input.disconnect();
        }
    }

    public getAudio(device: AudioDevice) {
        // if buffer size isn't created or is a different length, create a new buffer
        // otherwise reuse the old one
        if (this._outputBuffer === null || this._outputBuffer.length !== device.bufferSize) {
            this._outputBuffer = [];

            for (let ch = 0; ch < device.channelCount; ch++) {
                this._outputBuffer.push(new Float32Array(device.bufferSize));
            }
        }

        // accumulate inputs
        let inputArrays = [];

        for (let input of this._inputs) {
            inputArrays.push(input.getAudio(device));
        }

        // processing
        this.process(inputArrays, this._outputBuffer, device);
        return this._outputBuffer;
    }

    public event(event: NoteEvent) { }
    public abstract process(inputs: Float32Array[][], output: Float32Array[], device: AudioDevice): void
}