/**********************************************************************
 *  helper.cpp
 **********************************************************************
 * Copyright (C) 2026-2026 MX Authors
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

#include <QByteArray>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QProcess>
#include <QRegularExpression>
#include <QScopeGuard>
#include <QTemporaryDir>
#include <QSaveFile>
#include <QStringList>

#include <cstdio>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <cerrno>
#include <unistd.h>

#include "helper/path_validation.h"
#include "helper/grub_config.h"

namespace
{
using HelperPath::TargetRoot;
using HelperPath::isValidAbsolutePath;

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

[[nodiscard]] QString resolveHostBinary(const QStringList &candidates)
{
    for (const QString &path : candidates) {
        if (QFileInfo(path).isExecutable()) return path;
    }
    return {};
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

[[nodiscard]] int runAllowedCommand(const QString &command, const QStringList &commandArgs,
                                    const QByteArray &input = {})
{
    const auto it = allowedCommands().constFind(command);
    if (it == allowedCommands().constEnd()) {
        printError(QStringLiteral("Command is not allowed: %1").arg(command));
        return 127;
    }

    const QString resolvedCommand = resolveHostBinary(it.value());
    if (resolvedCommand.isEmpty()) {
        printError(QStringLiteral("Command is not available: %1").arg(command));
        return 127;
    }

    return relayResult(runProcess(resolvedCommand, commandArgs, input));
}

[[nodiscard]] int handleExec(const QStringList &args)
{
    if (args.isEmpty()) {
        printError(QStringLiteral("exec requires a command name"));
        return 1;
    }
    return runAllowedCommand(args.constFirst(), args.mid(1), readHelperInput());
}

[[nodiscard]] int handleReadFile(const QStringList &args)
{
    if (args.size() != 1) {
        printError(QStringLiteral("read-file requires exactly one path"));
        return 1;
    }

    const QString path = args.constFirst();
    if (!isValidAbsolutePath(path)) {
        printError(QStringLiteral("Invalid read-file path: %1").arg(path));
        return 1;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        printError(QStringLiteral("Unable to read %1").arg(path));
        return 1;
    }

    writeAndFlush(stdout, file.readAll());
    return 0;
}

[[nodiscard]] int handleListDir(const QStringList &args)
{
    if (args.size() != 1) {
        printError(QStringLiteral("list-dir requires exactly one path"));
        return 1;
    }

    const QString path = args.constFirst();
    if (!isValidAbsolutePath(path)) {
        printError(QStringLiteral("Invalid list-dir path: %1").arg(path));
        return 1;
    }

    const QDir dir(path);
    if (!dir.exists()) {
        printError(QStringLiteral("Directory does not exist: %1").arg(path));
        return 1;
    }

    const QStringList entries = dir.entryList(QDir::AllEntries | QDir::NoDotAndDotDot, QDir::Name);
    writeAndFlush(stdout, entries.join('\n').toUtf8());
    return 0;
}

[[nodiscard]] int handlePathCheck(const QStringList &args)
{
    if (args.size() != 2) {
        printError(QStringLiteral("path-check requires mode and path"));
        return 1;
    }

    const QString mode = args.at(0);
    const QString path = args.at(1);
    if (!isValidAbsolutePath(path)) {
        printError(QStringLiteral("Invalid path-check path: %1").arg(path));
        return 1;
    }

    const QFileInfo info(path);
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

[[nodiscard]] int handleDirHasEntries(const QStringList &args)
{
    if (args.size() != 1) {
        printError(QStringLiteral("dir-has-entries requires exactly one path"));
        return 1;
    }

    const QString path = args.constFirst();
    if (!isValidAbsolutePath(path)) {
        printError(QStringLiteral("Invalid dir-has-entries path: %1").arg(path));
        return 1;
    }

    const QDir dir(path);
    if (!dir.exists()) {
        return 1;
    }

    return dir.entryList(QDir::AllEntries | QDir::NoDotAndDotDot).isEmpty() ? 1 : 0;
}

[[nodiscard]] bool isMountpoint(const QString &path)
{
    return runAllowedCommand(QStringLiteral("mountpoint"), {QStringLiteral("-q"), path}) == 0;
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

    if (!QDir(target).exists() || isMountpoint(target)) {
        printError(QStringLiteral("Mount directory is unavailable or already mounted: %1").arg(target));
        return 1;
    }

    int exitCode = runAllowedCommand(QStringLiteral("mount"), {source, target});
    if (exitCode != 0) {
        return exitCode;
    }

    auto rollback = qScopeGuard([&] {
        if (runAllowedCommand(QStringLiteral("umount"), {QStringLiteral("-R"), target}) != 0) {
            printError(QStringLiteral("Failed to roll back mounts at %1").arg(target));
        }
    });

    const TargetRoot root(target);
    const auto bind = [&](const QString &path, bool optional, bool create = false) {
        const auto destination = root.directory(path, create);
        if (!destination) return optional && errno == ENOENT;
        return ::mount(QFile::encodeName(path).constData(), QFile::encodeName(destination.procPath()).constData(),
                       nullptr, MS_BIND | MS_REC, nullptr) == 0;
    };
    if (!bind("/dev", false) || !bind("/sys", true) || !bind("/proc", true)) {
        printError(QStringLiteral("Failed to bind system directories in the repair root"));
        return 1;
    }
    const auto run = root.directory("/run");
    if (run) {
        if (::mount("tmpfs", QFile::encodeName(run.procPath()).constData(), "tmpfs", MS_NODEV, "size=100m,mode=755") != 0
            || (QDir("/run/udev").exists() && !bind("/run/udev", false, true))) {
            printError(QStringLiteral("Failed to prepare /run in the repair root"));
            return 1;
        }
    } else if (errno != ENOENT) {
        printError(QStringLiteral("Could not open /run in the repair root"));
        return 1;
    }

    rollback.dismiss();
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

    return runAllowedCommand(QStringLiteral("umount"), {QStringLiteral("-R"), target});
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

    return runAllowedCommand(QStringLiteral("mount"),
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
    return arch;
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

    const bool hostBuild = !rootPath.isEmpty() && useHostBinary;
    const TargetRoot root(rootPath);
    if (!rootPath.isEmpty() && !hostBuild && !root.enter()) return 1;
    if (!QDir().mkpath("/run")) return 1;
    QTemporaryDir work("/run/mxbr-grub-XXXXXX");
    if (!work.isValid()) return 1;

    QStringList processArgs;
    if (hostBuild) {
        // The host binary needs the target's modules. Snapshot the flat module files so
        // symlinks inside that directory are also resolved against the installation.
        const QString modulePath = "/usr/lib/grub/" + efiTarget;
        const auto moduleDir = root.directory(modulePath);
        const QString snapshot = work.filePath("modules");
        if (!moduleDir || !QDir().mkdir(snapshot)) return 1;
        const auto entries = QDir(moduleDir.procPath()).entryList(QDir::AllEntries | QDir::System | QDir::NoDotAndDotDot);
        for (const QString &name : entries) {
            const auto file = root.open(modulePath + '/' + name, O_RDONLY | O_NONBLOCK);
            struct stat info {};
            if (!file || fstat(file.get(), &info) != 0) return 1;
            if (S_ISDIR(info.st_mode)) continue;
            if (!S_ISREG(info.st_mode) || !QFile::copy(file.procPath(), snapshot + '/' + name)) return 1;
        }
        processArgs << "--directory=" + snapshot;
    }
    const auto grubDirectory = hostBuild ? root.directory("/boot/grub", true) : HelperPath::Descriptor();
    if ((hostBuild && !grubDirectory) || (!hostBuild && !QDir().mkpath("/boot/grub"))) {
        printError("Could not open the GRUB configuration directory");
        return 1;
    }
    const QString grubPath = hostBuild ? grubDirectory.procPath() : QStringLiteral("/boot/grub");
    const QString probe = resolveHostBinary({"/usr/sbin/grub-probe", "/usr/bin/grub-probe", "/sbin/grub-probe"});
    const QString relpath = resolveHostBinary({"/usr/bin/grub-mkrelpath", "/bin/grub-mkrelpath"});
    if (probe.isEmpty() || relpath.isEmpty()) {
        printError("grub-probe and grub-mkrelpath are required to identify the boot filesystem");
        return 1;
    }
    const ProcessResult uuid = runProcess(probe, {"--target=fs_uuid", grubPath});
    const ProcessResult path = runProcess(relpath, {grubPath});
    if (!uuid.started || uuid.exitStatus != QProcess::NormalExit || uuid.exitCode != 0
        || !path.started || path.exitStatus != QProcess::NormalExit || path.exitCode != 0) {
        writeAndFlush(stderr, uuid.standardError + path.standardError);
        printError("Could not identify the filesystem and GRUB path for /boot/grub");
        return 1;
    }
    QByteArray prefix = path.standardOutput;
    if (prefix.endsWith('\n')) prefix.chop(1);
    const QByteArray config = standaloneGrubConfig(uuid.standardOutput.trimmed(), prefix);
    if (config.isEmpty()) {
        printError("Invalid GRUB filesystem UUID or path");
        return 1;
    }
    QFile earlyCfg(work.filePath("early.cfg"));
    if (!earlyCfg.open(QIODevice::WriteOnly) || earlyCfg.write(config) != config.size() || !earlyCfg.flush()) return 1;
    earlyCfg.close();

    QFile image(work.filePath("grub.efi"));
    processArgs << "-O" << efiTarget << "-o" << image.fileName() << "boot/grub/grub.cfg=" + earlyCfg.fileName();
    const int result = runAllowedCommand("grub-mkstandalone", processArgs);
    if (result != 0) return result;
    if (!image.open(QIODevice::ReadOnly)) return 1;
    // Keep the finished image open across chroot. Remove the host staging files first.
    if (!work.remove()) return 1;
    work.setAutoRemove(false);
    if (hostBuild && !root.enter()) return 1;
    const QString efiDir = "/boot/efi/EFI/" + bootloaderId;
    const QString fallbackDir = "/boot/efi/EFI/BOOT";
    if (!QDir().mkpath(efiDir) || !QDir().mkpath(fallbackDir)) return 1;
    const QString outPath = efiDir + "/grub" + suffix + ".efi";
    QSaveFile loader(outPath);
    if (!loader.open(QIODevice::WriteOnly)) return 1;
    while (!image.atEnd()) {
        const QByteArray chunk = image.read(64 * 1024);
        if (chunk.isEmpty() || loader.write(chunk) != chunk.size()) return 1;
    }
    if (!loader.commit()) return 1;
    const QString fallbackPath = fallbackDir + "/BOOT" + upperSuffix + ".EFI";
    QFile::remove(fallbackPath);
    if (!QFile::copy(outPath, fallbackPath)) {
        printError(QStringLiteral("Unable to copy %1 to %2").arg(outPath, fallbackPath));
        return 1;
    }
    return 0;
}

[[nodiscard]] bool pathExistsNoFollow(const QString &path)
{
    struct stat st {};
    const QByteArray encodedPath = QFile::encodeName(path);
    return ::lstat(encodedPath.constData(), &st) == 0;
}

[[nodiscard]] bool isRegularFileNoFollow(const QString &path)
{
    struct stat st {};
    const QByteArray encodedPath = QFile::encodeName(path);
    return ::lstat(encodedPath.constData(), &st) == 0 && S_ISREG(st.st_mode);
}

[[nodiscard]] int copyRegularFileNoFollow(const QString &source, const QString &target)
{
    const QByteArray encodedSource = QFile::encodeName(source);
    const int sourceFd = ::open(encodedSource.constData(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    if (sourceFd < 0) {
        printError(QStringLiteral("Unable to open log source: %1").arg(source));
        return 1;
    }

    struct stat sourceStat {};
    if (::fstat(sourceFd, &sourceStat) != 0 || !S_ISREG(sourceStat.st_mode)) {
        ::close(sourceFd);
        printError(QStringLiteral("Log source is not a regular file: %1").arg(source));
        return 1;
    }

    QFile sourceFile;
    if (!sourceFile.open(sourceFd, QFile::ReadOnly, QFileDevice::AutoCloseHandle)) {
        ::close(sourceFd);
        printError(QStringLiteral("Unable to read log source: %1").arg(source));
        return 1;
    }

    const QByteArray encodedTarget = QFile::encodeName(target);
    const int targetFd = ::open(encodedTarget.constData(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
    if (targetFd < 0) {
        printError(QStringLiteral("Unable to create log target: %1").arg(target));
        return 1;
    }

    QFile targetFile;
    if (!targetFile.open(targetFd, QFile::WriteOnly, QFileDevice::AutoCloseHandle)) {
        ::close(targetFd);
        QFile::remove(target);
        printError(QStringLiteral("Unable to write log target: %1").arg(target));
        return 1;
    }

    QByteArray buffer(64 * 1024, Qt::Uninitialized);
    while (true) {
        const qint64 bytesRead = sourceFile.read(buffer.data(), buffer.size());
        if (bytesRead < 0) {
            targetFile.close();
            QFile::remove(target);
            printError(QStringLiteral("Unable to read log source: %1").arg(source));
            return 1;
        }
        if (bytesRead == 0) {
            break;
        }

        qint64 bytesWritten = 0;
        while (bytesWritten < bytesRead) {
            const qint64 chunkWritten = targetFile.write(buffer.constData() + bytesWritten, bytesRead - bytesWritten);
            if (chunkWritten <= 0) {
                targetFile.close();
                QFile::remove(target);
                printError(QStringLiteral("Unable to write log target: %1").arg(target));
                return 1;
            }
            bytesWritten += chunkWritten;
        }
    }

    if (!targetFile.flush()) {
        targetFile.close();
        QFile::remove(target);
        printError(QStringLiteral("Unable to flush log target: %1").arg(target));
        return 1;
    }
    return 0;
}

[[nodiscard]] int handleCopyLog()
{
    const QString name = QStringLiteral("mx-boot-repair.log");
    const QString target = QStringLiteral("/var/log/") + name;
    const QString backup = QStringLiteral("/var/log/") + name + QStringLiteral(".old");

    // Where the app wrote its log (see app_init sessionLogPath): a GUI run logs
    // to the caller's private runtime dir (/run/user/<uid>); a root CLI run logs
    // to /run. Both are non-world-writable, unlike the legacy /tmp fallback. The
    // caller's uid may come from pkexec or sudo.
    QStringList searchDirs;
    bool uidOk = false;
    uint uid = qEnvironmentVariable("PKEXEC_UID").toUInt(&uidOk);
    if (!uidOk || uid == 0) {
        uid = qEnvironmentVariable("SUDO_UID").toUInt(&uidOk);
    }
    if (uidOk && uid != 0) {
        searchDirs << QStringLiteral("/run/user/%1").arg(uid);
    }
    searchDirs << QStringLiteral("/run") << QStringLiteral("/tmp");

    QString source;
    for (const QString &dir : searchDirs) {
        const QString candidate = dir + QLatin1Char('/') + name;
        // Only copy a real regular file, never a symlink (lstat + S_ISREG), so
        // the source cannot be redirected at a root-owned file.
        if (isRegularFileNoFollow(candidate)) {
            source = candidate;
            break;
        }
    }
    if (source.isEmpty()) {
        return 0; // no log to copy
    }

    // copy-log is a dedicated helper action, not generic command dispatch, so
    // it resolves and runs the required host binary directly here.
    const QString mvBinary =
        resolveHostBinary({QStringLiteral("/usr/bin/mv"), QStringLiteral("/bin/mv")});
    if (mvBinary.isEmpty()) {
        printError(QStringLiteral("Unable to find mv"));
        return 127;
    }

    if (pathExistsNoFollow(target)) {
        const int exitCode =
            relayResult(runProcess(mvBinary, {QStringLiteral("--backup=numbered"), target, backup}));
        if (exitCode != 0) {
            return exitCode;
        }
    }

    return copyRegularFileNoFollow(source, target);
}

[[nodiscard]] int handleCopyGrubLocales()
{
    const QString localeRoot = QStringLiteral("/usr/share/locale");
    const QString grubLocaleDir = QStringLiteral("/boot/grub/locale");
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
    if (geteuid() != 0) return 1;
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

    if (!rootPath.isEmpty() && action != "grub-mkstandalone-efi") {
        const QStringList rootedActions {"exec", "read-file", "list-dir", "path-check", "dir-has-entries",
                                         "copy-grub-locales"};
        if (!rootedActions.contains(action) || !TargetRoot(rootPath).enter()) {
            printError(QStringLiteral("Could not enter the repair root for %1").arg(action));
            return 1;
        }
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
        return handleCopyGrubLocales();
    }
    if (action == QLatin1String("dir-has-entries")) {
        return handleDirHasEntries(arguments);
    }
    if (action == QLatin1String("ensure-efivarfs")) {
        if (!rootPath.isEmpty()) {
            printError(QStringLiteral("ensure-efivarfs does not accept --root"));
            return 1;
        }
        return handleEnsureEfivarfs();
    }
    if (action == QLatin1String("exec")) {
        return handleExec(arguments);
    }
    if (action == QLatin1String("grub-mkstandalone-efi")) {
        return handleGrubMkstandaloneEfi(rootPath, arguments);
    }
    if (action == QLatin1String("list-dir")) {
        return handleListDir(arguments);
    }
    if (action == QLatin1String("mount-chroot-env")) {
        if (!rootPath.isEmpty()) {
            printError(QStringLiteral("mount-chroot-env does not accept --root"));
            return 1;
        }
        return handleMountChrootEnv(arguments);
    }
    if (action == QLatin1String("path-check")) {
        return handlePathCheck(arguments);
    }
    if (action == QLatin1String("read-file")) {
        return handleReadFile(arguments);
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
