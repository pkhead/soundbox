"use strict";
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.AudioDevice = void 0;
const speaker_1 = __importDefault(require("speaker"));
const stream_1 = require("stream");
class AudioDevice {
    get bitDepth() { return this._bitDepth; }
    get sampleRate() { return this._sampleRate; }
    get samplesPerFrame() { return this._samplesPerFrame; }
    constructor(options) {
        this._bitDepth = (options === null || options === void 0 ? void 0 : options.bitDepth) === undefined ? 16 : options.bitDepth;
        this._channels = (options === null || options === void 0 ? void 0 : options.channels) === undefined ? 2 : options.channels;
        this._sampleRate = (options === null || options === void 0 ? void 0 : options.sampleRate) === undefined ? 44100 : options.sampleRate;
        this._samplesPerFrame = (options === null || options === void 0 ? void 0 : options.samplesPerFrame) === undefined ? 1024 : options.samplesPerFrame;
        console.log(this);
        if (this._bitDepth !== 8 && this._bitDepth !== 16 && this._bitDepth !== 32) {
            throw new Error(`unsupported bit depth of ${this._bitDepth}`);
        }
        this.speaker = new speaker_1.default({
            bitDepth: this._bitDepth,
            channels: this._channels,
            sampleRate: this._sampleRate,
            samplesPerFrame: this._samplesPerFrame
        });
        this.process = function (channels) {
            for (let channel of channels) {
                channel.fill(0);
            }
        };
        const stream = new stream_1.Readable();
        stream._read = (size) => {
            const sampleSize = this._bitDepth / 8;
            const blockSize = sampleSize * this._channels;
            const numSamples = size / blockSize | 0;
            let buf = Buffer.alloc(numSamples * blockSize);
            let channels = [];
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
        stream.pipe(this.speaker);
    }
    close(flush) {
        return this.speaker.close(flush);
    }
}
exports.AudioDevice = AudioDevice;
//# sourceMappingURL=data:application/json;base64,eyJ2ZXJzaW9uIjozLCJmaWxlIjoiYXVkaW9kZXZpY2UuanMiLCJzb3VyY2VSb290IjoiIiwic291cmNlcyI6WyJzcmMvYXVkaW9kZXZpY2UudHMiXSwibmFtZXMiOltdLCJtYXBwaW5ncyI6Ijs7Ozs7O0FBQUEsc0RBQThCO0FBQzlCLG1DQUFrQztBQVNsQyxNQUFhLFdBQVc7SUFPcEIsSUFBVyxRQUFRLEtBQUssT0FBTyxJQUFJLENBQUMsU0FBUyxDQUFDLENBQUMsQ0FBQztJQUNoRCxJQUFXLFVBQVUsS0FBSyxPQUFPLElBQUksQ0FBQyxXQUFXLENBQUMsQ0FBQyxDQUFDO0lBQ3BELElBQVcsZUFBZSxLQUFLLE9BQU8sSUFBSSxDQUFDLGdCQUFnQixDQUFDLENBQUMsQ0FBQztJQUk5RCxZQUFZLE9BQTRCO1FBQ3BDLElBQUksQ0FBQyxTQUFTLEdBQUcsQ0FBQSxPQUFPLGFBQVAsT0FBTyx1QkFBUCxPQUFPLENBQUUsUUFBUSxNQUFLLFNBQVMsQ0FBQyxDQUFDLENBQUMsRUFBRSxDQUFDLENBQUMsQ0FBQyxPQUFPLENBQUMsUUFBUSxDQUFDO1FBQ3pFLElBQUksQ0FBQyxTQUFTLEdBQUcsQ0FBQSxPQUFPLGFBQVAsT0FBTyx1QkFBUCxPQUFPLENBQUUsUUFBUSxNQUFLLFNBQVMsQ0FBQyxDQUFDLENBQUMsQ0FBQyxDQUFDLENBQUMsQ0FBQyxPQUFPLENBQUMsUUFBUSxDQUFDO1FBQ3hFLElBQUksQ0FBQyxXQUFXLEdBQUcsQ0FBQSxPQUFPLGFBQVAsT0FBTyx1QkFBUCxPQUFPLENBQUUsVUFBVSxNQUFLLFNBQVMsQ0FBQyxDQUFDLENBQUMsS0FBSyxDQUFDLENBQUMsQ0FBQyxPQUFPLENBQUMsVUFBVSxDQUFDO1FBQ2xGLElBQUksQ0FBQyxnQkFBZ0IsR0FBRyxDQUFBLE9BQU8sYUFBUCxPQUFPLHVCQUFQLE9BQU8sQ0FBRSxlQUFlLE1BQUssU0FBUyxDQUFDLENBQUMsQ0FBQyxJQUFJLENBQUMsQ0FBQyxDQUFDLE9BQU8sQ0FBQyxlQUFlLENBQUM7UUFFaEcsT0FBTyxDQUFDLEdBQUcsQ0FBQyxJ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