#pragma once

#include <QProcess>

enum class QuietMode { No, Yes };
enum class PathCheck { Exists, Directory, Executable };

class QTextStream;

class Cmd : public QProcess {
    Q_OBJECT
public:
    explicit Cmd(QObject *parent = nullptr);
    bool proc(const QString &cmd, const QStringList &args = {}, QString *output = nullptr,
              const QByteArray *input = nullptr, QuietMode quiet = QuietMode::No);
    bool procAsRoot(const QString &cmd, const QStringList &args = {}, QString *output = nullptr,
                    const QByteArray *input = nullptr, QuietMode quiet = QuietMode::No);
    bool procAsRootInTarget(const QString &rootPath, const QString &cmd, const QStringList &args = {},
                            QString *output = nullptr, const QByteArray *input = nullptr,
                            QuietMode quiet = QuietMode::No);
    bool run(const QString &cmd, QString *output = nullptr, const QByteArray *input = nullptr,
             QuietMode quiet = QuietMode::No);
    [[nodiscard]] QString getCmdOut(const QString &cmd, QuietMode quiet = QuietMode::No);
    [[nodiscard]] QString getOutAsRoot(const QString &cmd, const QStringList &args = {},
                                       QuietMode quiet = QuietMode::No);
    [[nodiscard]] QString getOutAsRootInTarget(const QString &rootPath, const QString &cmd,
                                               const QStringList &args = {},
                                               QuietMode quiet = QuietMode::No);
    [[nodiscard]] QString readFileAsRoot(const QString &path, QuietMode quiet = QuietMode::No,
                                         const QString &rootPath = {});
    [[nodiscard]] QStringList listDirAsRoot(const QString &path, QuietMode quiet = QuietMode::No,
                                            const QString &rootPath = {});
    [[nodiscard]] bool pathCheckAsRoot(const QString &path, PathCheck check = PathCheck::Exists,
                                       QuietMode quiet = QuietMode::No, const QString &rootPath = {});
    [[nodiscard]] bool dirHasEntriesAsRoot(const QString &path, QuietMode quiet = QuietMode::No,
                                           const QString &rootPath = {});
    bool copyLogAsRoot(QuietMode quiet = QuietMode::No);
    bool mountChrootEnvAsRoot(const QString &source, const QString &target, QuietMode quiet = QuietMode::No);
    bool cleanupChrootEnvAsRoot(const QString &target, QuietMode quiet = QuietMode::No);
    bool ensureEfivarfsAsRoot(QuietMode quiet = QuietMode::No);
    bool removeEfiDumpVarsAsRoot(QuietMode quiet = QuietMode::No);
    bool grubMkstandaloneEfiAsRoot(const QString &arch, const QString &bootloaderId, bool useHostBinary,
                                   QuietMode quiet = QuietMode::No, const QString &rootPath = {});
    void setOutputSuppressed(bool suppressed);
    [[nodiscard]] bool outputSuppressed() const;

signals:
    void done();
    void errorAvailable(const QString &err);
    void outputAvailable(const QString &out);

private:
    QString out_buffer;
    QString asRoot;
    QString helper;
    bool suppressOutput = false;

    bool helperProc(const QStringList &helperArgs, QString *output = nullptr, const QByteArray *input = nullptr,
                    QuietMode quiet = QuietMode::No);
    [[nodiscard]] QStringList helperExecArgs(const QString &cmd, const QStringList &args,
                                             const QString &rootPath = {}) const;
    [[nodiscard]] static QStringList helperRootArgs(const QString &rootPath);
};
