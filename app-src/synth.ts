import { ModuleController, NoteEventType, createModule } from "./system-audio";

class NoteData {
    public key: number
    public end: number

    constructor(key: number, end: number) {
        this.key = key;
        this.end = end;
    }
}

export class AudioModule {
    protected module: ModuleController | null
    protected modType: string

    constructor(modType: string) {
        this.module = null;
        this.modType = modType;
    }

    /**
     * @returns `true` if the module is initialized, `false` if not
     */
    public get isReady() { return this.module !== null }

    /**
     * Load the module. The module type is given in the `AudioModule` base class constructor
     * @returns A `Promise<void>` for the successful initialization of the module
     */
    public async init() {
        if (!this.module) this.module = await createModule(this.modType);
    }

    /**
     * Unload the module
     */
    public release() {
        this.module?.release();
        this.module = null;
    }

    /**
     * Connect this AudioModule to another
     * @param dest The AudioModule to connect to
     * @returns A `Promise<void>` for the success of the connection
     */
    public async connect(dest: AudioModule) {
        if (!this.module) {
            throw new Error("module is not initialized");
        }

        if (!dest.module) {
            throw new Error("module is not initialized");
        }

        await this.module.connect(dest.module);
    }

    /**
     * Connect this AudioModule to the audio device
     * @returns A `Promise<void>` for the success of the connection
     */
    public async connectToOutput() {
        if (!this.module) throw new Error("module is not initialized");
        await this.module.connectToOutput();
    }

    /**
     * Disconnect this AudioModule from its output
     * @returns A `Promise<void>` for the success of the disconnection
     */
    public async disconnect() {
        if (!this.module) throw new Error("module is not initialized");
        await this.module.disconnect();
    }
    
    /**
     * Get the value of a parameter
     * @param paramName The name of the parameter
     * @returns A `Promise<any>` for the value of the parameter 
     */
    public getParam(paramName: string) {
        if (!this.module) throw new Error("module is not initialized");
        return this.module.getParam(paramName);
    }

    /**
     * Set the value of a parameter
     * @param paramName The name of the parameter
     * @returns A `Promise<void>` for the success of the parameter
     */
    public setParam(paramName: string, value: any) {
        if (!this.module) throw new Error("module is not initialized");
        return this.module.setParam(paramName, value);
    }
}

export class NoteModule extends AudioModule {
    private curNotes: NoteData[]

    constructor(_modType: string) {
        super(_modType);
        this.curNotes = [];
    }

    public get moduleType() { return this.modType; }

    public async init() {
        await super.init();

        const redraw = () => {
            if (this.module) {
                requestAnimationFrame(redraw);

                let curTime = Date.now();

                for (let i = this.curNotes.length - 1; i >= 0; i--) {
                    let note = this.curNotes[i];

                    if (curTime >= note.end) {
                        this.module.sendEvent({
                            type: NoteEventType.NoteOff,
                            key: note.key
                        });

                        this.curNotes.splice(i, 1);
                    }
                }
            }
        }
        
        redraw();
    }

    public playNote(key: number, duration: number, volume: number = 1) {
        if (this.module) {
            this.beginNote(key, volume);
            this.curNotes.push(new NoteData(key, Date.now() + duration));
        }
    }

    public beginNote(key: number, volume: number = 1) {
        if (this.module) {
            //const freq = 440 * 2 ** ((key - 69) / 12);
            this.module.sendEvent({
                type: NoteEventType.NoteOn,
                key: key,
                volume: volume
            });
        }
    }

    public endNote(key: number) {
        this.module?.sendEvent({
            type: NoteEventType.NoteOff,
            key: key
        });
    }
}

/*
export class EffectModule implements AudioModule {
    private module: ModuleController | null;
    private modType: string;

    constructor(modType: string) {
        this.module = null;
        this.modType = modType;
    }

    public get moduleType() { return this.modType; }

    public async init() {
        this.module = await createModule(this.modType);
    }

    public async release() {
        this.module?.release();
        this.module = null;
    }
}

export class EffectChain {
    private modules: ModuleController[];
    
    constructor() {
        this.modules = [];
    }
}
*/