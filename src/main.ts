import { app, BrowserWindow, Menu, dialog, globalShortcut } from "electron";
import { AudioDevice } from "./audiodevice";
import path from "path";
import { Readable } from "stream";

var mainWindow: BrowserWindow | null;
const audioDevice = new AudioDevice();

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

    let samplesGenerated = 0;

    audioDevice.process = function(channels) {
        const t = (2*Math.PI * 440) / this.sampleRate;

        for (let i = 0; i < channels[0].length; i++) {
            const s = samplesGenerated + i;
            const val = 0.5 * Math.sin(t * s);
            
            channels[0][i] = val;
            channels[1][i] = val;
        }

        samplesGenerated += channels[0].length;
    }
});

app.on("window-all-closed", () => {
    if (process.platform !== "darwin") {
        audioDevice.close(true);
        app.quit();
    }
})