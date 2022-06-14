/*****************************************************************************
 * mainwindow.h
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
 * along with MX Boot Repair.  If not, see <http://www.gnu.org/licenses/>.
 **********************************************************************/

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "cmd.h"

#include <QMessageBox>
#include <QProcess>
#include <QTemporaryDir>
#include <QTimer>

namespace Ui {
class MainWindow;
}

class MainWindow : public QDialog
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

public slots:
    bool openLuks(const QString &part);
    void disableOutput();
    void displayOutput();
    void displayResult(bool success);
    void outputAvailable(const QString &output);
    void procDone();
    void procStart();
    void progress();

private slots:
    void buttonAbout_clicked();
    void buttonApply_clicked();
    void buttonHelp_clicked();
    void grubEspButton_clicked();
    void grubMbrButton_clicked();
    void grubRootButton_clicked();

private:
    Cmd *shell;
    Ui::MainWindow *ui;
    QStringList ListDisk;
    QStringList ListPart;
    QTimer *timer;
    QTemporaryDir tmpdir;

    QString selectPart(const QString &path, const QString &mountpoint);
    bool checkAndMountPart(const QString &path, const QString &mountpoint);
    void addDevToList();
    void backupBR(const QString &filename);
    void cleanupMountPoints(const QString &path, bool isLuks);
    void guessPartition();
    void installGRUB();
    void installGRUB(const QString &location, const QString &path, bool isLuks);
    void refresh();
    void repairGRUB();
    void restoreBR(const QString &filename);
    void setEspDefaults();
    void targetSelection();

};

#endif // MAINWINDOW_H
