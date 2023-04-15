import { Note, Song } from "./song";

class NoteData {
    public key: number;
    public end: number;

    constructor(key: number, end: number) {
        this.key = key;
        this.end = end;
    }
}

export class Synthesizer {
    private audioContext: AudioContext;
    private audioProcessor: AudioWorkletNode | null = null;

    private curNotes: NoteData[];

    constructor() {
        this.curNotes = [];
        this.audioContext = new AudioContext();
    }

    public async init() {
        const {audioContext} = this;

        if (!this.audioProcessor) {
            await audioContext.resume();
            await audioContext.audioWorklet.addModule("synth-worker.js");
            this.audioProcessor = new AudioWorkletNode(audioContext, "audio-processor", {
                numberOfInputs: 0,
                numberOfOutputs: 1,
                outputChannelCount: [2]
            });
        }

        const redraw = () => {
            requestAnimationFrame(redraw);

            if (this.audioProcessor) {
                let curTime = audioContext.currentTime;

                for (let i = this.curNotes.length - 1; i >= 0; i--) {
                    let note = this.curNotes[i];

                    if (curTime >= note.end) {
                        this.audioProcessor.port.postMessage({
                            msg: "end",
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
        if (this.audioProcessor) this.audioProcessor.connect(this.audioContext.destination);
    }

    public stop() {
        if (this.audioProcessor) this.audioProcessor.disconnect();
    }

    public playNote(key: number, duration: number) {
        if (this.audioProcessor) {
            this.beginNote(key, 0.1);
            this.curNotes.push(new NoteData(key, this.audioContext.currentTime + duration));
        }
    }

    public beginNote(key: number, volume: number = 0.1) {
        if (this.audioProcessor) {
            const freq = 440 * 2 ** ((key - 49) / 12);
            
            this.audioProcessor.port.postMessage({
                msg: "start",
                key: key,
                freq: freq,
                volume: volume
            });
        }
    }

    public endNote(key: number) {
        this.audioProcessor?.port.postMessage({
            msg: "end",
            key: key
        })
    }
}