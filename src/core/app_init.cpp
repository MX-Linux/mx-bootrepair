// Shared application initialization helpers (QtCore-only)
#include "core/app_init.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QLibraryInfo>
#include <QLocale>
#include <QTextStream>
#include <QTranslator>

#include <fcntl.h>
#include <unistd.h>

namespace {
static QFile logFile; // persists for process lifetime
static QString logFilePath; // path of logFile, kept independent of how it was opened
constexpr qint64 MIN_PERSISTED_LOG_SIZE = 256;

// Per-session log path kept out of world-writable /tmp:
//   running as root -> /run (root-only)
//   running as the user -> private per-user runtime dir ($XDG_RUNTIME_DIR)
//   fallback -> /tmp (opened with O_NOFOLLOW below)
QString sessionLogPath()
{
    const QString appName = QCoreApplication::applicationName();
    const QString name = (appName.isEmpty() ? QStringLiteral("mx-boot-repair") : appName) + QStringLiteral(".log");
    if (geteuid() == 0) {
        return QStringLiteral("/run/") + name;
    }
    const QString runtimeDir = qEnvironmentVariable("XDG_RUNTIME_DIR");
    if (!runtimeDir.isEmpty() && QFileInfo(runtimeDir).isDir()) {
        return runtimeDir + QLatin1Char('/') + name;
    }
    return QStringLiteral("/tmp/") + name;
}

void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    const bool quietTerminal = QCoreApplication::instance()
        && QCoreApplication::instance()->property("cliQuietTerminal").toBool();
    if (!quietTerminal) {
        QTextStream term_out(stdout);
        term_out << msg;
        if (!msg.endsWith('\n')) term_out << '\n';
    }

    if (!logFile.isOpen()) return;

    QTextStream out(&logFile);
    out << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz ");
    switch (type) {
    case QtInfoMsg:    out << "INF "; break;
    case QtDebugMsg:   out << "DBG "; break;
    case QtWarningMsg: out << "WRN "; break;
    case QtCriticalMsg:out << "CRT "; break;
    case QtFatalMsg:   out << "FTL "; break;
    }
    out << context.category << ": " << msg;
    if (!msg.endsWith('\n')) out << '\n';
}
}

namespace AppInit {

void setupRootEnv()
{
    if (getuid() == 0) {
        qputenv("XDG_RUNTIME_DIR", "/run/user/0");
        qunsetenv("SESSION_MANAGER");
        qputenv("HOME", "/root");
    }
}

void installTranslations()
{
    static QTranslator qtTran;
    static QTranslator qtBaseTran;
    static QTranslator appTran;

    const QString transpath = QLibraryInfo::path(QLibraryInfo::TranslationsPath);
    if (qtTran.isEmpty()) {
        const bool ok = qtTran.load(QLocale::system(), "qt", "_", transpath);
        if (ok) QCoreApplication::installTranslator(&qtTran);
    }
    if (qtBaseTran.isEmpty()) {
        const bool ok = qtBaseTran.load(QLocale::system(), "qtbase", "_", transpath);
        if (ok) QCoreApplication::installTranslator(&qtBaseTran);
    }

    const QString appName = QCoreApplication::applicationName();
    if (!appName.isEmpty() && appTran.isEmpty()) {
        const bool ok = appTran.load(QLocale::system(), appName, "_", "/usr/share/mx-bootrepair/locale");
        if (ok) QCoreApplication::installTranslator(&appTran);
    }
}

void setupLogging()
{
    const QString log_name = sessionLogPath();
    logFilePath = log_name;
    const QFileInfo logInfo(log_name);
    if (logInfo.exists() || logInfo.isSymLink()) {
        QFile::remove(log_name + ".old");
        QFile::rename(log_name, log_name + ".old");
    }
    logFile.setFileName(log_name);
    // Open without following a symlink at the final component, so the
    // world-writable /tmp fallback cannot be redirected at another file.
    const QByteArray encodedLogName = QFile::encodeName(log_name);
    const int fd = ::open(encodedLogName.constData(),
                          O_WRONLY | O_CREAT | O_APPEND | O_NOFOLLOW | O_CLOEXEC, 0600);
    if (fd < 0 || !logFile.open(fd, QFile::Append | QFile::Text, QFileDevice::AutoCloseHandle)) {
        if (fd >= 0) {
            ::close(fd);
        }
        qWarning() << "Failed to open log file:" << log_name;
    }
    qInstallMessageHandler(messageHandler);
}

bool shouldPersistLog()
{
    if (logFile.isOpen()) {
        logFile.flush();
    }

    const QString logPath = logFilePath;
    if (logPath.isEmpty()) {
        return false;
    }

    const QFileInfo info(logPath);
    return info.exists() && info.isFile() && info.size() >= MIN_PERSISTED_LOG_SIZE;
}

} // namespace AppInit
