/*****************************************************************************
 * mxbootrepair.h
 *****************************************************************************
 * Copyright (C) 2014 MX Authors
 *
 * Authors: Adrian
 *          MEPIS Community <http://forum.mepiscommunity.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/


#ifndef MXBOOTREPAIR_H
#define MXBOOTREPAIR_H

#include <QMessageBox>
#include <QProcess>
#include <QTimer>

namespace Ui {
class mxbootrepair;
}

class mxbootrepair : public QDialog
{
    Q_OBJECT
    
protected:
    QProcess *proc;
    QTimer *timer;

public:
    explicit mxbootrepair(QWidget *parent = 0);
    ~mxbootrepair();

    QString getCmdOut(QString cmd);
    void refresh();
    void displaySite(QString site);
    void addDevToList();
    void mbrOrRoot();

    void reinstallGRUB();
    void repairGRUB();
    void backupBR(QString filename);
    void restoreBR(QString filename);


public slots:
    void procStart();
    void procTime();
    void procDone(int exitCode);
    void setConnections(QTimer* timer, QProcess* proc);
    void onStdoutAvailable();

    virtual void on_grubRootButton_toggled();
    virtual void on_buttonOk_clicked();
    virtual void on_buttonAbout_clicked();
    virtual void on_buttonHelp_clicked();


private:
    Ui::mxbootrepair *ui;

};

#endif // MXBOOTREPAIR_H
