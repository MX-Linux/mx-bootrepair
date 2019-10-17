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

#include "mainwindow.h"
#include <unistd.h>

#include <QApplication>
#include <QFile>
#include <QDateTime>
#include <QIcon>
#include <QLocale>
#include <QScopedPointer>
#include <QTranslator>

static QScopedPointer<QFile> logFile;
void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg);

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setWindowIcon(QIcon::fromTheme("mx-bootrepair"));

    QString log_name= "/var/log/" + QCoreApplication::applicationName() + ".log";
    logFile.reset(new QFile(log_name));
    logFile.data()->open(QFile::Append | QFile::Text);
    qInstallMessageHandler(messageHandler);

    QTranslator qtTran;
    qtTran.load(QString("qt_") + QLocale::system().name());
    a.installTranslator(&qtTran);

    QTranslator appTran;
    appTran.load(QString("mx-bootrepair_") + QLocale::system().name(), "/usr/share/mx-bootrepair/locale");
    a.installTranslator(&appTran);

    if (getuid() == 0) {
        MainWindow w;
        w.show();

        return a.exec();

    } else {
        QApplication::beep();
        QMessageBox::critical(nullptr, QString::null,
                              QApplication::tr("You must run this program as root."));
        return EXIT_FAILURE;
    }
}

// The implementation of the handler
void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    // Write to terminal
    QTextStream term_out(stdout);
    term_out << msg << endl;

    // Open stream file writes
    QTextStream out(logFile.data());

    // Write the date of recording
    out << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz ");
    // By type determine to what level belongs message
    switch (type)
    {
    case QtInfoMsg:     out << "INF "; break;
    case QtDebugMsg:    out << "DBG "; break;
    case QtWarningMsg:  out << "WRN "; break;
    case QtCriticalMsg: out << "CRT "; break;
    case QtFatalMsg:    out << "FTL "; break;
    }
    // Write to the output category of the message and the message itself
    out << context.category << ": "
        << msg << endl;
    out.flush();    // Clear the buffered data
}
