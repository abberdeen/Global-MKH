#include <napi.h>
#include <windows.h>
#include <string>
#include <iostream>
#include <atomic>

Napi::ThreadSafeFunction _tsfn;
HANDLE _hThread;
std::atomic_bool captureMouseMove = false;
// PostThreadMessage races with the actual thread; we'll get a thread ID
// and won't be able to post to it because it's "invalid" during the early
// lifecycle of the thread. To ensure that immediate pauses don't get dropped,
// we'll use this flag instead of distinct message IDs.
std::atomic_bool installEventHook = false;
DWORD dwThreadID = 0;

struct MouseEventContext {
    public:
        int nCode;
        WPARAM wParam;
        LONG ptX;
        LONG ptY;
        DWORD mouseData;
};

void onMainThread(Napi::Env env, Napi::Function function, MouseEventContext *pMouseEvent) {
    auto nCode = pMouseEvent->nCode;
    auto wParam = pMouseEvent->wParam;
    auto ptX = pMouseEvent->ptX;
    auto ptY = pMouseEvent->ptY;
    auto nMouseData = pMouseEvent->mouseData;

    delete pMouseEvent;

    if (nCode >= 0) {
        auto name = "";
        auto button = -1;

        // Isolate mouse movement, as it's more CPU intensive
        if (wParam == WM_MOUSEMOVE) {
            // Is mouse movement
            if(captureMouseMove.load()) {
                name = "mousemove";
            }
        } else {
            // Is not mouse movement

            // Determine event type
            if (wParam == WM_LBUTTONUP || wParam == WM_RBUTTONUP || wParam == WM_MBUTTONUP || WM_XBUTTONUP) {
                name = "mouseup";
            } else if (wParam == WM_LBUTTONDOWN || wParam == WM_RBUTTONDOWN || wParam == WM_MBUTTONDOWN || WM_XBUTTONDOWN ) {
                name = "mousedown";
            } else if (wParam == WM_MOUSEWHEEL || wParam == WM_MOUSEHWHEEL) {
                name = "mousewheel";
            }

            // Determine button
            if (wParam == WM_LBUTTONUP || wParam == WM_LBUTTONDOWN) {
                button = 1;
            } else if (wParam == WM_RBUTTONUP || wParam == WM_RBUTTONDOWN) {
                button = 2;
            } else if (wParam == WM_MBUTTONUP || wParam == WM_MBUTTONDOWN) {
                button = 3;
            } else if (wParam == WM_MOUSEWHEEL) {
                button = 0;
            } else if (wParam == WM_MOUSEHWHEEL) {
                button = 1;
            }
        }

        // Only proceed if an event was identified
        if (name != "") {
            Napi::HandleScope scope(env);

            auto x = Napi::Number::New(env, ptX);
            auto y = Napi::Number::New(env, ptY);

            auto mouseData = Napi::Number::New(env, nMouseData);

            // Yell back to NodeJS
            function.Call(env.Global(),
                    {Napi::String::New(env, name), x, y,
                     Napi::Number::New(env, button), mouseData});
        }
    }
}

LRESULT CALLBACK MouseHookCallback(int nCode, WPARAM wParam, LPARAM lParam) {

    // If not WM_MOUSEMOVE or WM_MOUSEMOVE has been requested, process event
    if(!(wParam == WM_MOUSEMOVE && !captureMouseMove.load())) {
        // Prepare data to be processed
        MSLLHOOKSTRUCT *data = (MSLLHOOKSTRUCT *)lParam;
        auto pMouseEvent = new MouseEventContext();
        pMouseEvent->nCode = nCode;
        pMouseEvent->wParam = wParam;
        pMouseEvent->ptX = data->pt.x;
        pMouseEvent->ptY = data->pt.y;
        pMouseEvent->mouseData = data->mouseData;

        // Process event on non-blocking thread
        _tsfn.NonBlockingCall(pMouseEvent, onMainThread);
    }

    // Let Windows continue with this event as normal
    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

DWORD WINAPI MouseHookThread(LPVOID lpParam) {
    MSG msg;
    HHOOK hook = installEventHook.load() ? SetWindowsHookEx(WH_MOUSE_LL, MouseHookCallback, NULL, 0) : NULL;

    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        if (msg.message != WM_USER) continue;
        if (!installEventHook.load() && hook != NULL) {
            if (!UnhookWindowsHookEx(hook)) break;
            hook = NULL;
        } else if (installEventHook.load() && hook == NULL) {
            hook = SetWindowsHookEx(WH_MOUSE_LL, MouseHookCallback, NULL, 0);
            if (hook == NULL) break;
        }
    }

    _tsfn.Release();
    return GetLastError();
}

Napi::Boolean createMouseHook(const Napi::CallbackInfo &info) {
    _hThread = CreateThread(NULL, 0, MouseHookThread, NULL, CREATE_SUSPENDED, &dwThreadID);
    _tsfn = Napi::ThreadSafeFunction::New(
        info.Env(),
        info[0].As<Napi::Function>(),
        "WH_MOUSE_LL Hook Thread",
        512,
        1,
        [] ( Napi::Env ) { CloseHandle(_hThread); }
    );

    ResumeThread(_hThread);
    return Napi::Boolean::New(info.Env(), true);
}

void enableMouseMove(const Napi::CallbackInfo &info) {
    captureMouseMove = true;
}

void disableMouseMove(const Napi::CallbackInfo &info) {
    captureMouseMove = false;
}

Napi::Boolean pauseMouseEvents(const Napi::CallbackInfo &info) {
    BOOL bDidPost = FALSE;
    if (dwThreadID != 0) {
        installEventHook = false;
        bDidPost = PostThreadMessageW(dwThreadID, WM_USER, NULL, NULL);
    }
    return Napi::Boolean::New(info.Env(), bDidPost);
}

Napi::Boolean resumeMouseEvents(const Napi::CallbackInfo &info) {
    BOOL bDidPost = FALSE;
    if (dwThreadID != 0) {
        installEventHook = true;
        bDidPost = PostThreadMessageW(dwThreadID, WM_USER, NULL, NULL);
    }
    return Napi::Boolean::New(info.Env(), bDidPost);
}

Napi::Object InitMouse(Napi::Env env, Napi::Object exports) {
    exports.Set(Napi::String::New(env, "createMouseHook"),
                Napi::Function::New(env, createMouseHook));

    exports.Set(Napi::String::New(env, "enableMouseMove"),
                Napi::Function::New(env, enableMouseMove));

    exports.Set(Napi::String::New(env, "disableMouseMove"),
                Napi::Function::New(env, disableMouseMove));

    exports.Set(Napi::String::New(env, "pauseMouseEvents"),
                Napi::Function::New(env, pauseMouseEvents));

    exports.Set(Napi::String::New(env, "resumeMouseEvents"),
                Napi::Function::New(env, resumeMouseEvents));

    return exports;
}
 