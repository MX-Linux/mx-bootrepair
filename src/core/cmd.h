#pragma once

#include <QProcess>

enum class QuietMode { No, Yes };
enum class Elevation { No, Yes };

class QTextStream;

class Cmd : public QProcess {
    Q_OBJECT
public:
    explicit Cmd(QObject *parent = nullptr);
    bool proc(const QString &cmd, const QStringList &args = {}, QString *output = nullptr,
              const QByteArray *input = nullptr, QuietMode quiet = QuietMode::No,
              Elevation elevate = Elevation::No);
    bool procAsRoot(const QString &cmd, const QStringList &args = {}, QString *output = nullptr,
                    const QByteArray *input = nullptr, QuietMode quiet = QuietMode::No);
    bool run(const QString &cmd, QString *output = nullptr, const QByteArray *input = nullptr,
             QuietMode quiet = QuietMode::No, Elevation elevate = Elevation::No);
    bool runAsRoot(const QString &cmd, QString *output = nullptr, const QByteArray *input = nullptr,
                   QuietMode quiet = QuietMode::No);
    [[nodiscard]] QString getCmdOut(const QString &cmd, QuietMode quiet = QuietMode::No,
                                    Elevation elevate = Elevation::No);

signals:
    void done();
    void errorAvailable(const QString &err);
    void outputAvailable(const QString &out);

private:
    QString out_buffer;
    QString asRoot;
    QString helper;
};
