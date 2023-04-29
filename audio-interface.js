const bindings = require("./build/Release/native.node");

let openDevices = [];

process.on("exit", () => {
    for (let device of openDevices) {
        bindings.closeOutputDevice(device.__handle__);
    }

    bindings.close();
})

exports.OutputDevice = class {
    constructor(options) {
        this.__handle__ = native.createOutputDevice(options);
        this.isClosed = false;
        devices.push(this);

        /*
        this__device__ = {
            callback: (output: ArrayBuffer) => void,
            sampleRate: number,
            channels: number
        }
        */
    }

    close() {
        if (this.isClosed) throw new Error("OutputDevice already closed");
        bindings.closeOutputDevice(this.__handle__);
        this.isClosed = true;
        
        let idx = openDevices.indexOf(this);
        if (idx >= 0) openDevices.splice(idx, 1);
    }

    get process() {
        return this.__handle__.callback;
    }

    set process(callback) {
        this.__handle__.callback = callback;
    }

    get sampleRate() {
        return this.__handle__.sampleRate;
    }

    get numChannels() {
        return this.__handle__.numChannels;
    }
}