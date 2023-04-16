class AudioProcessor extends AudioWorkletProcessor {
    private bufferL: Float32Array[] = [];
    private bufferR: Float32Array[] = [];

    constructor() {
        super();

        this.port.onmessage = (e) => {
            const data: Buffer[][] = e.data;

            for (let block of data) {
                this.bufferL.push(new Float32Array(block[0]));
                this.bufferR.push(new Float32Array(block[1]));
            }
        }
    }

    process(inputs: Float32Array[][], outputs: Float32Array[][], parameters: Record<string, Float32Array>): boolean {
        const output = outputs[0];

        let numWritten = 0;

        while (numWritten < output[0].length) {
            let blockL = this.bufferL.shift();
            if (!blockL) break;
            let blockR = this.bufferR.shift();
            if (!blockR) break;
        }

        return true;
    }
}

registerProcessor("audio-processor", AudioProcessor);