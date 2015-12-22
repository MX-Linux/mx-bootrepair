/*****************************************************************************
 * mxbootrepair.cpp
 *****************************************************************************
 * Copyright (C) 2014 MX Authors
 *
 * Authors: Adrian
 *          MEPIS Community <http://forum.mepiscommunity.org>
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

#include "mxbootrepair.h"
#include "ui_mxbootrepair.h"

#include <QFileDialog>


mxbootrepair::mxbootrepair(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::mxbootrepair)
{
    ui->setupUi(this);
    refresh();
    addDevToList();
}

mxbootrepair::~mxbootrepair()
{
    delete ui;
}

// Util function
QString mxbootrepair::getCmdOut(QString cmd) {
    QProcess *proc = new QProcess();
    proc->start(cmd);
    proc->setReadChannel(QProcess::StandardOutput);
    proc->setReadChannelMode(QProcess::MergedChannels);
    proc->waitForFinished(-1);
    return proc->readAllStandardOutput().trimmed();
}

// Get version of the program
QString mxbootrepair::getVersion(QString name) {
    QString cmd = QString("bash -c \"dpkg -l %1 | awk 'NR==6 {print $3}'\"").arg(name);
    return getCmdOut(cmd);
}

void mxbootrepair::refresh() {
    proc = new QProcess(this);
    timer = new QTimer(this);
    proc->setReadChannel(QProcess::StandardOutput);
    proc->setReadChannelMode(QProcess::MergedChannels);
    ui->stackedWidget->setCurrentIndex(0);
    ui->reinstallRadioButton->setFocus();
    ui->reinstallRadioButton->setChecked(true);
    ui->progressBar->hide();
    ui->progressBar->setValue(0);
    ui->outputBox->setPlainText("");
    ui->outputLabel->setText("");
    ui->grubInsLabel->show();
    ui->grubRootButton->show();
    ui->grubMbrButton->show();
    ui->rootLabel->hide();
    ui->rootCombo->hide();
    ui->buttonOk->setText("Ok");
    ui->buttonOk->setIcon(QIcon("/usr/share/mx-bootrepair/icons/dialog-ok.png"));
    ui->buttonOk->setEnabled(true);
    ui->buttonCancel->setEnabled(true);
    setCursor(QCursor(Qt::ArrowCursor));
}

void mxbootrepair::reinstallGRUB() {
    QString cmd;
    ui->progressBar->show();
    setCursor(QCursor(Qt::WaitCursor));
    ui->buttonCancel->setEnabled(false);
    ui->buttonOk->setEnabled(false);
    ui->stackedWidget->setCurrentWidget(ui->outputPage);
    QString location = QString(ui->grubBootCombo->currentText()).section(" ", 0, 0);
    QString root = QString(ui->rootCombo->currentText()).section(" ", 0, 0);
    QString text = QString("GRUB is being installed on %1 device.").arg(location);
    ui->outputLabel->setText(text);
    setConnections(timer, proc);
    // create a temp folder and mount dev sys proc
    QString path = getCmdOut("mktemp -d --tmpdir -p /mnt");
    cmd = QString("mount /dev/%1 %2 && mount -o bind /dev %2/dev && mount -o bind /sys %2/sys && mount -o bind /proc %2/proc").arg(root).arg(path);
    if (system(cmd.toUtf8()) == 0) {
        QEventLoop loop;
        connect(proc, SIGNAL(finished(int)), &loop, SLOT(quit()));
        cmd = QString("bash -c \"chroot %1 grub-install --target=i386-pc --recheck --force /dev/%2\"").arg(path).arg(location);
        proc->start(cmd);
        loop.exec();
    } else {
        QMessageBox::critical(this, tr("Error"),
                              tr("Could not set up chroot environment.\nPlease double-check the selected location."));
        setCursor(QCursor(Qt::ArrowCursor));
        ui->buttonOk->setEnabled(true);
        ui->buttonCancel->setEnabled(true);
        ui->progressBar->hide();
        ui->stackedWidget->setCurrentWidget(ui->selectionPage);
    }
    // umount and clean temp folder
    cmd = QString("umount %1/proc %1/sys %1/dev; umount %1; rmdir %1").arg(path);
    system(cmd.toUtf8());
}

void mxbootrepair::repairGRUB() {
    QString cmd;
    ui->progressBar->show();
    setCursor(QCursor(Qt::WaitCursor));
    ui->buttonCancel->setEnabled(false);
    ui->buttonOk->setEnabled(false);
    ui->stackedWidget->setCurrentWidget(ui->outputPage);
    QString location = QString(ui->grubBootCombo->currentText()).section(" ", 0, 0);
    ui->outputLabel->setText("The GRUB configuration file (grub.cfg) is being rebuild.");
    setConnections(timer, proc);
    // create a temp folder and mount dev sys proc
    QString path = getCmdOut("mktemp -d --tmpdir -p /mnt");
    cmd = QString("mount /dev/%1 %2 && mount -o bind /dev %2/dev && mount -o bind /sys %2/sys && mount -o bind /proc %2/proc").arg(location).arg(path);
    if (system(cmd.toUtf8()) == 0) {
        QEventLoop loop;
        connect(proc, SIGNAL(finished(int)), &loop, SLOT(quit()));
        cmd = QString("bash -c \"chroot %1 update-grub\"").arg(path);
        proc->start(cmd);
        loop.exec();
    } else {
        QMessageBox::critical(this, tr("Error"),
                              tr("Could not set up chroot environment.\nPlease double-check the selected location."));
        setCursor(QCursor(Qt::ArrowCursor));
        ui->buttonOk->setEnabled(true);
        ui->buttonCancel->setEnabled(true);
        ui->progressBar->hide();
        ui->stackedWidget->setCurrentWidget(ui->selectionPage);
    }
    // umount and clean temp folder
    cmd = QString("umount %1/proc %1/sys %1/dev; umount %1; rmdir %1").arg(path);
    system(cmd.toUtf8());
}


void mxbootrepair::backupBR(QString filename) {
    ui->progressBar->show();
    setCursor(QCursor(Qt::WaitCursor));
    ui->buttonCancel->setEnabled(false);
    ui->buttonOk->setEnabled(false);
    ui->stackedWidget->setCurrentWidget(ui->outputPage);
    QString location = QString(ui->grubBootCombo->currentText()).section(" ", 0, 0);
    QString text = QString("Backing up MBR or PBR from %1 device.").arg(location);
    ui->outputLabel->setText(text);
    setConnections(timer, proc);
    QString cmd = "dd if=/dev/" + location + " of=" + filename + " bs=446 count=1";
    proc->start(cmd);
}

void mxbootrepair::restoreBR(QString filename) {
    ui->progressBar->show();
    setCursor(QCursor(Qt::WaitCursor));
    ui->buttonCancel->setEnabled(false);
    ui->buttonOk->setEnabled(false);
    ui->stackedWidget->setCurrentWidget(ui->outputPage);
    QString location = QString(ui->grubBootCombo->currentText()).section(" ", 0, 0);
    if (QMessageBox::warning(this, tr("Warning"),
                              tr("You are going to write the content of ") + filename + tr(" to ") + location + tr("\n\nAre you sure?"),
                                         tr("Yes"), tr("No")) == 1){
        refresh();
        return;
    }
    QString text = QString("Restoring MBR/PBR from backup to %1 device.").arg(location);
    ui->outputLabel->setText(text);
    setConnections(timer, proc);
    QString cmd = "dd if=" + filename + " of=/dev/" + location + " bs=446 count=1";
    proc->start(cmd);
}


//// sync process events ////

void mxbootrepair::procStart() {
    timer->start(100);
}

void mxbootrepair::procTime() {
    int i = ui->progressBar->value() + 1;
    if (i > 100) {
        i = 0;
    }
    ui->progressBar->setValue(i);
}

void mxbootrepair::procDone(int exitCode) {
    timer->stop();
    ui->progressBar->setValue(100);
    setCursor(QCursor(Qt::ArrowCursor));
    ui->buttonCancel->setEnabled(true);
    ui->buttonOk->setEnabled(true);
    if (exitCode == 0) {
        if (QMessageBox::information(this, tr("Success"),
                                     tr("Process finished with success.<p><b>Do you want to exit MX Boot Repair?</b>"),
                                     tr("Yes"), tr("No")) == 0){
            qApp->exit(0);
        } else {
            ui->buttonOk->setText("< Back");
            ui->buttonOk->setIcon(QIcon());
        }
    } else {
        QMessageBox::critical(this, tr("Error"),
                              tr("Process finished. Errors have occurred."));
        ui->buttonOk->setText("< Back");
        ui->buttonOk->setIcon(QIcon());
    }
}

// set proc and timer connections
void mxbootrepair::setConnections(QTimer* timer, QProcess* proc) {
    disconnect(timer, SIGNAL(timeout()), 0, 0);
    connect(timer, SIGNAL(timeout()), this, SLOT(procTime()));
    disconnect(proc, SIGNAL(started()), 0, 0);
    connect(proc, SIGNAL(started()), this, SLOT(procStart()));
    disconnect(proc, SIGNAL(finished(int)), 0, 0);
    connect(proc, SIGNAL(finished(int)), this, SLOT(procDone(int)));
    disconnect(proc, SIGNAL(readyReadStandardOutput()), 0, 0);
    connect(proc, SIGNAL(readyReadStandardOutput()), this, SLOT(onStdoutAvailable()));
}

// add list of devices to grubBootCombo
void mxbootrepair::addDevToList() {
    QString cmd = "/bin/bash -c \"lsblk -ln -o NAME,SIZE,LABEL,MODEL -d -e 2,11 | grep '^[h,s,v].[a-z]' | sort\"";
    proc->start(cmd);
    proc->waitForFinished();
    QString out = proc->readAllStandardOutput();
    ListDisk = out.split("\n", QString::SkipEmptyParts);

    cmd = "/bin/bash -c \"lsblk -ln -o NAME,SIZE,FSTYPE,MOUNTPOINT,LABEL -e 2,11 | grep '[h,s,v].[a-z][0-9]' | sort\"";
    proc->start(cmd);
    proc->waitForFinished();
    out = proc->readAllStandardOutput();
    ListPart = out.split("\n", QString::SkipEmptyParts);
    ui->rootCombo->clear();
    ui->rootCombo->addItems(ListPart);

    ui->grubBootCombo->clear();
    // add only disks
    if (ui->grubMbrButton->isChecked()) {
        ui->grubBootCombo->addItems(ListDisk);
    } else { // add partition
        ui->grubBootCombo->addItems(ListPart);
    }

}

void mxbootrepair::mbrOrRoot() {
    ui->grubBootCombo->clear();
    // add only disks
    if (ui->grubMbrButton->isChecked()) {
        ui->grubBootCombo->addItems(ListDisk);
    // add partitions
    } else {
        ui->grubBootCombo->addItems(ListPart);
    }
}

//// slots ////

// update output box on Stdout
void mxbootrepair::onStdoutAvailable() {
    QString out = ui->outputBox->toPlainText() + proc->readAllStandardOutput();
    ui->outputBox->setPlainText(out);
}

// repopulate combo when grubRootButton is toggled
void mxbootrepair::on_grubRootButton_toggled() {
    mbrOrRoot();
}

// OK button clicked
void mxbootrepair::on_buttonOk_clicked() {
    // on first page
    if (ui->stackedWidget->currentIndex() == 0) {
        // Reinstall button selected
        if (ui->reinstallRadioButton->isChecked()) {
            ui->stackedWidget->setCurrentWidget(ui->selectionPage);
            ui->bootMethodGroup->setTitle("Select Boot Method");
            ui->grubInsLabel->setText("Install on:");
            ui->grubRootButton->setText("root");
            ui->rootLabel->show();
            ui->rootCombo->show();
        // Repair button selected
        } else if (ui->repairRadioButton->isChecked()) {
            ui->stackedWidget->setCurrentWidget(ui->selectionPage);
            ui->bootMethodGroup->setTitle("Select GRUB location");
            ui->grubInsLabel->hide();
            ui->grubRootButton->hide();
            ui->grubRootButton->setChecked(true);
            ui->grubMbrButton->hide();
        // Backup button selected
        } else if (ui->bakRadioButton->isChecked()) {
            ui->stackedWidget->setCurrentWidget(ui->selectionPage);
            ui->bootMethodGroup->setTitle("Select Item to Back Up");
            ui->grubInsLabel->setText("");
            ui->grubRootButton->setText("PBR");
        // Restore backup button selected
        } else if (ui->restoreBakRadioButton->isChecked()) {
            ui->stackedWidget->setCurrentWidget(ui->selectionPage);
            ui->bootMethodGroup->setTitle("Select Item to Restore");
            ui->grubInsLabel->setText("");
            ui->grubRootButton->setText("PBR");
        }
    // on selection page
    } else if (ui->stackedWidget->currentWidget() == ui->selectionPage) {
        if (ui->reinstallRadioButton->isChecked()) {
            reinstallGRUB();
        } else if (ui->bakRadioButton->isChecked()) {
            QString filename = QFileDialog::getSaveFileName(this, tr("Select backup file name"));
            if (filename == "") {
                QMessageBox::critical(this, tr("Error"), tr("No file was selected."));
                return;
            }
            backupBR(filename);
        } else if (ui->restoreBakRadioButton->isChecked()) {
            QString filename = QFileDialog::getOpenFileName(this, tr("Select MBR or PBR backup file"));
            if (filename == "") {
                QMessageBox::critical(this, tr("Error"), tr("No file was selected."));
                return;
            }
            restoreBR(filename);
        } else if (ui->repairRadioButton->isChecked()) {
            repairGRUB();
        }
    // on output page
    } else if (ui->stackedWidget->currentWidget() == ui->outputPage) {
        refresh();
    } else {
        qApp->exit(0);
    }
}

// About button clicked
void mxbootrepair::on_buttonAbout_clicked() {
    this->hide();
    QMessageBox msgBox(QMessageBox::NoIcon,
                       tr("About MX Boot Repair"), "<p align=\"center\"><b><h2>" +
                       tr("MX Boot Repair") + "</h2></b></p><p align=\"center\">" + tr("Version: ") +
                       getVersion("mx-bootrepair") + "</p><p align=\"center\"><h3>" +
                       tr("Simple boot repair program for MX Linux") + "</h3></p><p align=\"center\"><a href=\"http://www.mepiscommunity.org/mx\">http://www.mepiscommunity.org/mx</a><br /></p><p align=\"center\">" +
                       tr("Copyright (c) antiX") + "<br /><br /></p>", 0, this);
    msgBox.addButton(tr("License"), QMessageBox::AcceptRole);
    msgBox.addButton(tr("Cancel"), QMessageBox::RejectRole);
    if (msgBox.exec() == QMessageBox::AcceptRole) {
        QString cmd = QString("mx-viewer file:///usr/share/doc/mx-bootrepair/license.html %1").arg(tr("\'MX Boot Repair License\'"));
        system(cmd.toUtf8());
    }
    this->show();
}

// Help button clicked
void mxbootrepair::on_buttonHelp_clicked() {
    this->hide();
    QString cmd = QString("mx-viewer http://mepiscommunity.org/wiki/help-files/help-mx-boot-repair %1").arg(tr("\'MX Boot Repair Help\'"));
    system(cmd.toUtf8());
    this->show();
}


