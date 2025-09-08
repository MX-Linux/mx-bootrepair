// Core engine for boot repair logic (QtCore-only)
#pragma once

#include <QObject>
#include <QTemporaryDir>
#include <QStringList>

#include "cmd.h"

enum class GrubTarget { Mbr, Esp, Root };

struct BootRepairOptions {
    GrubTarget target {GrubTarget::Mbr};
    QString location;   // e.g. "sda" for MBR, "sda1" for ESP/Root
    QString root;       // e.g. "/dev/sda2" (target system root partition)
    QString backupPath; // for backup/restore operations
    QByteArray luksPassword; // optional; required if root is LUKS and not unlocked
    QString bootDevice; // e.g. "/dev/sda2" to mount at /boot in chroot
    QString espDevice;  // e.g. "/dev/sda1" to mount at /boot/efi in chroot
    bool dryRun {false}; // if true, log commands instead of executing
};

class BootRepairEngine : public QObject {
    Q_OBJECT
public:
    explicit BootRepairEngine(QObject* parent = nullptr);

    // Discovery
    QStringList listDisks() const;      // pretty strings from lsblk
    QStringList listPartitions() const; // pretty strings from lsblk
    static bool isUefi();
    bool isMounted(const QString& volume, const QString& mount) const; // public wrapper
    bool isLuks(const QString& device) const;                          // /dev/...
    bool isEspPartition(const QString& device) const;                  // sda1 or /dev/sda1
    bool isLinuxPartitionType(const QString& device) const;            // sdaX or /dev/sdaX
    bool labelContains(const QString& device, const QString& needle) const; // sdaX or /dev/sdaX

    // Operations (return true on success)
    bool installGrub(const BootRepairOptions& opt);
    bool repairGrub(const BootRepairOptions& opt);
    bool regenerateInitramfs(const BootRepairOptions& opt);
    bool backup(const BootRepairOptions& opt);
    bool restore(const BootRepairOptions& opt);

    // Utilities
    QString resolveFstabDevice(const QString& root, const QString& mountpoint, const QByteArray& luksPass = {});

signals:
    void log(const QString& line);
    void finished(bool ok);

private:
    // dry-run aware execution helpers
    bool execRunAsRoot(const QString& cmd, QString* output = nullptr, const QByteArray* input = nullptr, bool quiet = true);
    bool execProcAsRoot(const QString& cmd, const QStringList& args, QString* output = nullptr, const QByteArray* input = nullptr, bool quiet = true);

    // helpers
    bool isMountedTo(const QString& volume, const QString& mount) const;
    QString luksMapper(const QString& part) const; // returns mapper name or empty
    bool openLuks(const QString& part, const QString& mapper, const QByteArray& pass);
    bool mountChrootEnv(const QString& path);
    void cleanupMounts(const QString& path, const QString& luks);
    bool ensureMountFor(const QString& path, const QString& mountpoint, const QString& device);

    Cmd* shell;
    QTemporaryDir tmpdir;
    bool currentDryRun_ {false};
};
