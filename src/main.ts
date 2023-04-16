import { app, BrowserWindow, Menu, dialog, globalShortcut, ipcMain } from "electron";
import { AudioDevice, AudioModule, NoteEvent } from "./audio";
import path from "path";
import { Readable } from "stream";
import { BasicSynthesizer } from "./synth";

import { hello } from "../interface";
console.log(hello());

var mainWindow: BrowserWindow | null;
var audioDevice: AudioDevice | null = null;

const createWindow = () => {
    mainWindow = new BrowserWindow({
        width: 1280,
        height: 720,
        webPreferences: {
            preload: path.join(__dirname, "preload.js") 
        },
    });

    mainWindow.loadFile("app/index.html");
    mainWindow.webContents.openDevTools();
    
    mainWindow.on("close", (e) => {
        if (!mainWindow) return;

        const choice = dialog.showMessageBoxSync(mainWindow, {
            type: "question",
            buttons: ["Yes", "No"],
            title: "Confirm",
            message: "Are you sure you want to quit?" 
        });

        if (choice === 1) e.preventDefault();
        else mainWindow = null;
    });
}

const audioStart = () => {
    //audioDevice = new AudioDevice();

    let moduleMap: Map<number, AudioModule> = new Map();
    let modules: AudioModule[] = [];

    ipcMain.handle("module.create", (event, modName: string) => {
        let module: AudioModule;

        switch (modName) {
            case "basic-synth":
                module = new BasicSynthesizer();
                break;

            default:
                throw new Error(`unknown module name "${modName}"`);
        }

        // generate a unique random ID
        let id;
        do {
            id = (Math.random() * 2147483647) | 0;
        } while (moduleMap.has(id));

        moduleMap.set(id, module);

        modules.push(module);

        console.log(`create module ${id}`);

        return id;
    });

    ipcMain.on("module.remove", (event, id: number) => {
        let module = moduleMap.get(id);
        
        if (module) {
            moduleMap.delete(id);   

            console.log(`remove module ${id}`);

            let idx = modules.indexOf(module);
            if (idx >= 0) modules.splice(idx, 1);
        }
    });

    ipcMain.on("module.event", (event, id: number, ev: NoteEvent) => {
        let module = moduleMap.get(id);

        if (module) {
            module.event(ev);
        }
    })

    ipcMain.handle("audiorequest", (ev, sampleRate: number, numChannels: number, numSamples: number): Buffer[] => {
        let channels = []

        for (let ch = 0; ch < numChannels; ch++) {
            channels.push(new Float32Array(numSamples));
        }

        for (let mod of modules) {
            mod.process([], [channels], sampleRate);
        }

        return channels.map(v => Buffer.from(v.buffer));
    });

    /*
    audioDevice.process = function(channels) {
        for (let mod of modules) {
            mod.process([], [channels], this.sampleRate);
        }
    }
    */
}

app.whenReady().then(() => {
    const APP_MENU = Menu.buildFromTemplate([
        {
            label: "&File",
            submenu: [
                {
                    label: "New",
                    accelerator: "CmdOrCtrl+N",
                },
                {
                    label: "Save",
                    accelerator: "CmdOrCtrl+S",
                },
                {
                    label: "Save As...",
                    accelerator: "CmdOrCtrl+Shift+S",
                },
                { type: "separator" },
                {
                    label: "Export...",
                    click: () => {
                        console.log("export");
                    }
                },
                {
                    label: "Import...",
                    click: () => {
                        console.log("import");
                    }
                },
                { type: "separator" },
                { role: "quit" },
            ]
        },
        {
            label: "&Edit",
            submenu: [
                { role: "undo" },
                { role: "redo" },

                { type: "separator" },
                
                {
                    label: "Select All",
                    accelerator: "A"
                },
                {
                    label: "Select Channel",
                    accelerator: "Shift+A",
                },

                { type: "separator" },

                {
                    label: "Copy Pattern",
                    accelerator: "C",
                },
                {
                    label: "Paste Pattern Notes",
                    accelerator: "V",
                },
                {
                    label: "Paste Pattern Numbers",
                    accelerator: "CmdOrCtrl+Shift+V",
                },

                { type: "separator" },

                {
                    label: "Insert Bar",
                    accelerator: "Return",
                },
                {
                    label: "Delete Selected Bar",
                    accelerator: "Backspace",
                },
                {
                    label: "Delete Selected Channels",
                    accelerator: "CmdOrCtrl+Backspace",
                },
                {
                    label: "Duplicate Reused Patterns",
                    accelerator: "D",
                },

                { type: "separator" },

                {
                    label: "New Pattern",
                    accelerator: "N"
                },

                {
                    label: "Move Notes Up",
                    accelerator: "Up",
                },
                {
                    label: "Move Notes Down",
                    accelerator: "Down",
                },
                {
                    label: "Move All Notes Sideways...",
                    accelerator: "W",
                },
            ]
        },

        {
            label: "&Preferences",
            submenu: [
                { label: "Keep Current Pattern Selected" },
                { label: "Hear Preview of Added Notes" },
                { label: "Highlight Fifth" },
                { label: "Allow Adding Notes Not in Scale" },
                { label: "Show Notes From All Channels" },
                { label: "Show Octave Scroll Bar" },
                { label: "Always Fine Note Volume" },
                { label: "Show Playback Volume" },
                { type: "separator" },
                { label: "Set Theme..." },
                { label: "MIDI Configuration..." },
            ]
        },

        {
            label: "Help",
            submenu: [
                {
                    label: "About...",
                    click: () => {
                        if (mainWindow) {
                            const win = new BrowserWindow({
                                parent: mainWindow,
                                modal: true,
                                width: 300,
                                height: 300
                            });
                        
                            win.setMenuBarVisibility(false);
                            win.loadFile("app/about.html");
                        }
                    }
                },
                { label: "Shortcuts" },
            ]
        },

        {
            label: "Dev",
            submenu: [
                { role: "toggleDevTools" },
                { role: "reload" },
            ]
        }
    ]);
    
    Menu.setApplicationMenu(APP_MENU);

    createWindow();

    app.on("activate", () => {
        if (BrowserWindow.getAllWindows().length === 0) createWindow();
    });

    globalShortcut.register("CmdOrCtrl+N", () => {
        console.log("new song");
    });

    audioStart();
});

app.on("window-all-closed", () => {
    if (process.platform !== "darwin") {
        audioDevice?.close(true);
        app.quit();
    }
})