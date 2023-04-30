import { AudioDevice, AudioModule } from "../../audio";

/**
 * Effect used for controlling volume as a percentage
 */
export class VolumeEffect extends AudioModule {
    public multiplier: number;

    constructor() {
        super();
        this.multiplier = 1;
    }

    public getParam(paramName: string) {
        if (paramName === "multiplier") return this.multiplier;
    }

    public setParam(paramName: string, value: any) {
        if (paramName === "multiplier") this.multiplier = +value;
    }

    public process(inputs: Float32Array[][], outChannels: Float32Array[], _device: AudioDevice) {
        for (let input of inputs) {
            for (let ch = 0; ch < outChannels.length; ch++) {
                for (let i = 0; i < input.length; i++) {
                    outChannels[ch][i] += input[ch][i] * this.multiplier;
                }
            }
        }
    }
}