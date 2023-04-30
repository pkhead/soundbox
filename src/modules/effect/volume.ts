import { AudioDevice, AudioModule } from "../../audio";

/**
 * Effect used for controlling volume and panning
 */
export class VolumeEffect extends AudioModule {
    public volume: number;
    public panning: number;

    constructor() {
        super();
        this.volume = 1;
        this.panning = 0;
    }

    public getParam(paramName: string) {
        if (paramName === "volume") return this.volume;
        if (paramName === "panning") return this.panning;
    }

    public setParam(paramName: string, value: any) {
        if (paramName === "volume") this.volume = +value;
        // panning is clamped in range [-1, 1]
        else if (paramName === "panning") this.panning = Math.max(Math.min(+value, 1), -1);
    }

    public process(inputs: Float32Array[][], outChannels: Float32Array[], device: AudioDevice) {
        // map range [-1, 1] to [0, 1]
        let panFactor = (this.panning + 1) / 2;

        for (let input of inputs) {
            for (let i = 0; i < device.bufferSize; i++) {
                outChannels[0][i] += input[0][i] * this.volume * (1 - panFactor); // left channel
                outChannels[1][i] += input[1][i] * this.volume * panFactor; // right channel
            }
        }
    }
}