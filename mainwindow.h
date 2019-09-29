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

#include <QMessageBox>
#include <QProcess>
#include <QTimer>

namespace Ui {
class MainWindow;
}

class MainWindow : public QDialog
{
    Q_OBJECT

protected:
    QProcess *proc;
    QTimer *timer;


public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

    QString getCmdOut(QString cmd);

    void addDevToList();
    void backupBR(QString filename);
    void guessPartition();
    void refresh();
    void installGRUB();
    void repairGRUB();
    void restoreBR(QString filename);
    void setEspDefaults();
    void targetSelection();

public slots:
    void procStart();
    void procTime();
    void procDone(int exitCode);
    void setConnections(QTimer* timer, QProcess* proc);
    void onStdoutAvailable();

    virtual void on_buttonApply_clicked();
    virtual void on_buttonAbout_clicked();
    virtual void on_buttonHelp_clicked();


private slots:
    void on_grubMbrButton_clicked();
    void on_grubRootButton_clicked();
    void on_grubEspButton_clicked();

private:
    Ui::MainWindow *ui;
    QStringList ListDisk;
    QStringList ListPart;

};

#endif // MAINWINDOW_H
