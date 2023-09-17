#include <napi.h>

// Include other files that define the addon's functionality
#include "keyboard-hook.cc"
#include "mouse-hook.cc"

Napi::Object InitAll(Napi::Env env, Napi::Object exports) {
  // Initialize and export the addon's functionality
  InitKeyboard(env, exports);
  InitMouse(env, exports);

  return exports;
}

NODE_API_MODULE(NODE_GYP_MODULE_NAME, InitAll)