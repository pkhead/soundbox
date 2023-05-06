import { AudioDevice, AudioModule, NoteEvent, NoteEventType } from "../../audio"

export abstract class VoiceBase {
    public freq: number;
    public time: number;
    public volume: number;
    public key: number;

    constructor(key: number, freq: number, volume: number) {
        this.freq = freq;
        this.time = 0;
        this.volume = volume;
        this.key = key;
    }
    
    public abstract compute(sampleRate: number, buf: Float32Array, ...args: any[]): void;
}

type VoiceConstructor = (key: number, freq: number, volume: number) => VoiceBase

export abstract class SynthesizerBase extends AudioModule {
    protected voices: VoiceBase[];
    protected voiceBuf: Float32Array; // buffer for voice output
    //private voiceConstruct: VoiceConstructor

    constructor() {
        super();
        
        this.voices = [];
        this.voiceBuf = new Float32Array(2);
    }

    protected abstract createVoice(key: number, freq: number, volume: number): VoiceBase

    public event(event: NoteEvent) {
        switch (event.type) {
            case NoteEventType.NoteOn:
                const freq = 440 * 2 ** ((event.key - 69) / 12);

                this.voices.push(this.createVoice(event.key, freq, event.volume));
                break;

            case NoteEventType.NoteOff:
                for (let i = 0; i < this.voices.length; i++) {
                    if (this.voices[i].key === event.key) {
                        this.voices.splice(i, 1);
                        break;
                    }
                }
                break;
        }
    }

    public abstract process(_inputs: Float32Array[][], output: Float32Array[], device: AudioDevice): void;
}