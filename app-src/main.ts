import { TrackEditor } from "./track-editor";
import { PatternEditor } from "./pattern-editor";

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