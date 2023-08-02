#include "cmd.h"

#include <QDebug>
#include <QEventLoop>

Cmd::Cmd(QObject *parent)
    : QProcess(parent)
{
    connect(this, &Cmd::readyReadStandardOutput, [this] { emit outputAvailable(readAllStandardOutput()); });
    connect(this, &Cmd::readyReadStandardError, [this] { emit errorAvailable(readAllStandardError()); });
    connect(this, &Cmd::outputAvailable, [this](const QString &out) { out_buffer += out; });
    connect(this, &Cmd::errorAvailable, [this](const QString &out) { out_buffer += out; });
}

QString Cmd::getCmdOut(const QString &cmd, bool quiet)
{
    QString output;
    run(cmd, &output, nullptr, quiet);
    return output;
}

bool Cmd::proc(const QString &cmd, const QStringList &args, QString *output, const QByteArray *input, bool quiet)
{
    out_buffer.clear();
    connect(this, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, &Cmd::finished);
    if (this->state() != QProcess::NotRunning) {
        qDebug() << "Process already running:" << this->program() << this->arguments();
        return false;
    }
    if (!quiet) qDebug() << cmd << args;
    QEventLoop loop;
    connect(this, &Cmd::finished, &loop, &QEventLoop::quit);
    start(cmd, args);
    if (input) write(*input);
    closeWriteChannel();
    loop.exec();
    if (output) *output = out_buffer.trimmed();
    return (exitStatus() == QProcess::NormalExit && exitCode() == 0);
}
bool Cmd::run(const QString &cmd, QString *output, const QByteArray *input, bool quiet)
{
    if (!quiet) qDebug().noquote() << cmd;
    return proc("/bin/bash", {"-c", cmd}, output, input, true);
}
