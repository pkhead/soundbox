import { Colors } from "./colors";
import { SONG, Pattern, Note } from "./song";

const CELL_MARGIN = 2;

export class PatternEditor {
    private canvas: HTMLCanvasElement;
    private ctx: CanvasRenderingContext2D;

    private octaveRange: number;
    private numDivisions: number;

    private cellWidth: number = 0;
    private cellHeight: number = 0;

    private mouseGridX: number | null = null;
    private mouseGridY: number | null = null;
    private cursorWidth: number = 0.5;

    private selectedNote: Note | null = null; // the note currently hovered over
    private activeNote: Note | null = null; // the note currently being dragged
    private mouseStartX: number = 0; // the mouse x when drag started
    private activeNoteStart: number = 0; // the length of the note when drag started
    private noteDragMode: number = 0; // 0 = left, 1 = right

    constructor(canvas: HTMLCanvasElement) {
        const ctx = canvas.getContext("2d");

        if (!ctx) {
            throw new Error("could not get canvas context");
        }

        this.canvas = canvas;
        this.ctx = ctx;

        const canvasContainer = canvas.parentElement;
        if (!canvasContainer) throw new Error("could not find canvas container");

        this.octaveRange = 2;
        this.numDivisions = 8;

        const resize = () => {
            const styles = getComputedStyle(canvasContainer);
            
            canvas.width = canvasContainer.clientWidth - parseFloat(styles.paddingLeft) - parseFloat(styles.paddingRight);
            canvas.height = canvasContainer.clientHeight - parseFloat(styles.paddingTop) - parseFloat(styles.paddingBottom);
            this.cellWidth = Math.floor(canvas.width / this.numDivisions);
            this.cellHeight = Math.floor(canvas.height / (12 * this.octaveRange + 1));
            this.redraw();
        }

        window.addEventListener("mousemove", (ev) => {
            let increment = Math.min(this.cursorWidth, 0.25);
            const pattern = this.getPattern();

            let gridX = Math.floor(((ev.pageX - canvas.offsetLeft) / this.cellWidth - this.cursorWidth / 2) / increment) * increment;
            gridX = Math.max(gridX, 0);
            let gridFreeX = (ev.pageX - canvas.offsetLeft) / this.cellWidth;
            let gridY = Math.floor((canvas.height - ev.pageY - canvas.offsetTop) / this.cellHeight + 0.5);

            // prevent note collision/overlapping
            if (pattern) {
                for (let note of pattern.notes) {
                    if (note === this.activeNote || note.key !== gridY) continue;

                    // if note is on the left side
                    if (gridX < note.time && gridX + this.cursorWidth > note.time) {
                        gridX = note.time - this.cursorWidth;
                        break;
                    }
                    
                    // if note is on the right side
                    else if (gridX < note.time + note.length && gridX + this.cursorWidth > note.time + note.length) {
                        gridX = note.time + note.length;
                        break;
                    }
                }
            }

            const mouseGridX = this.mouseGridX;
            const mouseGridY = this.mouseGridY;

            if (this.activeNote) {
                if (this.noteDragMode === 0) {
                    this.activeNote.length = (gridX - this.mouseStartX) + this.activeNoteStart;
                }
            }

            if (mouseGridX !== null && mouseGridY !== null) {
                // draw area around old cursor position
                // so that there only appears to be one cursor outline at a time
                if (this.activeNote) {
                    this.redraw();
                } else {
                    let mouseGridX_int = Math.floor(mouseGridX);

                    for (let x = mouseGridX_int - 1; x <= Math.floor(mouseGridX_int + this.cursorWidth) + 1; x++) {
                        this.drawCell(mouseGridY - 1, x);
                        this.drawCell(mouseGridY, x);
                        this.drawCell(mouseGridY + 1, x);
                    }

                    this.drawNotes();
                }
            } else if (this.activeNote) {
                this.redraw();
            }

            if (gridX >= 0 && gridX < this.numDivisions && gridY >= 0 && gridY < 12 * this.octaveRange + 1) {
                // mouse is inside of pattern editor
                this.mouseGridX = gridX;
                this.mouseGridY = gridY;
                
                this.selectedNote = null;

                if (pattern) {
                    for (let note of pattern.notes) {
                        if (gridY == note.key && gridFreeX >= note.time && gridFreeX <= note.time + note.length) {
                            this.selectedNote = note;
                            break;
                        }
                    }
                }
                
                // draw mouse cursor
                this.drawCursor();
            } else {
                // mouse is not within inside pattern editor
                this.mouseGridX = null;
                this.mouseGridY = null;
            }
        });

        window.addEventListener("mousedown", (ev) => {
            if (this.mouseGridX !== null && this.mouseGridY !== null) {
                let pattern = this.getPattern();

                // if pattern is 0, create new pattern
                if (!pattern) {
                    let channel = SONG.channels[SONG.selectedChannel];
                    let pid = SONG.newPattern(SONG.selectedChannel);
                    channel.sequence[SONG.selectedBar] = pid;
                    pattern = channel.patterns[pid - 1];
                    SONG.dispatchEvent("trackChanged", SONG.selectedChannel, SONG.selectedBar);
                }

                if (this.selectedNote) {
                    this.activeNote = this.selectedNote;
                } else {
                    this.activeNote = pattern.addNote(this.mouseGridX, this.mouseGridY, this.cursorWidth);
                }

                this.mouseStartX = this.mouseGridX;
                this.activeNoteStart = this.activeNote.length;
                this.redraw();
            }
        });

        window.addEventListener("mouseup", (ev) => {
            if (this.activeNote) {
                this.cursorWidth = this.activeNote.length;
                this.activeNote = null;
                this.redraw();
            }
        });

        resize();
        const resizeObserver = new ResizeObserver(resize);
        resizeObserver.observe(canvasContainer);
    }

    // get the current pattern from the song data
    private getPattern(): Pattern | null {
        const channel = SONG.channels[SONG.selectedChannel];
        return channel.patterns[channel.sequence[SONG.selectedBar] - 1] || null;
    }

    private drawCell(row: number, col: number) {
        // don't draw if out of bounds
        if (col < 0 || col >= this.numDivisions || row < 0 || col >= 12 * this.octaveRange + 1) return;

        const {cellWidth, cellHeight, canvas, ctx} = this;
        let x = cellWidth * col;
        let y = canvas.height - cellHeight * (row + 1);

        ctx.fillStyle = Colors.background;
        ctx.fillRect(x, y, cellWidth, cellHeight);

        if (row % 12 === 0) {
            ctx.fillStyle = Colors.patternEditorOctaveColor;
        } else if (row % 12 === 7) {
            ctx.fillStyle = Colors.patternEditorFifthColor
        } else {
            ctx.fillStyle = Colors.patternEditorCellColor;
        }

        ctx.fillRect(
            (x + CELL_MARGIN),
            (y + CELL_MARGIN),
            (cellWidth - CELL_MARGIN),
            (cellHeight - CELL_MARGIN)
        );
    }

    private drawNotes() {
        const {canvas, ctx} = this;
        const pattern = this.getPattern();
        ctx.fillStyle = Colors.pitch[SONG.selectedChannel % Colors.pitch.length][1];

        if (pattern) {
            for (let note of pattern.notes) {
                let x = note.time * this.cellWidth;
                let y = canvas.height - (note.key + 1) * this.cellHeight;

                ctx.fillRect(x + 1, y + 1, this.cellWidth * note.length, this.cellHeight);
            }
        }
    }

    private drawCursor() {
        if (this.activeNote) return;

        const {canvas, ctx} = this;
        
        if (this.mouseGridX !== null && this.mouseGridY !== null) {
            let x = this.mouseGridX;
            let y = this.mouseGridY;
            let w = this.cursorWidth;

            if (this.selectedNote) {
                x = this.selectedNote.time;
                y = this.selectedNote.key;
                w = this.selectedNote.length;
            }

            ctx.strokeStyle = "white";
            ctx.lineWidth = 2;
            ctx.strokeRect(
                (x * this.cellWidth),
                (canvas.height - (y + 1) * this.cellHeight),
                this.cellWidth * w,
                this.cellHeight
            );
        }
    }

    public redraw() {
        const canvas = this.canvas;
        const ctx = this.ctx;

        ctx.fillStyle = Colors.background;
        ctx.fillRect(0, 0, canvas.width, canvas.height);

        for (let i = 0; i < 12 * this.octaveRange + 1; i++) {
            for (let j = 0; j < this.numDivisions; j++) {
                this.drawCell(i, j);
            }
        }

        this.drawNotes();
        this.drawCursor();
    }
}