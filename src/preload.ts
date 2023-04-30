import { contextBridge, ipcRenderer } from "electron";

contextBridge.exposeInMainWorld("systemAudio", {
    newModule: (modName: string) => ipcRenderer.invoke("module.create", modName),
    openModuleConfig: (id: string) => ipcRenderer.send("module.config", id),
    releaseModule: (id: string) => ipcRenderer.send("module.remove", id),
    sendEventToModule: (id: string, ev: any) => ipcRenderer.send("module.event", id, ev),
    moduleConnect: (srcId: string, destId: string) => ipcRenderer.invoke("module.connect", srcId, destId),
    moduleDisconnect: (id: string) => ipcRenderer.invoke("module.disconnect", id)
});