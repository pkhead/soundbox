import { contextBridge, ipcRenderer } from "electron";

let id: string;
let readyListeners: ((port: MessagePort) => void)[] = [];

ipcRenderer.on("init", (ev, _id: string) => {
    id = _id;
    
    debugger;

    for (let listener of readyListeners) {
        listener(ev.ports[0]);
    }

    readyListeners = [];
});

contextBridge.exposeInMainWorld("systemAudio", {
    onReady: (callback: (port: MessagePort) => void) => void readyListeners.push(callback),
    sendEventToModule: (ev: any) => ipcRenderer.send("module.event", id, ev),
    moduleParam: (action: string, name: string, value?: any) => ipcRenderer.invoke("module.param", id, action, name, value),
});