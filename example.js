const globalMKH = require("./index");

var keyboard_keys = {
  'A': 'Single key',
  'Ctrl+C': 'Key combination',
  'Shift': 'Single key',
  'B': 'Single key',
  'Ctrl+V': 'Key combination',
  'Enter': 'Single key',
  'F5': 'Single key',
  'Ctrl+Shift+Esc': 'Key combination',
  'Space': 'Single key'
}

globalMKH.on("keyup", data => {
  console.log("keyup: ", data);
});

globalMKH.on("keydown", data => {
  console.log("keydown: ", data);
});

// Perform long running tasks.
globalMKH.on("keydown", data => { 
  console.time("keydownLong"); // Start the timer
  if (keyboard_keys.hasOwnProperty(data.keyName)) {
    runTask(data.keyName)
      .then((result) => {
        console.log(result);
      })
      .catch((error) => {
        console.error(error);
  }); 
  }
  console.timeEnd("keydownLong"); 
});

function runTask(keyName) {
  return new Promise((resolve, reject) => {
    // Perform your task asynchronously here using the passed argument
    setTimeout(() => {
      // You can use the passed argument in the task
      const result ="Keydown on registered key: '" + keyName + "'."
      resolve(result);
    }, 2000);
  });
}

globalMKH.on("mouseup", data => {
  console.log(data);
});

globalMKH.on("mousemove", data => {
 // console.log(data);
});

globalMKH.on("mousedown", data => {
  console.log(data);
});

globalMKH.on("mousewheel", data => {
  console.log(data);
});

setInterval(() => {
  if (!globalMKH.getMousePaused()) {
    console.error("Still listening...");
  }
}, 5000);

process.on("SIGBREAK", () => {
  if (globalMKH.getMousePaused()) {
    console.error("resuming mouse events");
    globalMKH.resumeglobalMKH();
  } else {
    console.error("pausing mouse events");
    globalMKH.pauseglobalMKH();
  }
});

console.error("Press Ctrl+Break to toggle listening");
