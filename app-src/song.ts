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

    private trackChangedListeners: ((channelId: number, bar: number) => void)[] = [];
    private selectionChangedListeners: (() => void)[] = [];

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

    public addEventListener(evName: string, callback: (...args: any[]) => any) {
        switch (evName) {
            case "trackChanged":
                this.trackChangedListeners.push(callback);
                break;

            case "selectionChanged":
                this.selectionChangedListeners.push(callback);
                break;
                
            default:
                throw new Error(`unknown event name "${evName}"`);
        }
    }

    public dispatchEvent(evName: string, ...args: any[]) {
        switch (evName) {
            case "trackChanged":
                let [channelId, bar] = args;
                for (let listener of this.trackChangedListeners) listener(channelId, bar);
                break;

            case "selectionChanged":
                for (let listener of this.selectionChangedListeners) listener();
                break;

            default:
                throw new Error(`unknown evnt name "${evName}"`);
        }
    }
}

