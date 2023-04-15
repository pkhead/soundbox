"use strict";
class Voice {
    constructor(key, freq, volume) {
        this.freq = freq;
        this.time = 0;
        this.volume = volume;
        this.key = key;
    }
    compute(buf) {
        const val = Math.sin(this.time) * this.volume;
        buf[0] = val;
        buf[1] = val;
        this.time += (this.freq * 2 * Math.PI) / sampleRate;
    }
}
class AudioProcessor extends AudioWorkletProcessor {
    static get parameterDescriptors() {
        return [
            {
                name: "freq",
                defaultValue: 440,
                minValue: 0,
                maxValue: 100000,
                automationRate: "a-rate"
            }
        ];
    }
    constructor() {
        super();
        this.time = 0;
        this.voices = [];
        this.voiceBuf = new Float32Array(2);
        this.port.onmessage = (e) => {
            const data = e.data;
            switch (data.msg) {
                case "start":
                    this.voices.push(new Voice(data.key, data.freq, data.volume));
                    break;
                case "end":
                    for (let i = 0; i < this.voices.length; i++) {
                        if (this.voices[i].key === data.key) {
                            this.voices.splice(i, 1);
                            break;
                        }
                    }
                    break;
            }
        };
    }
    process(inputs, outputs, parameters) {
        const output = outputs[0];
        for (let i = 0; i < output[0].length; i++) {
            let valL = 0;
            let valR = 0;
            for (let voice of this.voices) {
                voice.compute(this.voiceBuf);
                valL += this.voiceBuf[0];
                valR += this.voiceBuf[1];
            }
            output[0][i] = valL;
            output[1][i] = valR;
        }
        return true;
    }
}
registerProcessor("audio-processor", AudioProcessor);
//# sourceMappingURL=data:application/json;base64,eyJ2ZXJzaW9uIjozLCJmaWxlIjoic3ludGgtd29ya2VyLmpzIiwic291cmNlUm9vdCI6IiIsInNvdXJjZXMiOlsiLi4vd29ya2VyLXNyYy9tYWluLnRzIl0sIm5hbWVzIjpbXSwibWFwcGluZ3MiOiI7QUFBQSxNQUFNLEtBQUs7SUFNUCxZQUFZLEdBQVcsRUFBRSxJQUFZLEVBQUUsTUFBYztRQUNqRCxJQUFJLENBQUMsSUFBSSxHQUFHLElBQUksQ0FBQztRQUNqQixJQUFJLENBQUMsSUFBSSxHQUFHLENBQUMsQ0FBQztRQUNkLElBQUksQ0FBQyxNQUFNLEdBQUcsTUFBTSxDQUFDO1FBQ3JCLElBQUksQ0FBQyxHQUFHLEdBQUcsR0FBRyxDQUFDO0lBQ25CLENBQUM7SUFFTSxPQUFPLENBQUMsR0FBaUI7UUFDNUIsTUFBTSxHQUFHLEdBQUcsSUFBSSxDQUFDLEdBQUcsQ0FBQyxJQUFJLENBQUMsSUFBSSxDQUFDLEdBQUcsSUFBSSxDQUFDLE1BQU0sQ0FBQztRQUU5QyxHQUFHLENBQUMsQ0FBQyxDQUFDLEdBQUcsR0FBRyxDQUFDO1FBQ2IsR0FBRyxDQUFDLENBQUMsQ0FBQyxHQUFHLEdBQUcsQ0FBQztRQUViLElBQUksQ0FBQyxJQUFJLElBQUksQ0FBQyxJQUFJLENBQUMsSUFBSSxHQUFHLENBQUMsR0FBRyxJQUFJLENBQUMsRUFBRSxDQUFDLEdBQUcsVUFBVSxDQUFDO0lBQ3hELENBQUM7Q0FDSjtBQUVELE1BQU0sY0FBZSxTQUFRLHFCQUFxQjtJQUs5QyxNQUFNLEtBQUssb0JBQW9CO1FBQzNCLE9BQU87WUFDSDtnQkFDSSxJQUFJLEVBQUUsTUFBTTtnQkFDWixZQUFZLEVBQUUsR0FBRztnQkFDakIsUUFBUSxFQUFFLENBQUM7Z0JBQ1gsUUFBUSxFQUFFLE1BQU07Z0JBQ2hCLGNBQWMsRUFBRSxRQUFRO2FBQzNCO1NBQ0osQ0FBQTtJQUNMLENBQUM7SUFFRDtRQUNJLEtBQUssRUFBRSxDQUFDO1FBQ1IsSUFBSSxDQUFDLElBQUksR0FBRyxDQUFDLENBQUM7UUFDZCxJQUFJLENBQUMsTUFBTSxHQUFHLEVBQUUsQ0FBQztRQUNqQixJQUFJLENBQUMsUUFBUSxHQUFHLElBQUksWUFBWSxDQUFDLENBQUMsQ0FBQyxDQUFDO1FBRXBDLElBQUksQ0FBQyxJQUFJLENBQUMsU0FBUyxHQUFHLENBQUMsQ0FBQyxFQUFFLEVBQUU7WUFDeEIsTUFBTSxJQUFJLEdBQUcsQ0FBQyxDQUFDLElBQUksQ0FBQztZQUVwQixRQUFRLElBQUksQ0FBQyxHQUFHLEVBQUU7Z0JBQ2QsS0FBSyxPQUFPO29CQUNSLElBQUksQ0FBQyxNQUFNLENBQUMsSUFBSSxDQUFDLElBQUksS0FBSyxDQUFDLElBQUksQ0FBQyxHQUFHLEVBQUUsSUFBSSxDQUFDLElBQUksRUFBRSxJQUFJLENBQUMsTUFBTSxDQUFDLENBQUMsQ0FBQztvQkFDOUQsTUFBTTtnQkFFVixLQUFLLEtBQUs7b0JBQ04sS0FBSyxJQUFJLENBQUMsR0FBRyxDQUFDLEVBQUUsQ0FBQyxHQUFHLElBQUksQ0FBQyxNQUFNLENBQUMsTUFBTSxFQUFFLENBQUMsRUFBRSxFQUFFO3dCQUN6QyxJQUFJLElBQUksQ0FBQyxNQUFNLENBQUMsQ0FBQyxDQUFDLENBQUMsR0FBRyxLQUFLLElBQUksQ0FBQyxHQUFHLEVBQUU7NEJBQ2pDLElBQUksQ0FBQyxNQUFNLENBQUMsTUFBTSxDQUFDLENBQUMsRUFBRSxDQUFDLENBQUMsQ0FBQzs0QkFDekIsTUFBTTt5QkFDVDtxQkFDSjtvQkFDRCxNQUFNO2FBQ2I7UUFDTCxDQUFDLENBQUE7SUFDTCxDQUFDO0lBRUQsT0FBTyxDQUFDLE1BQXdCLEVBQUUsT0FBeUIsRUFBRSxVQUF3QztRQUNqRyxNQUFNLE1BQU0sR0FBRyxPQUFPLENBQUMsQ0FBQyxDQUFDLENBQUM7UUFFMUIsS0FBSyxJQUFJLENBQUMsR0FBRyxDQUFDLEVBQUUsQ0FBQyxHQUFHLE1BQU0sQ0FBQyxDQUFDLENBQUMsQ0FBQyxNQUFNLEVBQUUsQ0FBQyxFQUFFLEVBQUU7WUFDdkMsSUFBSSxJQUFJLEdBQUcsQ0FBQyxDQUFDO1lBQ2IsSUFBSSxJQUFJLEdBQUcsQ0FBQyxDQUFDO1lBRWIsS0FBSyxJQUFJLEtBQUssSUFBSSxJQUFJLENBQUMsTUFBTSxFQUFFO2dCQUMzQixLQUFLLENBQUMsT0FBTyxDQUFDLElBQUksQ0FBQyxRQUFRLENBQUMsQ0FBQztnQkFDN0IsSUFBSSxJQUFJLElBQUksQ0FBQyxRQUFRLENBQUMsQ0FBQyxDQUFDLENBQUM7Z0JBQ3pCLElBQUksSUFBSSxJQUFJLENBQUMsUUFBUSxDQUFDLENBQUMsQ0FBQyxDQUFDO2FBQzVCO1lBRUQsTUFBTSxDQUFDLENBQUMsQ0FBQyxDQUFDLENBQUMsQ0FBQyxHQUFHLElBQUksQ0FBQztZQUNwQixNQUFNLENBQUMsQ0FBQyxDQUFDLENBQUMsQ0FBQyxDQUFDLEdBQUcsSUFBSSxDQUFDO1NBQ3ZCO1FBRUQsT0FBTyxJQUFJLENBQUM7SUFDaEIsQ0FBQztDQUNKO0FBRUQsaUJBQWlCLENBQUMsaUJBQWlCLEVBQUUsY0FBYyxDQUFDLENBQUMifQ==