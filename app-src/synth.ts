import { ModuleController, NoteEventType, createModule } from "./system-audio";

class NoteData {
    public key: number
    public end: number

    constructor(key: number, end: number) {
        this.key = key;
        this.end = end;
    }
}

/*
const audioContext = new AudioContext();
let audioProcessor: AudioWorkletNode | undefined;

audioContext.audioWorklet.addModule("synth-worker.js").then(() => {
    audioProcessor = new AudioWorkletNode(audioContext, "audio-processor", {
        numberOfInputs: 0,
        numberOfOutputs: 1,
        outputChannelCount: [2]
    });

    audioProcessor.port.onmessage = (ev) => {
        systemAudio.requestAudio(audioContext.sampleRate, ev.data.channels, ev.data.numSamples).then(output => {
            audioProcessor?.port.postMessage(output, output);
        });
    }

    audioProcessor.connect(audioContext.destination);
});
*/

export interface AudioModule {
    init(): Promise<void>;
    release(): void;
    // connect(dest: AudioModule): Promise<void>
    // disconnect(): Promise<void>
}

export class NoteModule implements AudioModule {
    private module: ModuleController | null
    private curNotes: NoteData[]
    private modType: string

    constructor(_modType: string) {
        this.curNotes = [];
        this.module = null;
        this.modType = "basic-synth";
    }

    public get moduleType() { return this.modType; }

    public async init() {
        this.module = await createModule(this.modType);

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

    public async release() {
        this.module?.release();
        this.module = null;
    }

    public playNote(key: number, duration: number) {
        if (this.module) {
            this.beginNote(key, 0.1);
            this.curNotes.push(new NoteData(key, Date.now() + duration));
        }
    }

    public beginNote(key: number, volume: number = 0.1) {
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