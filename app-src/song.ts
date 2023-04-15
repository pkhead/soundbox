export class Note {
    public length: number;
    public key: number;

    constructor(key: number, length: number) {
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
        this._maxPatterns = numPatterns;

        // TODO change array lengths
    }
}

export var SONG = new Song(4, 16, 4);