#include "core/bootrepair_engine.h"

#include <QCoreApplication>
#include <QSignalBlocker>
#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QSysInfo>

namespace {
QString stripQuotes(QString value)
{
    value = value.trimmed();
    if (value.size() >= 2 && value.startsWith('"') && value.endsWith('"')) {
        value = value.mid(1, value.size() - 2).trimmed();
    }
    return value;
}

QString normalizeArch(QString arch)
{
    arch = arch.trimmed();
    if (arch == "i686") {
        arch = "i386";
    }
    return arch;
}

QString detectArch(Cmd* shell)
{
    const QString cmd = QStringLiteral("uname -m");
    QString arch = shell->getCmdOut(cmd, QuietMode::Yes).trimmed();
    if (arch.contains("not found", Qt::CaseInsensitive)) {
        arch.clear();
    }
    if (arch.isEmpty()) {
        arch = QSysInfo::currentCpuArchitecture();
    }
    arch = normalizeArch(arch);
    if (arch.isEmpty()) {
        arch = QStringLiteral("x86_64");
    }
    return arch;
}

QString mxReleaseFromRoot(const QString& rootPath)
{
    const QString lsbPath = rootPath.isEmpty() ? QStringLiteral("/etc/lsb-release")
                                               : QDir(rootPath).filePath("etc/lsb-release");
    QFile file(lsbPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    QString distribId;
    QString distribRelease;
    while (!file.atEnd()) {
        const QString line = QString::fromUtf8(file.readLine()).trimmed();
        if (line.startsWith("DISTRIB_ID=")) {
            distribId = line.mid(QStringLiteral("DISTRIB_ID=").size());
        } else if (line.startsWith("DISTRIB_RELEASE=")) {
            distribRelease = line.mid(QStringLiteral("DISTRIB_RELEASE=").size());
        }
    }
    distribId = stripQuotes(distribId);
    distribRelease = stripQuotes(distribRelease);
    if (!distribId.toLower().startsWith("mx")) {
        return {};
    }
    distribRelease.remove(QRegularExpression("[^A-Za-z0-9]"));
    return distribRelease.left(2);
}

bool grubSupportsForceExtraRemovable(Cmd* shell, const QString& helpCmd, Elevation elevate)
{
    const bool wasSuppressed = shell->outputSuppressed();
    shell->setOutputSuppressed(true);
    const QString helpText = shell->getCmdOut(helpCmd, QuietMode::Yes, elevate);
    shell->setOutputSuppressed(wasSuppressed);
    return helpText.contains("--force-extra-removable");
}

QString grubInstallCmd(const QString& baseCmd, const QString& arch, const QString& bootloaderId, bool forceExtraRemovable)
{
    const QString extra = forceExtraRemovable ? QStringLiteral(" --force-extra-removable") : QString();
    return QStringLiteral("%1 --target=%2-efi --efi-directory=/boot/efi --bootloader-id=%3%4 --recheck")
        .arg(baseCmd, arch, bootloaderId, extra);
}

QString detectInitramfsCmd(const QString& rootPath)
{
    const QStringList updateInitramfs = {"/usr/sbin/update-initramfs", "/usr/bin/update-initramfs"};
    const QStringList mkinitcpio = {"/usr/bin/mkinitcpio"};
    const QStringList dracut = {"/usr/bin/dracut", "/usr/sbin/dracut"};
    auto existsInRoot = [&rootPath](const QString& path) {
        if (rootPath.isEmpty()) {
            return QFile::exists(path);
        }
        return QFile::exists(QDir(rootPath).filePath(path.mid(1)));
    };
    for (const auto& path : updateInitramfs) {
        if (existsInRoot(path)) {
            return QStringLiteral("update-initramfs");
        }
    }
    for (const auto& path : mkinitcpio) {
        if (existsInRoot(path)) {
            return QStringLiteral("mkinitcpio");
        }
    }
    for (const auto& path : dracut) {
        if (existsInRoot(path)) {
            return QStringLiteral("dracut");
        }
    }
    return {};
}

QString detectUpdateGrubCmd(const QString& rootPath)
{
    const QStringList updateGrub = {"/usr/sbin/update-grub", "/usr/bin/update-grub"};
    const QStringList grubMkconfig = {"/usr/bin/grub-mkconfig", "/usr/sbin/grub-mkconfig"};
    auto existsInRoot = [&rootPath](const QString& path) {
        if (rootPath.isEmpty()) {
            return QFile::exists(path);
        }
        return QFile::exists(QDir(rootPath).filePath(path.mid(1)));
    };
    for (const auto& path : updateGrub) {
        if (existsInRoot(path)) {
            return QStringLiteral("update-grub");
        }
    }
    for (const auto& path : grubMkconfig) {
        if (existsInRoot(path)) {
            return QStringLiteral("grub-mkconfig");
        }
    }
    return {};
}
} // namespace

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
    return shell->runAsRoot(cmd, output, input, quiet ? QuietMode::Yes : QuietMode::No);
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
    return shell->procAsRoot(cmd, args, output, input, quiet ? QuietMode::Yes : QuietMode::No);
}

QStringList BootRepairEngine::listDisks() const
{
    const QString cmd = QStringLiteral(
        "lsblk -ln -o NAME,SIZE,LABEL,MODEL -d -e 2,11 -x NAME | grep -E '^x?[h,s,v].[a-z]|^mmcblk|^nvme'");
    emit const_cast<BootRepairEngine*>(this)->log(QStringLiteral("$ %1").arg(cmd));
    const QSignalBlocker blocker(const_cast<BootRepairEngine*>(this)); // suppress engine::log emissions
    return shell->getCmdOut(cmd, QuietMode::Yes).split('\n', Qt::SkipEmptyParts);
}

QStringList BootRepairEngine::listPartitions() const
{
    const QString cmd = QStringLiteral(
        "lsblk -ln -o NAME,SIZE,FSTYPE,MOUNTPOINT,LABEL -e 2,11 -x NAME | grep -E "
        "'^x?[h,s,v].[a-z][0-9]|^mmcblk[0-9]+p|^nvme[0-9]+n[0-9]+p'");
    emit const_cast<BootRepairEngine*>(this)->log(QStringLiteral("$ %1").arg(cmd));
    const QSignalBlocker blocker(const_cast<BootRepairEngine*>(this)); // suppress engine::log emissions
    return shell->getCmdOut(cmd, QuietMode::Yes).split('\n', Qt::SkipEmptyParts);
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
    const bool wasSuppressed = shell->outputSuppressed();
    shell->setOutputSuppressed(true);
    QString points;
    if (!shell->proc("lsblk", {"-nro", "MOUNTPOINTS", volume}, &points, nullptr, QuietMode::Yes)) {
        shell->proc("lsblk", {"-nro", "MOUNTPOINT", volume}, &points, nullptr, QuietMode::Yes);
    }
    shell->setOutputSuppressed(wasSuppressed);
    return points.split('\n', Qt::SkipEmptyParts).contains(mount);
}

QString BootRepairEngine::luksMapper(const QString& part) const
{
    QString mapper;
    if (!shell->procAsRoot("cryptsetup", {"isLuks", part}, nullptr, nullptr, QuietMode::Yes)) {
        return {};
    }
    if (!shell->procAsRoot("cryptsetup", {"luksUUID", part}, &mapper, nullptr, QuietMode::Yes)) {
        return {};
    }
    return QStringLiteral("luks-") + mapper.trimmed();
}

bool BootRepairEngine::isLuks(const QString& device) const
{
    const QString dev = device.startsWith("/dev/") ? device : ("/dev/" + device);
    return shell->procAsRoot("cryptsetup", {"isLuks", dev}, nullptr, nullptr, QuietMode::Yes);
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
    return shell->run(cmd, nullptr, nullptr, QuietMode::Yes);
}

bool BootRepairEngine::isLinuxPartitionType(const QString& device) const
{
    const QString dev = normalizeDev(device);
    const QString cmd = QStringLiteral("lsblk -ln -o PARTTYPE %1 | grep -qEi '0x83|0fc63daf-8483-4772-8e79-3d69d8477de4|44479540-F297-41B2-9AF7-D131D5F0458A|4F68BCE3-E8CD-4DB1-96E7-FBCAF984B709'")
                            .arg(dev);
    emit const_cast<BootRepairEngine*>(this)->log(QStringLiteral("$ %1").arg(cmd));
    return shell->run(cmd, nullptr, nullptr, QuietMode::Yes);
}

bool BootRepairEngine::labelContains(const QString& device, const QString& needle) const
{
    const QString dev = normalizeDev(device);
    const QString cmd = QStringLiteral("lsblk -ln -o LABEL %1 | grep -q %2").arg(dev, needle);
    emit const_cast<BootRepairEngine*>(this)->log(QStringLiteral("$ %1").arg(cmd));
    return shell->run(cmd, nullptr, nullptr, QuietMode::Yes);
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
            emit log(QStringLiteral("$ uname -m"));
            const QString arch = detectArch(shell);
            const QString release = mxReleaseFromRoot({});
            const QString bootloaderId = release.isEmpty() ? QStringLiteral("MX")
                                                           : QStringLiteral("MX%1").arg(release);
            const QString helpCmd = QStringLiteral("grub-install --help");
            const bool forceExtraRemovable = grubSupportsForceExtraRemovable(shell, helpCmd, Elevation::No);
            cmd = grubInstallCmd(QStringLiteral("grub-install"), arch, bootloaderId, forceExtraRemovable);
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
        emit log(QStringLiteral("$ uname -m"));
        const QString arch = detectArch(shell);
        const QString release = mxReleaseFromRoot(tmpdir.path());
        const QString bootloaderId = release.isEmpty() ? QStringLiteral("MX")
                                                       : QStringLiteral("MX%1").arg(release);
        const QString helpCmd = QStringLiteral("chroot %1 grub-install --help").arg(tmpdir.path());
        const bool forceExtraRemovable = grubSupportsForceExtraRemovable(shell, helpCmd, Elevation::Yes);
        cmd = grubInstallCmd(QStringLiteral("chroot %1 grub-install").arg(tmpdir.path()), arch, bootloaderId, forceExtraRemovable);
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
        const QString tool = detectUpdateGrubCmd({});
        bool ok = false;
        if (tool == "update-grub") {
            ok = execRunAsRoot("update-grub", nullptr, nullptr, true);
        } else if (tool == "grub-mkconfig") {
            ok = execRunAsRoot("grub-mkconfig -o /boot/grub/grub.cfg", nullptr, nullptr, true);
        } else {
            emit log(QStringLiteral("No GRUB config tool found (update-grub/grub-mkconfig)."));
            emit finished(false);
            currentDryRun_ = false;
            return false;
        }
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
    const QString tool = detectUpdateGrubCmd(tmpdir.path());
    bool ok = false;
    if (tool == "update-grub") {
        ok = execRunAsRoot(QStringLiteral("chroot %1 update-grub").arg(tmpdir.path()), nullptr, nullptr, true);
    } else if (tool == "grub-mkconfig") {
        ok = execRunAsRoot(QStringLiteral("chroot %1 grub-mkconfig -o /boot/grub/grub.cfg").arg(tmpdir.path()),
                           nullptr, nullptr, true);
    } else {
        emit log(QStringLiteral("No GRUB config tool found in target root (update-grub/grub-mkconfig)."));
        cleanupMounts(tmpdir.path(), mapper);
        emit finished(false);
        currentDryRun_ = false;
        return false;
    }
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
        const QString tool = detectInitramfsCmd({});
        bool ok = false;
        if (tool == "update-initramfs") {
            ok = execProcAsRoot("update-initramfs", {"-c", "-v", "-k", "all"}, nullptr, nullptr, true);
        } else if (tool == "mkinitcpio") {
            ok = execProcAsRoot("mkinitcpio", {"-P"}, nullptr, nullptr, true);
        } else if (tool == "dracut") {
            ok = execProcAsRoot("dracut", {"--regenerate-all", "--force"}, nullptr, nullptr, true);
        } else {
            emit log(QStringLiteral("No initramfs generator found (update-initramfs/mkinitcpio/dracut)."));
            emit finished(false);
            currentDryRun_ = false;
            return false;
        }
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
    const QString tool = detectInitramfsCmd(tmpdir.path());
    bool ok = false;
    if (tool == "update-initramfs") {
        ok = execProcAsRoot("chroot", {tmpdir.path(), "update-initramfs", "-c", "-v", "-k", "all"}, nullptr, nullptr, true);
    } else if (tool == "mkinitcpio") {
        ok = execProcAsRoot("chroot", {tmpdir.path(), "mkinitcpio", "-P"}, nullptr, nullptr, true);
    } else if (tool == "dracut") {
        ok = execProcAsRoot("chroot", {tmpdir.path(), "dracut", "--regenerate-all", "--force"}, nullptr, nullptr, true);
    } else {
        emit log(QStringLiteral("No initramfs generator found in target root (update-initramfs/mkinitcpio/dracut)."));
        cleanupMounts(tmpdir.path(), mapper);
        emit finished(false);
        currentDryRun_ = false;
        return false;
    }
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
        if (opened) shell->procAsRoot("cryptsetup", {"luksClose", mapper}, nullptr, nullptr, QuietMode::Yes);
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
        if (shell->runAsRoot(cmd, &resolved, nullptr, QuietMode::Yes)) {
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
