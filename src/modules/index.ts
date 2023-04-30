import { AudioModule } from "../audio";
import { DummySynthesizer } from "./synth/dummy";
import { WaveformSynthesizer } from "./synth/waveform";

export function createModule(name: string): AudioModule | undefined {
    switch (name) {
        case "synth.dummy": return new DummySynthesizer();
        case "synth.waveform": return new WaveformSynthesizer();
        default: return undefined;
    }
}