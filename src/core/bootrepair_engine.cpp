#include "core/bootrepair_engine.h"

#include <QCoreApplication>
#include <QSignalBlocker>
#include <QDir>
#include <QFile>

BootRepairEngine::BootRepairEngine(QObject* parent)
    : QObject(parent), shell(new Cmd(this))
{
    connect(shell, &Cmd::outputAvailable, this, [this](const QString& s) { emit log(s); });
    connect(shell, &Cmd::errorAvailable, this, [this](const QString& s) { emit log(s); });
}

bool BootRepairEngine::execRunAsRoot(const QString& cmd, QString* output, const QByteArray* input, bool quiet)
{
    if (currentDryRun_) {
        emit log(QStringLiteral("[dry-run] %1").arg(cmd));
        if (output) *output = {};
        return true;
    }
    if (quiet) {
        emit log(QStringLiteral("# %1").arg(cmd));
    }
    return shell->runAsRoot(cmd, output, input, quiet);
}

bool BootRepairEngine::execProcAsRoot(const QString& cmd, const QStringList& args, QString* output, const QByteArray* input, bool quiet)
{
    if (currentDryRun_) {
        emit log(QStringLiteral("[dry-run] %1 %2").arg(cmd, args.join(' ')));
        if (output) *output = {};
        return true;
    }
    if (quiet) {
        emit log(QStringLiteral("# %1 %2").arg(cmd, args.join(' ')));
    }
    return shell->procAsRoot(cmd, args, output, input, quiet);
}

QStringList BootRepairEngine::listDisks() const
{
    const QString cmd = QStringLiteral(
        "lsblk -ln -o NAME,SIZE,LABEL,MODEL -d -e 2,11 -x NAME | grep -E '^x?[h,s,v].[a-z]|^mmcblk|^nvme'");
    emit const_cast<BootRepairEngine*>(this)->log(QStringLiteral("$ %1").arg(cmd));
    const QSignalBlocker blocker(const_cast<BootRepairEngine*>(this)); // suppress engine::log emissions
    return shell->getCmdOut(cmd, /*quiet*/ true).split('\n', Qt::SkipEmptyParts);
}

QStringList BootRepairEngine::listPartitions() const
{
    const QString cmd = QStringLiteral(
        "lsblk -ln -o NAME,SIZE,FSTYPE,MOUNTPOINT,LABEL -e 2,11 -x NAME | grep -E "
        "'^x?[h,s,v].[a-z][0-9]|^mmcblk[0-9]+p|^nvme[0-9]+n[0-9]+p'");
    emit const_cast<BootRepairEngine*>(this)->log(QStringLiteral("$ %1").arg(cmd));
    const QSignalBlocker blocker(const_cast<BootRepairEngine*>(this)); // suppress engine::log emissions
    return shell->getCmdOut(cmd, /*quiet*/ true).split('\n', Qt::SkipEmptyParts);
}

bool BootRepairEngine::isUefi()
{
    QDir dir("/sys/firmware/efi/efivars");
    return dir.exists() && !dir.entryList(QDir::NoDotAndDotDot | QDir::AllEntries).isEmpty();
}

bool BootRepairEngine::isMounted(const QString& volume, const QString& mount) const
{
    return isMountedTo(volume, mount);
}

bool BootRepairEngine::isMountedTo(const QString& volume, const QString& mount) const
{
    QString points;
    if (!shell->proc("lsblk", {"-nro", "MOUNTPOINTS", volume}, &points, nullptr, true)) {
        shell->proc("lsblk", {"-nro", "MOUNTPOINT", volume}, &points, nullptr, true);
    }
    return points.split('\n', Qt::SkipEmptyParts).contains(mount);
}

QString BootRepairEngine::luksMapper(const QString& part) const
{
    QString mapper;
    if (!shell->procAsRoot("cryptsetup", {"isLuks", part}, nullptr, nullptr, true)) {
        return {};
    }
    if (!shell->procAsRoot("cryptsetup", {"luksUUID", part}, &mapper, nullptr, true)) {
        return {};
    }
    return QStringLiteral("luks-") + mapper.trimmed();
}

bool BootRepairEngine::isLuks(const QString& device) const
{
    const QString dev = device.startsWith("/dev/") ? device : ("/dev/" + device);
    return shell->procAsRoot("cryptsetup", {"isLuks", dev}, nullptr, nullptr, true);
}

static inline QString normalizeDev(const QString& device)
{
    return device.startsWith("/dev/") ? device : ("/dev/" + device);
}

bool BootRepairEngine::isEspPartition(const QString& device) const
{
    const QString dev = normalizeDev(device);
    const QString cmd = QStringLiteral("lsblk -ln -o PARTTYPE %1 | grep -qiE 'c12a7328-f81f-11d2-ba4b-00a0c93ec93b|0xef'")
                            .arg(dev);
    emit const_cast<BootRepairEngine*>(this)->log(QStringLiteral("$ %1").arg(cmd));
    return shell->run(cmd, nullptr, nullptr, true);
}

bool BootRepairEngine::isLinuxPartitionType(const QString& device) const
{
    const QString dev = normalizeDev(device);
    const QString cmd = QStringLiteral("lsblk -ln -o PARTTYPE %1 | grep -qEi '0x83|0fc63daf-8483-4772-8e79-3d69d8477de4|44479540-F297-41B2-9AF7-D131D5F0458A|4F68BCE3-E8CD-4DB1-96E7-FBCAF984B709'")
                            .arg(dev);
    emit const_cast<BootRepairEngine*>(this)->log(QStringLiteral("$ %1").arg(cmd));
    return shell->run(cmd, nullptr, nullptr, true);
}

bool BootRepairEngine::labelContains(const QString& device, const QString& needle) const
{
    const QString dev = normalizeDev(device);
    const QString cmd = QStringLiteral("lsblk -ln -o LABEL %1 | grep -q %2").arg(dev, needle);
    emit const_cast<BootRepairEngine*>(this)->log(QStringLiteral("$ %1").arg(cmd));
    return shell->run(cmd, nullptr, nullptr, true);
}

bool BootRepairEngine::openLuks(const QString& part, const QString& mapper, const QByteArray& pass)
{
    if (pass.isEmpty()) {
        emit log(QStringLiteral("LUKS device requires a password for %1").arg(part));
        return false;
    }
    return execProcAsRoot("cryptsetup", {"luksOpen", part, mapper, "-"}, nullptr, &const_cast<QByteArray&>(pass), true);
}

bool BootRepairEngine::mountChrootEnv(const QString& path)
{
    if (!tmpdir.isValid()) {
        emit log(QStringLiteral("Could not create a temporary folder"));
        return false;
    }
    if (!QFile::exists(tmpdir.path())) {
        execRunAsRoot(QStringLiteral("mkdir -p %1").arg(tmpdir.path()), nullptr, nullptr, true);
    }
    const QString cmd = QStringLiteral(
        "/bin/mount %1 %2 && /bin/mount --rbind --make-rslave /dev %2/dev && "
        "/bin/mount --rbind --make-rslave /sys %2/sys && /bin/mount --rbind /proc %2/proc && "
        "/bin/mount -t tmpfs -o size=100m,nodev,mode=755 tmpfs %2/run && /bin/mkdir %2/run/udev && "
        "/bin/mount --rbind /run/udev %2/run/udev").arg(path, tmpdir.path());
    return execRunAsRoot(cmd, nullptr, nullptr, true);
}

void BootRepairEngine::cleanupMounts(const QString& path, const QString& luks)
{
    if (path != "/") {
        execRunAsRoot("mountpoint -q " + path + "/boot/efi && umount " + path + "/boot/efi", nullptr, nullptr, true);
        execRunAsRoot("mountpoint -q " + path + "/boot && umount -R " + path + "/boot", nullptr, nullptr, true);
        const QString cmd = QStringLiteral(
                                 "mountpoint -q %1 && /bin/umount -R %1/run && /bin/umount -R %1/proc && "
                                 "/bin/umount -R %1/sys && /bin/umount -R %1/dev && umount %1 && rmdir %1")
                                 .arg(path);
        execRunAsRoot(cmd, nullptr, nullptr, true);
    }
    if (!luks.isEmpty()) {
        execProcAsRoot("cryptsetup", {"luksClose", luks}, nullptr, nullptr, true);
    }
}

bool BootRepairEngine::ensureMountFor(const QString& path, const QString& mountpoint, const QString& device)
{
    // If directory is not populated, mount provided device to path/mountpoint
    if (!execRunAsRoot("test -n \"$(ls -A " + path + mountpoint + ")\"", nullptr, nullptr, true)) {
        if (device.isEmpty()) {
            emit log(QStringLiteral("No device provided for %1; please mount manually").arg(mountpoint));
            return false;
        }
        if (!execRunAsRoot("mount " + device + ' ' + path + mountpoint, nullptr, nullptr, true)) {
            emit log(QStringLiteral("Failed to mount %1 on %2%3").arg(device, path, mountpoint));
            return false;
        }
    }
    return true;
}

bool BootRepairEngine::installGrub(const BootRepairOptions& opt)
{
    currentDryRun_ = opt.dryRun;
    // Handle LUKS if needed
    QString root = opt.root;
    const QString mapper = luksMapper(root);
    if (!mapper.isEmpty() && !root.startsWith("/dev/mapper/")) {
        if (!openLuks(root, mapper, opt.luksPassword)) {
            emit finished(false);
            currentDryRun_ = false;
            return false;
        }
        root = "/dev/mapper/" + mapper;
    }

    // If installing on current root
    if (isMountedTo(root, "/")) {
        QString cmd = QStringLiteral("grub-install --target=i386-pc --recheck --force /dev/%1").arg(opt.location);
        if (opt.target == GrubTarget::Esp) {
            emit log(QStringLiteral("$ arch"));
            QString arch = shell->getCmdOut("arch", true).trimmed();
            if (arch == "i686") arch = "i386";
            const QString grepRel = QStringLiteral("grep -oP '(?<=DISTRIB_RELEASE=).*' /etc/lsb-release");
            emit log(QStringLiteral("$ %1").arg(grepRel));
            const QString release = shell->getCmdOut(grepRel, true).left(2);
            cmd = QStringLiteral(
                      "grub-install --target=%1-efi --efi-directory=/boot/efi --bootloader-id=MX%2 --force-extra-removable --recheck")
                      .arg(arch, release);
        }
        const bool ok = (opt.target == GrubTarget::Esp)
                             ? execRunAsRoot("test -d /boot/efi || mkdir /boot/efi", nullptr, nullptr, true) &&
                                   execRunAsRoot(cmd, nullptr, nullptr, true)
                             : execRunAsRoot(cmd, nullptr, nullptr, true);
        emit finished(ok);
        currentDryRun_ = false;
        return ok;
    }

    // Otherwise, chroot into target root
    if (opt.target == GrubTarget::Esp) {
        // Mount efivarfs if needed for NVRAM
        execRunAsRoot("grep -sq ^efivarfs /proc/self/mounts || { test -d /sys/firmware/efi/efivars && mount -t efivarfs efivarfs /sys/firmware/efi/efivars; }",
                        nullptr, nullptr, true);
        execRunAsRoot("ls -1 /sys/firmware/efi/efivars | grep -sq ^dump && rm /sys/firmware/efi/efivars/dump*",
                        nullptr, nullptr, true);
    }

    if (!mountChrootEnv(root)) {
        emit log(QStringLiteral("Failed to set up chroot"));
        emit finished(false);
        currentDryRun_ = false;
        return false;
    }
    if (!ensureMountFor(tmpdir.path(), "/boot", opt.bootDevice)) {
        cleanupMounts(tmpdir.path(), mapper);
        emit finished(false);
        currentDryRun_ = false;
        return false;
    }

    QString cmd = QStringLiteral("chroot %1 grub-install --target=i386-pc --recheck --force /dev/%2").arg(tmpdir.path(), opt.location);
    if (opt.target == GrubTarget::Esp) {
        execRunAsRoot("test -d " + tmpdir.path() + "/boot/efi || mkdir " + tmpdir.path() + "/boot/efi",
                        nullptr, nullptr, true);
        if (!ensureMountFor(tmpdir.path(), "/boot/efi", opt.espDevice)) {
            cleanupMounts(tmpdir.path(), mapper);
            emit finished(false);
            currentDryRun_ = false;
            return false;
        }
        emit log(QStringLiteral("$ arch"));
        QString arch = shell->getCmdOut("arch", true).trimmed();
        if (arch == "i686") arch = "i386";
        const QString grepRel2 = QStringLiteral("grep -oP '(?<=DISTRIB_RELEASE=).*' /etc/lsb-release");
        emit log(QStringLiteral("$ %1").arg(grepRel2));
        const QString release = shell->getCmdOut(grepRel2, true).left(2);
        cmd = QStringLiteral("chroot %1 grub-install --target=%2-efi --efi-directory=/boot/efi --bootloader-id=MX%3 --force-extra-removable --recheck")
                  .arg(tmpdir.path(), arch, release);
    }

    const bool ok = execRunAsRoot(cmd, nullptr, nullptr, true);
    cleanupMounts(tmpdir.path(), mapper);
    emit finished(ok);
    currentDryRun_ = false;
    return ok;
}

bool BootRepairEngine::repairGrub(const BootRepairOptions& opt)
{
    currentDryRun_ = opt.dryRun;
    QString root = opt.root;
    const QString mapper = luksMapper(root);
    if (!mapper.isEmpty() && !root.startsWith("/dev/mapper/")) {
        if (!openLuks(root, mapper, opt.luksPassword)) {
            emit finished(false);
            currentDryRun_ = false;
            return false;
        }
        root = "/dev/mapper/" + mapper;
    }

    if (isMountedTo(root, "/")) {
        const bool ok = execRunAsRoot("update-grub", nullptr, nullptr, true);
        emit finished(ok);
        currentDryRun_ = false;
        return ok;
    }

    if (!mountChrootEnv(root)) {
        emit finished(false);
        currentDryRun_ = false;
        return false;
    }
    // Ensure /boot and possibly /boot/efi
    if (!ensureMountFor(tmpdir.path(), "/boot", opt.bootDevice)) {
        cleanupMounts(tmpdir.path(), mapper);
        emit finished(false);
        return false;
    }
    if (QFile::exists(tmpdir.path() + "/boot/efi")) {
        ensureMountFor(tmpdir.path(), "/boot/efi", opt.espDevice);
    }
    const bool ok = execRunAsRoot(QStringLiteral("chroot %1 update-grub").arg(tmpdir.path()), nullptr, nullptr, true);
    cleanupMounts(tmpdir.path(), mapper);
    emit finished(ok);
    currentDryRun_ = false;
    return ok;
}

bool BootRepairEngine::regenerateInitramfs(const BootRepairOptions& opt)
{
    currentDryRun_ = opt.dryRun;
    QString root = opt.root;
    const QString mapper = luksMapper(root);
    if (!mapper.isEmpty() && !root.startsWith("/dev/mapper/")) {
        if (!openLuks(root, mapper, opt.luksPassword)) {
            emit finished(false);
            currentDryRun_ = false;
            return false;
        }
        root = "/dev/mapper/" + mapper;
    }

    if (isMountedTo(root, "/")) {
        const bool ok = execProcAsRoot("update-initramfs", {"-c", "-v", "-k", "all"}, nullptr, nullptr, true);
        emit finished(ok);
        currentDryRun_ = false;
        return ok;
    }

    if (!mountChrootEnv(root)) {
        emit finished(false);
        currentDryRun_ = false;
        return false;
    }
    if (!ensureMountFor(tmpdir.path(), "/boot", opt.bootDevice)) {
        cleanupMounts(tmpdir.path(), mapper);
        emit finished(false);
        return false;
    }
    const bool ok = execProcAsRoot("chroot", {tmpdir.path(), "update-initramfs", "-c", "-v", "-k", "all"}, nullptr, nullptr, true);
    cleanupMounts(tmpdir.path(), mapper);
    emit finished(ok);
    currentDryRun_ = false;
    return ok;
}

QString BootRepairEngine::resolveFstabDevice(const QString& root, const QString& mountpoint, const QByteArray& luksPass)
{
    QString part = root;
    const QString mapper = luksMapper(part);
    bool opened = false;
    if (!mapper.isEmpty() && !part.startsWith("/dev/mapper/")) {
        if (!openLuks(part, mapper, luksPass)) {
            return {};
        }
        part = "/dev/mapper/" + mapper;
        opened = true;
    }
    if (!mountChrootEnv(part)) {
        if (opened) shell->procAsRoot("cryptsetup", {"luksClose", mapper}, nullptr, nullptr, true);
        return {};
    }
    QFile file(tmpdir.path() + "/etc/fstab");
    QString device;
    if (file.open(QIODevice::ReadOnly)) {
        while (!file.atEnd()) {
            const QString line = QString::fromUtf8(file.readLine()).simplified();
            if (line.isEmpty() || line.startsWith('#')) continue;
            const QStringList fields = line.split(' ');
            if (fields.size() >= 2 && fields.at(1) == mountpoint) {
                device = fields.at(0);
                break;
            }
        }
        file.close();
    }
    QString resolved;
    if (!device.isEmpty()) {
        const QString cmd = "readlink -e \"$(echo " + device
                           + " | sed -r 's:((PART)?(UUID|LABEL))=:\\L/dev/disk/by-\\1/:g; s:[\\\"]::g;')\"";
        if (shell->runAsRoot(cmd, &resolved, nullptr, true)) {
            resolved = resolved.trimmed();
        }
    }
    cleanupMounts(tmpdir.path(), opened ? mapper : QString());
    return resolved;
}

bool BootRepairEngine::backup(const BootRepairOptions& opt)
{
    currentDryRun_ = opt.dryRun;
    if (opt.location.isEmpty() || opt.backupPath.isEmpty()) return false;
    const QString cmd = QStringLiteral("dd if=/dev/%1 of=%2 bs=446 count=1").arg(opt.location, opt.backupPath);
    const bool ok = execRunAsRoot(cmd, nullptr, nullptr, true);
    emit finished(ok);
    currentDryRun_ = false;
    return ok;
}

bool BootRepairEngine::restore(const BootRepairOptions& opt)
{
    currentDryRun_ = opt.dryRun;
    if (opt.location.isEmpty() || opt.backupPath.isEmpty()) return false;
    const QString cmd = QStringLiteral("dd if=%1 of=/dev/%2 bs=446 count=1").arg(opt.backupPath, opt.location);
    const bool ok = execRunAsRoot(cmd, nullptr, nullptr, true);
    emit finished(ok);
    currentDryRun_ = false;
    return ok;
}
