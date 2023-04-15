import { Colors } from "./colors";

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
                ctx.strokeRect(
                    (gridX * this.cellWidth + CELL_MARGIN),
                    (canvas.height - (gridY + 1) * this.cellHeight + CELL_MARGIN),
                    (this.cellWidth - CELL_MARGIN) * this.cursorWidth,
                    (this.cellHeight - CELL_MARGIN)
                );
            } else {
                // mouse is not within inside pattern editor
                this.mouseGridX = null;
                this.mouseGridY = null;
            }
        });

        resize();
        const resizeObserver = new ResizeObserver(resize);
        resizeObserver.observe(canvasContainer);
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

    private redraw() {
        const canvas = this.canvas;
        const ctx = this.ctx;

        ctx.fillStyle = Colors.background;
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