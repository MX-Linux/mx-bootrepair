#pragma once

#include <QCoreApplication>

inline QString sharePath(const QString &relativePath)
{
#ifdef APPIMAGE_BUILD
    return QCoreApplication::applicationDirPath() + QStringLiteral("/../share/") + relativePath;
#else
    return QStringLiteral("/usr/share/") + relativePath;
#endif
}
