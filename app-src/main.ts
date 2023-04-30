import { TrackEditor } from "./track-editor";
import { PatternEditor } from "./pattern-editor";
import { Song } from "./song";
import { AudioModule, NoteModule } from "./synth";

const song = new Song(4, 16, 4);
song.initInstruments().then(() => {
    const playButton = document.getElementById("play-button") as HTMLButtonElement;
    const tempoField = document.getElementById("tempo-field") as HTMLInputElement;
    const instrumentOpen = document.getElementById("instrument-open") as HTMLOptionElement;
    const instrumentEdit = document.getElementById("instrument-edit") as HTMLButtonElement;

    tempoField.value = song.tempo.toString();

    const togglePlay = () => {
        if (song.isPlaying) {
            song.stop();
            playButton.innerText = "Play";
            playButton.className = "play-button";
        } else {
            song.play();
            playButton.innerText = "Pause";
            playButton.className = "pause-button";
        }
    };

    playButton.onclick = togglePlay;

    tempoField.onchange = () => {
        song.tempo = +tempoField.value;
    };

    // channel instrument controls
    instrumentOpen.onchange = () => {
        let openValue = instrumentOpen.value;
        instrumentOpen.value = "label";

        let channel = song.channels[song.selectedChannel];

        if (openValue === "plugin") {
            console.log("Open plugin");
        } else {
            channel.loadInstrument(openValue);
        }
    }

    window.addEventListener("keydown", (ev) => {
        if (document.activeElement && (document.activeElement.nodeName === "INPUT" || document.activeElement.nodeName == "TEXTBOX")) return;

        if (ev.code === "Space") {
            ev.preventDefault();
            togglePlay();
        }
    });

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

        return new PatternEditor(song, canvas);
    })();

    song.addEventListener("selectionChanged", () => {
        console.log("selection changed");
        patternEditor.stopAllNotes();
        patternEditor.redraw();
    });

    song.addEventListener("trackChanged", () => {
        console.log("track changed");
        patternEditor.redraw();
    });
});