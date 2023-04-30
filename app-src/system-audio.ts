import { unescapeLeadingUnderscores } from "../node_modules/typescript/lib/typescript";

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
    moduleConnect: (srcID: string, destID: string) => Promise<void>,
    moduleDisconnect: (id: string) => Promise<void>
}

declare global {
    const systemAudio: SystemAudioInterface
}

export class ModuleController {
    private _id: string;
    private _unloadCallback; // listen to unload event so that module can be released before load
    private _loaded: boolean;

    constructor(id: string) {
        this._loaded = true;
        this._id = id;

        this._unloadCallback = () => {
            this.release();
        };

        window.addEventListener("beforeunload", this._unloadCallback);
    };

    public openConfig() {
        systemAudio.openModuleConfig(this._id);
    }

    public release() {
        if (this._loaded) {
            window.removeEventListener("beforeunload", this._unloadCallback);
            systemAudio.releaseModule(this._id);
            this._loaded = false;
        }
    }

    public sendEvent(event: NoteEvent) {
        systemAudio.sendEventToModule(this._id, event);
    }

    public async connect(mod: ModuleController) {
        await systemAudio.moduleConnect(this._id, mod._id);
    }

    public async connectToOutput() {
        await systemAudio.moduleConnect(this._id, "output");
    }

    public async disconnect() {
        await systemAudio.moduleDisconnect(this._id);
    }
}

export async function createModule(modName: string) {
    return new ModuleController(await systemAudio.newModule(modName));
}