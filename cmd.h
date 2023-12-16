#pragma once

#include <QProcess>

class QTextStream;

class Cmd : public QProcess
{
    Q_OBJECT
public:
    explicit Cmd(QObject *parent = nullptr);
    bool proc(const QString &cmd, const QStringList &args = {}, QString *output = nullptr,
              const QByteArray *input = nullptr, bool quiet = false, bool elevate = false);
    bool procAsRoot(const QString &cmd, const QStringList &args = {}, QString *output = nullptr,
                    const QByteArray *input = nullptr, bool quiet = false);
    bool run(const QString &cmd, QString *output = nullptr, const QByteArray *input = nullptr, bool quiet = false,
             bool elevate = false);
    bool runAsRoot(const QString &cmd, QString *output = nullptr, const QByteArray *input = nullptr,
                   bool quiet = false);
    [[nodiscard]] QString getCmdOut(const QString &cmd, bool quiet = false, bool elevate = false);

signals:
    void done();
    void errorAvailable(const QString &err);
    void outputAvailable(const QString &out);

private:
    QString out_buffer;
    QString asRoot;
    QString helper;
};
