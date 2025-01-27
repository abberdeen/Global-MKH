"use strict";

const { EventEmitter } = require("events");
const addon = require("bindings")("global_mkh");

let mousePaused = true;
let keyboardPaused = true;
let modifiers = ['Shift', 'Ctrl', 'Alt'];
class GlobalMKH extends EventEmitter {
    constructor() {
        super();

        if (require("os").platform() !== "win32")
            return;

        let createdMouseListener = false;
        let createdKeyboardListener = false;
        let registeredEvents = [];

        this.on("newListener", event => {
            if (registeredEvents.indexOf(event) !== -1)
                return;

            // Enable WM_MOUSEMOVE capture if requested
            if (event === "mousemove") {
                addon.enableMouseMove();
            }

            if ((event === "mouseup" || event === "mousedown" || event === "mousemove" || event === "mousewheel") && !createdMouseListener) {
                // Careful: this currently "leaks" a thread every time it's called.
                // We should probably get around to fixing that.
                createdMouseListener = addon.createMouseHook((event, x, y, button, delta) => {
                    const payload = { x, y };
                    if (event === "mousewheel") {
                        payload.delta = FromInt32(delta) / 120;
                        payload.axis = button;
                    } else if (event === "mousedown" || event === "mouseup") {
                        payload.button = button;
                    }
                    this.emit(event, payload);
                });
                if (createdMouseListener) {
                    this.resumeMouseEvents();
                }
            } else if ((event === "keyup" || event === "keydown") && !createdKeyboardListener) {
                // Careful: this currently "leaks" a thread every time it's called.
                // We should probably get around to fixing that.
                createdKeyboardListener = addon.createKeyboardHook((event, keyName, shiftKey, ctrlKey, altKey, metaKey, crazyCombination) => {
                    var keys = [];

                    if (shiftKey || (keyName === 'Shift' && !shiftKey)) keys.push('Shift');
                    if (ctrlKey || (keyName === 'Ctrl' && !ctrlKey)) keys.push('Ctrl');
                    if (altKey || (keyName === 'Alt' && !altKey)) keys.push('Alt');

                    if (!modifiers.includes(keyName)) {
                        keys.push(keyName);
                    }

                    var combination = keys.join('+');

                    const payload = { keyName, combination, shiftKey, ctrlKey, altKey, metaKey };
                    if (event === "keyup") {
                        payload.crazyCombination = crazyCombination;
                    }
                    this.emit(event, payload);
                });
                if (createdKeyboardListener) {
                    this.resumeKeyboardEvents();
                }
            }
            else {
                return;
            }

            registeredEvents.push(event);
        });

        this.on("removeListener", event => {
            if (this.listenerCount(event) > 0)
                return;

            registeredEvents = registeredEvents.filter(x => x !== event);
            if (event === "mousemove") {
                addon.disableMouseMove();
            }
        });
    }

    getMousePaused() {
        return mousePaused;
    }

    pauseMouseEvents() {
        if (mousePaused) return false;
        mousePaused = true;
        return addon.pauseMouseEvents();
    }

    resumeMouseEvents() {
        if (!mousePaused) return false;
        mousePaused = false;
        return addon.resumeMouseEvents();
    }

    resumeKeyboardEvents() {
        if (!keyboardPaused) return false;
        keyboardPaused = false;
        return addon.resumeKeyboardEvents();
    }
}

function FromInt32(x) {
    var uint32 = x - Math.floor(x / 4294967296) * 4294967296;
    if (uint32 >= 2147483648) {
        return (uint32 - 4294967296) / 65536
    } else {
        return uint32 / 65536;
    }
}


module.exports = new GlobalMKH();