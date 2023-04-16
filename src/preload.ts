import { contextBridge, ipcRenderer } from "electron";

contextBridge.exposeInMainWorld("systemAudio", {
    newModule: (modName: string) => ipcRenderer.invoke("module.create", modName),
    openModuleConfig: (id: string) => ipcRenderer.send("module.config", id),
    releaseModule: (id: string) => ipcRenderer.send("module.remove", id),
    sendEventToModule: (id: string, ev: any) => ipcRenderer.send("module.event", id, ev),
    requestAudio: (sampleRate: number, numChannels: number, numSamples: number) => ipcRenderer.invoke("audiorequest", sampleRate, numChannels, numSamples),
});