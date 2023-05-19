/*****************************************************************************
 * main.cpp
 *****************************************************************************
 * Copyright (C) 2014 MX Authors
 *
 * Authors: Adrian
 *          MX Linux <http://mxlinux.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * MX Boot Repair is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with MX Snapshot.  If not, see <http://www.gnu.org/licenses/>.
 **********************************************************************/

#include <QApplication>
#include <QDateTime>
#include <QFile>
#include <QIcon>
#include <QLibraryInfo>
#include <QLocale>
#include <QTextStream>
#include <QTranslator>

#include "mainwindow.h"
#include "version.h"
#include <unistd.h>

static QFile logFile;
extern const QString starting_home = qEnvironmentVariable("HOME");

void messageHandler(QtMsgType /*type*/, const QMessageLogContext & /*context*/, const QString & /*msg*/);

int main(int argc, char *argv[])
{
    if (getuid() == 0) {
        qputenv("XDG_RUNTIME_DIR", "/run/user/0");
        qunsetenv("SESSION_MANAGER");
    }
    QApplication app(argc, argv);
    if (getuid() == 0)
        qputenv("HOME", "/root");

    QApplication::setWindowIcon(QIcon::fromTheme(QApplication::applicationName()));
    QApplication::setApplicationDisplayName(QObject::tr("MX Boot Repair"));
    QApplication::setOrganizationName(QStringLiteral("MX-Linux"));
    QApplication::setApplicationVersion(VERSION);


    QTranslator qtTran;
    if (qtTran.load("qt_" + QLocale().name(), QLibraryInfo::location(QLibraryInfo::TranslationsPath)))
        QApplication::installTranslator(&qtTran);

    QTranslator qtBaseTran;
    if (qtBaseTran.load("qtbase_" + QLocale().name(), QLibraryInfo::location(QLibraryInfo::TranslationsPath)))
        QApplication::installTranslator(&qtBaseTran);

    QTranslator appTran;
    if (appTran.load(QApplication::applicationName() + "_" + QLocale().name(),
                     QStringLiteral("/usr/share/mx-bootrepair/locale")))
        QApplication::installTranslator(&appTran);

    QString log_name = "/var/log/" + QApplication::applicationName() + ".log";
    if (QFileInfo::exists(log_name)) {
        QFile::remove(log_name + ".old");
        QFile::rename(log_name, log_name + ".old");
    }
    logFile.setFileName(log_name);
    logFile.open(QFile::Append | QFile::Text);
    qInstallMessageHandler(messageHandler);

    if (getuid() == 0) {
        MainWindow w;
        w.show();
        return QApplication::exec();
    } else {
        QProcess::startDetached(QStringLiteral("/usr/bin/mxbr-launcher"), {});
    }
}

// The implementation of the handler
void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    QTextStream term_out(stdout);
    term_out << msg << QStringLiteral("\n");

    QTextStream out(&logFile);
    out << QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd hh:mm:ss.zzz "));
    switch (type) {
    case QtInfoMsg:
        out << QStringLiteral("INF ");
        break;
    case QtDebugMsg:
        out << QStringLiteral("DBG ");
        break;
    case QtWarningMsg:
        out << QStringLiteral("WRN ");
        break;
    case QtCriticalMsg:
        out << QStringLiteral("CRT ");
        break;
    case QtFatalMsg:
        out << QStringLiteral("FTL ");
        break;
    }
    out << context.category << QStringLiteral(": ") << msg << QStringLiteral("\n");
}
