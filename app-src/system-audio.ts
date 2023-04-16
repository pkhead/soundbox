import { sys } from "../node_modules/typescript/lib/typescript";

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

export interface SystemAudioInterface {
    newModule: (modName: string) => Promise<string>,
    openModuleConfig: (id: string) => void,
    releaseModule: (id: string) => void,
    sendEventToModule: (id: string, event: NoteEvent) => void,
    requestAudio: (sampleRate: number, numChannels: number, numSamples: number) => Promise<ArrayBuffer[]>
}

declare global {
    const systemAudio: SystemAudioInterface
}

export class ModuleController {
    private _id: string;

    constructor(id: string) {
        this._id = id;
    };

    public openConfig() {
        systemAudio.openModuleConfig(this._id);
    }

    public release() {
        systemAudio.releaseModule(this._id);
    }

    public sendEvent(event: NoteEvent) {
        systemAudio.sendEventToModule(this._id, event);
    }
}

export async function createModule(modName: string) {
    return new ModuleController(await systemAudio.newModule(modName));
}