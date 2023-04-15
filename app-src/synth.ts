import { Song } from "./song";

export class Synthesizer {
    audioContext: AudioContext;
    audioProcessor: AudioWorkletNode | null = null;

    constructor() {
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

        window.addEventListener("mousemove", (ev) => {
            this.audioProcessor?.parameters.get("freq")?.setValueAtTime(ev.pageX, 0)
        });
    }

    public start() {
        this.audioProcessor?.connect(this.audioContext.destination);
    }

    public stop() {
        this.audioProcessor?.disconnect();
    }
}