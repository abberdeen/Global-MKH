const globalMKH = require("./index");

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
