#include <napi.h>
#include <windows.h>
#include <string>
#include <iostream>
#include <atomic>
#include <unordered_map>

Napi::ThreadSafeFunction _tsfnKeyboard;
HANDLE _hThreadKeyboard;

// PostThreadMessage races with the actual thread; we'll get a thread ID
// and won't be able to post to it because it's "invalid" during the early
// lifecycle of the thread. To ensure that immediate pauses don't get dropped,
// we'll use this flag instead of distinct message IDs.
std::atomic_bool installKeyboardEventHook = false;
DWORD dwThreadIDKeyboard = 0;

static std::unordered_map<int, std::string> keyNames;

struct KeyboardEventContext
{
public:
    int nCode;
    WPARAM wParam;
    std::string eventName;
    std::string keyName; 
    bool shiftKey;
    bool ctrlKey;
    bool altKey;
    bool metaKey;
};

void init()
{
    keyNames = {
        {VK_HOME, "Home"},
        {VK_PRIOR, "PageUp"},
        {VK_NEXT, "PageDown"},
        {VK_END, "End"},
        {VK_INSERT, "Insert"},
        {VK_DELETE, "Delete"},
        {VK_RIGHT, "Right"},
        {VK_LEFT, "Left"},
        {VK_UP, "Up"},
        {VK_DOWN, "Down"},
        {VK_SNAPSHOT, "PrintScreen"},
        {VK_CAPITAL, "CapsLock"},
        {VK_SCROLL, "ScrollLock"},
        {VK_CONTROL, "Control"},
        {VK_LCONTROL, "Control"},
        {VK_RCONTROL, "Control"},
        {VK_MENU, "Alt"},
        {VK_LMENU, "Alt"},
        {VK_RMENU, "Alt"},
        {VK_SHIFT, "Shift"},
        {VK_LSHIFT, "Shift"},
        {VK_RSHIFT, "Shift"}
        };
}

bool isModifier(int vkCode)
{
    if (vkCode == VK_CONTROL &&
        vkCode == VK_LCONTROL &&
        vkCode == VK_RCONTROL &&
        vkCode == VK_SHIFT &&
        vkCode == VK_LSHIFT &&
        vkCode == VK_RSHIFT &&
        vkCode == VK_MENU &&
        vkCode == VK_LMENU &&
        vkCode == VK_RMENU)
    {
        return true;
    }

    return true;
}

std::string GetKeyName(KBDLLHOOKSTRUCT *keyboardHook)
{
    if (keyNames.empty())
    {
        init();
    }
 
    auto it = keyNames.find(keyboardHook->vkCode);
    if (it != keyNames.end())
    {
        return it->second;
    }

    char buffer[256];
    if (GetKeyNameTextA(keyboardHook->scanCode << 16, buffer, sizeof(buffer)))
    {
        return buffer;
    }

    return "";
}

void onKeyboardMainThread(Napi::Env env, Napi::Function function, KeyboardEventContext *pKeyboardEvent)
{
    auto nCode = pKeyboardEvent->nCode;
    auto wParam = pKeyboardEvent->wParam;
    auto eventName = pKeyboardEvent->eventName;
    auto pKeyName = pKeyboardEvent->keyName; 
    auto pAltKey = pKeyboardEvent->altKey;
    auto pCtrlKey = pKeyboardEvent->ctrlKey;
    auto pShiftKey = pKeyboardEvent->shiftKey;
    auto pMetaKey = pKeyboardEvent->metaKey;

    delete pKeyboardEvent;

    if (nCode >= 0)
    {
        Napi::HandleScope scope(env);

        // Yell back to NodeJS
        function.Call(env.Global(), {Napi::String::New(env, eventName),
                                     Napi::String::New(env, pKeyName), 
                                     Napi::Boolean::New(env, pShiftKey),
                                     Napi::Boolean::New(env, pCtrlKey),
                                     Napi::Boolean::New(env, pAltKey),
                                     Napi::Boolean::New(env, pMetaKey)});
    }
}

LRESULT CALLBACK KeyboardHookCallback(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode == HC_ACTION)
    {
        KBDLLHOOKSTRUCT *keyboardHook = (KBDLLHOOKSTRUCT *)lParam;

        std::string eventName = "";
        if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)
        {
            eventName = "keydown";
        }
        else if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP)
        {
            eventName = "keyup";
        }

        std::string keyName = GetKeyName(keyboardHook);

        if (!keyName.empty() && !eventName.empty())
        {
            auto pKeyboardEvent = new KeyboardEventContext();
            pKeyboardEvent->nCode = nCode;
            pKeyboardEvent->wParam = wParam;
            pKeyboardEvent->eventName = eventName;
            pKeyboardEvent->keyName = keyName; 
            pKeyboardEvent->altKey = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0 || (GetAsyncKeyState(VK_LMENU) & 0x8000) != 0 || (GetAsyncKeyState(VK_RMENU) & 0x8000) != 0;
            pKeyboardEvent->shiftKey = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0 || (GetAsyncKeyState(VK_LSHIFT) & 0x8000) != 0  || (GetAsyncKeyState(VK_RSHIFT) & 0x8000) != 0;
            pKeyboardEvent->ctrlKey = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0 || (GetAsyncKeyState(VK_LCONTROL) & 0x8000) != 0 || (GetAsyncKeyState(VK_RCONTROL) & 0x8000) != 0;
            pKeyboardEvent->metaKey = (GetAsyncKeyState(VK_LWIN) & 0x8000) || (GetAsyncKeyState(VK_RWIN) & 0x8000);

            // Process event on non-blocking thread
            _tsfnKeyboard.NonBlockingCall(pKeyboardEvent, onKeyboardMainThread);
        }
    }
    // Let Windows continue with this event as normal
    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

DWORD WINAPI KeyboardHookThread(LPVOID lpParam)
{
    MSG msg;
    HHOOK hook = installKeyboardEventHook.load() ? SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardHookCallback, NULL, 0) : NULL;

    while (GetMessage(&msg, NULL, 0, 0) > 0)
    {
        if (msg.message != WM_USER)
            continue;
        if (!installKeyboardEventHook.load() && hook != NULL)
        {
            if (!UnhookWindowsHookEx(hook))
                break;
            hook = NULL;
        }
        else if (installKeyboardEventHook.load() && hook == NULL)
        {
            hook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardHookCallback, NULL, 0);
            if (hook == NULL)
                break;
        }
    }

    _tsfnKeyboard.Release();
    return GetLastError();
}

Napi::Boolean createKeyboardHook(const Napi::CallbackInfo &info)
{
    _hThreadKeyboard = CreateThread(NULL, 0, KeyboardHookThread, NULL, CREATE_SUSPENDED, &dwThreadIDKeyboard);
    _tsfnKeyboard = Napi::ThreadSafeFunction::New(
        info.Env(),
        info[0].As<Napi::Function>(),
        "WH_KEYBOARD_LL Hook Thread",
        512,
        1,
        [](Napi::Env)
        { CloseHandle(_hThreadKeyboard); });

    ResumeThread(_hThreadKeyboard);
    return Napi::Boolean::New(info.Env(), true);
}

Napi::Boolean pauseKeyboardEvents(const Napi::CallbackInfo &info)
{
    BOOL bDidPost = FALSE;
    if (dwThreadIDKeyboard != 0)
    {
        installKeyboardEventHook = false;
        bDidPost = PostThreadMessageW(dwThreadIDKeyboard, WM_USER, NULL, NULL);
    }
    return Napi::Boolean::New(info.Env(), bDidPost);
}

Napi::Boolean resumeKeyboardEvents(const Napi::CallbackInfo &info)
{
    BOOL bDidPost = FALSE;
    if (dwThreadIDKeyboard != 0)
    {
        installKeyboardEventHook = true;
        bDidPost = PostThreadMessageW(dwThreadIDKeyboard, WM_USER, NULL, NULL);
    }
    return Napi::Boolean::New(info.Env(), bDidPost);
}

Napi::Object InitKeyboard(Napi::Env env, Napi::Object exports)
{
    exports.Set(Napi::String::New(env, "createKeyboardHook"),
                Napi::Function::New(env, createKeyboardHook));

    exports.Set(Napi::String::New(env, "resumeKeyboardEvents"),
                Napi::Function::New(env, resumeKeyboardEvents));

    return exports;
}
