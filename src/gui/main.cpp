/*****************************************************************************
 * main.cpp
 *****************************************************************************
 * Copyright (C) 2014-2025 MX Authors
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
#include <QCoreApplication>
#include <QIcon>
#include <QMessageBox>

#include "core/app_init.h"
#include "cli/controller.h"
#include "core/cmd.h"
#include "mainwindow.h"
#include <unistd.h>

#ifndef VERSION
    #define VERSION "?.?.?.?"
#endif

int main(int argc, char *argv[])
{
    // Detect CLI flag or headless first
    bool forceCli = false;
    for (int i = 1; i < argc; ++i) {
        const QString arg = QString::fromLocal8Bit(argv[i]);
        if (arg == "-c" || arg == "--cli") { forceCli = true; break; }
    }
    const bool headless = qEnvironmentVariableIsEmpty("DISPLAY") && qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY");

    if (forceCli || headless) {
        QCoreApplication app(argc, argv);
        app.setProperty("cliMode", true);
        app.setProperty("cliQuietTerminal", true); // CLI mode: suppress log mirroring to terminal
        QCoreApplication::setApplicationName("mx-boot-repair");
        QCoreApplication::setOrganizationName("MX-Linux");
        QCoreApplication::setApplicationVersion(VERSION);
        AppInit::setupRootEnv();
        AppInit::installTranslations();
        AppInit::setupLogging();
        CliController controller;
        const int code = controller.run();
        Cmd().runAsRoot("/usr/lib/mx-boot-repair/mxbr-lib copy_log", nullptr, nullptr, QuietMode::Yes);
        return code;
    }

    // GUI mode
    AppInit::setupRootEnv();
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
        if (!qEnvironmentVariableIsEmpty("DISPLAY") && qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY")) {
            qputenv("QT_QPA_PLATFORM", "xcb");
        }
    }

    QApplication app(argc, argv);
    QApplication::setWindowIcon(QIcon::fromTheme(QApplication::applicationName()));
    QApplication::setApplicationDisplayName(QObject::tr("MX Boot Repair"));
    QApplication::setOrganizationName("MX-Linux");
    QApplication::setApplicationVersion(VERSION);
    AppInit::installTranslations();
    AppInit::setupLogging();

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
