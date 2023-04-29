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
    //private speaker: Speaker;
    private _device: AudioOutputDevice;
    //private _samplesPerFrame: number;

    public get channelCount() { return this._device.channelCount; }
    public get sampleRate() { return this._device.sampleRate; }
    //public get samplesPerFrame() { return this._samplesPerFrame; }

    public process: (channels: Float32Array[]) => void;

    private timeout: NodeJS.Timeout

    constructor() {
        this._device = new AudioOutputDevice();
        this.process = function(channels: Float32Array[]) { }
        
        let audioBuf = new Float32Array(BUFFER_SIZE * this._device.channelCount);
        let channelL = new Float32Array(BUFFER_SIZE);
        let channelR = new Float32Array(BUFFER_SIZE);

        let timeoutCallback = () => {
            while (this._device.numQueuedFrames < BUFFER_SIZE * 5) {
                this.process([channelL, channelR]);

                for (let i = 0; i < BUFFER_SIZE; i++) {
                    audioBuf[i * 2] = channelL[i];
                    audioBuf[i * 2 + 1] = channelR[i];
                }

                this._device.queue(audioBuf.buffer);
            }
            
            setTimeout(timeoutCallback, 10);
        };

        this.timeout = setTimeout(timeoutCallback, 10);
    }

    public close() {
        return this._device.close();
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
    public event(event: NoteEvent) { }
    public abstract process(inputs: Float32Array[][], outputs: Float32Array[][], sampleRate: number): void
}