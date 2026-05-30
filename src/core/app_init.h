// Shared application initialization helpers (QtCore-only)
#pragma once

namespace AppInit {

// Adjust environment when running as root (HOME, XDG_RUNTIME_DIR, etc.)
void setupRootEnv();

// Install Qt and app translations (persistent for process lifetime)
void installTranslations();

// Install a Qt message handler that writes the session log and echoes to stdout.
void setupLogging();

// True when the current session log contains enough content to be worth persisting.
bool shouldPersistLog();

}
