//import Speaker from "speaker";
import Speaker = require("speaker");
//import PortAudio = require("naudiodon");
import { Readable } from "stream";

interface AudioDeviceOptions {
    bitDepth?: number,
    channels?: number,
    sampleRate?: number,
    samplesPerFrame?: number
}

export class AudioDevice {
    //private audioOutput: PortAudio.IoStreamWrite;
    private audioOutput: Speaker;
    private _bitDepth: number;
    private _channels: number;
    private _sampleRate: number;
    //private _samplesPerFrame: number;

    public get bitDepth() { return this._bitDepth; }
    public get sampleRate() { return this._sampleRate; }
    //public get samplesPerFrame() { return this._samplesPerFrame; }

    public process: (channels: Float32Array[]) => void;

    constructor(options?: AudioDeviceOptions) {
        this._bitDepth = options?.bitDepth === undefined ? 16 : options.bitDepth;
        this._channels = options?.channels === undefined ? 2 : options.channels;
        this._sampleRate = options?.sampleRate === undefined ? 44100 : options.sampleRate;
        //this._samplesPerFrame = options?.samplesPerFrame === undefined ? 1024 : options.samplesPerFrame;

        console.log(this);

        /*
        if (this._bitDepth !== 8 && this._bitDepth !== 16 && this._bitDepth !== 32) {
            throw new Error(`unsupported bit depth of ${this._bitDepth}`);
        }
        */

        this.audioOutput = new Speaker({
            bitDepth: this._bitDepth,
            channels: this._channels,
            sampleRate: this._sampleRate,
        });

        /*
        this.device = new OutputDevice({
            channels: this._channels,
            sampleRate: this._sampleRate
        });*/
        
        /*
        this.audioOutput = PortAudio.AudioIO({
            outOptions: {
                channelCount: this._channels,
                sampleFormat: PortAudio.SampleFormat16Bit,
                sampleRate: this._sampleRate,
                deviceId: -1,
                closeOnError: true
            }
        });
        */

        this.process = function(channels: Float32Array[]) { }

        /*
        this.device.process = (size: number) => {
            const buf = Buffer.alloc(size);
            const outputArr = new Float32Array(buf);
            const numChannels = this.device.numChannels;

            let channels: Float32Array[] = [];
            for (let c = 0; c < numChannels; c++) {
                channels.push(new Float32Array(size / numChannels));
            }

            this.process(channels);

            for (let c = 0; c < numChannels; c++) {
                for (let i = 0; i < channels[c].length; i++) {
                    outputArr[i * numChannels + c] = channels[c][i];
                }
            }

            return buf;
        }*/

        const stream = new Readable({
            highWaterMark: 512
        });

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

        stream.pipe(this.audioOutput);

        //this.audioOutput.start();
    }

    public close(flush: boolean) {
        //this.audioOutput.end();
        this.audioOutput.close(flush);
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