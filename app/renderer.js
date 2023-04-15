define("colors", ["require", "exports"], function (require, exports) {
    "use strict";
    Object.defineProperty(exports, "__esModule", { value: true });
    exports.Colors = void 0;
    var Colors;
    (function (Colors) {
        Colors.background = "#040410";
        Colors.pitch = [
            // [dim, bright]
            ["#bb1111", "#ff5757"],
            ["rgb(187, 128, 17)", "rgb(255, 197, 89)"],
            ["rgb(136, 187, 17)", "rgb(205, 255, 89)"],
            ["rgb(25, 187, 17)", "rgb(97, 255, 89)"]
        ];
        Colors.patternEditorCellColor = "rgb(57, 62, 79)";
        Colors.patternEditorFifthColor = "rgb(84, 84, 122)";
        Colors.patternEditorOctaveColor = "rgb(114, 74, 145)";
    })(Colors = exports.Colors || (exports.Colors = {}));
});
define("song", ["require", "exports"], function (require, exports) {
    "use strict";
    Object.defineProperty(exports, "__esModule", { value: true });
    exports.SONG = exports.Song = exports.Channel = exports.Pattern = exports.Note = void 0;
    class Note {
        constructor(key, length) {
            this.key = key;
            this.length = length;
        }
    }
    exports.Note = Note;
    class Pattern {
        constructor() {
            this.notes = [];
            this.length = 1;
        }
        isEmpty() {
            return this.notes.length === 0;
        }
    }
    exports.Pattern = Pattern;
    class Channel {
        constructor(songLength, maxPatterns) {
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
    exports.Channel = Channel;
    class Song {
        constructor(numChannels, length, maxPatterns) {
            this.channels = [];
            this.selectedChannel = 0;
            this.selectedBar = 0;
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
        setLength(len) {
            this._length = len;
            // TODO change channel sequence arrays
        }
        get maxPatterns() {
            return this._maxPatterns;
        }
        setMaxPatterns(numPatterns) {
            this._maxPatterns = numPatterns;
            // TODO change array lengths
        }
    }
    exports.Song = Song;
    exports.SONG = new Song(4, 16, 4);
});
define("track-editor", ["require", "exports", "song", "colors"], function (require, exports, song_1, colors_1) {
    "use strict";
    Object.defineProperty(exports, "__esModule", { value: true });
    exports.TrackEditor = void 0;
    const CELL_WIDTH = 32;
    const CELL_HEIGHT = 32;
    const CELL_PADDING = 2;
    const CELL_WPAD = CELL_WIDTH + CELL_PADDING;
    const CELL_HPAD = CELL_HEIGHT + CELL_PADDING;
    class TrackEditor {
        drawCell(channel_i, bar) {
            const ctx = this.ctx;
            if (bar < 0 || bar >= song_1.SONG.length)
                return;
            const channel = song_1.SONG.channels[channel_i];
            if (!channel)
                return;
            ctx.font = "bold 18px monospace";
            ctx.textAlign = "center";
            ctx.textBaseline = "top";
            const colors = colors_1.Colors.pitch[channel_i % colors_1.Colors.pitch.length];
            // draw in background
            ctx.fillStyle = colors_1.Colors.background;
            ctx.fillRect(bar * CELL_WPAD, channel_i * CELL_HPAD, CELL_WIDTH + CELL_PADDING * 2, CELL_HEIGHT + CELL_PADDING * 2);
            let cellX = bar * CELL_WPAD + CELL_PADDING;
            let cellY = channel_i * CELL_HPAD + CELL_PADDING;
            let isSelected = bar === song_1.SONG.selectedBar && channel_i === song_1.SONG.selectedChannel;
            let pid = channel.sequence[bar] || 0;
            const pattern = channel.patterns[pid - 1];
            //let patternLength = channel.patterns[pid - 1]?.length || 1;
            const patternLength = 1;
            /*
            if (bar + patternLength > SONG.length) {
                patternLength = SONG.length - bar;
            }
            */
            let textColor = colors[0];
            if (isSelected) {
                ctx.fillStyle = colors[1];
                textColor = "black";
            }
            else {
                if (pid > 0) {
                    if (pattern.isEmpty()) {
                        ctx.fillStyle = "rgb(28, 29, 40)";
                        ;
                        textColor = colors[0];
                    }
                    else {
                        ctx.fillStyle = "#393e4f";
                        textColor = colors[1];
                    }
                }
                else {
                    ctx.fillStyle = colors_1.Colors.background;
                    textColor = colors[0];
                }
            }
            // draw in rectangle
            ctx.fillRect(cellX, cellY, CELL_WIDTH * patternLength + CELL_PADDING * (patternLength - 1), CELL_HEIGHT);
            ctx.fillStyle = textColor;
            ctx.fillText(pid.toString(), cellX + CELL_WIDTH / 2, cellY + 8);
        }
        redraw() {
            const ctx = this.ctx;
            let start = performance.now();
            ctx.fillStyle = "#040410";
            ctx.fillRect(0, 0, this.canvas.width, this.canvas.height);
            for (let channel = 0; channel < song_1.SONG.channels.length; channel++) {
                if (channel * CELL_HPAD + CELL_PADDING > this.canvas.height)
                    break;
                for (let bar = 0; bar < song_1.SONG.length; bar++) {
                    if (bar * CELL_WPAD + CELL_PADDING > this.canvas.width)
                        break;
                    this.drawCell(channel, bar);
                }
            }
            console.log(performance.now() - start);
        }
        drawCursor() {
            const ctx = this.ctx;
            const mouseGridX = this.mouseGridX;
            const mouseGridY = this.mouseGridY;
            if (mouseGridX !== null && mouseGridY !== null) {
                ctx.strokeStyle = "white";
                ctx.lineWidth = 2;
                ctx.strokeRect(mouseGridX * CELL_WPAD + CELL_PADDING, mouseGridY * CELL_HPAD + CELL_PADDING, CELL_WIDTH, CELL_HEIGHT);
            }
        }
        setPattern(channel_i, bar, pid) {
            const channel = song_1.SONG.channels[channel_i];
            /*
            // get max size between old and new pattern
            const oldSize = channel.patterns[channel.sequence[bar] - 1]?.length || 1;
            const newSize = channel.patterns[pid - 1]?.length || 1;
            channel.sequence[bar] = pid;
    
            for (let i = newSize - 1; i > 0; i--) {
                if (bar + i < SONG.length) {
                    this.setPattern(channel_i, bar + i, 0);
                }
            }
    
            for (let i = Math.max(oldSize, newSize) - 1; i >= 0; i--) {
                this.drawCell(channel_i, bar + i);
            }
            */
            channel.sequence[bar] = pid;
            this.drawCell(channel_i, bar);
            this.drawCursor();
        }
        // returns the length of the pattern at a bar
        // if pattern = 0, then return 1
        getBarLength(channel_i, bar) {
            var _a;
            const channel = song_1.SONG.channels[channel_i];
            return ((_a = channel.patterns[channel.sequence[bar] - 1]) === null || _a === void 0 ? void 0 : _a.length) || 1;
        }
        constructor(canvas) {
            this.mouseGridX = null;
            this.mouseGridY = null;
            this.cursorX = song_1.SONG.selectedBar;
            const canvasContainer = canvas.parentElement;
            const ctx = canvas.getContext("2d");
            if (!ctx) {
                throw new Error("could not get canvas context");
            }
            if (!canvasContainer) {
                throw new Error("canvas container");
            }
            this.canvas = canvas;
            this.ctx = ctx;
            const resize = () => {
                const styles = getComputedStyle(canvasContainer);
                canvas.width = canvasContainer.clientWidth - parseFloat(styles.paddingLeft) - parseFloat(styles.paddingRight);
                canvas.height = canvasContainer.clientHeight - parseFloat(styles.paddingTop) - parseFloat(styles.paddingBottom);
                this.redraw();
            };
            let typeBuf = "";
            window.addEventListener("mousemove", (ev) => {
                let gridX = Math.floor((ev.pageX - canvas.offsetLeft - CELL_PADDING) / CELL_WPAD);
                let gridY = Math.floor((ev.pageY - canvas.offsetTop - CELL_PADDING) / CELL_WPAD);
                const mouseGridX = this.mouseGridX;
                const mouseGridY = this.mouseGridY;
                if (mouseGridX !== null && mouseGridY !== null) {
                    ctx.fillStyle = colors_1.Colors.background;
                    ctx.fillRect((mouseGridX - 1) * CELL_WPAD + CELL_PADDING - 2, (mouseGridY - 1) * CELL_WPAD + CELL_PADDING - 2, CELL_WIDTH * 3 + CELL_PADDING * 2 + 4, CELL_HEIGHT * 3 + CELL_PADDING * 2 + 4);
                    this.drawCell(mouseGridY, mouseGridX);
                    this.drawCell(mouseGridY - 1, mouseGridX);
                    this.drawCell(mouseGridY + 1, mouseGridX);
                    this.drawCell(mouseGridY, mouseGridX - 1);
                    this.drawCell(mouseGridY, mouseGridX + 1);
                    this.drawCell(mouseGridY - 1, mouseGridX - 1);
                    this.drawCell(mouseGridY + 1, mouseGridX - 1);
                    this.drawCell(mouseGridY - 1, mouseGridX + 1);
                    this.drawCell(mouseGridY + 1, mouseGridX + 1);
                }
                if (gridX >= 0 && gridX < song_1.SONG.length && gridY >= 0 && gridY < song_1.SONG.channels.length) {
                    this.mouseGridX = gridX;
                    this.mouseGridY = gridY;
                    this.drawCursor();
                }
                else {
                    this.mouseGridX = null;
                    this.mouseGridY = null;
                }
            });
            canvas.addEventListener("mousedown", (ev) => {
                let prevCh = song_1.SONG.selectedChannel;
                let prevBar = song_1.SONG.selectedBar;
                if (this.mouseGridX !== null && this.mouseGridY !== null) {
                    song_1.SONG.selectedChannel = this.mouseGridY;
                    song_1.SONG.selectedBar = this.mouseGridX;
                    this.drawCell(prevCh, prevBar);
                    this.drawCell(this.mouseGridY, this.mouseGridX);
                }
                this.drawCursor();
            });
            const fixPatternOverlap = () => {
                // if selection is in the middle of a pattern, go to its beginning
                for (let i = 0; i < song_1.SONG.selectedBar;) {
                    let patternLen = this.getBarLength(song_1.SONG.selectedChannel, i);
                    if (patternLen > 1 && song_1.SONG.selectedBar <= i + patternLen - 1) {
                        song_1.SONG.selectedBar = i;
                        break;
                    }
                    i += patternLen;
                }
            };
            window.addEventListener("keydown", (ev) => {
                if (document.activeElement && (document.activeElement.nodeName === "input" || document.activeElement.nodeName == "textbox"))
                    return;
                console.log(ev.code);
                let prevBar = song_1.SONG.selectedBar;
                let prevCh = song_1.SONG.selectedChannel;
                switch (ev.code) {
                    case "ArrowRight":
                        ev.preventDefault();
                        typeBuf = "";
                        song_1.SONG.selectedBar += this.getBarLength(song_1.SONG.selectedChannel, song_1.SONG.selectedBar);
                        if (song_1.SONG.selectedBar >= song_1.SONG.length)
                            song_1.SONG.selectedBar = 0;
                        this.drawCell(prevCh, prevBar);
                        this.drawCell(song_1.SONG.selectedChannel, song_1.SONG.selectedBar);
                        this.drawCursor();
                        this.cursorX = song_1.SONG.selectedBar;
                        break;
                    case "ArrowLeft":
                        ev.preventDefault();
                        typeBuf = "";
                        song_1.SONG.selectedBar--;
                        if (song_1.SONG.selectedBar < 0)
                            song_1.SONG.selectedBar = song_1.SONG.length - 1;
                        //fixPatternOverlap();
                        this.drawCell(prevCh, prevBar);
                        this.drawCell(song_1.SONG.selectedChannel, song_1.SONG.selectedBar);
                        this.drawCursor();
                        this.cursorX = song_1.SONG.selectedBar;
                        break;
                    case "ArrowDown":
                        ev.preventDefault();
                        typeBuf = "";
                        song_1.SONG.selectedChannel++;
                        if (song_1.SONG.selectedChannel >= song_1.SONG.channels.length)
                            song_1.SONG.selectedChannel = 0;
                        song_1.SONG.selectedBar = this.cursorX;
                        //fixPatternOverlap();
                        this.drawCell(prevCh, prevBar);
                        this.drawCell(song_1.SONG.selectedChannel, song_1.SONG.selectedBar);
                        this.drawCursor();
                        break;
                    case "ArrowUp":
                        ev.preventDefault();
                        typeBuf = "";
                        song_1.SONG.selectedChannel--;
                        if (song_1.SONG.selectedChannel < 0)
                            song_1.SONG.selectedChannel = song_1.SONG.channels.length - 1;
                        song_1.SONG.selectedBar = this.cursorX;
                        //fixPatternOverlap();
                        this.drawCell(prevCh, prevBar);
                        this.drawCell(song_1.SONG.selectedChannel, song_1.SONG.selectedBar);
                        this.drawCursor();
                        break;
                    // shortcut: new pattern
                    /*
                    case "KeyN":
                        let channel = channels[selectedChannel];
    
                        if (selectedBar >= channel.sequence.length) {
                            while (channel.sequence.length < selectedBar) {
                                channel.sequence.push(0);
                            }
                        }
    
                        for (let i = 0; i < maxPatterns; i++) {
    
                        }
    
                        channel.sequence[selectedBar] = channel.patterns.length;
                        drawCell(selectedChannel, selectedBar);
                    */
                    case "Equal":
                        if (ev.ctrlKey) {
                            console.log("increment");
                            ev.preventDefault();
                            let channel = song_1.SONG.channels[song_1.SONG.selectedChannel];
                            let pid = channel.sequence[song_1.SONG.selectedBar] + 1;
                            if (pid > song_1.SONG.maxPatterns)
                                pid = 0;
                            this.setPattern(song_1.SONG.selectedChannel, song_1.SONG.selectedBar, pid);
                        }
                        break;
                    case "Minus":
                        if (ev.ctrlKey) {
                            console.log("decrement");
                            ev.preventDefault();
                            let channel = song_1.SONG.channels[song_1.SONG.selectedChannel];
                            let pid = channel.sequence[song_1.SONG.selectedBar] - 1;
                            if (pid < 0)
                                pid = song_1.SONG.maxPatterns - 1;
                            this.setPattern(song_1.SONG.selectedChannel, song_1.SONG.selectedBar, pid);
                        }
                        break;
                    default:
                        {
                            let channel = song_1.SONG.channels[song_1.SONG.selectedChannel];
                            let pi = channel.sequence[song_1.SONG.selectedBar];
                            if (ev.code.slice(0, 5) === "Digit") {
                                typeBuf += ev.code.slice(5);
                                if (+typeBuf > song_1.SONG.maxPatterns) {
                                    typeBuf = ev.code.slice(5);
                                    if (+typeBuf > song_1.SONG.maxPatterns) {
                                        break;
                                    }
                                }
                                this.setPattern(song_1.SONG.selectedChannel, song_1.SONG.selectedBar, +typeBuf);
                            }
                        }
                }
            });
            resize();
            const resizeObserver = new ResizeObserver(resize);
            resizeObserver.observe(canvasContainer);
        }
    }
    exports.TrackEditor = TrackEditor;
});
define("pattern-editor", ["require", "exports", "colors"], function (require, exports, colors_2) {
    "use strict";
    Object.defineProperty(exports, "__esModule", { value: true });
    exports.PatternEditor = void 0;
    const CELL_MARGIN = 2;
    class PatternEditor {
        constructor(canvas) {
            this.cellWidth = 0;
            this.cellHeight = 0;
            this.mouseGridX = null;
            this.mouseGridY = null;
            this.cursorWidth = 0.5;
            const ctx = canvas.getContext("2d");
            if (!ctx) {
                throw new Error("could not get canvas context");
            }
            this.canvas = canvas;
            this.ctx = ctx;
            const canvasContainer = canvas.parentElement;
            if (!canvasContainer)
                throw new Error("could not find canvas container");
            this.octaveRange = 2;
            this.numDivisions = 8;
            const resize = () => {
                const styles = getComputedStyle(canvasContainer);
                canvas.width = canvasContainer.clientWidth - parseFloat(styles.paddingLeft) - parseFloat(styles.paddingRight);
                canvas.height = canvasContainer.clientHeight - parseFloat(styles.paddingTop) - parseFloat(styles.paddingBottom);
                this.cellWidth = Math.floor(canvas.width / this.numDivisions);
                this.cellHeight = Math.floor(canvas.height / (12 * this.octaveRange + 1));
                this.redraw();
            };
            window.addEventListener("mousemove", (ev) => {
                let gridX = Math.floor((ev.pageX - canvas.offsetLeft) / this.cellWidth / this.cursorWidth) * this.cursorWidth;
                let gridY = Math.floor((canvas.height - ev.pageY - canvas.offsetTop) / this.cellHeight + 0.5);
                const mouseGridX = this.mouseGridX;
                const mouseGridY = this.mouseGridY;
                if (mouseGridX !== null && mouseGridY !== null) {
                    // draw area around old cursor position
                    // so that there only appears to be one cursor outline at a time
                    let mouseGridX_int = Math.floor(mouseGridX);
                    this.drawCell(mouseGridY, mouseGridX_int);
                    this.drawCell(mouseGridY - 1, mouseGridX_int);
                    this.drawCell(mouseGridY + 1, mouseGridX_int);
                    this.drawCell(mouseGridY, mouseGridX_int - 1);
                    this.drawCell(mouseGridY, mouseGridX_int + 1);
                    this.drawCell(mouseGridY - 1, mouseGridX_int - 1);
                    this.drawCell(mouseGridY + 1, mouseGridX_int - 1);
                    this.drawCell(mouseGridY - 1, mouseGridX_int + 1);
                    this.drawCell(mouseGridY + 1, mouseGridX_int + 1);
                }
                if (gridX >= 0 && gridX < this.numDivisions && gridY >= 0 && gridY < 12 * this.octaveRange + 1) {
                    // mouse is inside of pattern editor
                    this.mouseGridX = gridX;
                    this.mouseGridY = gridY;
                    // draw mouse cursor
                    ctx.strokeStyle = "white";
                    ctx.lineWidth = 2;
                    ctx.strokeRect((gridX * this.cellWidth + CELL_MARGIN), (canvas.height - (gridY + 1) * this.cellHeight + CELL_MARGIN), (this.cellWidth - CELL_MARGIN) * this.cursorWidth, (this.cellHeight - CELL_MARGIN));
                }
                else {
                    // mouse is not within inside pattern editor
                    this.mouseGridX = null;
                    this.mouseGridY = null;
                }
            });
            resize();
            const resizeObserver = new ResizeObserver(resize);
            resizeObserver.observe(canvasContainer);
        }
        drawCell(row, col) {
            // don't draw if out of bounds
            if (col < 0 || col >= this.numDivisions || row < 0 || col >= 12 * this.octaveRange + 1)
                return;
            const { cellWidth, cellHeight, canvas, ctx } = this;
            let x = cellWidth * col;
            let y = canvas.height - cellHeight * (row + 1);
            ctx.fillStyle = colors_2.Colors.background;
            ctx.fillRect(x, y, cellWidth, cellHeight);
            if (row % 12 === 0) {
                ctx.fillStyle = colors_2.Colors.patternEditorOctaveColor;
            }
            else if (row % 12 === 7) {
                ctx.fillStyle = colors_2.Colors.patternEditorFifthColor;
            }
            else {
                ctx.fillStyle = colors_2.Colors.patternEditorCellColor;
            }
            ctx.fillRect((x + CELL_MARGIN), (y + CELL_MARGIN), (cellWidth - CELL_MARGIN), (cellHeight - CELL_MARGIN));
        }
        redraw() {
            const canvas = this.canvas;
            const ctx = this.ctx;
            ctx.fillStyle = colors_2.Colors.background;
            ctx.fillRect(0, 0, canvas.width, canvas.height);
            for (let i = 0; i < 12 * this.octaveRange + 1; i++) {
                for (let j = 0; j < this.numDivisions; j++) {
                    /*let x = this.cellWidth * j + CELL_MARGIN;
                    let y = canvas.height - this.cellHeight * (i + 1) + CELL_MARGIN;
    
                    if (i % 12 === 0) {
                        ctx.fillStyle = CELL_OCTAVE_BG_COLOR;
                    } else if (i % 12 === 7) {
                        ctx.fillStyle = CELL_FIFTH_BG_COLOR
                    } else {
                        ctx.fillStyle = CELL_BG_COLOR;
                    }
    
                    ctx.fillRect(x, y, this.cellWidth - CELL_MARGIN, this.cellHeight - CELL_MARGIN);*/
                    this.drawCell(i, j);
                }
            }
        }
    }
    exports.PatternEditor = PatternEditor;
});
define("main", ["require", "exports", "track-editor", "pattern-editor"], function (require, exports, track_editor_1, pattern_editor_1) {
    "use strict";
    Object.defineProperty(exports, "__esModule", { value: true });
    const trackEditor = (() => {
        let canvas = document.getElementById("track-editor");
        if (!(canvas && canvas instanceof HTMLCanvasElement)) {
            throw new Error("could not find #track-editor");
        }
        return new track_editor_1.TrackEditor(canvas);
    })();
    const patternEditor = (() => {
        let canvas = document.getElementById("pattern-editor");
        if (!(canvas && canvas instanceof HTMLCanvasElement)) {
            throw new Error("could not find #pattern-editor");
        }
        return new pattern_editor_1.PatternEditor(canvas);
    })();
});
//# sourceMappingURL=renderer.js.map