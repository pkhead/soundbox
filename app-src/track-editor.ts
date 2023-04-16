
import { Channel, Pattern, Song } from "./song";
import { Colors } from "./colors";

const CELL_WIDTH = 32;
const CELL_HEIGHT = 32;
const CELL_PADDING = 2;
const CELL_WPAD = CELL_WIDTH + CELL_PADDING;
const CELL_HPAD = CELL_HEIGHT + CELL_PADDING;

export class TrackEditor {
    public song: Song;
    
    private canvas: HTMLCanvasElement;
    private ctx: CanvasRenderingContext2D;
    private viewportWidth: number = 0;
    private viewportHeight: number = 0;

    private mouseGridX: number | null = null;
    private mouseGridY: number | null = null;

    // this is used to keep selectedBar the same across channel selection changes
    private cursorX: number;

    private drawCell(channel_i: number, bar: number) {
        const ctx = this.ctx;

        if (bar < 0 || bar >= this.song.length) return;
        const channel = this.song.channels[channel_i];
        if (!channel) return;

        ctx.font = "bold 18px monospace";
        ctx.textAlign = "center";
        ctx.textBaseline = "top";

        const colors = Colors.pitch[channel_i % Colors.pitch.length];

        // draw in background
        ctx.fillStyle = Colors.background;
        ctx.fillRect(bar * CELL_WPAD, channel_i * CELL_HPAD, CELL_WIDTH + CELL_PADDING * 2, CELL_HEIGHT + CELL_PADDING * 2);

        let cellX = bar * CELL_WPAD + CELL_PADDING;
        let cellY = channel_i * CELL_HPAD + CELL_PADDING;
        
        let isSelected = bar === this.song.selectedBar && channel_i === this.song.selectedChannel;
        let pid = channel.sequence[bar] || 0;
        const pattern = channel.patterns[pid - 1];
        //let patternLength = channel.patterns[pid - 1]?.length || 1;
        const patternLength = 1;
        
        /*
        if (bar + patternLength > this.song.length) {
            patternLength = this.song.length - bar;
        }
        */

        let textColor = colors[0];

        if (isSelected) {
            ctx.fillStyle = colors[1];
            textColor = "black";
        } else {
            if (pid > 0) {
                if (pattern.isEmpty()) {
                    ctx.fillStyle = Colors.interactableBgColorDim;
                    textColor = colors[0];
                } else {
                    ctx.fillStyle = Colors.interactableBgColor; // "#393e4f";
                    textColor = colors[1];
                }
            } else {
                ctx.fillStyle = Colors.background;
                textColor = colors[0];
            }
        }
        
        // draw in rectangle
        ctx.fillRect(cellX, cellY, CELL_WIDTH * patternLength + CELL_PADDING * (patternLength - 1), CELL_HEIGHT);
        ctx.fillStyle = textColor;
        ctx.fillText(pid.toString(), cellX + CELL_WIDTH / 2, cellY + 8);
    }

    private redraw() {
        const ctx = this.ctx;

        let start = Date.now();

        ctx.fillStyle = Colors.background;
        ctx.fillRect(0, 0, this.viewportWidth, this.viewportHeight);

        for (let channel = 0; channel < this.song.channels.length; channel++) {
            if (channel * CELL_HPAD + CELL_PADDING > this.viewportHeight) break;

            for (let bar = 0; bar < this.song.length; bar++) {
                if (bar * CELL_WPAD + CELL_PADDING > this.viewportWidth) break;

                this.drawCell(channel, bar);
            }
        }

        this.drawPlayhead();

        console.log(`track editor redraw: ${Date.now() - start}ms`);
    }

    private drawCursor() {
        const ctx = this.ctx;
        const mouseGridX = this.mouseGridX;
        const mouseGridY = this.mouseGridY;

        if (mouseGridX !== null && mouseGridY !== null) {
            ctx.strokeStyle = "white";
            ctx.lineWidth = 2;
            ctx.strokeRect(mouseGridX * CELL_WPAD + CELL_PADDING, mouseGridY * CELL_HPAD + CELL_PADDING, CELL_WIDTH, CELL_HEIGHT);
        }
    }

    private drawPlayhead() {
        const {canvas, ctx, song} = this;

        ctx.fillStyle = "white";
        ctx.fillRect(song.position * CELL_WPAD + CELL_PADDING, 0, 2, this.viewportHeight);
    }

    private setPattern(channel_i: number, bar: number, pid: number) {
        const channel = this.song.channels[channel_i];

        /*
        // get max size between old and new pattern
        const oldSize = channel.patterns[channel.sequence[bar] - 1]?.length || 1;
        const newSize = channel.patterns[pid - 1]?.length || 1;
        channel.sequence[bar] = pid;

        for (let i = newSize - 1; i > 0; i--) {
            if (bar + i < this.song.length) {
                this.setPattern(channel_i, bar + i, 0);
            }
        }

        for (let i = Math.max(oldSize, newSize) - 1; i >= 0; i--) {
            this.drawCell(channel_i, bar + i);
        }
        */

        channel.sequence[bar] = pid;
    }

    // returns the length of the pattern at a bar
    // if pattern = 0, then return 1
    private getBarLength(channel_i: number, bar: number) {
        const channel = this.song.channels[channel_i];
        return channel.patterns[channel.sequence[bar] - 1]?.length || 1;
    }

    constructor(song: Song, canvas: HTMLCanvasElement) {
        this.song = song;
        this.cursorX = this.song.selectedBar;

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
            this.viewportWidth = canvasContainer.clientWidth - parseFloat(styles.paddingLeft) - parseFloat(styles.paddingRight);
            this.viewportHeight = canvasContainer.clientHeight - parseFloat(styles.paddingTop) - parseFloat(styles.paddingBottom);

            canvas.width = this.viewportWidth * window.devicePixelRatio;
            canvas.height = this.viewportHeight * window.devicePixelRatio;
            canvas.style.width = `${this.viewportWidth}px`;
            canvas.style.height = `${this.viewportHeight}px`;

            ctx.resetTransform();
            ctx.scale(window.devicePixelRatio, window.devicePixelRatio);

            this.redraw();
        }

        let typeBuf = "";

        window.addEventListener("mousemove", (ev) => {
            let gridX = Math.floor((ev.pageX - canvas.offsetLeft - CELL_PADDING) / CELL_WPAD);
            let gridY = Math.floor((ev.pageY - canvas.offsetTop - CELL_PADDING) / CELL_WPAD);

            const mouseGridX = this.mouseGridX;
            const mouseGridY = this.mouseGridY;

            if (mouseGridX !== null && mouseGridY !== null) {
                ctx.fillStyle = Colors.background;
                ctx.fillRect(
                    (mouseGridX - 1) * CELL_WPAD + CELL_PADDING - 2,
                    (mouseGridY - 1) * CELL_WPAD + CELL_PADDING - 2,
                    CELL_WIDTH * 3 + CELL_PADDING * 2 + 4,
                    CELL_HEIGHT * 3 + CELL_PADDING * 2 + 4
                );
                
                this.drawCell(mouseGridY, mouseGridX);
                this.drawCell(mouseGridY - 1, mouseGridX);
                this.drawCell(mouseGridY + 1, mouseGridX);
                this.drawCell(mouseGridY, mouseGridX - 1);
                this.drawCell(mouseGridY, mouseGridX + 1);

                this.drawCell(mouseGridY - 1, mouseGridX - 1);
                this.drawCell(mouseGridY + 1, mouseGridX - 1);
                this.drawCell(mouseGridY - 1, mouseGridX + 1);
                this.drawCell(mouseGridY + 1, mouseGridX + 1);

                this.drawPlayhead();
            }

            if (gridX >= 0 && gridX < this.song.length && gridY >= 0 && gridY < this.song.channels.length) {
                this.mouseGridX = gridX;
                this.mouseGridY = gridY;
                this.drawCursor();
            } else {
                this.mouseGridX = null;
                this.mouseGridY = null;
            }
        });

        canvas.addEventListener("mousedown", (ev) => {
            let prevCh = this.song.selectedChannel;
            let prevBar = this.song.selectedBar;
            
            if (this.mouseGridX !== null && this.mouseGridY !== null) {
                this.song.selectedChannel = this.mouseGridY;
                this.song.selectedBar = this.mouseGridX;
                this.song.dispatchEvent("selectionChanged");
                
                this.drawCell(prevCh, prevBar);
                this.drawCell(this.mouseGridY, this.mouseGridX);
            }

            this.drawPlayhead();
            this.drawCursor();
        });

        const fixPatternOverlap = () => {
            // if selection is in the middle of a pattern, go to its beginning
            for (let i = 0; i < this.song.selectedBar;) {
                let patternLen = this.getBarLength(this.song.selectedChannel, i);
                if (patternLen > 1 && this.song.selectedBar <= i + patternLen - 1) {
                    this.song.selectedBar = i;
                    break;
                }

                i += patternLen;
            }
        }

        window.addEventListener("keydown", (ev) => {
            if (document.activeElement && (document.activeElement.nodeName === "INPUT" || document.activeElement.nodeName == "TEXTBOX")) return;

            let prevBar = this.song.selectedBar;
            let prevCh = this.song.selectedChannel;

            switch (ev.code) {
                // move playhead left by one bar
                case "BracketLeft":
                    ev.preventDefault();

                    this.song.position--;
                    if (this.song.position < 0) this.song.position = this.song.length - 1
                    this.song.dispatchEvent("playheadMoved");
                    break;

                // move playhead right by one bar
                case "BracketRight":
                    ev.preventDefault();
                    
                    this.song.position = (this.song.position + 1) % this.song.length;
                    this.song.dispatchEvent("playheadMoved");
                    break;

                case "ArrowRight":
                    ev.preventDefault();

                    typeBuf = "";
                    this.song.selectedBar += this.getBarLength(this.song.selectedChannel, this.song.selectedBar);
                    if (this.song.selectedBar >= this.song.length) this.song.selectedBar = 0;
                    this.song.dispatchEvent("selectionChanged");

                    this.drawCell(prevCh, prevBar);
                    this.drawCell(this.song.selectedChannel, this.song.selectedBar);
                    this.drawCursor();
                    this.cursorX = this.song.selectedBar;


                    break;

                case "ArrowLeft":
                    ev.preventDefault();

                    typeBuf = "";
                    this.song.selectedBar--;
                    if (this.song.selectedBar < 0) this.song.selectedBar = this.song.length - 1;
                    this.song.dispatchEvent("selectionChanged");
                    //fixPatternOverlap();
                    
                    this.drawCell(prevCh, prevBar);
                    this.drawCell(this.song.selectedChannel, this.song.selectedBar);
                    this.drawCursor();
                    this.drawPlayhead();
                    this.cursorX = this.song.selectedBar;


                    break;

                case "ArrowDown":
                    ev.preventDefault();

                    typeBuf = "";
                    this.song.selectedChannel++;
                    if (this.song.selectedChannel >= this.song.channels.length) this.song.selectedChannel = 0;

                    this.song.selectedBar = this.cursorX;
                    this.song.dispatchEvent("selectionChanged");
                    
                    this.drawCell(prevCh, prevBar);
                    this.drawCell(this.song.selectedChannel, this.song.selectedBar);
                    this.drawPlayhead();
                    this.drawCursor();
                    break;
                
                case "ArrowUp":
                    ev.preventDefault();

                    typeBuf = "";
                    this.song.selectedChannel--;
                    if (this.song.selectedChannel < 0) this.song.selectedChannel = this.song.channels.length - 1;

                    this.song.selectedBar = this.cursorX;
                    this.song.dispatchEvent("selectionChanged");
                    
                    this.drawCell(prevCh, prevBar);
                    this.drawCell(this.song.selectedChannel, this.song.selectedBar);
                    this.drawPlayhead();
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
                        ev.preventDefault();

                        let channel = this.song.channels[this.song.selectedChannel];
                        let pid = channel.sequence[this.song.selectedBar] + 1;
                        if (pid > this.song.maxPatterns) pid = 0;

                        this.setPattern(this.song.selectedChannel, this.song.selectedBar, pid);
                        this.song.dispatchEvent("trackChanged", this.song.selectedChannel, this.song.selectedBar);
                    }

                    break;

                case "Minus":
                    if (ev.ctrlKey) {
                        ev.preventDefault();

                        let channel = this.song.channels[this.song.selectedChannel];
                        let pid = channel.sequence[this.song.selectedBar] - 1;
                        if (pid < 0) pid = this.song.maxPatterns - 1;

                        this.setPattern(this.song.selectedChannel, this.song.selectedBar, pid);
                        this.song.dispatchEvent("trackChanged", this.song.selectedChannel, this.song.selectedBar);
                    }

                    break;

                default:
                    {
                        let channel = this.song.channels[this.song.selectedChannel];
                        let pi = channel.sequence[this.song.selectedBar];
                        
                        if (ev.code.slice(0, 5) === "Digit") {
                            typeBuf += ev.code.slice(5);

                            if (+typeBuf > this.song.maxPatterns) {
                                typeBuf = ev.code.slice(5);

                                if (+typeBuf > this.song.maxPatterns) {
                                    break;
                                }
                            }

                            this.setPattern(this.song.selectedChannel, this.song.selectedBar, +typeBuf);
                            this.song.dispatchEvent("trackChanged", this.song.selectedChannel, this.song.selectedBar);
                        }
                    }
            }
        });

        this.song.addEventListener("trackChanged", (channel_i: number, bar: number) => {
            this.drawCell(channel_i, bar);
            this.drawPlayhead();
            this.drawCursor();
        });

        let lastSongPos = this.song.position;

        this.song.addEventListener("playheadMoved", () => {
            let bar = Math.floor(lastSongPos);

            for (let ch = 0; ch < this.song.channels.length; ch++) {
                this.drawCell(ch, bar - 1);
                this.drawCell(ch, bar);
                this.drawCell(ch, bar + 1);
            }

            ctx.fillStyle = Colors.background;
            let bottom = this.song.channels.length * CELL_HPAD + CELL_PADDING;
            ctx.fillRect(0, bottom, this.viewportWidth, this.viewportHeight - bottom);

            this.drawPlayhead();
            this.drawCursor();

            lastSongPos = this.song.position;
        });

        resize();
        const resizeObserver = new ResizeObserver(resize);
        resizeObserver.observe(canvasContainer);
    }
}