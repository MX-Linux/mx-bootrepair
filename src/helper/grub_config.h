#pragma once

#include <QByteArray>
#include <QRegularExpression>

inline QByteArray standaloneGrubConfig(const QByteArray &uuid, QByteArray prefix)
{
    static const QRegularExpression validUuid(QRegularExpression::anchoredPattern("[A-Za-z0-9-]+"));
    if (!validUuid.match(QString::fromLatin1(uuid)).hasMatch() || (!prefix.isEmpty() && !prefix.startsWith('/')))
        return {};
    for (const char c : prefix) {
        if (static_cast<unsigned char>(c) < 0x20) return {};
    }
    prefix.replace("\\", "\\\\").replace("\"", "\\\"").replace("$", "\\$");
    return "if search --no-floppy --fs-uuid --set=root " + uuid + "; then\n"
           "  set prefix=\"($root)" + prefix + "\"\n"
           "  configfile \"$prefix/grub.cfg\"\nfi\n";
}
