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
#include <QCommandLineParser>
#include <QTextStream>

#include "cli/controller.h"
#include "core/app_init.h"
#include "core/cmd.h"
#include "mainwindow.h"
#include <unistd.h>

#ifndef VERSION
#define VERSION "?.?.?.?"
#endif

int main(int argc, char *argv[])
{
    // Check for CLI mode or headless environment first (simple check)
    bool forceCli = false;
    bool headless = qEnvironmentVariableIsEmpty("DISPLAY") && qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY");

    // Quick check for CLI flag
    for (int i = 1; i < argc; ++i) {
        const QString arg = QString::fromLocal8Bit(argv[i]);
        if (arg == "-c" || arg == "--cli") {
            forceCli = true;
            break;
        }
    }

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
        if (QFile::exists("/usr/bin/pkexec")) {
            Cmd().run("pkexec /usr/lib/mx-boot-repair/mxbr-lib copy_log", nullptr, nullptr, QuietMode::Yes);
        } else {
            Cmd().runAsRoot("/usr/lib/mx-boot-repair/mxbr-lib copy_log", nullptr, nullptr, QuietMode::Yes);
        }
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

    // Parse command line arguments for GUI mode
    QCommandLineParser parser;
    parser.setApplicationDescription(QObject::tr("MX Boot Repair - GUI and CLI tool for repairing GRUB bootloader"));
    parser.addHelpOption();
    parser.addVersionOption();

    // Define command line options
    QCommandLineOption cliOpt({"c", "cli"}, QObject::tr("Force CLI mode"));
    QCommandLineOption dryRunOpt({"d", "dry-run"}, QObject::tr("Print actions without executing"));
    QCommandLineOption nonIntOpt({"n", "non-interactive"}, QObject::tr("Do not prompt; require flags"));
    QCommandLineOption actionOpt("action", QObject::tr("Action: install, repair, initramfs, backup, restore"), "name");
    QCommandLineOption targetOpt("target", QObject::tr("Target for install: mbr, esp, root"), "name");
    QCommandLineOption locationOpt("location", QObject::tr("Device for target (e.g., sda, sda1)"), "dev");
    QCommandLineOption rootOpt("root", QObject::tr("Root partition (e.g., /dev/sda2)"), "dev");
    QCommandLineOption bootDevOpt("boot-device", QObject::tr("Partition to mount at /boot in chroot"), "dev");
    QCommandLineOption espDevOpt("esp-device", QObject::tr("Partition to mount at /boot/efi in chroot"), "dev");
    QCommandLineOption pathOpt("backup-path", QObject::tr("Path for backup/restore image"), "path");
    QCommandLineOption forceOpt({"f", "force"}, QObject::tr("Skip confirmations (for restore)"));
    QCommandLineOption verboseOpt("verbose", QObject::tr("Enable verbose output"));
    QCommandLineOption quietOpt({"q", "quiet"}, QObject::tr("Suppress non-error output"));

    parser.addOptions({cliOpt, dryRunOpt, nonIntOpt, actionOpt, targetOpt, locationOpt, rootOpt, bootDevOpt, espDevOpt, pathOpt, forceOpt, verboseOpt, quietOpt});

    // Process arguments
    parser.process(QCoreApplication::arguments());

    // Validate conflicting options
    if (parser.isSet(verboseOpt) && parser.isSet(quietOpt)) {
        QTextStream out(stderr);
        out << QObject::tr("Error: --verbose and --quiet options are mutually exclusive\n");
        return 2;
    }

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
