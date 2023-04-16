//import Speaker from "speaker";
import { Readable } from "stream";

interface AudioDeviceOptions {
    bitDepth?: number,
    channels?: number,
    sampleRate?: number,
    samplesPerFrame?: number
}

export class AudioDevice {
    //private speaker: Speaker;
    private _bitDepth: number;
    private _channels: number;
    private _sampleRate: number;
    private _samplesPerFrame: number;

    public get bitDepth() { return this._bitDepth; }
    public get sampleRate() { return this._sampleRate; }
    public get samplesPerFrame() { return this._samplesPerFrame; }

    public process: (channels: Float32Array[]) => void;

    constructor(options?: AudioDeviceOptions) {
        this._bitDepth = options?.bitDepth === undefined ? 16 : options.bitDepth;
        this._channels = options?.channels === undefined ? 2 : options.channels;
        this._sampleRate = options?.sampleRate === undefined ? 44100 : options.sampleRate;
        this._samplesPerFrame = options?.samplesPerFrame === undefined ? 1024 : options.samplesPerFrame;

        console.log(this);

        if (this._bitDepth !== 8 && this._bitDepth !== 16 && this._bitDepth !== 32) {
            throw new Error(`unsupported bit depth of ${this._bitDepth}`);
        }

        /*this.speaker = new Speaker({
            bitDepth: this._bitDepth,
            channels: this._channels,
            sampleRate: this._sampleRate,
            samplesPerFrame: this._samplesPerFrame
        } as Object);*/

        this.process = function(channels: Float32Array[]) { }

        const stream = new Readable();
        stream._read = (size: number) => {
            const sampleSize = this._bitDepth / 8;
            const blockSize = sampleSize * this._channels;
            const numSamples = size / blockSize | 0;
            let buf = Buffer.alloc(numSamples * blockSize);

            let channels: Float32Array[] = [];

            for (let c = 0; c < this._channels; c++) {
                channels.push(new Float32Array(numSamples));
            }

            this.process(channels);

            const amplitude = 2 ** (this._bitDepth - 1);

            for (let c = 0; c < this._channels; c++) {
                for (let i = 0; i < channels[c].length; i++) {
                    let offset = i * blockSize + c * sampleSize;
                    let val = (Math.max(Math.min(channels[c][i], 1), -1) * amplitude) | 0;

                    switch (this._bitDepth) {
                        case 8:
                            buf.writeInt8(val, offset);
                            break;

                        case 16:
                            buf.writeInt16LE(val, offset);
                            break;

                        case 32:
                            buf.writeInt32LE(val, offset);
                            break;
                    }
                }
            }

            stream.push(buf);
        };

        //stream.pipe(this.speaker);
    }

    public close(flush: boolean) {
        //return this.speaker.close(flush)
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