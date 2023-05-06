import { read } from "original-fs";
import { globalAudio, AudioDevice } from "../audio";

export interface Parameter {
    target: number, // The target value of the parameter
    value: number, // The starting value of the parameter
    time: number, // The time when the parameter was set
    interpolate: boolean // Whether or not to interpolate the value when set
}

const INTERPOLATION_TIME = 0.015;

export const clamp = (v: number, min: number, max: number) => Math.min(Math.max(v, min), max);
export const lerp = (min: number, max: number, a: number) => (max - min) * a + min;
export const readParameter = (p: Parameter, t: number) =>
    lerp(p.value, p.target, clamp(
        (t - p.time) / INTERPOLATION_TIME,
        0, 1
    ));

/**
 * Class for handling parameters for modules
 * 
 * This does a fixed linear interpolation when reading the value, instead
 * of doing an instantenous change.
 */
export class Parameters {
    private map: Map<string, Parameter>;

    constructor(params: Iterable<readonly [string, number, boolean]>) {
        this.map = new Map();

        for (let pair of params) {
            this.map.set(pair[0], {
                target: pair[1],
                value: pair[1],
                interpolate: pair[2],
                time: 0,
            });
        }
    }    

    /**
     * Set the target value of a parameter
     * @param name The name of the parameter
     * @param value The new target value
     */
    public setValue(name: string, value: number) {
        if (!globalAudio.device) throw new Error("audio device not found");
        
        let param = this.map.get(name);
        if (!param) throw new ReferenceError(`parameter ${name} does not exist`);

        if (param.interpolate) {
            param.value = readParameter(param, globalAudio.device.time);
            param.target = value;
            param.time = globalAudio.device.time;
        } else {
            param.target = value;
            param.value = value;
        }
    }

    /**
     * Get the target value of a parameter
     * @param name The name of the parameter
     * @returns The target value of the parameter
     */
    public getValue(name: string) {
        let param = this.map.get(name);
        if (!param) throw new ReferenceError(`parameter ${name} does not exist`);

        return param.target;
    }

    /**
     * Get parameter data
     * @param name The name of the parameter
     * @returns The parameter data
     */
    public get(name: string) {
        let param = this.map.get(name);
        if (!param) throw new ReferenceError(`parameter ${name} does not exist`);

        return param;
    }
}