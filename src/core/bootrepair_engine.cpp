#include "core/bootrepair_engine.h"

#include <QCoreApplication>
#include <QDebug>
#include <QSignalBlocker>
#include <QScopeGuard>
#include <QSaveFile>
#include "helper/device.h"
#include "helper/path_validation.h"

#include <mntent.h>
#include <optional>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSysInfo>

QString BootRepairEngine::efiPlatform(const QString& machine, int firmwareBits)
{
    static const QRegularExpression x86(QStringLiteral("^(i[3-6]86|x86|x86_64|amd64)$"));
    if (x86.match(machine).hasMatch()) {
        if (firmwareBits == 32) return QStringLiteral("i386");
        if (firmwareBits == 64) return QStringLiteral("x86_64");
        return machine == "x86_64" || machine == "amd64" ? QStringLiteral("x86_64") : QStringLiteral("i386");
    }
    if (machine == "aarch64" || machine.startsWith("arm")) {
        if (firmwareBits == 32) return QStringLiteral("arm");
        if (firmwareBits == 64 || machine == "arm64" || machine == "aarch64") return QStringLiteral("arm64");
        return QStringLiteral("arm");
    }
    return machine;
}

namespace {
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

QString detectEfiPlatform()
{
    QFile sizeFile(QStringLiteral("/sys/firmware/efi/fw_platform_size"));
    const int bits = sizeFile.open(QIODevice::ReadOnly) ? sizeFile.readAll().trimmed().toInt() : 0;
    return BootRepairEngine::efiPlatform(QSysInfo::currentCpuArchitecture(), bits);
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
QString fstabSpec(const QString& fstabContent, const QString& mountpoint)
{
    const QStringList lines = fstabContent.split('\n', Qt::SkipEmptyParts);
    for (const auto& rawLine : lines) {
        const QString line = rawLine.simplified();
        if (line.isEmpty() || line.startsWith('#')) {
            continue;
        }
        const QStringList fields = line.split(' ');
        if (fields.size() >= 2 && fields.at(1) == mountpoint) {
            return fields.at(0);
        }
    }
    return {};
}

// An empty source means this directory is part of its parent filesystem.
std::optional<QString> exactMountSource(const QString& target)
{
    FILE* table = setmntent("/proc/self/mounts", "r");
    if (!table) return std::nullopt;
    const auto close = qScopeGuard([table] { endmntent(table); });
    const QByteArray path = QFile::encodeName(target);
    QString source;
    while (const mntent* entry = getmntent(table)) {
        if (path == entry->mnt_dir) source = QFile::decodeName(entry->mnt_fsname);
    }
    if (ferror(table)) return std::nullopt;
    return source;
}
} // namespace

BootRepairEngine::BootRepairEngine(QObject* parent, Execution mode)
    : QObject(parent), execution(mode), shell(new Cmd(this)),
      tmpdir((mode == Execution::PrivilegedHelper ? QStringLiteral("/run") : QDir::tempPath())
             + QStringLiteral("/mx-boot-repair-XXXXXX"))
{
    tmpdir.setAutoRemove(false);
    connect(shell, &Cmd::outputAvailable, this, [this](const QString& s) { emit log(s); });
    connect(shell, &Cmd::errorAvailable, this, [this](const QString& s) { emit log(s); });
}

BootRepairEngine::~BootRepairEngine()
{
    // rmdir only removes an empty, unmounted directory, even after a failed repair.
    if (tmpdir.isValid() && !QDir().rmdir(tmpdir.path())) {
        qWarning() << "Could not remove repair mount directory:" << tmpdir.path();
    }
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
    emit const_cast<BootRepairEngine*>(this)->log(
        QStringLiteral("$ lsblk -ln -o NAME,SIZE,LABEL,MODEL -d -e 2,11 -x NAME"));
    const QSignalBlocker blocker(const_cast<BootRepairEngine*>(this));
    QString output;
    shell->proc("lsblk", {"-ln", "-o", "NAME,SIZE,LABEL,MODEL", "-d", "-e", "2,11", "-x", "NAME"},
                &output, nullptr, QuietMode::Yes);
    static const QRegularExpression diskRx(
        QStringLiteral(R"(^(x?[hsv]d[a-z]+|mmcblk[0-9]+|nvme[0-9]+n[0-9]+)\b)"));
    QStringList result;
    for (const auto &line : output.split('\n', Qt::SkipEmptyParts)) {
        if (diskRx.match(line).hasMatch()) {
            result << line;
        }
    }
    return result;
}

QStringList BootRepairEngine::listPartitions() const
{
    emit const_cast<BootRepairEngine*>(this)->log(
        QStringLiteral("$ lsblk -ln -o NAME,SIZE,FSTYPE,MOUNTPOINT,LABEL -e 2,11 -x NAME"));
    const QSignalBlocker blocker(const_cast<BootRepairEngine*>(this));
    QString output;
    shell->proc("lsblk", {"-ln", "-o", "NAME,SIZE,FSTYPE,MOUNTPOINT,LABEL", "-e", "2,11", "-x", "NAME"},
                &output, nullptr, QuietMode::Yes);
    static const QRegularExpression partRx(
        QStringLiteral(R"(^(x?[hsv]d[a-z]+[0-9]+|mmcblk[0-9]+p[0-9]+|nvme[0-9]+n[0-9]+p[0-9]+)\b)"));
    QStringList result;
    for (const auto &line : output.split('\n', Qt::SkipEmptyParts)) {
        if (partRx.match(line).hasMatch()) {
            result << line;
        }
    }
    return result;
}

bool BootRepairEngine::isUefi()
{
    QDir dir("/sys/firmware/efi/efivars");
    return dir.exists() && !dir.entryList(QDir::NoDotAndDotDot | QDir::AllEntries).isEmpty();
}

MountState BootRepairEngine::isMounted(const QString& volume, const QString& mount) const
{
    return isMountedTo(volume, mount);
}

MountState BootRepairEngine::isMountedTo(const QString& volume, const QString& mount) const
{
    const bool wasSuppressed = shell->outputSuppressed();
    shell->setOutputSuppressed(true);
    QString points;
    bool queried = shell->proc("lsblk", {"-nro", "MOUNTPOINTS", volume}, &points, nullptr, QuietMode::Yes);
    if (!queried) {
        queried = shell->proc("lsblk", {"-nro", "MOUNTPOINT", volume}, &points, nullptr, QuietMode::Yes);
    }
    shell->setOutputSuppressed(wasSuppressed);
    if (!queried) {
        // Empty output is normal for an unmounted device; only warn when lsblk itself failed.
        qWarning().noquote() << "isMountedTo: lsblk failed to query mount points for" << volume;
        return MountState::QueryFailed;
    }
    return points.split('\n', Qt::SkipEmptyParts).contains(mount) ? MountState::Mounted : MountState::NotMounted;
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
    if (execution == Execution::Frontend) {
        QString fsType;
        if (shell->proc("lsblk", {"-dnro", "FSTYPE", dev}, &fsType, nullptr, QuietMode::Yes)
            && !fsType.isEmpty()) return fsType == "crypto_LUKS";
        return shell->repairAction({"is-luks", dev});
    }
    return shell->procAsRoot("cryptsetup", {"isLuks", dev}, nullptr, nullptr, QuietMode::Yes);
}

bool BootRepairEngine::canUnlockLuks(const QString& device, const QByteArray& pass)
{
    const QString dev = device.startsWith("/dev/") ? device : ("/dev/" + device);
    if (execution == Execution::Frontend) return shell->repairAction({"check-passphrase", dev}, nullptr, &pass);
    if (!isLuks(dev)) {
        return true;
    }

    const QString probeMapper = QStringLiteral("mxbr-probe-%1-%2")
                                    .arg(QCoreApplication::applicationPid())
                                    .arg(QString::number(QRandomGenerator::global()->generate64(), 16));
    if (!openLuks(dev, probeMapper, pass)) {
        return false;
    }
    return closeLuksMapper(probeMapper);
}

static inline QString normalizeDev(const QString& device)
{
    return device.startsWith("/dev/") ? device : ("/dev/" + device);
}

QString BootRepairEngine::mountSource(const QString& mountpoint) const
{
    return exactMountSource(mountpoint).value_or(QString());
}

bool BootRepairEngine::matchesEspPartitionType(const QString& partType)
{
    static const QRegularExpression rx(QStringLiteral("c12a7328-f81f-11d2-ba4b-00a0c93ec93b|0xef"),
                                        QRegularExpression::CaseInsensitiveOption);
    return rx.match(partType).hasMatch();
}

bool BootRepairEngine::matchesLinuxPartitionType(const QString& partType)
{
    static const QRegularExpression rx(
        QStringLiteral("0x83|0fc63daf-8483-4772-8e79-3d69d8477de4|"
                       "44479540-F297-41B2-9AF7-D131D5F0458A|4F68BCE3-E8CD-4DB1-96E7-FBCAF984B709"),
        QRegularExpression::CaseInsensitiveOption);
    return rx.match(partType).hasMatch();
}

static QString unescapeLsblkHex(const QString& value)
{
    static const QRegularExpression rx(QStringLiteral(R"re(\\x([0-9A-Fa-f]{2}))re"));
    QString result;
    result.reserve(value.size());
    int lastEnd = 0;
    auto it = rx.globalMatch(value);
    while (it.hasNext()) {
        const auto match = it.next();
        result += value.mid(lastEnd, match.capturedStart() - lastEnd);
        bool ok = false;
        const int byte = match.captured(1).toInt(&ok, 16);
        result += ok ? QChar(byte) : match.captured(0);
        lastEnd = match.capturedEnd();
    }
    result += value.mid(lastEnd);
    return result;
}

PartitionInfo BootRepairEngine::parsePartitionInfo(const QString& lsblkPairsOutput)
{
    PartitionInfo info;
    static const QRegularExpression rx(QStringLiteral(R"re((\w+)="([^"]*)")re"));
    auto it = rx.globalMatch(lsblkPairsOutput);
    while (it.hasNext()) {
        const auto match = it.next();
        const QString key = match.captured(1);
        const QString value = unescapeLsblkHex(match.captured(2));
        if (key == QLatin1String("PARTTYPE")) {
            info.partType = value;
        } else if (key == QLatin1String("FSTYPE")) {
            info.fsType = value;
        } else if (key == QLatin1String("LABEL")) {
            info.label = value;
        }
    }
    return info;
}

// This aggregate is used for single partitions. Whole-disk type checks use dedicated
// multi-row queries so any matching child partition is detected.
PartitionInfo BootRepairEngine::partitionInfo(const QString& device) const
{
    const QString dev = normalizeDev(device);
    emit const_cast<BootRepairEngine*>(this)->log(
        QStringLiteral("$ lsblk -n -P -o PARTTYPE,FSTYPE,LABEL %1").arg(dev));
    QString output;
    shell->proc("lsblk", {"-n", "-P", "-o", "PARTTYPE,FSTYPE,LABEL", dev}, &output, nullptr, QuietMode::Yes);
    return parsePartitionInfo(output);
}

bool BootRepairEngine::isEspPartition(const QString& device) const
{
    const QString dev = normalizeDev(device);
    emit const_cast<BootRepairEngine*>(this)->log(
        QStringLiteral("$ lsblk -ln -o PARTTYPE %1").arg(dev));
    QString output;
    shell->proc("lsblk", {"-ln", "-o", "PARTTYPE", dev}, &output, nullptr, QuietMode::Yes);
    return matchesEspPartitionType(output);
}

bool BootRepairEngine::isLinuxPartitionType(const QString& device) const
{
    const QString dev = normalizeDev(device);
    emit const_cast<BootRepairEngine*>(this)->log(
        QStringLiteral("$ lsblk -ln -o PARTTYPE %1").arg(dev));
    QString output;
    shell->proc("lsblk", {"-ln", "-o", "PARTTYPE", dev}, &output, nullptr, QuietMode::Yes);
    return matchesLinuxPartitionType(output);
}

bool BootRepairEngine::labelContains(const QString& device, const QString& needle) const
{
    return partitionInfo(device).label.contains(needle);
}

QString BootRepairEngine::filesystemType(const QString& device) const
{
    return partitionInfo(device).fsType;
}

QString BootRepairEngine::partitionLabel(const QString& device) const
{
    return partitionInfo(device).label;
}

bool BootRepairEngine::openLuks(const QString& part, const QString& mapper, const QByteArray& pass)
{
    if (pass.isEmpty()) {
        emit log(QStringLiteral("LUKS device requires a password for %1").arg(part));
        return false;
    }
    return execProcAsRoot("cryptsetup", {"luksOpen", part, mapper, "-"}, nullptr, &pass, true);
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

bool BootRepairEngine::cleanupMounts(const QString& path, const QString& luks)
{
    bool ok = true;
    if (path != "/") {
        if (currentDryRun_) {
            emit log(QStringLiteral("[dry-run] cleanup-chroot-env %1").arg(path));
        } else {
            emit log(QStringLiteral("# cleanup-chroot-env %1").arg(path));
            ok = shell->cleanupChrootEnvAsRoot(path, QuietMode::Yes);
            if (!ok) emit log(tr("Failed to unmount %1").arg(path));
        }
    }
    return closeLuksMapper(luks) && ok;
}

bool BootRepairEngine::closeLuksMapper(const QString& mapper)
{
    return mapper.isEmpty() || execProcAsRoot("cryptsetup", {"luksClose", mapper}, nullptr, nullptr, true);
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

    QString expected = device;
    if (expected.isEmpty()) {
        QString fstab;
        if (!shell->readFileAsRoot("/etc/fstab", &fstab, QuietMode::Yes, path)) {
            emit log(tr("Could not read the target fstab to identify %1").arg(mountpoint));
            return false;
        }
        const QString spec = fstabSpec(fstab, mountpoint);
        if (spec.isEmpty()) return true;
        expected = resolveDeviceSpec(spec);
        if (expected.isEmpty()) {
            emit log(tr("Could not resolve %1 for %2").arg(spec, mountpoint));
            return false;
        }
    }
    if (RepairDevice::canonicalBlock(expected).isEmpty()) {
        emit log(tr("Invalid boot device: %1").arg(expected));
        return false;
    }
    const HelperPath::TargetRoot root(path);
    const auto directory = root.directory(mountpoint, true);
    if (!directory) {
        emit log(tr("Could not open the target mount directory %1").arg(mountpoint));
        return false;
    }
    const auto mounted = exactMountSource(QFileInfo(directory.procPath()).canonicalFilePath());
    if (!mounted) {
        emit log(tr("Could not read the mount table for %1").arg(target));
        return false;
    }
    if (!mounted->isEmpty()) {
        const QString realDevice = QFileInfo(expected).canonicalFilePath();
        if (!realDevice.isEmpty() && realDevice == QFileInfo(*mounted).canonicalFilePath()) return true;
        emit log(tr("%1 is mounted from %2, expected %3").arg(target, *mounted, expected));
        return false;
    }
    return execProcAsRoot("mount", {"--no-canonicalize", expected, directory.procPath()});
}

bool BootRepairEngine::copyGrubLocales(const QString& rootPath)
{
    if (currentDryRun_) {
        if (rootPath.isEmpty()) {
            emit log(QStringLiteral("[dry-run] copy-grub-locales"));
        } else {
            emit log(QStringLiteral("[dry-run] copy-grub-locales --root %1").arg(rootPath));
        }
        return true;
    }

    if (rootPath.isEmpty()) {
        emit log(QStringLiteral("# copy-grub-locales"));
        return shell->copyGrubLocalesAsRoot(QuietMode::Yes);
    }

    emit log(QStringLiteral("# copy-grub-locales --root %1").arg(rootPath));
    return shell->copyGrubLocalesAsRoot(QuietMode::Yes, rootPath);
}

bool BootRepairEngine::planOfflineRepair(const QString& action, const BootRepairOptions& opt)
{
    const auto plan = [this](const QString& step) { emit log("[dry-run] " + step); };
    plan(tr("Unlock %1 if encrypted and mount the root filesystem at <target>").arg(opt.root));
    plan(tr("Prepare /dev, /proc, /sys and /run inside <target>"));
    if (opt.bootDevice.isEmpty()) plan(tr("Mount /boot from the target fstab if configured"));
    else plan(tr("Mount %1 at <target>/boot").arg(opt.bootDevice));
    if (action == "install" && opt.target == GrubTarget::Esp) {
        plan(tr("Mount %1 at <target>/boot/efi").arg(opt.espDevice));
        plan(tr("Run grub-install --target=%1-efi --efi-directory=/boot/efi in <target> using the detected bootloader ID, or grub-mkstandalone if required").arg(detectEfiPlatform()));
    } else if (action == "install") {
        plan(tr("Run grub-install --target=i386-pc --recheck --force --compress=gz --locales= %1 in <target>").arg(normalizeDev(opt.location)));
    } else if (action == "repair") {
        if (opt.espDevice.isEmpty()) plan(tr("Mount /boot/efi from the target fstab if configured"));
        else plan(tr("Mount %1 at <target>/boot/efi").arg(opt.espDevice));
        plan(tr("Run update-grub or grub-mkconfig -o /boot/grub/grub.cfg in <target>"));
    } else {
        plan(tr("Run the installed initramfs generator in <target> using update-initramfs -c -v -k all, mkinitcpio -P, or dracut --regenerate-all --force"));
    }
    if (action == "install") plan(tr("Copy GRUB locales after a successful grub-install"));
    plan(tr("Unmount <target> and close any mapping opened for the repair"));
    currentDryRun_ = false;
    emit finished(true);
    return true;
}

bool BootRepairEngine::delegateRepair(const QStringList& args, const BootRepairOptions& opt)
{
    const bool ok = shell->repairAction(args, nullptr, &opt.luksPassword);
    emit finished(ok);
    return ok;
}

bool BootRepairEngine::installGrub(const BootRepairOptions& opt)
{
    if (execution == Execution::Frontend && !opt.dryRun) {
        const QString target = opt.target == GrubTarget::Esp ? "esp" : (opt.target == GrubTarget::Mbr ? "mbr" : "root");
        return delegateRepair({"install", opt.root, target, normalizeDev(opt.location), opt.bootDevice, opt.espDevice}, opt);
    }
    currentDryRun_ = opt.dryRun;
    // Handle LUKS if needed
    QString root = opt.root;
    QString mapper;

    // If already mounted, no need to unlock LUKS
    const MountState initialRootState = isMountedTo(root, "/");
    if (initialRootState == MountState::QueryFailed) {
        emit log(QStringLiteral("Could not determine whether %1 is mounted; aborting.").arg(root));
        emit finished(false);
        currentDryRun_ = false;
        return false;
    }
    if (opt.dryRun && initialRootState == MountState::NotMounted) return planOfflineRepair("install", opt);
    if (initialRootState == MountState::NotMounted) {
        mapper = luksMapper(root);
        if (!mapper.isEmpty() && !root.startsWith("/dev/mapper/")) {
            if (!openLuks(root, mapper, opt.luksPassword)) {
                emit finished(false);
                currentDryRun_ = false;
                return false;
            }
            root = "/dev/mapper/" + mapper;
        }
    }

    // If installing on current root
    const MountState rootState = opt.dryRun ? initialRootState : isMountedTo(root, "/");
    if (rootState == MountState::QueryFailed) {
        closeLuksMapper(mapper);
        emit log(QStringLiteral("Could not determine whether %1 is mounted; aborting.").arg(root));
        emit finished(false);
        currentDryRun_ = false;
        return false;
    }
    if (rootState == MountState::Mounted) {
        bool ok = false;
        bool mountedEsp = false;
        if (opt.target == GrubTarget::Esp) {
            const auto original = exactMountSource("/boot/efi");
            const QString selected = opt.espDevice;
            if (!original || selected.isEmpty()) {
                emit log(tr("Could not determine the EFI mount source or destination."));
                emit finished(false);
                currentDryRun_ = false;
                return false;
            }
            const QString selectedPath = QFileInfo(selected).canonicalFilePath();
            if (original->isEmpty() || selectedPath.isEmpty()
                || QFileInfo(*original).canonicalFilePath() != selectedPath) {
                // Mount over the previous filesystem so its options and submounts survive intact.
                if (!execProcAsRoot("mkdir", {"-p", "/boot/efi"})
                    || !execProcAsRoot("mount", {selected, "/boot/efi"})) {
                    emit log(tr("Failed to mount %1 on /boot/efi").arg(selected));
                    emit finished(false);
                    currentDryRun_ = false;
                    return false;
                }
                mountedEsp = true;
            }
            const QString arch = detectEfiPlatform();
            const QString bootloaderId = detectBootloaderId(QStringLiteral("/boot/efi"), {});
            const QString grubTool = detectGrubInstallCmd({});
            if (grubTool == "grub-install") {
                QStringList args {QStringLiteral("--target=%1-efi").arg(arch), QStringLiteral("--efi-directory=/boot/efi"),
                                  QStringLiteral("--bootloader-id=%1").arg(bootloaderId)};
                if (grubSupportsForceExtraRemovable(shell)) {
                    args << QStringLiteral("--force-extra-removable");
                }
                args << QStringLiteral("--recheck") << QStringLiteral("--compress=gz") << QStringLiteral("--locales=");
                ok = execProcAsRoot("grub-install", args, nullptr, nullptr, true);
                if (ok) {
                    ok = copyGrubLocales();
                }
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
            }
        } else {
            const QString grubTool = detectGrubInstallCmd({});
            if (grubTool == "grub-install") {
                ok = execProcAsRoot("grub-install",
                                    {QStringLiteral("--target=i386-pc"), QStringLiteral("--recheck"),
                                     QStringLiteral("--force"), QStringLiteral("--compress=gz"), QStringLiteral("--locales="),
                                     QStringLiteral("/dev/%1").arg(opt.location)},
                                    nullptr, nullptr, true);
                if (ok) {
                    ok = copyGrubLocales();
                }
            } else {
                emit log(QStringLiteral("grub-install is required for MBR/Root target but was not found."));
                emit finished(false);
                currentDryRun_ = false;
                return false;
            }
        }
        if (mountedEsp && !execProcAsRoot("umount", {"/boot/efi"})) {
            emit log(tr("Failed to unmount the temporary EFI mount at /boot/efi."));
            ok = false;
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
        closeLuksMapper(mapper);
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
        if (!ensureMountFor(tmpdir.path(), "/boot/efi", opt.espDevice)) {
            cleanupMounts(tmpdir.path(), mapper);
            emit finished(false);
            currentDryRun_ = false;
            return false;
        }
        const QString arch = detectEfiPlatform();
        const QString bootloaderId = detectBootloaderId(tmpdir.path() + "/boot/efi", tmpdir.path(), shell);
        const QString grubTool = detectGrubInstallCmd(tmpdir.path(), shell);
        if (grubTool == "grub-install") {
            QStringList args {QStringLiteral("--target=%1-efi").arg(arch), QStringLiteral("--efi-directory=/boot/efi"),
                              QStringLiteral("--bootloader-id=%1").arg(bootloaderId)};
            if (grubSupportsForceExtraRemovable(shell, tmpdir.path())) {
                args << QStringLiteral("--force-extra-removable");
            }
            args << QStringLiteral("--recheck") << QStringLiteral("--compress=gz") << QStringLiteral("--locales=");
            ok = execProcAsRootInTarget(tmpdir.path(), "grub-install", args, nullptr, nullptr, true);
            if (ok) {
                ok = copyGrubLocales(tmpdir.path());
            }
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
                                         QStringLiteral("--force"), QStringLiteral("--compress=gz"), QStringLiteral("--locales="),
                                         QStringLiteral("/dev/%1").arg(opt.location)},
                                        nullptr, nullptr, true);
            if (ok) {
                ok = copyGrubLocales(tmpdir.path());
            }
        } else {
            emit log(QStringLiteral("grub-install is required for MBR/Root target but was not found in target root."));
            cleanupMounts(tmpdir.path(), mapper);
            emit finished(false);
            currentDryRun_ = false;
            return false;
        }
    }

    ok = cleanupMounts(tmpdir.path(), mapper) && ok;
    emit finished(ok);
    currentDryRun_ = false;
    return ok;
}

bool BootRepairEngine::repairGrub(const BootRepairOptions& opt)
{
    if (execution == Execution::Frontend && !opt.dryRun) {
        return delegateRepair({"repair", opt.root, opt.bootDevice, opt.espDevice}, opt);
    }
    currentDryRun_ = opt.dryRun;
    QString root = opt.root;
    QString mapper;

    // If already mounted, no need to unlock LUKS
    const MountState initialRootState = isMountedTo(root, "/");
    if (initialRootState == MountState::QueryFailed) {
        emit log(QStringLiteral("Could not determine whether %1 is mounted; aborting.").arg(root));
        emit finished(false);
        currentDryRun_ = false;
        return false;
    }
    if (opt.dryRun && initialRootState == MountState::NotMounted) return planOfflineRepair("repair", opt);
    if (initialRootState == MountState::NotMounted) {
        mapper = luksMapper(root);
        if (!mapper.isEmpty() && !root.startsWith("/dev/mapper/")) {
            if (!openLuks(root, mapper, opt.luksPassword)) {
                emit finished(false);
                currentDryRun_ = false;
                return false;
            }
            root = "/dev/mapper/" + mapper;
        }
    }

    const MountState rootState = opt.dryRun ? initialRootState : isMountedTo(root, "/");
    if (rootState == MountState::QueryFailed) {
        closeLuksMapper(mapper);
        emit log(QStringLiteral("Could not determine whether %1 is mounted; aborting.").arg(root));
        emit finished(false);
        currentDryRun_ = false;
        return false;
    }
    if (rootState == MountState::Mounted) {
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
        closeLuksMapper(mapper);
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
    if ((!currentDryRun_ || !opt.espDevice.isEmpty())
        && !ensureMountFor(tmpdir.path(), "/boot/efi", opt.espDevice)) {
        if (!opt.espDevice.isEmpty() || lastFailureWasElevation()) {
            cleanupMounts(tmpdir.path(), mapper);
            emit finished(false);
            currentDryRun_ = false;
            return false;
        }
        emit log(tr("Warning: the configured ESP is unavailable. Continuing with GRUB configuration on /boot."));
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
    ok = cleanupMounts(tmpdir.path(), mapper) && ok;
    emit finished(ok);
    currentDryRun_ = false;
    return ok;
}

bool BootRepairEngine::regenerateInitramfs(const BootRepairOptions& opt)
{
    if (execution == Execution::Frontend && !opt.dryRun) {
        return delegateRepair({"initramfs", opt.root, opt.bootDevice}, opt);
    }
    currentDryRun_ = opt.dryRun;
    QString root = opt.root;
    QString mapper;

    // If already mounted, no need to unlock LUKS
    const MountState initialRootState = isMountedTo(root, "/");
    if (initialRootState == MountState::QueryFailed) {
        emit log(QStringLiteral("Could not determine whether %1 is mounted; aborting.").arg(root));
        emit finished(false);
        currentDryRun_ = false;
        return false;
    }
    if (opt.dryRun && initialRootState == MountState::NotMounted) return planOfflineRepair("initramfs", opt);
    if (initialRootState == MountState::NotMounted) {
        mapper = luksMapper(root);
        if (!mapper.isEmpty() && !root.startsWith("/dev/mapper/")) {
            if (!openLuks(root, mapper, opt.luksPassword)) {
                emit finished(false);
                currentDryRun_ = false;
                return false;
            }
            root = "/dev/mapper/" + mapper;
        }
    }

    const MountState rootState = opt.dryRun ? initialRootState : isMountedTo(root, "/");
    if (rootState == MountState::QueryFailed) {
        closeLuksMapper(mapper);
        emit log(QStringLiteral("Could not determine whether %1 is mounted; aborting.").arg(root));
        emit finished(false);
        currentDryRun_ = false;
        return false;
    }
    if (rootState == MountState::Mounted) {
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
        closeLuksMapper(mapper);
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
    ok = cleanupMounts(tmpdir.path(), mapper) && ok;
    emit finished(ok);
    currentDryRun_ = false;
    return ok;
}

bool BootRepairEngine::lastFailureWasElevation() const
{
    return shell->lastElevationFailed();
}

bool BootRepairEngine::backup(const BootRepairOptions& opt)
{
    if (opt.dryRun) {
        emit log(QStringLiteral("[dry-run] backup %1 to %2").arg(normalizeDev(opt.location), opt.backupPath));
        emit finished(true);
        return true;
    }
    QString encoded;
    shell->setOutputSuppressed(true);
    const bool readOk = shell->repairAction({"read-record", normalizeDev(opt.location)}, &encoded);
    shell->setOutputSuppressed(false);
    const QByteArray record = QByteArray::fromBase64(encoded.toLatin1(), QByteArray::AbortOnBase64DecodingErrors);
    QSaveFile file(opt.backupPath);
    const bool ok = readOk && record.size() == 446 && file.open(QIODevice::WriteOnly)
        && file.write(record) == record.size() && file.commit();
    if (!ok) emit log(tr("Could not back up the boot record to %1").arg(opt.backupPath));
    emit finished(ok);
    return ok;
}

bool BootRepairEngine::restore(const BootRepairOptions& opt)
{
    if (opt.dryRun) {
        emit log(QStringLiteral("[dry-run] restore %1 to %2").arg(opt.backupPath, normalizeDev(opt.location)));
        emit finished(true);
        return true;
    }
    QFile image(opt.backupPath);
    if (!image.open(QIODevice::ReadOnly)) {
        emit log(image.errorString());
        emit finished(false);
        return false;
    }
    const QByteArray record = image.read(446);
    if (record.size() != 446 || image.error() != QFileDevice::NoError) {
        emit log(tr("The backup does not contain a complete 446-byte boot record."));
        emit finished(false);
        return false;
    }
    const bool ok = shell->repairAction({"write-record", normalizeDev(opt.location)}, nullptr, &record);
    emit finished(ok);
    return ok;
}
