
import { Pattern, SONG } from "./song";

const BG_COLOR = "#040410";
const CHANNEL_COLORS = [
    ["#bb1111", "#ff5757"]
];

const CELL_WIDTH = 32;
const CELL_HEIGHT = 32;
const CELL_PADDING = 2;
const CELL_WPAD = CELL_WIDTH + CELL_PADDING;
const CLEL_HPAD = CELL_HEIGHT + CELL_PADDING;

export class SequenceEditor {
    private canvas: HTMLCanvasElement;
    private ctx: CanvasRenderingContext2D;

    private mouseGridX: number | null = null;
    private mouseGridY: number | null = null;

    private drawCell(channel_i: number, bar: number) {
        const ctx = this.ctx;

        if (bar < 0 || bar >= SONG.length) return;
        const channel = SONG.channels[channel_i];
        if (!channel) return;

        ctx.font = "bold 18px monospace";
        ctx.textAlign = "center";
        ctx.textBaseline = "top";

        const colors = CHANNEL_COLORS[channel_i % CHANNEL_COLORS.length];

        let cellX = bar * CELL_WPAD + CELL_PADDING;
        let cellY = channel_i * CLEL_HPAD + CELL_PADDING;
        
        let isSelected = bar === SONG.selectedBar && channel_i === SONG.selectedChannel;
        let pid = channel.sequence[bar] || 0;

        let textColor = colors[0];

        if (isSelected) {
            ctx.fillStyle = colors[1];
            textColor = "black";
        } else {
            if (pid > 0) {
                ctx.fillStyle = "#393e4f";
                textColor = colors[1];
            } else {
                ctx.fillStyle = BG_COLOR;
                textColor = colors[0];
            }
        }
        
        ctx.fillRect(cellX, cellY, CELL_WIDTH, CELL_HEIGHT);
        ctx.fillStyle = textColor;
        ctx.fillText(pid.toString(), cellX + CELL_WIDTH / 2, cellY + 8);
    }

    private redraw() {
        const ctx = this.ctx;

        let start = performance.now();

        ctx.fillStyle = "#040410";
        ctx.fillRect(0, 0, this.canvas.width, this.canvas.height);

        for (let channel = 0; channel < SONG.channels.length; channel++) {
            if (channel * CLEL_HPAD + CELL_PADDING > this.canvas.height) break;

            for (let bar = 0; bar < SONG.length; bar++) {
                if (bar * CELL_WPAD + CELL_PADDING > this.canvas.width) break;

                this.drawCell(channel, bar);
            }
        }

        console.log(performance.now() - start);
    }

    private drawCursor() {
        const ctx = this.ctx;
        const mouseGridX = this.mouseGridX;
        const mouseGridY = this.mouseGridY;

        if (mouseGridX !== null && mouseGridY !== null) {
            ctx.strokeStyle = "white";
            ctx.lineWidth = 2;
            ctx.strokeRect(mouseGridX * CELL_WPAD + CELL_PADDING, mouseGridY * CLEL_HPAD + CELL_PADDING, CELL_WIDTH, CELL_HEIGHT);
        }
    }

    constructor(canvas: HTMLCanvasElement) {
        const canvasContainer = canvas.parentElement;
        const ctx = canvas.getContext("2d");

        if (!ctx) {
            throw new Error("Could not create 2D context");
        }

        if (!canvasContainer) {
            throw new Error("canvas container");
        }

        this.canvas = canvas;
        this.ctx = ctx;

        let resize = () => {
            canvas.width = canvasContainer.offsetWidth;
            canvas.height = canvasContainer.offsetHeight;
            this.redraw();
        }

        let typeBuf = "";

        window.addEventListener("mousemove", (ev) => {
            let gridX = Math.floor((ev.pageX - canvas.offsetLeft - CELL_PADDING) / CELL_WPAD);
            let gridY = Math.floor((ev.pageY - canvas.offsetTop - CELL_PADDING) / CELL_WPAD);

            const mouseGridX = this.mouseGridX;
            const mouseGridY = this.mouseGridY;

            if (mouseGridX !== null && mouseGridY !== null) {
                ctx.fillStyle = BG_COLOR;
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
            }

            if (gridX >= 0 && gridX < SONG.length && gridY >= 0 && gridY < SONG.channels.length) {
                this.mouseGridX = gridX;
                this.mouseGridY = gridY;
                this.drawCursor();
            } else {
                this.mouseGridX = null;
                this.mouseGridY = null;
            }
        });

        canvas.addEventListener("mousedown", (ev) => {
            let prevCh = SONG.selectedChannel;
            let prevBar = SONG.selectedBar;
            
            if (this.mouseGridX !== null && this.mouseGridY !== null) {
                SONG.selectedChannel = this.mouseGridY;
                SONG.selectedBar = this.mouseGridX;

                this.drawCell(prevCh, prevBar);
                this.drawCell(this.mouseGridY, this.mouseGridX);
            }

            this.drawCursor();
        });

        window.addEventListener("keydown", (ev) => {
            if (document.activeElement && (document.activeElement.nodeName === "input" || document.activeElement.nodeName == "textbox")) return;

            console.log(ev.code);

            let prevBar = SONG.selectedBar;
            let prevCh = SONG.selectedChannel;

            switch (ev.code) {
                case "ArrowRight":
                    ev.preventDefault();

                    typeBuf = "";
                    SONG.selectedBar++;
                    if (SONG.selectedBar >= SONG.length) SONG.selectedBar = 0;

                    this.drawCell(prevCh, prevBar);
                    this.drawCell(SONG.selectedChannel, SONG.selectedBar);
                    this.drawCursor();
                    break;

                case "ArrowLeft":
                    ev.preventDefault();

                    typeBuf = "";
                    SONG.selectedBar--;
                    if (SONG.selectedBar < 0) SONG.selectedBar = SONG.length - 1;
                    
                    this.drawCell(prevCh, prevBar);
                    this.drawCell(SONG.selectedChannel, SONG.selectedBar);
                    this.drawCursor();
                    break;

                case "ArrowDown":
                    ev.preventDefault();

                    typeBuf = "";
                    SONG.selectedChannel++;
                    if (SONG.selectedChannel >= SONG.channels.length) SONG.selectedChannel = 0;
                    
                    this.drawCell(prevCh, prevBar);
                    this.drawCell(SONG.selectedChannel, SONG.selectedBar);
                    this.drawCursor();
                    break;
                
                case "ArrowUp":
                    ev.preventDefault();

                    typeBuf = "";
                    SONG.selectedChannel--;
                    if (SONG.selectedChannel < 0) SONG.selectedChannel = SONG.channels.length - 1;
                    
                    this.drawCell(prevCh, prevBar);
                    this.drawCell(SONG.selectedChannel, SONG.selectedBar);
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

                        let channel = SONG.channels[SONG.selectedChannel];
                        channel.sequence[SONG.selectedBar]++;
                        if (channel.sequence[SONG.selectedBar] > SONG.maxPatterns) channel.sequence[SONG.selectedBar] = 0;

                        this.drawCell(SONG.selectedChannel, SONG.selectedBar);
                        this.drawCursor();
                    }

                    break;

                case "Minus":
                    if (ev.ctrlKey) {
                        console.log("decrement");
                        ev.preventDefault();

                        let channel = SONG.channels[SONG.selectedChannel];
                        channel.sequence[SONG.selectedBar]--;
                        if (channel.sequence[SONG.selectedBar] < 0) channel.sequence[SONG.selectedBar] = SONG.maxPatterns - 1;

                        this.drawCell(SONG.selectedChannel, SONG.selectedBar);
                        this.drawCursor();
                    }

                    break;

                default:
                    {
                        let channel = SONG.channels[SONG.selectedChannel];
                        let pi = channel.sequence[SONG.selectedBar];
                        
                        if (ev.code.slice(0, 5) === "Digit") {
                            typeBuf += ev.code.slice(5);

                            if (+typeBuf > SONG.maxPatterns) {
                                typeBuf = ev.code.slice(5);

                                if (+typeBuf > SONG.maxPatterns) {
                                    break;
                                }
                            }

                            SONG.channels[SONG.selectedChannel].sequence[SONG.selectedBar] = +typeBuf;
                            this.drawCell(SONG.selectedChannel, SONG.selectedBar);
                        }
                    }
            }
        })

        resize();
        window.addEventListener("resize", resize);
    }
}