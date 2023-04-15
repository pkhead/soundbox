import { TrackEditor } from "./track-editor";
console.log("Hello, world!");

let trackEditor; {
    let canvas = document.getElementById("song-editor");
    if (!canvas || !(canvas instanceof HTMLCanvasElement)) {
        throw new Error("could not find #song-editor")
    }
    trackEditor = new TrackEditor(canvas);
}