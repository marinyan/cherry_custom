/* Cherry 1.4.3: in-process UI-thread wheel adapter, no extra DLL or CRT.
 * Original WinMain / import thunk RVAs are from src.rar/chysharp-mwhook.asm.
 * The installer verifies the exact original executable before using them.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <intrin.h>
#include "wheel-plan.h"

static HHOOK hook;
static HWND previousTarget[2];
static HWND previousBar[2];
static int remainder[2];
static DWORD uiThread;
static HHOOK (WINAPI *pSetHook)(int, HOOKPROC, HINSTANCE, DWORD);
static BOOL (WINAPI *pUnhook)(HHOOK);
static LRESULT (WINAPI *pNext)(HHOOK, int, WPARAM, LPARAM);
static BOOL (WINAPI *pSettings)(UINT, UINT, PVOID, UINT);
static HWND (WINAPI *pFromPoint)(POINT);
static LONG (WINAPI *pStyle)(HWND, int);
static HWND (WINAPI *pParent)(HWND);
static HWND (WINAPI *pWindow)(HWND, UINT);
static BOOL (WINAPI *pVisible)(HWND);
static BOOL (WINAPI *pEnabled)(HWND);
static DWORD (WINAPI *pThread)(HWND, LPDWORD);
static int (WINAPI *pClass)(HWND, LPSTR, int);
static BOOL (WINAPI *pPost)(HWND, UINT, WPARAM, LPARAM);

static int Equal(const char *a, const char *b)
{
    while (*a && *a == *b) { ++a; ++b; }
    return *a == *b;
}

typedef struct ScrollDestination { HWND window; HWND bar; } ScrollDestination;

static int IsBar(HWND window, int horizontal)
{
    char name[64];
    return pVisible(window) && pEnabled(window) &&
        pClass(window, name, sizeof(name)) && Equal(name, "ScrollBar") &&
        !(pStyle(window, GWL_STYLE) & SBS_SIZEBOX) &&
        !!(pStyle(window, GWL_STYLE) & SBS_VERT) == !horizontal;
}

static ScrollDestination ScrollTarget(POINT point, int horizontal)
{
    ScrollDestination result = {0, 0};
    HWND window = pFromPoint(point);
    while (window && pThread(window, 0) == uiThread) {
        char name[64];
        HWND child;
        if (!pClass(window, name, sizeof(name))) return result;
        /* Let native controls handle their own wheel messages. */
        if (Equal(name, "Edit") || Equal(name, "ComboBox") ||
            Equal(name, "ComboLBox") || Equal(name, "ListBox") ||
            Equal(name, "SysListView32") || Equal(name, "SysTreeView32") ||
            Equal(name, "Button")) return result;
        if (IsBar(window, horizontal)) {
            result.window = pParent(window);
            result.bar = window;
            return result;
        }
        if (pStyle(window, GWL_STYLE) & (horizontal ? WS_HSCROLL : WS_VSCROLL)) {
            result.window = window;
            return result;
        }
        /* Cherry's horizontal bars are separate controls owned by the pane's
         * container. Walk to that container, then notify it with the bar HWND.
         * Do not recurse into sibling panes or select their scrollbars. */
        for (child = pWindow(window, GW_CHILD); child; child = pWindow(child, GW_HWNDNEXT)) {
            if (IsBar(child, horizontal)) {
                result.window = window;
                result.bar = child;
                return result;
            }
        }
        if (!(pStyle(window, GWL_STYLE) & WS_CHILD)) break;
        window = pParent(window);
    }
    return result;
}

static LRESULT CALLBACK WheelMessages(int code, WPARAM removing, LPARAM data)
{
    if (code >= 0 && removing == PM_REMOVE) {
        MSG *message = (MSG *)data;
        if (message->message == WM_MOUSEWHEEL || message->message == WM_MOUSEHWHEEL) {
            int horizontal = message->message == WM_MOUSEHWHEEL;
            POINT point;
            ScrollDestination target;
            point.x = (short)LOWORD(message->lParam);
            point.y = (short)HIWORD(message->lParam);
            target = ScrollTarget(point, horizontal);
            if (target.window) {
                UINT lines = 3;
                WheelPlan plan;
                unsigned int i;
                int delta = (short)HIWORD(message->wParam);
                UINT scrollMessage = horizontal ? WM_HSCROLL : WM_VSCROLL;
                if (target.window != previousTarget[horizontal] || target.bar != previousBar[horizontal])
                    remainder[horizontal] = 0;
                previousTarget[horizontal] = target.window;
                previousBar[horizontal] = target.bar;
                pSettings(horizontal ? SPI_GETWHEELSCROLLCHARS : SPI_GETWHEELSCROLLLINES, 0, &lines, 0);
                /* WM_MOUSEHWHEEL positive = right; WM_MOUSEWHEEL positive = up. */
                plan = PlanWheel(remainder[horizontal], horizontal ? -delta : delta, lines);
                remainder[horizontal] = plan.remainder;
                for (i = 0; i < plan.count; ++i)
                    pPost(target.window, scrollMessage, plan.command, (LPARAM)target.bar);
                if (plan.count) pPost(target.window, scrollMessage, SB_ENDSCROLL, (LPARAM)target.bar);
                /* Consume once: no propagation or duplicate native scrolling. */
                message->message = WM_NULL;
                message->wParam = 0;
                message->lParam = 0;
            } else {
                previousTarget[horizontal] = 0;
                previousBar[horizontal] = 0;
                remainder[horizontal] = 0;
            }
        }
    }
    return pNext(hook, code, removing, data);
}

/* Win32 guarantees these function-pointer conversions for GetProcAddress. */
#pragma warning(push)
#pragma warning(disable: 4152)
int WINAPI WheelMain(HINSTANCE instance, HINSTANCE previous, LPSTR command, int show)
{
    /* Use the actual image base so the original PE relocations still work. */
    BYTE *base = *(BYTE **)(__readfsdword(0x30) + 8);
    HMODULE (WINAPI *load)(LPCSTR) = (void *)(base + 0x9dc2a);
    FARPROC (WINAPI *get)(HMODULE, LPCSTR) = (void *)(base + 0x9dc30);
    DWORD (WINAPI *currentThread)(void) = (void *)(base + 0x9dbd6);
    int (WINAPI *realMain)(HINSTANCE, HINSTANCE, LPSTR, int) = (void *)(base + 0x8bd5c);
    HMODULE user = load("user32.dll");
    int result;
#define RESOLVE(variable, name) variable = (void *)get(user, name)
    RESOLVE(pSetHook, "SetWindowsHookExA");
    RESOLVE(pUnhook, "UnhookWindowsHookEx");
    RESOLVE(pNext, "CallNextHookEx");
    RESOLVE(pSettings, "SystemParametersInfoA");
    RESOLVE(pFromPoint, "WindowFromPoint");
    RESOLVE(pStyle, "GetWindowLongA");
    RESOLVE(pParent, "GetParent");
    RESOLVE(pWindow, "GetWindow");
    RESOLVE(pVisible, "IsWindowVisible");
    RESOLVE(pEnabled, "IsWindowEnabled");
    RESOLVE(pThread, "GetWindowThreadProcessId");
    RESOLVE(pClass, "GetClassNameA");
    RESOLVE(pPost, "PostMessageA");
    uiThread = currentThread();
    if (pSetHook && pUnhook && pNext && pSettings && pFromPoint &&
        pStyle && pParent && pWindow && pVisible && pEnabled && pThread && pClass && pPost)
        hook = pSetHook(WH_GETMESSAGE, WheelMessages, 0, uiThread);
    result = realMain(instance, previous, command, show);
    if (hook) pUnhook(hook);
    return result;
}
#pragma warning(pop)
