// Shared application initialization helpers (QtCore-only)
#pragma once

namespace AppInit {

// Adjust environment when running as root (HOME, XDG_RUNTIME_DIR, etc.)
void setupRootEnv();

// Install Qt and app translations (persistent for process lifetime)
void installTranslations();

// Install a Qt message handler that logs to /tmp/<appname>.log and echoes to stdout
void setupLogging();

}

