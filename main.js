"use strict";
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
const electron_1 = require("electron");
const path_1 = __importDefault(require("path"));
const createWindow = () => {
    const win = new electron_1.BrowserWindow({
        width: 1280,
        height: 720,
        webPreferences: {
            preload: path_1.default.join(__dirname, "preload.js")
        },
    });
    win.loadFile("app/index.html");
    win.webContents.openDevTools();
    win.on("close", (e) => {
        const choice = electron_1.dialog.showMessageBoxSync(win, {
            type: "question",
            buttons: ["Yes", "No"],
            title: "Confirm",
            message: "Are you sure you want to quit?"
        });
        if (choice === 1)
            e.preventDefault();
    });
};
electron_1.app.whenReady().then(() => {
    const APP_MENU = electron_1.Menu.buildFromTemplate([
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
                { label: "About" },
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
    electron_1.Menu.setApplicationMenu(APP_MENU);
    createWindow();
    electron_1.app.on("activate", () => {
        if (electron_1.BrowserWindow.getAllWindows().length === 0)
            createWindow();
    });
    electron_1.globalShortcut.register("CmdOrCtrl+N", () => {
        console.log("new song");
    });
});
electron_1.app.on("window-all-closed", () => {
    if (process.platform !== "darwin")
        electron_1.app.quit();
});
//# sourceMappingURL=data:application/json;base64,eyJ2ZXJzaW9uIjozLCJmaWxlIjoibWFpbi5qcyIsInNvdXJjZVJvb3QiOiIiLCJzb3VyY2VzIjpbInNyYy9tYWluLnRzIl0sIm5hbWVzIjpbXSwibWFwcGluZ3MiOiI7Ozs7O0FBQUEsdUNBQTRFO0FBQzVFLGdEQUF3QjtBQUV4QixNQUFNLFlBQVksR0FBRyxHQUFHLEVBQUU7SUFDdEIsTUFBTSxHQUFHLEdBQUcsSUFBSSx3QkFBYSxDQUFDO1FBQzFCLEtBQUssRUFBRSxJQUFJO1FBQ1gsTUFBTSxFQUFFLEdBQUc7UUFDWCxjQUFjLEVBQUU7WUFDWixPQUFPLEVBQUUsY0FBSSxDQUFDLElBQUksQ0FBQyxTQUFTLEVBQUUsWUFBWSxDQUFDO1NBQzlDO0tBQ0osQ0FBQyxDQUFDO0lBRUgsR0FBRyxDQUFDLFFBQVEsQ0FBQyxnQkFBZ0IsQ0FBQyxDQUFDO0lBQy9CLEdBQUcsQ0FBQyxXQUFXLENBQUMsWUFBWSxFQUFFLENBQUM7SUFFL0IsR0FBRyxDQUFDLEVBQUUsQ0FBQyxPQUFPLEVBQUUsQ0FBQyxDQUFDLEVBQUUsRUFBRTtRQUNsQixNQUFNLE1BQU0sR0FBRyxpQkFBTSxDQUFDLGtCQUFrQixDQUFDLEdBQUcsRUFBRTtZQUMxQyxJQUFJLEVBQUUsVUFBVTtZQUNoQixPQUFPLEVBQUUsQ0FBQyxLQUFLLEVBQUUsSUFBSSxDQUFDO1lBQ3RCLEtBQUssRUFBRSxTQUFTO1lBQ2hCLE9BQU8sRUFBRSxnQ0FBZ0M7U0FDNUMsQ0FBQyxDQUFDO1FBRUgsSUFBSSxNQUFNLEtBQUssQ0FBQztZQUFFLENBQUMsQ0FBQyxjQUFjLEVBQUUsQ0FBQztJQUN6QyxDQUFDLENBQUMsQ0FBQztBQUNQLENBQUMsQ0FBQTtBQUVELGNBQUcsQ0FBQyxTQUFTLEVBQUUsQ0FBQyxJQUFJLENBQUMsR0FBRyxFQUFFO0lBQ3RCLE1BQU0sUUFBUSxHQUFHLGVBQUksQ0FBQyxpQkFBaUIsQ0FBQztRQUNwQztZQUNJLEtBQUssRUFBRSxPQUFPO1lBQ2QsT0FBTyxFQUFFO2dCQUNMO29CQUNJLEtBQUssRUFBRSxLQUFLO29CQUNaLFdB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