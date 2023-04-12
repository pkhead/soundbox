import { SequenceEditor } from "./seq-editor";
console.log("Hello, world!");

let sequenceEditor; {
    let canvas = document.getElementById("song-editor");
    if (!canvas || !(canvas instanceof HTMLCanvasElement)) {
        throw new Error("could not find #song-editor")
    }
    sequenceEditor = new SequenceEditor(canvas);
}