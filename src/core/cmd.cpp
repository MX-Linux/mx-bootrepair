#include "cmd.h"

#include <QCoreApplication>
#include <QDebug>
#include <QEventLoop>
#include <QFile>
#include <QVariant>

#include <unistd.h>

Cmd::Cmd(QObject *parent)
    : QProcess(parent),
      asRoot(
          // Prefer sudo in CLI mode; otherwise use pkexec/gksu
          (QCoreApplication::instance() && QCoreApplication::instance()->property("cliMode").toBool()
           && QFile::exists("/usr/bin/sudo"))
              ? QStringLiteral("/usr/bin/sudo")
              : (QFile::exists("/usr/bin/pkexec") ? QStringLiteral("/usr/bin/pkexec")
                                                  : QStringLiteral("/usr/bin/gksu"))),
      helper {"/usr/lib/" + QCoreApplication::applicationName() + "/helper"}
{
    connect(this, &Cmd::readyReadStandardOutput, [this] { emit outputAvailable(readAllStandardOutput()); });
    connect(this, &Cmd::readyReadStandardError, [this] { emit errorAvailable(readAllStandardError()); });
    connect(this, &Cmd::outputAvailable, [this](const QString &out) { out_buffer += out; });
    connect(this, &Cmd::errorAvailable, [this](const QString &out) { out_buffer += out; });
}

QString Cmd::getCmdOut(const QString &cmd, bool quiet, bool elevate)
{
    QString output;
    run(cmd, &output, nullptr, quiet, elevate);
    return output;
}

bool Cmd::proc(const QString &cmd, const QStringList &args, QString *output, const QByteArray *input, bool quiet,
               bool elevate)
{
    out_buffer.clear();
    connect(this, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, &Cmd::done);
    if (this->state() != QProcess::NotRunning) {
        qDebug() << "Process already running:" << this->program() << this->arguments();
        return false;
    }
    if (!quiet) {
        qDebug() << cmd << args;
    }
    QEventLoop loop;
    connect(this, &Cmd::done, &loop, &QEventLoop::quit);
    if (elevate && getuid() != 0) {
        QStringList cmdAndArgs = QStringList() << helper << cmd << args;
        start(asRoot, {cmdAndArgs});
    } else {
        start(cmd, args);
    }
    if (input) {
        write(*input);
    }
    closeWriteChannel();
    loop.exec();
    if (output) {
        *output = out_buffer.trimmed();
    }
    return (exitStatus() == QProcess::NormalExit && exitCode() == 0);
}

bool Cmd::procAsRoot(const QString &cmd, const QStringList &args, QString *output, const QByteArray *input, bool quiet)
{
    return proc(cmd, args, output, input, quiet, true);
}

bool Cmd::run(const QString &cmd, QString *output, const QByteArray *input, bool quiet, bool elevate)
{
    if (!quiet) {
        qDebug().noquote() << cmd;
    }
    if (elevate && getuid() != 0) {
        return proc(asRoot, {helper, cmd}, output, input, true);
    } else {
        return proc("/bin/bash", {"-c", cmd}, output, input, true);
    }
}

bool Cmd::runAsRoot(const QString &cmd, QString *output, const QByteArray *input, bool quiet)
{
    return run(cmd, output, input, quiet, true);
}
