#ifndef DIALOGS_H
#define DIALOGS_H

#include <stddef.h>

/* Asks the user to pick a .org file with the system's file dialog,
   writing its path into path. Returns 1 if a file was chosen, or 0 if
   the user cancelled or there is no file dialog on this platform. */
int choose_org_file(char* path, size_t size);

/* Shows a dialog saying which song is playing, and returns once the
   user clicks its Stop button. Returns 0 straight away if there is no
   such dialog on this platform. */
int show_playing_dialog(const char* path);

#endif // DIALOGS_H
