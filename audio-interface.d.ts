export interface OutputDeviceOptions {
    sampleRate?: number,
    channels?: number
}

export class OutputDevice {
    public process: (size: number) => Buffer | undefined

    constructor(options?: OutputDeviceOptions)
    public close(): void

    public get sampleRate(): number
    public get numChannels(): number
}