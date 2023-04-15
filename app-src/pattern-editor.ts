import { Colors } from "./colors";
import { SONG, Pattern, Note } from "./song";

const CELL_MARGIN = 2;
const KEY_WIDTH = 40;

export class PatternEditor {
    private canvas: HTMLCanvasElement;
    private ctx: CanvasRenderingContext2D;

    private octaveRange: number;
    private numDivisions: number;

    private cellWidth: number = 0;
    private cellHeight: number = 0;

    private mouseGridX: number | null = null;
    private mouseGridY: number | null = null;
    private mouseNoteX: number | null = null;
    private mouseInPianoKey: boolean = false;
    private cursorWidth: number = 0.5;

    private selectedNote: Note | null = null; // the note currently hovered over
    private activeNote: Note | null = null; // the note currently being dragged
    private mouseStartX: number = 0; // the mouse x when drag started
    private activeNoteStart: number = 0; // the length of the note when drag started
    private noteDragMode: number = 0; // 0 = left, 1 = right
    private mouseMoved: boolean = false; // if mouse did move on a drag

    private curPattern: Pattern | null = null;

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
            this.cellWidth = Math.floor((canvas.width - KEY_WIDTH) / this.numDivisions);
            this.cellHeight = Math.floor(canvas.height / (12 * this.octaveRange + 1));
            this.redraw();
            this.drawPianoKeys();
        }

        const onMouseMove = (ev: MouseEvent) => {
            const snapping = 0.25;

            let increment = Math.min(this.cursorWidth, snapping);
            const pattern = this.getPattern();

            const mouseX = ev.pageX - canvas.offsetLeft;
            const mouseY = ev.pageY - canvas.offsetTop;

            let gridFreeX = (mouseX - KEY_WIDTH) / this.cellWidth;
            let gridX = Math.floor((gridFreeX - this.cursorWidth / 2) / increment + 0.5) * increment;
            let gridY = Math.floor((canvas.height - mouseY) / this.cellHeight);

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

            gridX = Math.max(gridX, 0);

            const mouseGridX = this.mouseGridX;
            const mouseGridY = this.mouseGridY;

            // note dragging
            if (this.activeNote && mouseGridX) {
                if (
                    Math.floor((gridFreeX - this.mouseStartX) / snapping) * snapping !==
                    Math.floor((mouseGridX - this.mouseStartX) / snapping) * snapping
                ) {
                    this.mouseMoved = true;
                    console.log("mouse moved");
                }

                if (this.noteDragMode === 1) {
                    this.activeNote.length = Math.floor((gridFreeX - this.mouseStartX) / snapping) * snapping + this.activeNoteStart;
                    
                    if (this.activeNote.length < 0) {
                        // if length becomes negative, swap sides
                        this.activeNote.time += this.activeNote.length;
                        this.activeNote.length = -this.activeNote.length;
                        this.activeNoteStart = this.activeNote.length;
                        this.noteDragMode = 0;
                        this.mouseStartX = gridFreeX;
                    }
                    
                    // make sure length can't = 0
                    else if (this.activeNote.length < snapping) this.activeNote.length = snapping;
                    
                } else if (this.noteDragMode === 0) {
                    let end = this.activeNote.time + this.activeNote.length;
                    this.activeNote.length = Math.floor((this.mouseStartX - gridFreeX) / snapping) * snapping + this.activeNoteStart;
                    
                    if (this.activeNote.length < 0) {
                        // if length becomes negative, swap sides
                        this.activeNote.time -= this.activeNote.length;
                        this.activeNote.length = -this.activeNote.length;
                        this.activeNoteStart = this.activeNote.length;
                        this.noteDragMode = 1;
                        this.mouseStartX = gridFreeX;
                    }
                    
                    // make sure length can't = 0
                    else if (this.activeNote.length < snapping) this.activeNote.length = snapping;
                    else this.activeNote.time = end - this.activeNote.length;
                }
            }

            if (mouseGridX !== null && mouseGridY !== null && this.mouseNoteX !== null) {
                // draw area around old cursor position
                // so that there only appears to be one cursor outline at a time
                if (this.activeNote) {
                    this.redraw();
                } else {
                    let mouseGridX_int = Math.floor(this.mouseNoteX);

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

            if (gridFreeX >= 0 && gridFreeX < this.numDivisions && gridY >= 0 && gridY <= 12 * this.octaveRange) {
                // mouse is inside of pattern editor
                this.mouseGridX = gridFreeX;
                this.mouseNoteX = gridX;
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
                this.mouseNoteX = null;
                this.mouseGridY = null;
            }

            // if mouse is inside piano key area
            const oldMouseInPianoKey = this.mouseInPianoKey;

            if (mouseX >= 0 && mouseX < KEY_WIDTH) {
                this.mouseInPianoKey = true;
                this.mouseGridY = gridY;
            } else {
                this.mouseInPianoKey = false;
            }

            if (this.mouseInPianoKey || oldMouseInPianoKey !== this.mouseInPianoKey) this.drawPianoKeys();
        };

        window.addEventListener("mousemove", onMouseMove);

        window.addEventListener("mousedown", (ev) => {
            if (this.mouseGridX !== null && this.mouseGridY !== null && this.mouseNoteX !== null) {
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
                    // change the length of an existing note
                    this.activeNote = this.selectedNote;
                    this.mouseMoved = false;

                    // detect if dragging left/right sides of note
                    if (this.mouseGridX > this.activeNote.time + this.activeNote.length / 2) {
                        this.noteDragMode = 1;
                    } else {
                        this.noteDragMode = 0;
                    }
                } else {
                    // add a new note
                    this.activeNote = pattern.addNote(this.mouseNoteX, this.mouseGridY, this.cursorWidth);
                    this.noteDragMode = 1;
                    this.mouseMoved = true;
                }

                this.mouseStartX = this.mouseGridX;
                this.activeNoteStart = this.activeNote.length;
                
                this.redraw();
            }
        });

        window.addEventListener("mouseup", (ev) => {
            if (this.activeNote) {
                if (this.mouseMoved) {
                    this.cursorWidth = this.activeNote.length;
                } else {
                    // remove active note if mouse did not move
                    const pattern = this.getPattern();

                    if (pattern) {
                        pattern.notes.splice(pattern.notes.indexOf(this.activeNote), 1);
                    }
                }

                this.activeNote = null;
                this.redraw();

                onMouseMove(ev);
            }
        });

        resize();
        const resizeObserver = new ResizeObserver(resize);
        resizeObserver.observe(canvasContainer);
    }

    // get the current pattern from the song data
    private getPattern(): Pattern | null {
        const channel = SONG.channels[SONG.selectedChannel];
        const pattern = channel.patterns[channel.sequence[SONG.selectedBar] - 1] || null;

        // if pattern had changed, clear active/selected note data
        if (pattern !== this.curPattern) {
            this.activeNote = null;
            this.selectedNote = null;
        }

        this.curPattern = pattern;
        return pattern
    }

    private drawCell(row: number, col: number) {
        // don't draw if out of bounds
        if (col < 0 || col >= this.numDivisions || row < 0 || col >= 12 * this.octaveRange + 1) return;

        const {cellWidth, cellHeight, canvas, ctx} = this;
        let x = cellWidth * col + KEY_WIDTH;
        let y = canvas.height - cellHeight * (row + 1);

        ctx.fillStyle = Colors.background;
        ctx.fillRect(x, y, cellWidth, cellHeight);

        if (row % 12 === 0) {
            ctx.fillStyle = Colors.patternEditorOctaveColor;
        } else if (row % 12 === 7) {
            ctx.fillStyle = Colors.patternEditorFifthColor
        } else {
            ctx.fillStyle = Colors.interactableBgColor;
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
                let x = note.time * this.cellWidth + KEY_WIDTH;
                let y = canvas.height - (note.key + 1) * this.cellHeight;

                ctx.fillRect(x + 1, y + 1, this.cellWidth * note.length, this.cellHeight);
            }
        }
    }

    private drawCursor() {
        if (this.activeNote) return;

        const {canvas, ctx} = this;
        
        if (this.mouseNoteX !== null && this.mouseGridY !== null) {
            let x = this.mouseNoteX;
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
                (x * this.cellWidth) + KEY_WIDTH,
                (canvas.height - (y + 1) * this.cellHeight),
                this.cellWidth * w,
                this.cellHeight
            );
        }
    }

    private drawPianoKeys() {
        const {canvas, ctx} = this;
        const KEY_NAMES = ["C", "D♭", "D", "E♭", "E", "F", "F♯", "G", "A♭", "A", "B♭", "B"];
        const KEY_IS_ACCIDENTAL = [false, true, false, true, false, false, true, false, true, false, true, false];
        const TEXT_HEIGHT = 14;

        ctx.fillStyle = Colors.interactableBgColor;
        ctx.fillRect(0, 0, KEY_WIDTH, canvas.height);

        ctx.font = `${TEXT_HEIGHT}px monospace`;
        ctx.textBaseline = "top";
        ctx.textAlign = "left";

        for (let i = 0; i <= 12 * this.octaveRange; i++) {
            let x = 0;
            let y = canvas.height - this.cellHeight * (i + 1);
            let key = i % KEY_NAMES.length;

            ctx.fillStyle = KEY_IS_ACCIDENTAL[key] ? "#242430" : Colors.background;
            ctx.fillRect(x, y + CELL_MARGIN, KEY_WIDTH - CELL_MARGIN, this.cellHeight - CELL_MARGIN);

            ctx.fillStyle = "white";
            ctx.fillText(KEY_NAMES[key], 0.5 + 10, 0.5 + y + CELL_MARGIN + this.cellHeight / 2 - TEXT_HEIGHT / 2);
        }

        if (this.mouseInPianoKey && this.mouseGridY !== null) {
            let x = 0;
            let y = canvas.height - this.cellHeight * (this.mouseGridY + 1);

            ctx.strokeStyle = "white";
            ctx.lineWidth = 2;
            ctx.strokeRect(x, y, KEY_WIDTH, this.cellHeight);
        }
    }

    public redraw() {
        const canvas = this.canvas;
        const ctx = this.ctx;

        ctx.fillStyle = Colors.background;
        ctx.fillRect(0 + KEY_WIDTH, 0, canvas.width - KEY_WIDTH, canvas.height);

        for (let i = 0; i <= 12 * this.octaveRange; i++) {
            for (let j = 0; j < this.numDivisions; j++) {
                this.drawCell(i, j);
            }
        }

        this.drawNotes();
        this.drawCursor();
    }
}