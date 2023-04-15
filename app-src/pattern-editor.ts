const BG_COLOR = "rgb(4, 4, 16)";
const CELL_BG_COLOR = "rgb(57, 62, 79)";
const CELL_FIFTH_BG_COLOR = "rgb(84, 84, 122)";
const CELL_OCTAVE_BG_COLOR = "rgb(114, 74, 145)";

export class PatternEditor {
    private canvas: HTMLCanvasElement;
    private ctx: CanvasRenderingContext2D;

    constructor(canvas: HTMLCanvasElement) {
        const ctx = canvas.getContext("2d");

        if (!ctx) {
            throw new Error("could not get canvas context");
        }

        this.canvas = canvas;
        this.ctx = ctx;

        const canvasContainer = canvas.parentElement;
        if (!canvasContainer) throw new Error("could not find canvas container");

        const resize = () => {
            const styles = getComputedStyle(canvasContainer);
            
            canvas.width = canvasContainer.clientWidth - parseFloat(styles.paddingLeft) - parseFloat(styles.paddingRight);
            canvas.height = canvasContainer.clientHeight - parseFloat(styles.paddingTop) - parseFloat(styles.paddingBottom);
            this.redraw();
        }

        resize();
        const resizeObserver = new ResizeObserver(resize);
        resizeObserver.observe(canvasContainer);
    }

    private redraw() {
        const canvas = this.canvas;
        const ctx = this.ctx;

        ctx.fillStyle = BG_COLOR;
        ctx.fillRect(0, 0, canvas.width, canvas.height);

        const octaveRange = 2;
        const numDivisions = 8;

        let cellWidth = canvas.width / numDivisions;
        let cellHeight = canvas.height / (12 * octaveRange + 1);
        let cellMargin = 2;

        console.log(cellHeight);
        
        for (let i = 0; i < 12 * octaveRange + 1; i++) {
            for (let j = 0; j < numDivisions; j++) {
                let x = cellWidth * j + cellMargin;
                let y = canvas.height - cellHeight * (i + 1) + cellMargin;

                if (i % 12 === 0) {
                    ctx.fillStyle = CELL_OCTAVE_BG_COLOR;
                } else if (i % 12 === 7) {
                    ctx.fillStyle = CELL_FIFTH_BG_COLOR
                } else {
                    ctx.fillStyle = CELL_BG_COLOR;
                }

                ctx.fillRect(x, y, cellWidth - cellMargin, cellHeight - cellMargin);
            }
        }
    }
}