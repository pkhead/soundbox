import { TrackEditor } from "./track-editor";
import { PatternEditor } from "./pattern-editor";
import { Song } from "./song";
import { Synthesizer } from "./synth";

const song = new Song(4, 16, 4);

const synth = new Synthesizer();
synth.init().then(() => {
    synth.start();

    const trackEditor = (() => {
        let canvas = document.getElementById("track-editor");
        if (!(canvas && canvas instanceof HTMLCanvasElement)) {
            throw new Error("could not find #track-editor")
        }
    
        return new TrackEditor(song, canvas);
    })();
    
    const patternEditor = (() => {
        let canvas = document.getElementById("pattern-editor");
        if (!(canvas && canvas instanceof HTMLCanvasElement)) {
            throw new Error("could not find #pattern-editor");
        }
    
        return new PatternEditor(song, synth, canvas);
    })();
    
    song.addEventListener("selectionChanged", () => {
        console.log("selection changed");
        patternEditor.redraw();
    });
    
    song.addEventListener("trackChanged", () => {
        console.log("track changed");
        patternEditor.redraw();
    });
});
