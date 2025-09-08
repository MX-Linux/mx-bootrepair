/*****************************************************************************
 * mainwindow.h
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
 * along with MX Boot Repair.  If not, see <http://www.gnu.org/licenses/>.
 **********************************************************************/
#pragma once

#include "core/bootrepair_engine.h"

#include <QMessageBox>

namespace Ui
{
class MainWindow;
}

class MainWindow : public QDialog
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

public slots:
    void disableOutput();
    void displayOutput();
    void displayResult(bool success);
    void outputAvailable(const QString &output);
    void procDone();
    void procStart();

private slots:
    void buttonAbout_clicked();
    void buttonApply_clicked();
    void buttonHelp_clicked();

private:
    BootRepairEngine *engine;
    Ui::MainWindow *ui;
    QStringList ListDisk;
    QStringList ListPart;
    QString selectPartFromList(const QString &mountpoint);
    bool isMountedTo(const QString &volume, const QString &mount);
    static bool isUefi();
    void addDevToList();
    void backupBR(const QString &filename);
    void guessPartition();
    void installGRUB();
    void regenerateInitramfs();
    void refresh();
    void repairGRUB();
    void restoreBR(const QString &filename);
    void setEspDefaults();
    void targetSelection();
};
