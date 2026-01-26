// Shared application initialization helpers (QtCore-only)
#include "core/app_init.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QLibraryInfo>
#include <QLocale>
#include <QTextStream>
#include <QTranslator>

#include <unistd.h>

namespace {
static QFile logFile; // persists for process lifetime

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
    const QString appName = QCoreApplication::applicationName();
    const QString log_name = "/tmp/" + (appName.isEmpty() ? QStringLiteral("mx-boot-repair") : appName) + ".log";
    if (QFileInfo::exists(log_name)) {
        QFile::remove(log_name + ".old");
        QFile::rename(log_name, log_name + ".old");
    }
    logFile.setFileName(log_name);
    if (!logFile.open(QFile::Append | QFile::Text)) {
        qWarning() << "Failed to open log file:" << log_name;
    }
    qInstallMessageHandler(messageHandler);
}

} // namespace AppInit
