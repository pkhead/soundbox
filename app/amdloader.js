var define = (() => {
    let modules = new Map();

    let require = () => {
        throw new Error("what does require do?");
    }

    return (modName, deps, func) => {
        let exports = {};
        let args = [];

        for (let dep of deps) {
            if (dep === "require") args.push(require);
            else if (dep === "exports") args.push(exports);
            else args.push(modules.get(dep));
        }

        func(...args);

        modules.set(modName, exports);
    };
})();