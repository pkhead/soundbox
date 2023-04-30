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

    public async init() {
        this.module = await createModule(this.modType);
    }

    public async release() {
        this.module?.release();
        this.module = null;
    }

    /**
     * Connect this AudioModule to another
     * @param dest The AudioModule to connect to
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
     */
    public async connectToOutput() {
        if (!this.module) throw new Error("module is not initialized");
        await this.module.connectToOutput();
    }

    /**
     * Disconnect this AudioModule from its output
     */
    public async disconnect() {
        if (!this.module) throw new Error("module is not initialized");
        await this.module.disconnect();
    }

    // connect(dest: AudioModule): Promise<void>
    // disconnect(): Promise<void>
}

export class NoteModule extends AudioModule {
    private curNotes: NoteData[]

    constructor(_modType: string) {
        super("basic-synth");
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