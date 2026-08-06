#ifndef TOBINMUD_RESOURCE_H
#define TOBINMUD_RESOURCE_H

/* Shared between tobinmud.rc (windres) and src/win32/main.c (the app
 * icon resource id) -- a real embedded icon fixes Windows synthesizing
 * a generic letter-avatar icon (the "just shows a T" symptom, user
 * 2026-08-05) for an app with no icon resource of its own. */
#define IDI_APPICON 101

#endif
