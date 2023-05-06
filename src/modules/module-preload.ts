import { contextBridge, ipcRenderer } from "electron";

let id: string;
const eventTarget = new EventTarget();

ipcRenderer.on("moduleid", (ev, _id: string) => {
    id = _id;
    eventTarget.dispatchEvent(new Event("ready"));
});

contextBridge.exposeInMainWorld("systemAudio", {
    onReady: (callback: () => void) => eventTarget.addEventListener("ready", callback),
    sendEventToModule: (ev: any) => ipcRenderer.send("module.event", id, ev),
    moduleParam: (action: string, name: string, value?: any) => ipcRenderer.invoke("module.param", id, action, name, value),
});