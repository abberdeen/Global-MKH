# global-mkh

An event based, Global Keyboard and Mouse listener for Node.js (Windows only). Based off of [xanderfrangos/global-mouse-events](https://github.com/xanderfrangos/global-mouse-events) and [borecjeborec1/lepikEvents](https://github.com/borecjeborec1/lepikEvents).

## Installation

```cmd
npm i @abberdeen/global-mkh
```

## Usage
Import the module and register for the mouse events you'd like to listen to.

### Available event listeners

**`keyup`** / **`keydown`** — *Fires when a keyboard button is pressed / released.*\
Returns:
- **keyName:** Return the pressed single key name.
- **combination**  Return pressed key in the end along with the modifiers if they are (control, shift, and alt), separated by a '+'. 
- **shiftKey** Boolean is pressed Shift key.
- **ctrlKey** Boolean is pressed Control key.
- **altKey** Boolean is pressed Alt key.
- **metaKey** Boolean is pressed Meta (Win) key. 
- **crazyCombination** String key combinations, ordered by the order they are pressed.

**`mouseup`** / **`mousedown`** — *Fires when a mouse button is pressed / released.*\
Returns:
- **x:** The X position of the mouse, relative to the top left of the primary display.
- **y:** The Y position of the mouse, relative to the top left of the primary display.
- **button:** Which button was pressed. 1 is left-click. 2 is right-click. 3 is middle-click.

**`mousemove`** — *Fires when the mouse cursor is moved.*\
Returns:
- **x:** The X position of the mouse, relative to the top left of the primary display.
- **y:** The Y position of the mouse, relative to the top left of the primary display.

**`mousewheel`** — *Fires when the mouse wheel is scrolled. Some trackpads may not fire this event unless "Scroll inactive windows when I hover over them" is disabled in the Windows settings.*\
Returns:
- **x:** The X position of the mouse, relative to the top left of the primary display.
- **y:** The Y position of the mouse, relative to the top left of the primary display.
- **delta:** How much the mouse wheel was scrolled. Positive numbers are considered "up" and negative numbers are "down".
- **axis:** Whether the scroll was vertical or horizontal. 0 is vertical. 1 is horizontal.

### Example

```js
const globalMKH = require("global-mkh");

globalMKH.on("keyup", data => {
  console.log("keyup: ", data);
});

globalMKH.on("keydown", data => {
  console.log("keydown", data);
});

globalMKH.on("mouseup", data => {
  console.log(data);
});

globalMKH.on("mousemove", data => {
  console.log(data);
});

globalMKH.on("mousedown", data => {
  console.log(data);
});

globalMKH.on("mousewheel", data => {
  console.log(data);
});

```

### Available functions

- **`pauseMouseEvents`** — *Pauses all mouse events.*
- **`resumeMouseEvents`** — *Resumes all mouse events.*
- **`getPaused`** — *Returns the paused state as a boolean.*
