"use strict";
class AudioProcessor extends AudioWorkletProcessor {
    constructor() {
        super();
        this.bufferL = [];
        this.bufferR = [];
        this.port.onmessage = (e) => {
            const data = e.data;
            for (let block of data) {
                this.bufferL.push(new Float32Array(block[0]));
                this.bufferR.push(new Float32Array(block[1]));
            }
        };
    }
    process(inputs, outputs, parameters) {
        const output = outputs[0];
        let numWritten = 0;
        while (numWritten < output[0].length) {
            let blockL = this.bufferL.shift();
            if (!blockL)
                break;
            let blockR = this.bufferR.shift();
            if (!blockR)
                break;
        }
        return true;
    }
}
registerProcessor("audio-processor", AudioProcessor);
//# sourceMappingURL=data:application/json;base64,eyJ2ZXJzaW9uIjozLCJmaWxlIjoic3ludGgtd29ya2VyLmpzIiwic291cmNlUm9vdCI6IiIsInNvdXJjZXMiOlsiLi4vd29ya2VyLXNyYy9tYWluLnRzIl0sIm5hbWVzIjpbXSwibWFwcGluZ3MiOiI7QUFBQSxNQUFNLGNBQWUsU0FBUSxxQkFBcUI7SUFJOUM7UUFDSSxLQUFLLEVBQUUsQ0FBQztRQUpKLFlBQU8sR0FBbUIsRUFBRSxDQUFDO1FBQzdCLFlBQU8sR0FBbUIsRUFBRSxDQUFDO1FBS2pDLElBQUksQ0FBQyxJQUFJLENBQUMsU0FBUyxHQUFHLENBQUMsQ0FBQyxFQUFFLEVBQUU7WUFDeEIsTUFBTSxJQUFJLEdBQWUsQ0FBQyxDQUFDLElBQUksQ0FBQztZQUVoQyxLQUFLLElBQUksS0FBSyxJQUFJLElBQUksRUFBRTtnQkFDcEIsSUFBSSxDQUFDLE9BQU8sQ0FBQyxJQUFJLENBQUMsSUFBSSxZQUFZLENBQUMsS0FBSyxDQUFDLENBQUMsQ0FBQyxDQUFDLENBQUMsQ0FBQztnQkFDOUMsSUFBSSxDQUFDLE9BQU8sQ0FBQyxJQUFJLENBQUMsSUFBSSxZQUFZLENBQUMsS0FBSyxDQUFDLENBQUMsQ0FBQyxDQUFDLENBQUMsQ0FBQzthQUNqRDtRQUNMLENBQUMsQ0FBQTtJQUNMLENBQUM7SUFFRCxPQUFPLENBQUMsTUFBd0IsRUFBRSxPQUF5QixFQUFFLFVBQXdDO1FBQ2pHLE1BQU0sTUFBTSxHQUFHLE9BQU8sQ0FBQyxDQUFDLENBQUMsQ0FBQztRQUUxQixJQUFJLFVBQVUsR0FBRyxDQUFDLENBQUM7UUFFbkIsT0FBTyxVQUFVLEdBQUcsTUFBTSxDQUFDLENBQUMsQ0FBQyxDQUFDLE1BQU0sRUFBRTtZQUNsQyxJQUFJLE1BQU0sR0FBRyxJQUFJLENBQUMsT0FBTyxDQUFDLEtBQUssRUFBRSxDQUFDO1lBQ2xDLElBQUksQ0FBQyxNQUFNO2dCQUFFLE1BQU07WUFDbkIsSUFBSSxNQUFNLEdBQUcsSUFBSSxDQUFDLE9BQU8sQ0FBQyxLQUFLLEVBQUUsQ0FBQztZQUNsQyxJQUFJLENBQUMsTUFBTTtnQkFBRSxNQUFNO1NBQ3RCO1FBRUQsT0FBTyxJQUFJLENBQUM7SUFDaEIsQ0FBQztDQUNKO0FBRUQsaUJBQWlCLENBQUMsaUJBQWlCLEVBQUUsY0FBYyxDQUFDLENBQUMifQ==