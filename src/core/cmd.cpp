#include "cmd.h"

#include <QCoreApplication>
#include <QDebug>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QStringList>
#include <QVariant>

#include <unistd.h>

namespace
{
bool indicatesElevationFailure(const QString &wrapperProgram, const QString &output, int exitCode)
{
    const QString program = QFileInfo(wrapperProgram).fileName().toLower();
    const QString text = output.toLower();

    if (program == QLatin1String("pkexec")) {
        return exitCode == 126 || text.contains(QStringLiteral("not authorized"))
            || text.contains(QStringLiteral("authentication dialog was dismissed"))
            || text.contains(QStringLiteral("no authentication agent found"))
            || text.contains(QStringLiteral("error executing command as another user"));
    }

    if (program == QLatin1String("sudo")) {
        return text.contains(QStringLiteral("no password was provided"))
            || text.contains(QStringLiteral("a password is required"))
            || text.contains(QStringLiteral("incorrect password attempts"))
            || text.contains(QStringLiteral("sorry, try again"));
    }

    if (program == QLatin1String("gksu")) {
        return text.contains(QStringLiteral("not authorized")) || text.contains(QStringLiteral("denied"))
            || text.contains(QStringLiteral("cancelled")) || text.contains(QStringLiteral("canceled"));
    }

    return false;
}
} // namespace

Cmd::Cmd(QObject *parent)
    : QProcess(parent),
      asRoot(
          (QCoreApplication::instance() && QCoreApplication::instance()->property("cliMode").toBool()
           && QFile::exists("/usr/bin/sudo"))
              ? QStringLiteral("/usr/bin/sudo")
              : (QFile::exists("/usr/bin/pkexec") ? QStringLiteral("/usr/bin/pkexec")
                                                  : QStringLiteral("/usr/bin/gksu"))),
      helper(QStringLiteral("/usr/lib/%1/helper").arg(QCoreApplication::applicationName()))
{
    connect(this, &Cmd::readyReadStandardOutput, [this] {
        const QString out = readAllStandardOutput();
        out_buffer += out;
        if (!suppressOutput) {
            emit outputAvailable(out);
        }
    });
    connect(this, &Cmd::readyReadStandardError, [this] {
        const QString out = readAllStandardError();
        out_buffer += out;
        if (!suppressOutput) {
            emit errorAvailable(out);
        }
    });
}

QStringList Cmd::helperRootArgs(const QString &rootPath)
{
    if (rootPath.isEmpty()) {
        return {};
    }
    return {QStringLiteral("--root"), rootPath};
}

QStringList Cmd::helperExecArgs(const QString &cmd, const QStringList &args, const QString &rootPath) const
{
    QStringList helperArgs {QStringLiteral("exec")};
    helperArgs += helperRootArgs(rootPath);
    helperArgs << cmd;
    helperArgs += args;
    return helperArgs;
}

bool Cmd::helperProc(const QStringList &helperArgs, QString *output, const QByteArray *input, QuietMode quiet)
{
    lastElevationFailed_ = false;

    if (getuid() != 0 && asRoot.isEmpty()) {
        qWarning() << "No elevation helper available";
        lastElevationFailed_ = true;
        return false;
    }

    const QString program = (getuid() == 0) ? helper : asRoot;
    QStringList programArgs = helperArgs;
    if (getuid() != 0) {
        programArgs.prepend(helper);
    }

    const bool ok = proc(program, programArgs, output, input, quiet);
    if (getuid() != 0 && !ok && exitStatus() == QProcess::NormalExit
        && indicatesElevationFailure(program, out_buffer, exitCode())) {
        lastElevationFailed_ = true;
    }

    return ok;
}

QString Cmd::getCmdOut(const QString &cmd, QuietMode quiet)
{
    QString output;
    run(cmd, &output, nullptr, quiet);
    return output;
}

QString Cmd::getOutAsRoot(const QString &cmd, const QStringList &args, QuietMode quiet)
{
    QString output;
    procAsRoot(cmd, args, &output, nullptr, quiet);
    return output;
}

QString Cmd::getOutAsRootInTarget(const QString &rootPath, const QString &cmd, const QStringList &args,
                                  QuietMode quiet)
{
    QString output;
    procAsRootInTarget(rootPath, cmd, args, &output, nullptr, quiet);
    return output;
}

QString Cmd::readFileAsRoot(const QString &path, QuietMode quiet, const QString &rootPath)
{
    QString output;
    QStringList helperArgs {QStringLiteral("read-file")};
    helperArgs += helperRootArgs(rootPath);
    helperArgs << path;
    helperProc(helperArgs, &output, nullptr, quiet);
    return output;
}

QStringList Cmd::listDirAsRoot(const QString &path, QuietMode quiet, const QString &rootPath)
{
    QString output;
    QStringList helperArgs {QStringLiteral("list-dir")};
    helperArgs += helperRootArgs(rootPath);
    helperArgs << path;
    if (!helperProc(helperArgs, &output, nullptr, quiet)) {
        return {};
    }
    return output.split('\n', Qt::SkipEmptyParts);
}

bool Cmd::pathCheckAsRoot(const QString &path, PathCheck check, QuietMode quiet, const QString &rootPath)
{
    QStringList helperArgs {QStringLiteral("path-check")};
    helperArgs += helperRootArgs(rootPath);

    QString mode = QStringLiteral("exists");
    if (check == PathCheck::Directory) {
        mode = QStringLiteral("dir");
    } else if (check == PathCheck::Executable) {
        mode = QStringLiteral("exec");
    }

    helperArgs << mode << path;
    return helperProc(helperArgs, nullptr, nullptr, quiet);
}

bool Cmd::dirHasEntriesAsRoot(const QString &path, QuietMode quiet, const QString &rootPath)
{
    QStringList helperArgs {QStringLiteral("dir-has-entries")};
    helperArgs += helperRootArgs(rootPath);
    helperArgs << path;
    return helperProc(helperArgs, nullptr, nullptr, quiet);
}

bool Cmd::copyLogAsRoot(QuietMode quiet)
{
    return helperProc({QStringLiteral("copy-log")}, nullptr, nullptr, quiet);
}

bool Cmd::mountChrootEnvAsRoot(const QString &source, const QString &target, QuietMode quiet)
{
    return helperProc({QStringLiteral("mount-chroot-env"), source, target}, nullptr, nullptr, quiet);
}

bool Cmd::cleanupChrootEnvAsRoot(const QString &target, QuietMode quiet)
{
    return helperProc({QStringLiteral("cleanup-chroot-env"), target}, nullptr, nullptr, quiet);
}

bool Cmd::ensureEfivarfsAsRoot(QuietMode quiet)
{
    return helperProc({QStringLiteral("ensure-efivarfs")}, nullptr, nullptr, quiet);
}

bool Cmd::removeEfiDumpVarsAsRoot(QuietMode quiet)
{
    return helperProc({QStringLiteral("remove-efi-dump")}, nullptr, nullptr, quiet);
}

bool Cmd::copyGrubLocalesAsRoot(QuietMode quiet, const QString &rootPath)
{
    QStringList helperArgs {QStringLiteral("copy-grub-locales")};
    helperArgs += helperRootArgs(rootPath);
    return helperProc(helperArgs, nullptr, nullptr, quiet);
}

bool Cmd::grubMkstandaloneEfiAsRoot(const QString &arch, const QString &bootloaderId, bool useHostBinary,
                                    QuietMode quiet, const QString &rootPath)
{
    QStringList helperArgs {QStringLiteral("grub-mkstandalone-efi")};
    helperArgs += helperRootArgs(rootPath);
    helperArgs << arch << bootloaderId << (useHostBinary ? QStringLiteral("1") : QStringLiteral("0"));
    return helperProc(helperArgs, nullptr, nullptr, quiet);
}

bool Cmd::proc(const QString &cmd, const QStringList &args, QString *output, const QByteArray *input,
               QuietMode quiet)
{
    out_buffer.clear();
    connect(this, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, &Cmd::done);
    if (state() != QProcess::NotRunning) {
        qDebug() << "Process already running:" << program() << arguments();
        return false;
    }

    if (quiet == QuietMode::No) {
        qDebug() << cmd << args;
    }

    QEventLoop loop;
    connect(this, &Cmd::done, &loop, &QEventLoop::quit);
    start(cmd, args);
    if (input && !input->isEmpty()) {
        write(*input);
    }
    closeWriteChannel();
    loop.exec();

    if (output) {
        *output = out_buffer.trimmed();
    }
    return (exitStatus() == QProcess::NormalExit && exitCode() == 0);
}

bool Cmd::procAsRoot(const QString &cmd, const QStringList &args, QString *output, const QByteArray *input,
                     QuietMode quiet)
{
    return helperProc(helperExecArgs(cmd, args), output, input, quiet);
}

bool Cmd::procAsRootInTarget(const QString &rootPath, const QString &cmd, const QStringList &args, QString *output,
                             const QByteArray *input, QuietMode quiet)
{
    return helperProc(helperExecArgs(cmd, args, rootPath), output, input, quiet);
}

bool Cmd::run(const QString &cmd, QString *output, const QByteArray *input, QuietMode quiet)
{
    if (quiet == QuietMode::No) {
        qDebug().noquote() << cmd;
    }

    return proc(QStringLiteral("/bin/bash"), {QStringLiteral("-c"), cmd}, output, input, QuietMode::Yes);
}

void Cmd::setOutputSuppressed(bool suppressed)
{
    suppressOutput = suppressed;
}

bool Cmd::outputSuppressed() const
{
    return suppressOutput;
}

bool Cmd::lastElevationFailed() const
{
    return lastElevationFailed_;
}
