import { contextBridge, ipcRenderer } from "electron";

contextBridge.exposeInMainWorld("systemAudio", {
    newModule: async (modName: string) => await ipcRenderer.invoke("module.create", modName),
    openModuleConfig: (id: string) => ipcRenderer.send("module.config", id),
    releaseModule: (id: string) => ipcRenderer.send("module.remove", id),
    sendEventToModule: (id: string, ev: any) => ipcRenderer.send("module.event", id, ev),
});