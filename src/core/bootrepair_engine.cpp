#include "core/bootrepair_engine.h"

#include <QCoreApplication>
#include <QSignalBlocker>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRandomGenerator>
#include <QSysInfo>

namespace {
QString normalizeArch(QString arch)
{
    arch = arch.trimmed();
    if (arch == "i686") {
        arch = "i386";
    }
    return arch;
}

bool pathExistsInRoot(const QString& rootPath, const QString& path, PathCheck check, Cmd* shell = nullptr)
{
    if (shell && !rootPath.isEmpty()) {
        return shell->pathCheckAsRoot(path, check, QuietMode::Yes, rootPath);
    }

    const QString fullPath = rootPath.isEmpty() ? path : QDir(rootPath).filePath(path.mid(1));
    const QFileInfo info(fullPath);
    if (check == PathCheck::Directory) {
        return info.exists() && info.isDir();
    }
    if (check == PathCheck::Executable) {
        return info.exists() && info.isExecutable();
    }
    return info.exists();
}

QStringList listDirEntries(const QString& rootPath, const QString& path, Cmd* shell = nullptr)
{
    if (shell && !rootPath.isEmpty()) {
        return shell->listDirAsRoot(path, QuietMode::Yes, rootPath);
    }

    const QString fullPath = rootPath.isEmpty() ? path : QDir(rootPath).filePath(path.mid(1));
    return QDir(fullPath).entryList(QDir::AllEntries | QDir::NoDotAndDotDot, QDir::Name);
}

QString toRootRelativePath(const QString& path, const QString& rootPath)
{
    if (rootPath.isEmpty()) {
        return path;
    }
    if (!path.startsWith(rootPath)) {
        return {};
    }

    QString relativePath = path.mid(rootPath.size());
    if (relativePath.isEmpty()) {
        relativePath = QStringLiteral("/");
    } else if (!relativePath.startsWith('/')) {
        relativePath.prepend('/');
    }
    return relativePath;
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

bool isArchBuild(const QString& rootPath, Cmd* shell = nullptr)
{
    const QStringList mkinitcpio = {"/usr/bin/mkinitcpio", "/etc/mkinitcpio.conf"};
    for (const auto& path : mkinitcpio) {
        if (pathExistsInRoot(rootPath, path, PathCheck::Exists, shell)) {
            return true;
        }
    }
    return false;
}

bool dirContainsEfi(const QString& dirPath, const QString& rootPath, Cmd* shell)
{
    const QString relativePath = rootPath.isEmpty() ? dirPath : toRootRelativePath(dirPath, rootPath);
    if (relativePath.isEmpty()) {
        return false;
    }

    const QStringList files = listDirEntries(rootPath, relativePath, shell);
    for (const auto& file : files) {
        const QString filePath = dirPath + '/' + file;
        const QString filePathInRoot = rootPath.isEmpty() ? filePath : toRootRelativePath(filePath, rootPath);
        if (!filePathInRoot.isEmpty()
            && !pathExistsInRoot(rootPath, filePathInRoot, PathCheck::Directory, shell)
            && file.endsWith(".efi", Qt::CaseInsensitive)) {
            return true;
        }
    }
    return false;
}

QString detectBootloaderId(const QString& efiMountPath, const QString& rootPath, Cmd* shell = nullptr)
{
    const QString fallback = isArchBuild(rootPath, shell) ? QStringLiteral("MXarch") : QStringLiteral("MX");

    // Look for existing MX*/antiX* directories on the ESP that contain a GRUB binary.
    const QString efiPath = efiMountPath + "/EFI";
    const QString efiPathInRoot = rootPath.isEmpty() ? efiPath : toRootRelativePath(efiPath, rootPath);
    if (efiPathInRoot.isEmpty() || !pathExistsInRoot(rootPath, efiPathInRoot, PathCheck::Directory, shell)) {
        return fallback;
    }
    const QStringList entries = listDirEntries(rootPath, efiPathInRoot, shell);

    // Collect all MX*/antiX* candidates that have .efi files.
    QStringList candidates;
    for (const auto& entry : entries) {
        const QString entryPath = efiPath + '/' + entry;
        const QString entryPathInRoot = rootPath.isEmpty() ? entryPath : toRootRelativePath(entryPath, rootPath);
        if ((entry.startsWith("MX", Qt::CaseInsensitive) || entry.startsWith("antiX", Qt::CaseInsensitive))
            && !entryPathInRoot.isEmpty()
            && pathExistsInRoot(rootPath, entryPathInRoot, PathCheck::Directory, shell)
            && dirContainsEfi(efiPath + "/" + entry, rootPath, shell)) {
            candidates << entry;
        }
    }
    if (candidates.isEmpty()) {
        return fallback;
    }
    // Prefer the expected fallback name if present; otherwise take the first match.
    if (candidates.contains(fallback, Qt::CaseInsensitive)) {
        for (const auto& c : candidates) {
            if (c.compare(fallback, Qt::CaseInsensitive) == 0) {
                return c;
            }
        }
    }
    return candidates.first();
}

bool grubSupportsForceExtraRemovable(Cmd* shell, const QString& rootPath = {})
{
    const bool wasSuppressed = shell->outputSuppressed();
    shell->setOutputSuppressed(true);
    const QString helpText = rootPath.isEmpty()
        ? shell->getOutAsRoot(QStringLiteral("grub-install"), {QStringLiteral("--help")}, QuietMode::Yes)
        : shell->getOutAsRootInTarget(rootPath, QStringLiteral("grub-install"), {QStringLiteral("--help")},
                                      QuietMode::Yes);
    shell->setOutputSuppressed(wasSuppressed);
    return helpText.contains("--force-extra-removable");
}

QString detectGrubInstallCmd(const QString& rootPath, Cmd* shell = nullptr)
{
    const QStringList grubInstall = {"/usr/sbin/grub-install", "/usr/bin/grub-install", "/sbin/grub-install", "/bin/grub-install"};
    const QStringList grubMkstandalone = {"/usr/bin/grub-mkstandalone", "/bin/grub-mkstandalone", "/usr/sbin/grub-mkstandalone", "/sbin/grub-mkstandalone"};
    for (const auto& path : grubInstall) {
        if (pathExistsInRoot(rootPath, path, PathCheck::Executable, shell)) {
            return QStringLiteral("grub-install");
        }
    }
    for (const auto& path : grubMkstandalone) {
        if (pathExistsInRoot(rootPath, path, PathCheck::Executable, shell)) {
            return QStringLiteral("grub-mkstandalone");
        }
    }
    return {};
}

QString detectInitramfsCmd(const QString& rootPath, Cmd* shell = nullptr)
{
    const QStringList updateInitramfs = {"/usr/sbin/update-initramfs", "/usr/bin/update-initramfs"};
    const QStringList mkinitcpio = {"/usr/bin/mkinitcpio"};
    const QStringList dracut = {"/usr/bin/dracut", "/usr/sbin/dracut"};
    for (const auto& path : updateInitramfs) {
        if (pathExistsInRoot(rootPath, path, PathCheck::Executable, shell)) {
            return QStringLiteral("update-initramfs");
        }
    }
    for (const auto& path : mkinitcpio) {
        if (pathExistsInRoot(rootPath, path, PathCheck::Executable, shell)) {
            return QStringLiteral("mkinitcpio");
        }
    }
    for (const auto& path : dracut) {
        if (pathExistsInRoot(rootPath, path, PathCheck::Executable, shell)) {
            return QStringLiteral("dracut");
        }
    }
    return {};
}

QString detectUpdateGrubCmd(const QString& rootPath, Cmd* shell = nullptr)
{
    const QStringList updateGrub = {"/usr/sbin/update-grub", "/usr/bin/update-grub"};
    const QStringList grubMkconfig = {"/usr/bin/grub-mkconfig", "/usr/sbin/grub-mkconfig"};
    for (const auto& path : updateGrub) {
        if (pathExistsInRoot(rootPath, path, PathCheck::Executable, shell)) {
            return QStringLiteral("update-grub");
        }
    }
    for (const auto& path : grubMkconfig) {
        if (pathExistsInRoot(rootPath, path, PathCheck::Executable, shell)) {
            return QStringLiteral("grub-mkconfig");
        }
    }
    return {};
}

QString resolveDeviceSpec(QString spec)
{
    spec.remove('"');
    if (spec.startsWith("/dev/")) {
        return spec;
    }

    const auto valueFor = [&spec](const QString& prefix) -> QString {
        return spec.startsWith(prefix) ? spec.mid(prefix.size()) : QString();
    };

    QString byPath;
    if (const QString value = valueFor(QStringLiteral("UUID=")); !value.isEmpty()) {
        byPath = QStringLiteral("/dev/disk/by-uuid/%1").arg(value);
    } else if (const QString value = valueFor(QStringLiteral("PARTUUID=")); !value.isEmpty()) {
        byPath = QStringLiteral("/dev/disk/by-partuuid/%1").arg(value);
    } else if (const QString value = valueFor(QStringLiteral("LABEL=")); !value.isEmpty()) {
        byPath = QStringLiteral("/dev/disk/by-label/%1").arg(value);
    } else if (const QString value = valueFor(QStringLiteral("PARTLABEL=")); !value.isEmpty()) {
        byPath = QStringLiteral("/dev/disk/by-partlabel/%1").arg(value);
    } else {
        return {};
    }

    const QFileInfo info(byPath);
    return info.exists() ? info.canonicalFilePath() : QString();
}
} // namespace

BootRepairEngine::BootRepairEngine(QObject* parent)
    : QObject(parent), shell(new Cmd(this))
{
    connect(shell, &Cmd::outputAvailable, this, [this](const QString& s) { emit log(s); });
    connect(shell, &Cmd::errorAvailable, this, [this](const QString& s) { emit log(s); });
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

bool BootRepairEngine::execProcAsRootInTarget(const QString& rootPath, const QString& cmd, const QStringList& args,
                                              QString* output, const QByteArray* input, bool quiet)
{
    if (currentDryRun_) {
        emit log(QStringLiteral("[dry-run] chroot %1 %2 %3").arg(rootPath, cmd, args.join(' ')));
        if (output) *output = {};
        return true;
    }
    if (quiet) {
        emit log(QStringLiteral("# chroot %1 %2 %3").arg(rootPath, cmd, args.join(' ')));
    }
    return shell->procAsRootInTarget(rootPath, cmd, args, output, input, quiet ? QuietMode::Yes : QuietMode::No);
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

bool BootRepairEngine::canUnlockLuks(const QString& device, const QByteArray& pass)
{
    const QString dev = device.startsWith("/dev/") ? device : ("/dev/" + device);
    if (!isLuks(dev)) {
        return true;
    }

    const QString probeMapper = QStringLiteral("mxbr-probe-%1-%2")
                                    .arg(QCoreApplication::applicationPid())
                                    .arg(QString::number(QRandomGenerator::global()->generate64(), 16));
    if (!openLuks(dev, probeMapper, pass)) {
        return false;
    }
    execProcAsRoot("cryptsetup", {"luksClose", probeMapper}, nullptr, nullptr, true);
    return true;
}

static inline QString normalizeDev(const QString& device)
{
    return device.startsWith("/dev/") ? device : ("/dev/" + device);
}

QString BootRepairEngine::mountSource(const QString& mountpoint) const
{
    QString output;
    shell->proc("findmnt", {"-n", "-o", "SOURCE", "--target", mountpoint}, &output, nullptr, QuietMode::Yes);
    return output.trimmed();
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

QString BootRepairEngine::filesystemType(const QString& device) const
{
    QString output;
    shell->proc("lsblk", {"-ln", "-o", "FSTYPE", normalizeDev(device)}, &output, nullptr, QuietMode::Yes);
    return output.trimmed();
}

QString BootRepairEngine::partitionLabel(const QString& device) const
{
    QString output;
    shell->proc("lsblk", {"-ln", "-o", "LABEL", normalizeDev(device)}, &output, nullptr, QuietMode::Yes);
    return output.trimmed();
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
    if (currentDryRun_) {
        emit log(QStringLiteral("[dry-run] mount-chroot-env %1 %2").arg(path, tmpdir.path()));
        return true;
    }
    emit log(QStringLiteral("# mount-chroot-env %1 %2").arg(path, tmpdir.path()));
    return shell->mountChrootEnvAsRoot(path, tmpdir.path(), QuietMode::Yes);
}

void BootRepairEngine::cleanupMounts(const QString& path, const QString& luks)
{
    if (path != "/") {
        if (currentDryRun_) {
            emit log(QStringLiteral("[dry-run] cleanup-chroot-env %1").arg(path));
        } else {
            emit log(QStringLiteral("# cleanup-chroot-env %1").arg(path));
            shell->cleanupChrootEnvAsRoot(path, QuietMode::Yes);
        }
    }
    if (!luks.isEmpty()) {
        execProcAsRoot("cryptsetup", {"luksClose", luks}, nullptr, nullptr, true);
    }
}

bool BootRepairEngine::ensureMountFor(const QString& path, const QString& mountpoint, const QString& device)
{
    const QString target = path + mountpoint;
    if (currentDryRun_) {
        if (device.isEmpty()) {
            emit log(QStringLiteral("No device provided for %1; please mount manually").arg(mountpoint));
            return false;
        }
        emit log(QStringLiteral("[dry-run] mkdir -p %1").arg(target));
        emit log(QStringLiteral("[dry-run] mount %1 %2").arg(device, target));
        return true;
    }

    execProcAsRoot("mkdir", {"-p", target}, nullptr, nullptr, true);
    if (!shell->dirHasEntriesAsRoot(target, QuietMode::Yes)) {
        if (device.isEmpty()) {
            emit log(QStringLiteral("No device provided for %1; please mount manually").arg(mountpoint));
            return false;
        }
        if (!execProcAsRoot("mount", {device, target}, nullptr, nullptr, true)) {
            emit log(QStringLiteral("Failed to mount %1 on %2").arg(device, target));
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
        bool ok = false;
        QString prevEsp; // track original ESP mount so we can restore it
        if (opt.target == GrubTarget::Esp) {
            if (!shell->pathCheckAsRoot("/boot/efi", PathCheck::Directory, QuietMode::Yes)) {
                execProcAsRoot("mkdir", {"-p", "/boot/efi"}, nullptr, nullptr, true);
            }
            if (!opt.espDevice.isEmpty() && !isMountedTo(opt.espDevice, "/boot/efi")) {
                prevEsp = mountSource("/boot/efi");
                if (shell->procAsRoot("mountpoint", {"-q", "/boot/efi"}, nullptr, nullptr, QuietMode::Yes)
                    && !execProcAsRoot("umount", {"/boot/efi"}, nullptr, nullptr, true)) {
                    emit log(QStringLiteral("Failed to unmount /boot/efi (device may be busy)"));
                    emit finished(false);
                    currentDryRun_ = false;
                    return false;
                }
                if (!execProcAsRoot("mount", {opt.espDevice, "/boot/efi"}, nullptr, nullptr, true)) {
                    emit log(QStringLiteral("Failed to mount %1 on /boot/efi").arg(opt.espDevice));
                    if (!prevEsp.isEmpty()
                        && !execProcAsRoot("mount", {prevEsp, "/boot/efi"}, nullptr, nullptr, true)) {
                        emit log(QStringLiteral("Warning: could not restore %1 on /boot/efi; please remount manually").arg(prevEsp));
                    }
                    emit finished(false);
                    currentDryRun_ = false;
                    return false;
                }
            }
            emit log(QStringLiteral("$ uname -m"));
            const QString arch = detectArch(shell);
            const QString bootloaderId = detectBootloaderId(QStringLiteral("/boot/efi"), {});
            const QString grubTool = detectGrubInstallCmd({});
            if (grubTool == "grub-install") {
                QStringList args {QStringLiteral("--target=%1-efi").arg(arch), QStringLiteral("--efi-directory=/boot/efi"),
                                  QStringLiteral("--bootloader-id=%1").arg(bootloaderId)};
                if (grubSupportsForceExtraRemovable(shell)) {
                    args << QStringLiteral("--force-extra-removable");
                }
                args << QStringLiteral("--recheck");
                ok = execProcAsRoot("grub-install", args, nullptr, nullptr, true);
            } else if (grubTool == "grub-mkstandalone") {
                if (currentDryRun_) {
                    emit log(QStringLiteral("[dry-run] grub-mkstandalone-efi %1 %2").arg(arch, bootloaderId));
                    ok = true;
                } else {
                    emit log(QStringLiteral("# grub-mkstandalone-efi %1 %2").arg(arch, bootloaderId));
                    ok = shell->grubMkstandaloneEfiAsRoot(arch, bootloaderId, false, QuietMode::Yes);
                }
            } else {
                emit log(QStringLiteral("No GRUB installation tool found (grub-install/grub-mkstandalone)."));
                if (!prevEsp.isEmpty()) {
                    if (!execProcAsRoot("umount", {"/boot/efi"}, nullptr, nullptr, true)
                        || !execProcAsRoot("mount", {prevEsp, "/boot/efi"}, nullptr, nullptr, true)) {
                        emit log(QStringLiteral("Warning: could not restore %1 on /boot/efi; please remount manually").arg(prevEsp));
                    }
                }
                emit finished(false);
                currentDryRun_ = false;
                return false;
            }
        } else {
            const QString grubTool = detectGrubInstallCmd({});
            if (grubTool == "grub-install") {
                ok = execProcAsRoot("grub-install",
                                    {QStringLiteral("--target=i386-pc"), QStringLiteral("--recheck"),
                                     QStringLiteral("--force"), QStringLiteral("/dev/%1").arg(opt.location)},
                                    nullptr, nullptr, true);
            } else {
                emit log(QStringLiteral("grub-install is required for MBR/Root target but was not found."));
                emit finished(false);
                currentDryRun_ = false;
                return false;
            }
        }
        if (!prevEsp.isEmpty()) {
            if (!execProcAsRoot("umount", {"/boot/efi"}, nullptr, nullptr, true)
                || !execProcAsRoot("mount", {prevEsp, "/boot/efi"}, nullptr, nullptr, true)) {
                emit log(QStringLiteral("Warning: could not restore %1 on /boot/efi; please remount manually").arg(prevEsp));
            }
        }
        emit finished(ok);
        currentDryRun_ = false;
        return ok;
    }

    // Otherwise, chroot into target root
    if (opt.target == GrubTarget::Esp) {
        if (currentDryRun_) {
            emit log(QStringLiteral("[dry-run] ensure-efivarfs"));
            emit log(QStringLiteral("[dry-run] remove-efi-dump"));
        } else {
            emit log(QStringLiteral("# ensure-efivarfs"));
            shell->ensureEfivarfsAsRoot(QuietMode::Yes);
            emit log(QStringLiteral("# remove-efi-dump"));
            shell->removeEfiDumpVarsAsRoot(QuietMode::Yes);
        }
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

    bool ok = false;
    if (opt.target == GrubTarget::Esp) {
        if (!shell->pathCheckAsRoot("/boot/efi", PathCheck::Directory, QuietMode::Yes, tmpdir.path())) {
            execProcAsRoot("mkdir", {"-p", tmpdir.path() + "/boot/efi"}, nullptr, nullptr, true);
        }
        if (!ensureMountFor(tmpdir.path(), "/boot/efi", opt.espDevice)) {
            cleanupMounts(tmpdir.path(), mapper);
            emit finished(false);
            currentDryRun_ = false;
            return false;
        }
        emit log(QStringLiteral("$ uname -m"));
        const QString arch = detectArch(shell);
        const QString bootloaderId = detectBootloaderId(tmpdir.path() + "/boot/efi", tmpdir.path(), shell);
        const QString grubTool = detectGrubInstallCmd(tmpdir.path(), shell);
        if (grubTool == "grub-install") {
            QStringList args {QStringLiteral("--target=%1-efi").arg(arch), QStringLiteral("--efi-directory=/boot/efi"),
                              QStringLiteral("--bootloader-id=%1").arg(bootloaderId)};
            if (grubSupportsForceExtraRemovable(shell, tmpdir.path())) {
                args << QStringLiteral("--force-extra-removable");
            }
            args << QStringLiteral("--recheck");
            ok = execProcAsRootInTarget(tmpdir.path(), "grub-install", args, nullptr, nullptr, true);
        } else if (grubTool == "grub-mkstandalone") {
            const bool hostHasMkstandalone = QFile::exists("/usr/bin/grub-mkstandalone")
                || QFile::exists("/usr/sbin/grub-mkstandalone");
            if (currentDryRun_) {
                emit log(QStringLiteral("[dry-run] grub-mkstandalone-efi --root %1 %2 %3")
                             .arg(tmpdir.path(), arch, bootloaderId));
                ok = true;
            } else {
                emit log(QStringLiteral("# grub-mkstandalone-efi --root %1 %2 %3")
                             .arg(tmpdir.path(), arch, bootloaderId));
                ok = shell->grubMkstandaloneEfiAsRoot(arch, bootloaderId, hostHasMkstandalone, QuietMode::Yes,
                                                      tmpdir.path());
            }
        } else {
            emit log(QStringLiteral("No GRUB installation tool found in target root (grub-install/grub-mkstandalone)."));
            cleanupMounts(tmpdir.path(), mapper);
            emit finished(false);
            currentDryRun_ = false;
            return false;
        }
    } else {
        const QString grubTool = detectGrubInstallCmd(tmpdir.path(), shell);
        if (grubTool == "grub-install") {
            ok = execProcAsRootInTarget(tmpdir.path(), "grub-install",
                                        {QStringLiteral("--target=i386-pc"), QStringLiteral("--recheck"),
                                         QStringLiteral("--force"), QStringLiteral("/dev/%1").arg(opt.location)},
                                        nullptr, nullptr, true);
        } else {
            emit log(QStringLiteral("grub-install is required for MBR/Root target but was not found in target root."));
            cleanupMounts(tmpdir.path(), mapper);
            emit finished(false);
            currentDryRun_ = false;
            return false;
        }
    }

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
            ok = execProcAsRoot("update-grub", {}, nullptr, nullptr, true);
        } else if (tool == "grub-mkconfig") {
            ok = execProcAsRoot("grub-mkconfig", {"-o", "/boot/grub/grub.cfg"}, nullptr, nullptr, true);
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
    if (shell->pathCheckAsRoot("/boot/efi", PathCheck::Directory, QuietMode::Yes, tmpdir.path())) {
        ensureMountFor(tmpdir.path(), "/boot/efi", opt.espDevice);
    }
    const QString tool = detectUpdateGrubCmd(tmpdir.path(), shell);
    bool ok = false;
    if (tool == "update-grub") {
        ok = execProcAsRootInTarget(tmpdir.path(), "update-grub", {}, nullptr, nullptr, true);
    } else if (tool == "grub-mkconfig") {
        ok = execProcAsRootInTarget(tmpdir.path(), "grub-mkconfig", {"-o", "/boot/grub/grub.cfg"}, nullptr,
                                    nullptr, true);
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
    const QString tool = detectInitramfsCmd(tmpdir.path(), shell);
    bool ok = false;
    if (tool == "update-initramfs") {
        ok = execProcAsRootInTarget(tmpdir.path(), "update-initramfs", {"-c", "-v", "-k", "all"}, nullptr,
                                    nullptr, true);
    } else if (tool == "mkinitcpio") {
        ok = execProcAsRootInTarget(tmpdir.path(), "mkinitcpio", {"-P"}, nullptr, nullptr, true);
    } else if (tool == "dracut") {
        ok = execProcAsRootInTarget(tmpdir.path(), "dracut", {"--regenerate-all", "--force"}, nullptr, nullptr,
                                    true);
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
    QString device;
    const QString fstabContent = shell->readFileAsRoot("/etc/fstab", QuietMode::Yes, tmpdir.path());
    const QStringList lines = fstabContent.split('\n', Qt::SkipEmptyParts);
    for (const auto& rawLine : lines) {
        const QString line = rawLine.simplified();
        if (line.isEmpty() || line.startsWith('#')) {
            continue;
        }
        const QStringList fields = line.split(' ');
        if (fields.size() >= 2 && fields.at(1) == mountpoint) {
            device = fields.at(0);
            break;
        }
    }
    cleanupMounts(tmpdir.path(), opened ? mapper : QString());
    return resolveDeviceSpec(device);
}

bool BootRepairEngine::lastFailureWasElevation() const
{
    return shell->lastElevationFailed();
}

bool BootRepairEngine::backup(const BootRepairOptions& opt)
{
    currentDryRun_ = opt.dryRun;
    if (opt.location.isEmpty() || opt.backupPath.isEmpty()) return false;
    const bool ok = execProcAsRoot("dd",
                                   {QStringLiteral("if=/dev/%1").arg(opt.location),
                                    QStringLiteral("of=%1").arg(opt.backupPath), QStringLiteral("bs=446"),
                                    QStringLiteral("count=1")},
                                   nullptr, nullptr, true);
    emit finished(ok);
    currentDryRun_ = false;
    return ok;
}

bool BootRepairEngine::restore(const BootRepairOptions& opt)
{
    currentDryRun_ = opt.dryRun;
    if (opt.location.isEmpty() || opt.backupPath.isEmpty()) return false;
    const bool ok = execProcAsRoot("dd",
                                   {QStringLiteral("if=%1").arg(opt.backupPath),
                                    QStringLiteral("of=/dev/%1").arg(opt.location), QStringLiteral("bs=446"),
                                    QStringLiteral("count=1")},
                                   nullptr, nullptr, true);
    emit finished(ok);
    currentDryRun_ = false;
    return ok;
}
