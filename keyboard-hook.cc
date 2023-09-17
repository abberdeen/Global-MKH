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
        {VK_SCROLL, "ScrollLock"},
        {VK_CONTROL, "Control"}};
}

std::string GetKeyName(KBDLLHOOKSTRUCT *keyboardHook)
{
    if (keyNames.empty())
    {
        init();
    }

    std::string modifiers = "";
    modifiers += (GetAsyncKeyState(VK_SHIFT) & 0x8000) ? std::string("Shift") : "";
    modifiers += (GetAsyncKeyState(VK_CONTROL) & 0x8000) ? (!modifiers.empty() ? "+" : "") + std::string("Control") : "";
    modifiers += (GetAsyncKeyState(VK_MENU) & 0x8000) ? (!modifiers.empty() ? "+" : "") + std::string("Alt") : "";
    modifiers += !modifiers.empty() ? "+" : "";

    auto it = keyNames.find(keyboardHook->vkCode);
    if (it != keyNames.end())
    {
        return modifiers + it->second;
    }

    char buffer[256];
    if (GetKeyNameTextA(keyboardHook->scanCode << 16, buffer, sizeof(buffer)))
    {
        return modifiers + buffer;
    }

    return "";
}

void onKeyboardMainThread(Napi::Env env, Napi::Function function, KeyboardEventContext *pKeyboardEvent)
{ 
    auto nCode = pKeyboardEvent->nCode;
    auto wParam = pKeyboardEvent->wParam;
    auto eventName = pKeyboardEvent->eventName;
    auto pKeyName = pKeyboardEvent->keyName;

    delete pKeyboardEvent;

    if (nCode >= 0)
    {
        Napi::HandleScope scope(env);

        // Yell back to NodeJS
        function.Call(env.Global(), {Napi::String::New(env, eventName), Napi::String::New(env, pKeyName)});
    }
}

LRESULT CALLBACK KeyboardHookCallback(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode == HC_ACTION)
    {
        KBDLLHOOKSTRUCT *keyboardHook = (KBDLLHOOKSTRUCT *)lParam;

        if (keyboardHook->vkCode != VK_CONTROL &&
            keyboardHook->vkCode != VK_SHIFT &&
            keyboardHook->vkCode != VK_MENU &&
            keyboardHook->vkCode != VK_LSHIFT &&
            keyboardHook->vkCode != VK_RSHIFT &&
            keyboardHook->vkCode != VK_LCONTROL &&
            keyboardHook->vkCode != VK_RCONTROL &&
            keyboardHook->vkCode != VK_LMENU &&
            keyboardHook->vkCode != VK_RMENU)
        {
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

                // Process event on non-blocking thread
                _tsfnKeyboard.NonBlockingCall(pKeyboardEvent, onKeyboardMainThread);
            }
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
