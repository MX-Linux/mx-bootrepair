#pragma once

#include <QFile>
#include <QFileInfo>
#include <sys/stat.h>

namespace RepairDevice
{
inline QString canonicalBlock(const QString &path)
{
    if (!path.startsWith("/dev/")) return {};
    const QString canonical = QFileInfo(path).canonicalFilePath();
    if (!canonical.startsWith("/dev/")) return {};
    struct stat info {};
    return ::stat(QFile::encodeName(canonical).constData(), &info) == 0 && S_ISBLK(info.st_mode)
        ? canonical : QString();
}
}
