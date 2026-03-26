/**********************************************************************
 *  helper.cpp
 **********************************************************************
 * Copyright (C) 2026 MX Authors
 *
 * Authors: Adrian
 *          MX Linux <http://mxlinux.org>
 *          OpenAI Codex
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this package. If not, see <http://www.gnu.org/licenses/>.
 **********************************************************************/

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QProcess>
#include <QRegularExpression>
#include <QStringList>

#include <cstdio>

namespace
{
struct ProcessResult
{
    bool started = false;
    int exitCode = 1;
    QProcess::ExitStatus exitStatus = QProcess::NormalExit;
    QByteArray standardOutput;
    QByteArray standardError;
};

void writeAndFlush(FILE *stream, const QByteArray &data)
{
    if (!data.isEmpty()) {
        std::fwrite(data.constData(), 1, static_cast<size_t>(data.size()), stream);
        std::fflush(stream);
    }
}

void printError(const QString &message)
{
    writeAndFlush(stderr, message.toUtf8() + '\n');
}

[[nodiscard]] QString joinTargetPath(const QString &rootPath, const QString &path)
{
    if (rootPath.isEmpty()) {
        return path;
    }
    return QDir(rootPath).filePath(path.mid(1));
}

[[nodiscard]] bool isValidRootPath(const QString &rootPath)
{
    return rootPath.isEmpty() || (QDir::isAbsolutePath(rootPath) && QDir(rootPath).exists());
}

[[nodiscard]] bool isValidAbsolutePath(const QString &path)
{
    return QDir::isAbsolutePath(path) && !path.contains(QLatin1Char('\n'));
}

[[nodiscard]] bool isSafeChildPath(const QString &rootPath, const QString &requestedPath)
{
    if (rootPath.isEmpty()) {
        return isValidAbsolutePath(requestedPath);
    }

    const QString canonicalRoot = QDir(rootPath).canonicalPath();
    if (canonicalRoot.isEmpty()) {
        return false;
    }

    const QString candidate = QDir::cleanPath(joinTargetPath(rootPath, requestedPath));
    return candidate == canonicalRoot || candidate.startsWith(canonicalRoot + '/');
}

[[nodiscard]] const QHash<QString, QStringList> &allowedCommands()
{
    static const QHash<QString, QStringList> commands {
        {QStringLiteral("cryptsetup"),
         {QStringLiteral("/usr/sbin/cryptsetup"), QStringLiteral("/sbin/cryptsetup"),
          QStringLiteral("/usr/bin/cryptsetup")}},
        {QStringLiteral("dd"), {QStringLiteral("/usr/bin/dd"), QStringLiteral("/bin/dd")}},
        {QStringLiteral("dracut"),
         {QStringLiteral("/usr/bin/dracut"), QStringLiteral("/usr/sbin/dracut"),
          QStringLiteral("/sbin/dracut")}},
        {QStringLiteral("grub-install"),
         {QStringLiteral("/usr/sbin/grub-install"), QStringLiteral("/usr/bin/grub-install"),
          QStringLiteral("/sbin/grub-install"), QStringLiteral("/bin/grub-install")}},
        {QStringLiteral("grub-mkconfig"),
         {QStringLiteral("/usr/sbin/grub-mkconfig"), QStringLiteral("/usr/bin/grub-mkconfig"),
          QStringLiteral("/sbin/grub-mkconfig"), QStringLiteral("/bin/grub-mkconfig")}},
        {QStringLiteral("grub-mkstandalone"),
         {QStringLiteral("/usr/bin/grub-mkstandalone"), QStringLiteral("/usr/sbin/grub-mkstandalone"),
          QStringLiteral("/bin/grub-mkstandalone"), QStringLiteral("/sbin/grub-mkstandalone")}},
        {QStringLiteral("mkdir"), {QStringLiteral("/usr/bin/mkdir"), QStringLiteral("/bin/mkdir")}},
        {QStringLiteral("mount"), {QStringLiteral("/usr/bin/mount"), QStringLiteral("/bin/mount")}},
        {QStringLiteral("mountpoint"), {QStringLiteral("/usr/bin/mountpoint"), QStringLiteral("/bin/mountpoint")}},
        {QStringLiteral("mkinitcpio"), {QStringLiteral("/usr/bin/mkinitcpio")}},
        {QStringLiteral("rm"), {QStringLiteral("/usr/bin/rm"), QStringLiteral("/bin/rm")}},
        {QStringLiteral("rmdir"), {QStringLiteral("/usr/bin/rmdir"), QStringLiteral("/bin/rmdir")}},
        {QStringLiteral("umount"), {QStringLiteral("/usr/bin/umount"), QStringLiteral("/bin/umount")}},
        {QStringLiteral("update-grub"),
         {QStringLiteral("/usr/sbin/update-grub"), QStringLiteral("/usr/bin/update-grub"),
          QStringLiteral("/sbin/update-grub")}},
        {QStringLiteral("update-initramfs"),
         {QStringLiteral("/usr/sbin/update-initramfs"), QStringLiteral("/usr/bin/update-initramfs"),
          QStringLiteral("/sbin/update-initramfs")}},
    };
    return commands;
}

[[nodiscard]] QString resolveBinary(const QStringList &candidates, const QString &rootPath = {})
{
    for (const QString &candidate : candidates) {
        const QString fullPath = rootPath.isEmpty() ? candidate : joinTargetPath(rootPath, candidate);
        const QFileInfo info(fullPath);
        if (info.exists() && info.isExecutable()) {
            return candidate;
        }
    }
    return {};
}

[[nodiscard]] QString resolveHostBinary(const QStringList &candidates)
{
    return resolveBinary(candidates);
}

[[nodiscard]] QString resolveChrootBinary()
{
    static const QStringList candidates {QStringLiteral("/usr/sbin/chroot"), QStringLiteral("/usr/bin/chroot"),
                                         QStringLiteral("/sbin/chroot"), QStringLiteral("/bin/chroot")};
    return resolveHostBinary(candidates);
}

[[nodiscard]] ProcessResult runProcess(const QString &program, const QStringList &args, const QByteArray &input = {})
{
    ProcessResult result;

    QProcess process;
    process.start(program, args, QIODevice::ReadWrite);
    if (!process.waitForStarted()) {
        result.standardError = QStringLiteral("Failed to start %1").arg(program).toUtf8();
        result.exitCode = 127;
        return result;
    }

    result.started = true;
    if (!input.isEmpty()) {
        process.write(input);
    }
    process.closeWriteChannel();
    process.waitForFinished(-1);

    result.exitStatus = process.exitStatus();
    result.exitCode = process.exitCode();
    result.standardOutput = process.readAllStandardOutput();
    result.standardError = process.readAllStandardError();
    return result;
}

[[nodiscard]] int relayResult(const ProcessResult &result)
{
    writeAndFlush(stdout, result.standardOutput);
    writeAndFlush(stderr, result.standardError);
    if (!result.started) {
        return result.exitCode;
    }
    return result.exitStatus == QProcess::NormalExit ? result.exitCode : 1;
}

[[nodiscard]] QByteArray readHelperInput()
{
    QFile input;
    if (!input.open(stdin, QIODevice::ReadOnly)) {
        return {};
    }
    return input.readAll();
}

[[nodiscard]] int runAllowedCommand(const QString &rootPath, const QString &command, const QStringList &commandArgs,
                                    const QByteArray &input = {})
{
    const auto it = allowedCommands().constFind(command);
    if (it == allowedCommands().constEnd()) {
        printError(QStringLiteral("Command is not allowed: %1").arg(command));
        return 127;
    }

    const QString resolvedCommand = resolveBinary(it.value(), rootPath);
    if (resolvedCommand.isEmpty()) {
        printError(QStringLiteral("Command is not available: %1").arg(command));
        return 127;
    }

    if (rootPath.isEmpty()) {
        return relayResult(runProcess(resolvedCommand, commandArgs, input));
    }

    const QString chrootBinary = resolveChrootBinary();
    if (chrootBinary.isEmpty()) {
        printError(QStringLiteral("Unable to find chroot executable"));
        return 127;
    }

    QStringList chrootArgs {rootPath, resolvedCommand};
    chrootArgs += commandArgs;
    return relayResult(runProcess(chrootBinary, chrootArgs, input));
}

[[nodiscard]] int handleExec(const QString &rootPath, const QStringList &args)
{
    if (args.isEmpty()) {
        printError(QStringLiteral("exec requires a command name"));
        return 1;
    }
    return runAllowedCommand(rootPath, args.constFirst(), args.mid(1), readHelperInput());
}

[[nodiscard]] int handleReadFile(const QString &rootPath, const QStringList &args)
{
    if (args.size() != 1) {
        printError(QStringLiteral("read-file requires exactly one path"));
        return 1;
    }

    const QString path = args.constFirst();
    if (!isValidAbsolutePath(path) || !isSafeChildPath(rootPath, path)) {
        printError(QStringLiteral("Invalid read-file path: %1").arg(path));
        return 1;
    }

    QFile file(joinTargetPath(rootPath, path));
    if (!file.open(QIODevice::ReadOnly)) {
        printError(QStringLiteral("Unable to read %1").arg(path));
        return 1;
    }

    writeAndFlush(stdout, file.readAll());
    return 0;
}

[[nodiscard]] int handleListDir(const QString &rootPath, const QStringList &args)
{
    if (args.size() != 1) {
        printError(QStringLiteral("list-dir requires exactly one path"));
        return 1;
    }

    const QString path = args.constFirst();
    if (!isValidAbsolutePath(path) || !isSafeChildPath(rootPath, path)) {
        printError(QStringLiteral("Invalid list-dir path: %1").arg(path));
        return 1;
    }

    const QDir dir(joinTargetPath(rootPath, path));
    if (!dir.exists()) {
        printError(QStringLiteral("Directory does not exist: %1").arg(path));
        return 1;
    }

    const QStringList entries = dir.entryList(QDir::AllEntries | QDir::NoDotAndDotDot, QDir::Name);
    writeAndFlush(stdout, entries.join('\n').toUtf8());
    return 0;
}

[[nodiscard]] int handlePathCheck(const QString &rootPath, const QStringList &args)
{
    if (args.size() != 2) {
        printError(QStringLiteral("path-check requires mode and path"));
        return 1;
    }

    const QString mode = args.at(0);
    const QString path = args.at(1);
    if (!isValidAbsolutePath(path) || !isSafeChildPath(rootPath, path)) {
        printError(QStringLiteral("Invalid path-check path: %1").arg(path));
        return 1;
    }

    const QFileInfo info(joinTargetPath(rootPath, path));
    if (mode == QLatin1String("exists")) {
        return info.exists() ? 0 : 1;
    }
    if (mode == QLatin1String("dir")) {
        return info.exists() && info.isDir() ? 0 : 1;
    }
    if (mode == QLatin1String("exec")) {
        return info.exists() && info.isExecutable() ? 0 : 1;
    }

    printError(QStringLiteral("Unsupported path-check mode: %1").arg(mode));
    return 1;
}

[[nodiscard]] int handleDirHasEntries(const QString &rootPath, const QStringList &args)
{
    if (args.size() != 1) {
        printError(QStringLiteral("dir-has-entries requires exactly one path"));
        return 1;
    }

    const QString path = args.constFirst();
    if (!isValidAbsolutePath(path) || !isSafeChildPath(rootPath, path)) {
        printError(QStringLiteral("Invalid dir-has-entries path: %1").arg(path));
        return 1;
    }

    const QDir dir(joinTargetPath(rootPath, path));
    if (!dir.exists()) {
        return 1;
    }

    return dir.entryList(QDir::AllEntries | QDir::NoDotAndDotDot).isEmpty() ? 1 : 0;
}

[[nodiscard]] bool isMountpoint(const QString &path)
{
    return runAllowedCommand({}, QStringLiteral("mountpoint"), {QStringLiteral("-q"), path}) == 0;
}

[[nodiscard]] int handleMountChrootEnv(const QStringList &args)
{
    if (args.size() != 2) {
        printError(QStringLiteral("mount-chroot-env requires source and target"));
        return 1;
    }

    const QString source = args.at(0);
    const QString target = args.at(1);
    if (!isValidAbsolutePath(source) || !isValidAbsolutePath(target)) {
        printError(QStringLiteral("mount-chroot-env requires absolute paths"));
        return 1;
    }

    QDir().mkpath(target);

    int exitCode = runAllowedCommand({}, QStringLiteral("mount"), {source, target});
    if (exitCode != 0) {
        return exitCode;
    }

    exitCode = runAllowedCommand(
        {}, QStringLiteral("mount"),
        {QStringLiteral("--rbind"), QStringLiteral("--make-rslave"), QStringLiteral("/dev"), target + "/dev"});
    if (exitCode != 0) {
        return exitCode;
    }

    if (QDir(target + "/sys").exists()) {
        exitCode = runAllowedCommand(
            {}, QStringLiteral("mount"),
            {QStringLiteral("--rbind"), QStringLiteral("--make-rslave"), QStringLiteral("/sys"), target + "/sys"});
        if (exitCode != 0) {
            return exitCode;
        }
    }

    if (QDir(target + "/proc").exists()) {
        exitCode =
            runAllowedCommand({}, QStringLiteral("mount"), {QStringLiteral("--rbind"), QStringLiteral("/proc"), target + "/proc"});
        if (exitCode != 0) {
            return exitCode;
        }
    }

    if (QDir(target + "/run").exists()) {
        exitCode = runAllowedCommand({}, QStringLiteral("mount"),
                                     {QStringLiteral("-t"), QStringLiteral("tmpfs"), QStringLiteral("-o"),
                                      QStringLiteral("size=100m,nodev,mode=755"), QStringLiteral("tmpfs"),
                                      target + "/run"});
        if (exitCode != 0) {
            return exitCode;
        }

        if (QDir(QStringLiteral("/run/udev")).exists()) {
            QDir().mkpath(target + "/run/udev");
            exitCode = runAllowedCommand({}, QStringLiteral("mount"),
                                         {QStringLiteral("--rbind"), QStringLiteral("/run/udev"),
                                          target + "/run/udev"});
            if (exitCode != 0) {
                return exitCode;
            }
        }
    }

    return 0;
}

[[nodiscard]] int handleCleanupChrootEnv(const QStringList &args)
{
    if (args.size() != 1) {
        printError(QStringLiteral("cleanup-chroot-env requires target"));
        return 1;
    }

    const QString target = args.constFirst();
    if (!isValidAbsolutePath(target)) {
        printError(QStringLiteral("cleanup-chroot-env requires an absolute target"));
        return 1;
    }

    if (isMountpoint(target + "/boot/efi")) {
        const int exitCode = runAllowedCommand({}, QStringLiteral("umount"), {target + "/boot/efi"});
        if (exitCode != 0) {
            return exitCode;
        }
    }

    if (isMountpoint(target + "/boot")) {
        const int exitCode = runAllowedCommand({}, QStringLiteral("umount"), {QStringLiteral("-R"), target + "/boot"});
        if (exitCode != 0) {
            return exitCode;
        }
    }

    if (isMountpoint(target + "/run")) {
        const int exitCode = runAllowedCommand({}, QStringLiteral("umount"), {QStringLiteral("-R"), target + "/run"});
        if (exitCode != 0) {
            return exitCode;
        }
    }
    if (isMountpoint(target + "/proc")) {
        const int exitCode = runAllowedCommand({}, QStringLiteral("umount"), {QStringLiteral("-R"), target + "/proc"});
        if (exitCode != 0) {
            return exitCode;
        }
    }
    if (isMountpoint(target + "/sys")) {
        const int exitCode = runAllowedCommand({}, QStringLiteral("umount"), {QStringLiteral("-R"), target + "/sys"});
        if (exitCode != 0) {
            return exitCode;
        }
    }
    if (isMountpoint(target + "/dev")) {
        const int exitCode = runAllowedCommand({}, QStringLiteral("umount"), {QStringLiteral("-R"), target + "/dev"});
        if (exitCode != 0) {
            return exitCode;
        }
    }
    if (isMountpoint(target)) {
        const int exitCode = runAllowedCommand({}, QStringLiteral("umount"), {target});
        if (exitCode != 0) {
            return exitCode;
        }
    }

    return runAllowedCommand({}, QStringLiteral("rmdir"), {target});
}

[[nodiscard]] int handleEnsureEfivarfs()
{
    QFile mounts(QStringLiteral("/proc/self/mounts"));
    if (mounts.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const QString content = QString::fromUtf8(mounts.readAll());
        if (content.contains(QStringLiteral(" efivarfs "))) {
            return 0;
        }
    }

    if (!QDir(QStringLiteral("/sys/firmware/efi/efivars")).exists()) {
        return 0;
    }

    return runAllowedCommand({}, QStringLiteral("mount"),
                             {QStringLiteral("-t"), QStringLiteral("efivarfs"), QStringLiteral("efivarfs"),
                              QStringLiteral("/sys/firmware/efi/efivars")});
}

[[nodiscard]] int handleRemoveEfiDump()
{
    const QString path = QStringLiteral("/sys/firmware/efi/efivars");
    const QDir dir(path);
    if (!dir.exists()) {
        return 0;
    }

    const QStringList entries = dir.entryList({QStringLiteral("dump*")}, QDir::Files, QDir::Name);
    for (const QString &entry : entries) {
        if (!QFile::remove(path + '/' + entry)) {
            printError(QStringLiteral("Unable to remove %1/%2").arg(path, entry));
            return 1;
        }
    }
    return 0;
}

[[nodiscard]] QString efiArchSuffix(const QString &arch)
{
    if (arch == QLatin1String("x86_64")) {
        return QStringLiteral("x64");
    }
    if (arch == QLatin1String("i386")) {
        return QStringLiteral("ia32");
    }
    if (arch == QLatin1String("aarch64") || arch == QLatin1String("arm64")) {
        return QStringLiteral("aa64");
    }
    return QStringLiteral("x64");
}

[[nodiscard]] int handleGrubMkstandaloneEfi(const QString &rootPath, const QStringList &args)
{
    if (args.size() != 3) {
        printError(QStringLiteral("grub-mkstandalone-efi requires arch, bootloader-id, and host-binary flag"));
        return 1;
    }

    const QString arch = args.at(0);
    const QString bootloaderId = args.at(1);
    const bool useHostBinary = (args.at(2) == QLatin1String("1"));

    static const QRegularExpression nameRx(
        QRegularExpression::anchoredPattern(QStringLiteral(R"([A-Za-z0-9._+-]+)")));
    if (!nameRx.match(arch).hasMatch() || !nameRx.match(bootloaderId).hasMatch()) {
        printError(QStringLiteral("Invalid grub-mkstandalone-efi arguments"));
        return 1;
    }

    const QString suffix = efiArchSuffix(arch);
    const QString upperSuffix = suffix.toUpper();
    const QString efiTarget = QStringLiteral("%1-efi").arg(arch);

    const QString runDir = joinTargetPath(rootPath, QStringLiteral("/run"));
    const QString efiDir = joinTargetPath(rootPath, QStringLiteral("/boot/efi/EFI/%1").arg(bootloaderId));
    const QString fallbackDir = joinTargetPath(rootPath, QStringLiteral("/boot/efi/EFI/BOOT"));
    const QString earlyCfgPath = joinTargetPath(rootPath, QStringLiteral("/run/grub-early.cfg"));
    const QString outPath =
        joinTargetPath(rootPath, QStringLiteral("/boot/efi/EFI/%1/grub%2.efi").arg(bootloaderId, suffix));
    const QString fallbackPath =
        joinTargetPath(rootPath, QStringLiteral("/boot/efi/EFI/BOOT/BOOT%1.EFI").arg(upperSuffix));

    if (!QDir().mkpath(runDir) || !QDir().mkpath(efiDir) || !QDir().mkpath(fallbackDir)) {
        printError(QStringLiteral("Unable to prepare EFI output directories"));
        return 1;
    }

    QFile earlyCfg(earlyCfgPath);
    if (!earlyCfg.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        printError(QStringLiteral("Unable to create %1").arg(earlyCfgPath));
        return 1;
    }

    const QByteArray earlyCfgContent =
        "search --no-floppy --file /boot/grub/grub.cfg --set=root\n"
        "set prefix=($root)/boot/grub\n"
        "configfile $prefix/grub.cfg\n";
    earlyCfg.write(earlyCfgContent);
    earlyCfg.close();

    int exitCode = 1;
    const QString standaloneBinary = (rootPath.isEmpty() || !useHostBinary)
        ? resolveBinary(allowedCommands().value(QStringLiteral("grub-mkstandalone")), rootPath)
        : resolveHostBinary(allowedCommands().value(QStringLiteral("grub-mkstandalone")));
    if (standaloneBinary.isEmpty()) {
        printError(QStringLiteral("grub-mkstandalone is not available"));
        QFile::remove(earlyCfgPath);
        return 127;
    }

    if (rootPath.isEmpty() || useHostBinary) {
        QStringList processArgs;
        if (!rootPath.isEmpty()) {
            processArgs << QStringLiteral("--directory=%1").arg(joinTargetPath(rootPath, QStringLiteral("/usr/lib/grub/%1"))
                                                                    .arg(efiTarget));
        }
        processArgs << QStringLiteral("-O") << efiTarget << QStringLiteral("-o") << outPath
                    << QStringLiteral("boot/grub/grub.cfg=%1").arg(earlyCfgPath);
        exitCode = relayResult(runProcess(standaloneBinary, processArgs));
    } else {
        const QString chrootBinary = resolveChrootBinary();
        if (chrootBinary.isEmpty()) {
            printError(QStringLiteral("Unable to find chroot executable"));
            QFile::remove(earlyCfgPath);
            return 127;
        }

        const QString targetOutPath =
            QStringLiteral("/boot/efi/EFI/%1/grub%2.efi").arg(bootloaderId, suffix);
        const QString targetCfgPath = QStringLiteral("/run/grub-early.cfg");
        const QStringList processArgs {rootPath, standaloneBinary, QStringLiteral("-O"), efiTarget,
                                       QStringLiteral("-o"), targetOutPath,
                                       QStringLiteral("boot/grub/grub.cfg=%1").arg(targetCfgPath)};
        exitCode = relayResult(runProcess(chrootBinary, processArgs));
    }

    if (exitCode == 0) {
        QFile::remove(fallbackPath);
        if (!QFile::copy(outPath, fallbackPath)) {
            printError(QStringLiteral("Unable to copy %1 to %2").arg(outPath, fallbackPath));
            exitCode = 1;
        }
    }

    QFile::remove(earlyCfgPath);
    return exitCode;
}

[[nodiscard]] int handleCopyLog()
{
    const QString source = QStringLiteral("/tmp/mx-boot-repair.log");
    const QString target = QStringLiteral("/var/log/mx-boot-repair.log");
    const QString backup = QStringLiteral("/var/log/mx-boot-repair.log.old");

    // copy-log is a dedicated helper action, not generic command dispatch, so
    // it resolves and runs the two required host binaries directly here.
    const QString mvBinary =
        resolveHostBinary({QStringLiteral("/usr/bin/mv"), QStringLiteral("/bin/mv")});
    const QString cpBinary =
        resolveHostBinary({QStringLiteral("/usr/bin/cp"), QStringLiteral("/bin/cp")});
    if (mvBinary.isEmpty() || cpBinary.isEmpty()) {
        printError(QStringLiteral("Unable to find cp/mv"));
        return 127;
    }

    if (QFile::exists(target)) {
        const int exitCode =
            relayResult(runProcess(mvBinary, {QStringLiteral("--backup=numbered"), target, backup}));
        if (exitCode != 0) {
            return exitCode;
        }
    }

    return relayResult(runProcess(cpBinary, {source, target}));
}

[[nodiscard]] int handleCopyGrubLocales(const QString &rootPath)
{
    const QString localeRoot = joinTargetPath(rootPath, QStringLiteral("/usr/share/locale"));
    const QString grubLocaleDir = joinTargetPath(rootPath, QStringLiteral("/boot/grub/locale"));
    if (!QDir().mkpath(grubLocaleDir)) {
        printError(QStringLiteral("Unable to create %1").arg(grubLocaleDir));
        return 1;
    }

    const QDir localeDir(localeRoot);
    const QStringList localeNames =
        localeDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::Readable, QDir::Name);
    for (const QString &localeName : localeNames) {
        const QString sourcePath =
            localeDir.filePath(QStringLiteral("%1/LC_MESSAGES/grub.mo").arg(localeName));
        const QFileInfo sourceInfo(sourcePath);
        if (!sourceInfo.exists() || !sourceInfo.isFile()) {
            continue;
        }

        const QString targetPath = QDir(grubLocaleDir).filePath(localeName + QStringLiteral(".mo"));
        QFile::remove(targetPath);
        if (!QFile::copy(sourcePath, targetPath)) {
            printError(QStringLiteral("Unable to copy %1 to %2").arg(sourcePath, targetPath));
            return 1;
        }
        QFile targetFile(targetPath);
        targetFile.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ReadGroup
                                  | QFileDevice::ReadOther);
    }

    return 0;
}
} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QStringList arguments = app.arguments().mid(1);
    if (arguments.isEmpty()) {
        printError(QStringLiteral("Missing helper action"));
        return 1;
    }

    const QString action = arguments.takeFirst();

    QString rootPath;
    if (!arguments.isEmpty() && arguments.constFirst() == QLatin1String("--root")) {
        if (arguments.size() < 2) {
            printError(QStringLiteral("Missing value after --root"));
            return 1;
        }
        rootPath = arguments.at(1);
        arguments = arguments.mid(2);
    }

    if (!isValidRootPath(rootPath)) {
        printError(QStringLiteral("Invalid root path: %1").arg(rootPath));
        return 1;
    }

    if (action == QLatin1String("cleanup-chroot-env")) {
        if (!rootPath.isEmpty()) {
            printError(QStringLiteral("cleanup-chroot-env does not accept --root"));
            return 1;
        }
        return handleCleanupChrootEnv(arguments);
    }
    if (action == QLatin1String("copy-log")) {
        if (!rootPath.isEmpty()) {
            printError(QStringLiteral("copy-log does not accept --root"));
            return 1;
        }
        return handleCopyLog();
    }
    if (action == QLatin1String("copy-grub-locales")) {
        return handleCopyGrubLocales(rootPath);
    }
    if (action == QLatin1String("dir-has-entries")) {
        return handleDirHasEntries(rootPath, arguments);
    }
    if (action == QLatin1String("ensure-efivarfs")) {
        if (!rootPath.isEmpty()) {
            printError(QStringLiteral("ensure-efivarfs does not accept --root"));
            return 1;
        }
        return handleEnsureEfivarfs();
    }
    if (action == QLatin1String("exec")) {
        return handleExec(rootPath, arguments);
    }
    if (action == QLatin1String("grub-mkstandalone-efi")) {
        return handleGrubMkstandaloneEfi(rootPath, arguments);
    }
    if (action == QLatin1String("list-dir")) {
        return handleListDir(rootPath, arguments);
    }
    if (action == QLatin1String("mount-chroot-env")) {
        if (!rootPath.isEmpty()) {
            printError(QStringLiteral("mount-chroot-env does not accept --root"));
            return 1;
        }
        return handleMountChrootEnv(arguments);
    }
    if (action == QLatin1String("path-check")) {
        return handlePathCheck(rootPath, arguments);
    }
    if (action == QLatin1String("read-file")) {
        return handleReadFile(rootPath, arguments);
    }
    if (action == QLatin1String("remove-efi-dump")) {
        if (!rootPath.isEmpty()) {
            printError(QStringLiteral("remove-efi-dump does not accept --root"));
            return 1;
        }
        return handleRemoveEfiDump();
    }

    printError(QStringLiteral("Unsupported helper action: %1").arg(action));
    return 1;
}
