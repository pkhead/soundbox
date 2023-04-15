import { isJSDocThisTag } from "../node_modules/typescript/lib/typescript";

export class Note {
    public length: number;
    public time: number;
    public key: number;

    constructor(time: number, key: number, length: number) {
        this.time = time;
        this.key = key;
        this.length = length;
    }
}

export class Pattern {
    public length: number;
    public notes: Note[];

    constructor() {
        this.notes = [];
        this.length = 1;
    }

    public isEmpty() {
        return this.notes.length === 0;
    }

    public addNote(time: number, key: number, length: number) {
        const note = new Note(time, key, length)
        this.notes.push(note);
        return note;
    }
}

export class Channel {
    public sequence: number[];
    public patterns: Pattern[];

    constructor(songLength: number, maxPatterns: number) {
        this.sequence = [];
        this.patterns = [];

        for (let i = 0; i < songLength; i++) {
            this.sequence.push(0);
        }

        for (let i = 0; i < maxPatterns; i++) {
            this.patterns.push(new Pattern());
        }
    }
}

export class Song {
    public channels: Channel[] = [];
    private _length: number;
    private _maxPatterns: number;

    public selectedChannel: number = 0;
    public selectedBar: number = 0;

    public position: number = 0;
    public isPlaying: boolean = false;
    public tempo: number = 120;

    private trackChangedListeners: ((channelId: number, bar: number) => void)[] = [];
    private selectionChangedListeners: (() => void)[] = [];
    private noteStartEventListeners: ((key: number, channelId: number) => void)[] = [];
    private noteEndEventListeners: ((key: number, channelId: number) => void)[] = [];
    private playheadMoveListeners: (() => void)[] = [];

    private activeNotes: {channel: number, note: Note}[] = [];

    constructor(numChannels: number, length: number, maxPatterns: number) {
        this._length = length;
        this._maxPatterns = maxPatterns;
        
        for (let ch_i = 0; ch_i < numChannels; ch_i++) {
            let ch = new Channel(this._length, this._maxPatterns);
            this.channels.push(ch);
        }
    }

    get length() {
        return this._length;
    }

    setLength(len: number) {
        this._length = len;

        // TODO change channel sequence arrays
    }

    get maxPatterns() {
        return this._maxPatterns;
    }

    setMaxPatterns(numPatterns: number) {
        if (numPatterns > this._maxPatterns) {
            // add patterns
            for (let channel of this.channels) {
                while (channel.patterns.length < numPatterns) channel.patterns.push(new Pattern());
            }
        } else if (numPatterns < this._maxPatterns) {
            // remove patterns
            for (let channel of this.channels) {
                channel.patterns.splice(numPatterns, this._maxPatterns - numPatterns);

                if (channel.patterns.length !== numPatterns) throw new Error("assertion failed in Song::setMaxPatterns, this._maxPatterns < numPatterns");
            }
        }
        
        this._maxPatterns = numPatterns;
    }

    public newPattern(channelId: number): number {
        const channel = this.channels[channelId];

        // find first unused pattern slot
        for (let i = 0; i < channel.patterns.length; i++) {
            if (channel.patterns[i].isEmpty()) {
                return i + 1;
            }
        }

        // no unused patterns found, create a new one
        this.setMaxPatterns(this.maxPatterns + 1);
        return this.maxPatterns;
    }

    private getNotesAt(bar: number, beats: number) {
        let notes: {channel: number, note: Note}[] = [];

        for (let ch = 0; ch < this.channels.length; ch++) {
            let channel = this.channels[ch];
            let pattern = channel.patterns[channel.sequence[bar] - 1];

            if (pattern) {
                for (let note of pattern.notes) {
                    if (beats >= note.time && beats < note.time + note.length) {
                        notes.push({
                            channel: ch,
                            note: note
                        });
                    }
                }
            }
        }

        return notes;
    }

    public play() {
        if (this.isPlaying) return;

        this.isPlaying = true;
        let last = performance.now();
        let prevNotes: {channel: number, note: Note}[] = [];

        const update = (now: DOMHighResTimeStamp) => {
            if (!this.isPlaying) return;
            requestAnimationFrame(update);
            const dt = (now - last) / 1000;

            this.position += dt * (this.tempo / 60) / 8; // 8 = beats per bar/measure

            let notes = this.getNotesAt(Math.floor(this.position), (this.position % 1) * 8);

            // on new notes, send note start event
            for (let note of notes) {
                let isNewNote = true;

                // if same note was found in old note list, it is not a new note
                for (let oldNote of prevNotes) {
                    if (note.note === oldNote.note) {
                        isNewNote = false;
                        break;
                    }
                }

                if (isNewNote) {
                    this.activeNotes.push(note);
                    this.dispatchEvent("noteStart", note.note.key, note.channel);
                }
            }

            // on notes that ended, send note end event
            for (let oldNote of prevNotes) {
                let isNoteEnded = true;

                // if same note was found in new note list, note has not ended
                for (let newNote of notes) {
                    if (newNote.note === oldNote.note) {
                        isNoteEnded = false;
                        break;
                    }
                }

                if (isNoteEnded) {
                    let index = this.activeNotes.findIndex(v => v.note === oldNote.note);
                    if (index >= 0) this.activeNotes.splice(index, 1);

                    this.dispatchEvent("noteEnd", oldNote.note.key, oldNote.channel);
                }
            }

            prevNotes = notes;

            this.dispatchEvent("playheadMoved");
            last = now;
        }

        update(last);
    }

    public stop() {
        this.isPlaying = false;
        this.position = Math.floor(this.position);

        // stop all currently playing notes
        for (let note of this.activeNotes) {
            this.dispatchEvent("noteEnd", note.note.key, note.channel);
        }
        this.activeNotes = [];

        this.dispatchEvent("playheadMoved");
    }

    public addEventListener(evName: string, callback: (...args: any[]) => any) {
        switch (evName) {
            case "trackChanged":
                this.trackChangedListeners.push(callback);
                break;

            case "selectionChanged":
                this.selectionChangedListeners.push(callback);
                break;

            case "playheadMoved":
                this.playheadMoveListeners.push(callback);
                break;

            case "noteStart":
                this.noteStartEventListeners.push(callback);
                break;

            case "noteEnd":
                this.noteEndEventListeners.push(callback);
                break;
            
            default:
                throw new Error(`unknown event name "${evName}"`);
        }
    }

    public dispatchEvent(evName: string, ...args: any[]) {
        switch (evName) {
            case "trackChanged": {
                let [channelId, bar] = args;
                for (let listener of this.trackChangedListeners) listener(channelId, bar);
                break;
            }

            case "selectionChanged":
                for (let listener of this.selectionChangedListeners) listener();
                break;

            case "playheadMoved":
                for (let listener of this.playheadMoveListeners) listener();
                break;

            case "noteStart": {
                let [key, channelId] = args;
                for (let listener of this.noteStartEventListeners) listener(key, channelId);
                break;
            }

            case "noteEnd": {
                let [key, channelId] = args;
                for (let listener of this.noteEndEventListeners) listener(key, channelId);
                break;
            }

            default:
                throw new Error(`unknown event name "${evName}"`);
        }
    }
}

