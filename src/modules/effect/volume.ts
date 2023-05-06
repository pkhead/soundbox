import { AudioDevice, AudioModule } from "../../audio";
import { Parameter, Parameters, readParameter } from "../util";

/**
 * Effect used for controlling volume and panning
 */
export class VolumeEffect extends AudioModule {
    private panning: Parameter;
    private volume: Parameter;

    constructor() {
        super();

        this.parameters = new Parameters([
            ["volume", 1, true],
            ["panning", 0, true]
        ]);

        this.panning = this.parameters.get("panning");
        this.volume = this.parameters.get("volume");
    }

    public process(inputs: Float32Array[][], outChannels: Float32Array[], device: AudioDevice) {
        let t = device.time;

        for (let i = 0; i < device.bufferSize; i++) {
            let panning = readParameter(this.panning, t);
            let volume = readParameter(this.volume, t);

            // map range [-1, 1] to [0, 1]
            let panFactor = (panning + 1) / 2;

            for (let input of inputs) {
                outChannels[0][i] += input[0][i] * volume * (1 - panFactor); // left channel
                outChannels[1][i] += input[1][i] * volume * panFactor; // right channel
            }

            t += 1 / device.sampleRate;
        }
    }
}

/*
            // lerp volume
            if (Math.abs(this.volume - this.volumeLerp) < lerpRate)
                this.volumeLerp = this.volume;
            else
                this.volumeLerp += Math.sign(this.volume - this.volumeLerp) * lerpRate;
                
            // lerp panning
            if (Math.abs(this.panning - this.panningLerp) < lerpRate)
                this.panningLerp = this.panning;
            else
                this.panningLerp += Math.sign(this.panning - this.panningLerp) * lerpRate;
            */