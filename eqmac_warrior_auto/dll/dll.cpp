#define WINVER 0x0500

#include <iostream>
#include <fstream>
#include <vector>
#include <map>
#include <string>
#include <sstream>
#include <algorithm>
#include <cmath>

#include <boost/algorithm/string.hpp>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>

#include <boost/range/algorithm.hpp>

#include <boost/foreach.hpp>
#define foreach BOOST_FOREACH

#include <windows.h>
#include <psapi.h>

#include <winbase.h>

#include "memory.hpp"

#include "eqmac.hpp"

const std::string ini_file = "eqmac_warrior_auto.ini";

#define APPLICATION_NAME       "EQMac Warrior Automatic"
#define APPLICATION_CLASS_NAME "eqmac_warrior_auto"
#define APPLICATION_DLL_NAME   "eqmac_warrior_auto.dll"

#define EVERQUEST_CLIENT "eqgame.exe"
#define EVERQUEST_TITLE  "EQW beta 2.32"

#define WINDOW_HEIGHT 320
#define WINDOW_WIDTH  240

#define ID_TIMER_KEYS         10001
#define ID_TIMER_WARRIOR_AUTO 10002

#define VK_0 0x30
#define VK_1 0x31
#define VK_2 0x32
#define VK_3 0x33
#define VK_4 0x34
#define VK_5 0x35
#define VK_6 0x36
#define VK_7 0x37
#define VK_8 0x38
#define VK_9 0x39

#define VK_W 0x57

#define VK_ALT VK_MENU

memory memory;

HANDLE module;

HANDLE window_thread;

DWORD window_thread_id;

bool warrior_auto_activated = false;

void toggle_bool(bool &b)
{
    b = !b;
}

bool check_if_sitting()
{
    int player_spawn_info = everquest_get_player_spawn_info(memory);

    BYTE player_standing_state = memory.read_any<BYTE>(player_spawn_info + EVERQUEST_OFFSET_SPAWN_INFO_STANDING_STATE);

    if (player_standing_state == EVERQUEST_STANDING_STATE_SITTING)
    {
        return true;
    }

    return false;
}

int fix_hot_button_number(int hot_button)
{
    if (hot_button > 0 && hot_button < 10)
    {
        return hot_button - 1;
    }
    else
    {
        return 9;
    }
}

void timer_keys(HWND hwnd)
{
    if (memory.get_process_id() == 0)
    {
        return;
    }

    if (memory.is_foreground_window_current_process_id() == false)
    {
        return;
    }

    if
    (
        (GetAsyncKeyState(VK_CONTROL)) &&
        (GetAsyncKeyState(VK_ALT)) &&
        (GetAsyncKeyState(VK_BACK))
    )
    {
        DestroyWindow(hwnd);
    }

    if
    (
        (GetAsyncKeyState(VK_CONTROL)) &&
        (GetAsyncKeyState(VK_ALT)) &&
        (GetAsyncKeyState(VK_W))
    )
    {
        int player_spawn_info = everquest_get_player_spawn_info(memory);

        BYTE player_class = memory.read_any<BYTE>(player_spawn_info + EVERQUEST_OFFSET_SPAWN_INFO_CLASS);

        if (player_class != EVERQUEST_CLASS_WARRIOR)
        {
            everquest_function_chat_write_ex(APPLICATION_NAME " Error: You are not a Warrior.");

            return;
        }

        toggle_bool(warrior_auto_activated);

        if (warrior_auto_activated == true)
        {
            everquest_function_chat_write_ex(APPLICATION_NAME " activated.");
        }
        else
        {
            everquest_function_chat_write_ex(APPLICATION_NAME " deactivated.");
        }

        Sleep(1000);
    }
}

void timer_warrior_auto(HWND hwnd)
{
    if (memory.get_process_id() == 0)
    {
        return;
    }

    if (warrior_auto_activated == false)
    {
        return;
    }

    if (check_if_sitting() == true)
    {
        return;
    }

    bool is_auto_attack_enabled = everquest_is_auto_attack_enabled(memory);

    if (is_auto_attack_enabled == false)
    {
        return;
    }

    int target_spawn_info = everquest_get_target_spawn_info(memory);

    if (target_spawn_info == EVERQUEST_SPAWN_INFO_NULL)
    {
        return;
    }

    boost::property_tree::ptree pt;
    boost::property_tree::ini_parser::read_ini(ini_file, pt);

    bool warrior_auto_enabled = pt.get<bool>("WarriorAuto.bEnabled", false);

    if (warrior_auto_enabled == false)
    {
        return;
    }

    int warrior_hot_button = pt.get<int>("WarriorAuto.iWarriorHotButton");
  
    warrior_hot_button = fix_hot_button_number(warrior_hot_button);

    everquest_function_do_hot_button(warrior_hot_button);
    Sleep(100);
}

void on_create(HWND hwnd)
{
    memory.set_process_by_id(GetCurrentProcessId());

    SetTimer(hwnd, ID_TIMER_KEYS,         1, 0);
    SetTimer(hwnd, ID_TIMER_WARRIOR_AUTO, 1, 0);

    everquest_function_chat_write_ex(APPLICATION_NAME " loaded.");
}

void on_destroy(HWND hwnd)
{
    KillTimer(hwnd, ID_TIMER_KEYS);
    KillTimer(hwnd, ID_TIMER_WARRIOR_AUTO);

    everquest_function_chat_write_ex(APPLICATION_NAME " unloaded.");

    FreeLibraryAndExitThread((HINSTANCE)module, 0);

    ExitThread(0);

    PostQuitMessage(0);
}

void on_timer(HWND hwnd, WPARAM wparam, LPARAM lparam)
{
    switch (wparam)
    {
        case ID_TIMER_KEYS:
            timer_keys(hwnd);
            break;

        case ID_TIMER_WARRIOR_AUTO:
            timer_warrior_auto(hwnd);
            break;
    }
}

LRESULT CALLBACK window_procedure(HWND hwnd, UINT umsg, WPARAM wparam, LPARAM lparam)
{
    switch (umsg)
    {
        case WM_CLOSE:
            DestroyWindow(hwnd);
            break;

        case WM_CREATE:
            on_create(hwnd);
            break;

        case WM_DESTROY:
            on_destroy(hwnd);
            break;

       case WM_TIMER:
            on_timer(hwnd, wparam, lparam);
            break;

        default:
            return DefWindowProc(hwnd, umsg, wparam, lparam);
    }

    return 0;
}

extern "C" DWORD WINAPI create_window()
{
    WNDCLASSEX wcx;
    wcx.cbSize        = sizeof(WNDCLASSEX);
    wcx.style         = 0;
    wcx.lpfnWndProc   = window_procedure;
    wcx.cbClsExtra    = 0;
    wcx.cbWndExtra    = 0;
    wcx.hInstance     = GetModuleHandle(NULL);
    wcx.hIcon         = LoadIcon(NULL, IDI_WINLOGO);
    wcx.hIconSm       = LoadIcon(NULL, IDI_WINLOGO);
    wcx.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wcx.hbrBackground = (HBRUSH)CreateSolidBrush(GetSysColor(COLOR_BTNFACE));
    wcx.lpszMenuName  = NULL;
    wcx.lpszClassName = APPLICATION_CLASS_NAME;

    RegisterClassEx(&wcx);

    HWND hwnd;
    hwnd = CreateWindowEx
    (
        WS_EX_DLGMODALFRAME,
        APPLICATION_CLASS_NAME,
        APPLICATION_NAME,
        WS_CAPTION | WS_MINIMIZEBOX | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT,
        WINDOW_HEIGHT, WINDOW_WIDTH,
        HWND_DESKTOP,
        NULL,
        GetModuleHandle(NULL),
        NULL
    );

    //ShowWindow(hwnd, SW_SHOW);
    //UpdateWindow(hwnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return msg.wParam;
}

extern "C" BOOL APIENTRY DllMain(HANDLE dll, DWORD reason, LPVOID reserved)
{
    module = dll;

    if (reason == DLL_PROCESS_ATTACH)
    {
        //MessageBox(NULL, "fdwReason == DLL_PROCESS_ATTACH", "DllMain", MB_OK | MB_ICONINFORMATION);

        memory.enable_debug_privileges();

        window_thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)create_window, dll, 0, &window_thread_id);
    }

    if (reason == DLL_PROCESS_DETACH)
    {
        //MessageBox(NULL, "fdwReason == DLL_PROCESS_DETACH", "DllMain", MB_OK | MB_ICONINFORMATION);
    }

    return TRUE;
}
