import { TrackEditor } from "./track-editor";
import { PatternEditor } from "./pattern-editor";
import { SONG } from "./song";
import { Synthesizer } from "./synth";

const trackEditor = (() => {
    let canvas = document.getElementById("track-editor");
    if (!(canvas && canvas instanceof HTMLCanvasElement)) {
        throw new Error("could not find #track-editor")
    }

    return new TrackEditor(canvas);
})();

const patternEditor = (() => {
    let canvas = document.getElementById("pattern-editor");
    if (!(canvas && canvas instanceof HTMLCanvasElement)) {
        throw new Error("could not find #pattern-editor");
    }

    return new PatternEditor(canvas);
})();

SONG.addEventListener("selectionChanged", () => {
    console.log("selection changed");
    patternEditor.redraw();
});

SONG.addEventListener("trackChanged", () => {
    console.log("track changed");
    patternEditor.redraw();
});

const synth = new Synthesizer();
synth.init().then(() => {
    synth.start();
});