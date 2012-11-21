#Persistent

#Include, eqmac.ahk

Global psapi_dll_handle := 0

Global ini_file := "eqmac_multiclient.ini"

Global WS_CAPTION := 0xC00000

Global PROCESS_ALL_ACCESS := 0x001F0FFF

Global cpu_core_index := 2

Global resolution_width  := 1680 ; 1920 ; 1280
Global resolution_height := 1050 ; 1080 ; 720

EnableDebugPrivileges()

psapi_dll_handle := DllCall("LoadLibrary", "Str", "Psapi.dll")

IniRead, resolution_width,  %ini_file%, Resolution, Width
IniRead, resolution_height, %ini_file%, Resolution, Height

;IniWrite, TRUE, eqclient.ini, Defaults, WindowedMode
;IniWrite, %resolution_width%,  eqclient.ini, VideoMode, Width
;IniWrite, %resolution_height%, eqclient.ini, VideoMode, Height

GroupAdd, everquest, %EVERQUEST_TITLE%

; press tilde to switch between client windows (EQWindows)
`::
;WinActivateBottom, %EVERQUEST_TITLE%
GroupActivate, everquest
WinSet, AlwaysOnTop, On
WinSet, AlwaysOnTop, Off
SendInput, {Alt down}
SendInput, {Alt up}
Return

~XButton1::
Loop, 6
{
    SendInput, 0
    Sleep, 100
    GroupActivate, everquest
    Sleep, 100
}
Return

~XButton2::
Loop, 6
{
    SendInput, 5
    Sleep, 100
    GroupActivate, everquest
    Sleep, 100
}
Return

MButton::
Loop, 6
{
    SendInput, 8
    Sleep, 100
    GroupActivate, everquest
    Sleep, 100
}
Return

; press control+tilde for borderless fullscreen windowed mode
^`::
WinWait, %EVERQUEST_TITLE%
IfWinExist
{
    WinSet, Style, -%WS_CAPTION%
    WinMove, , , 0, 0, %resolution_width%, %resolution_height%
    WinSet, AlwaysOnTop, On
    WinSet, AlwaysOnTop, Off
}
Return

; press start+tilde to launch a new instance of the client
;#`::
;Run, C:\EQMac\eqw.exe
;Return

; press start+tilde to set each client's process affinity to a separate cpu core
#`::
SetProcessAffinityMask()
Return

SetProcessAffinityMask()
{
    IfWinNotActive, %EVERQUEST_TITLE%
    {
        Return
    }

    WinGet, active_process_id, PID, A

    script_process_id := DllCall("GetCurrentProcessId")

    If (active_process_id = script_process_id)
    {
        Return
    }

    psapi_size := 4096

    psapi_size := VarSetCapacity(psapi_processes, psapi_size)
    DllCall("Psapi.dll\EnumProcesses", "Ptr", &psapi_processes, "UInt", psapi_size, "UIntP", psapi_process)

    Loop, % psapi_process // 4
    {
        psapi_process_id := NumGet(psapi_processes, A_Index * 4, "UInt")

        psapi_process_handle := DllCall("OpenProcess", "UInt", 0x001F0FFF, "Int", false, "UInt", psapi_process_id, "Ptr")

        If (!psapi_process_handle)
        {
            Continue
        }

        VarSetCapacity(psapi_process_name, psapi_size, 0)
        psapi_result := DllCall("Psapi.dll\GetModuleBaseName", "Ptr", psapi_process_handle, "Ptr", 0, "Str", psapi_process_name, "UInt", A_IsUnicode ? psapi_size // 2 : psapi_size)

        If (!psapi_result)
        {
            If psapi_result := DllCall("Psapi.dll\GetProcessImageFileName", "Ptr", psapi_process_handle, "Str", psapi_process_name, "UInt", A_IsUnicode ? psapi_size // 2 : psapi_size)
            {
                SplitPath, psapi_process_name, psapi_process_name
            }
        }

        If (psapi_result && psapi_process_name)
        {
            If psapi_process_name = %EVERQUEST_CLIENT%
            {
                If (psapi_process_id = active_process_id)
                {
                    DllCall("SetProcessAffinityMask", "Ptr", psapi_process_handle, "Int", 1)
                }
                Else
                {
                    if (cpu_core_index > 8)
                    {
                        cpu_core_index := 2
                    }

                    DllCall("SetProcessAffinityMask", "Ptr", psapi_process_handle, "Int", cpu_core_index)

                    cpu_core_index := cpu_core_index * 2
                }
            }
        }

        DllCall("CloseHandle", "Ptr", psapi_process_handle)
    }
}

GuiClose:
GuiEscape:

DllCall("FreeLibrary", "Ptr", psapi_dll_handle)

ExitApp

Return