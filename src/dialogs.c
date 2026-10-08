#include "dialogs.h"

#ifdef _WIN32

#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <commdlg.h>

/* The hook that renames the playing dialog's button; see
   show_playing_dialog. */
static HHOOK stop_button_hook;

int choose_org_file(char* path, size_t size) {
    OPENFILENAMEA dialog = {0};

    path[0] = '\0';
    dialog.lStructSize = sizeof(dialog);
    dialog.lpstrFilter =
        "Organya songs (*.org)\0*.org\0All files (*.*)\0*.*\0";
    dialog.lpstrFile = path;
    dialog.nMaxFile = (DWORD)size;
    dialog.lpstrTitle = "Choose an Organya song";
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;

    return GetOpenFileNameA(&dialog) != 0;
}

/* Called as the message box is activated, so its OK button can be
   relabelled before it is shown. */
static LRESULT CALLBACK rename_stop_button(int code, WPARAM wparam,
                                           LPARAM lparam) {
    LRESULT result = CallNextHookEx(stop_button_hook, code, wparam, lparam);

    if (code == HCBT_ACTIVATE) {
        SetDlgItemTextA((HWND)wparam, IDOK, "Stop");
        UnhookWindowsHookEx(stop_button_hook);
    }
    return result;
}

int show_playing_dialog(const char* path) {
    const char* name = path;
    const char* separator;
    char message[512];

    for (separator = path; *separator != '\0'; separator++) {
        if (*separator == '\\' || *separator == '/') {
            name = separator + 1;
        }
    }
    snprintf(message, sizeof(message), "Playing %s", name);

    /* A message box's buttons can't be labelled directly, so rename OK
       to Stop with a hook that runs as the box opens. */
    stop_button_hook = SetWindowsHookExA(WH_CBT, rename_stop_button, NULL,
                                         GetCurrentThreadId());
    MessageBoxA(NULL, message, "COrg", MB_OK | MB_ICONINFORMATION);
    return 1;
}

#else

int choose_org_file(char* path, size_t size) {
    (void)path;
    (void)size;
    return 0;
}

int show_playing_dialog(const char* path) {
    (void)path;
    return 0;
}

#endif
