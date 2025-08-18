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

#include "common.h"
#include "mainwindow.h"
#include "version.h"
#include <unistd.h>

static QFile logFile;

void messageHandler(QtMsgType /*type*/, const QMessageLogContext & /*context*/, const QString & /*msg*/);

int main(int argc, char *argv[])
{
    if (getuid() == 0) {
        qputenv("XDG_RUNTIME_DIR", "/run/user/0");
        qunsetenv("SESSION_MANAGER");
    }
    // Set Qt platform to XCB (X11) if not already set and we're in X11 environment
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
        if (!qEnvironmentVariableIsEmpty("DISPLAY") && qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY")) {
            qputenv("QT_QPA_PLATFORM", "xcb");
        }
    }

    QApplication app(argc, argv);
    if (getuid() == 0) {
        qputenv("HOME", "/root");
    }

    QApplication::setWindowIcon(QIcon::fromTheme(QApplication::applicationName()));
    QApplication::setApplicationDisplayName(QObject::tr("MX Boot Repair"));
    QApplication::setOrganizationName("MX-Linux");
    QApplication::setApplicationVersion(VERSION);

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    const QString &transpath = QLibraryInfo::path(QLibraryInfo::TranslationsPath);
#else
    const QString &transpath = QLibraryInfo::path(QLibraryInfo::TranslationsPath);
#endif
    QTranslator qtTran;
    if (qtTran.load(QLocale::system(), "qt", "_", transpath)) {
        QApplication::installTranslator(&qtTran);
    }
    QTranslator qtBaseTran;
    if (qtBaseTran.load(QLocale::system(), "qtbase", "_", transpath)) {
        QApplication::installTranslator(&qtBaseTran);
    }
    QTranslator appTran;
    if (appTran.load(QLocale::system(), QApplication::applicationName(), "_", "/usr/share/mx-bootrepair/locale")) {
        QApplication::installTranslator(&appTran);
    }

    QString log_name = "/tmp/" + QApplication::applicationName() + ".log";
    if (QFileInfo::exists(log_name)) {
        QFile::remove(log_name + ".old");
        QFile::rename(log_name, log_name + ".old");
    }
    logFile.setFileName(log_name);
    logFile.open(QFile::Append | QFile::Text);
    qInstallMessageHandler(messageHandler);

    if (getuid() != 0) {
        if (!QFile::exists("/usr/bin/pkexec") && !QFile::exists("/usr/bin/gksu")) {
            QMessageBox::critical(nullptr, QObject::tr("Error"),
                                  QObject::tr("You must run this program with admin access."));
            exit(EXIT_FAILURE);
        }
    }
    MainWindow w;
    w.show();
    return QApplication::exec();
}

// The implementation of the handler
void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    QTextStream term_out(stdout);
    term_out << msg << '\n';

    QTextStream out(&logFile);
    out << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz ");
    switch (type) {
    case QtInfoMsg:
        out << "INF ";
        break;
    case QtDebugMsg:
        out << "DBG ";
        break;
    case QtWarningMsg:
        out << "WRN ";
        break;
    case QtCriticalMsg:
        out << "CRT ";
        break;
    case QtFatalMsg:
        out << "FTL ";
        break;
    }
    out << context.category << ": " << msg << '\n';
}
