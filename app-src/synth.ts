import { Note, Song } from "./song";
import { ModuleController, NoteEventType, createModule } from "./system-audio";

class NoteData {
    public key: number;
    public end: number;

    constructor(key: number, end: number) {
        this.key = key;
        this.end = end;
    }
}

export class Synthesizer {
    private module: ModuleController | null;
    private curNotes: NoteData[];

    constructor() {
        this.curNotes = [];
        this.module = null;
    }

    public async init() {
        this.module = await createModule("basic-synth");

        const redraw = () => {
            requestAnimationFrame(redraw);

            if (this.module) {
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

    public start() {
        //if (this.audioProcessor) this.audioProcessor.connect(this.audioContext.destination);
    }

    public stop() {
        //if (this.audioProcessor) this.audioProcessor.disconnect();
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