/*****************************************************************************
 * mxbootrepair.cpp
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

#include "mxbootrepair.h"
#include "ui_mxbootrepair.h"
#include "version.h"

#include <QFileDialog>
#include <QDebug>

mxbootrepair::mxbootrepair(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::mxbootrepair)
{
    qDebug() << "Program Version:" << VERSION;
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
    QProcess *proc = new QProcess(this);
    proc->start("/bin/bash", QStringList() << "-c" << cmd);
    proc->setReadChannel(QProcess::StandardOutput);
    proc->setReadChannelMode(QProcess::MergedChannels);
    proc->waitForFinished(-1);
    QString out = proc->readAllStandardOutput().trimmed();
    delete proc;
    return out;
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
    ui->grubEspButton->show();
    ui->rootLabel->hide();
    ui->rootCombo->hide();
    ui->buttonApply->setText(tr("Apply"));
    ui->buttonApply->setIcon(QIcon::fromTheme("dialog-ok"));
    ui->buttonApply->setEnabled(true);
    ui->buttonCancel->setEnabled(true);
    ui->rootCombo->setDisabled(false);
    setCursor(QCursor(Qt::ArrowCursor));
}

void mxbootrepair::installGRUB() {
    QString cmd;
    ui->progressBar->show();
    setCursor(QCursor(Qt::WaitCursor));
    ui->buttonCancel->setEnabled(false);
    ui->buttonApply->setEnabled(false);
    ui->stackedWidget->setCurrentWidget(ui->outputPage);

    QString location = QString(ui->grubBootCombo->currentText()).section(" ", 0, 0);
    QString root = QString(ui->rootCombo->currentText()).section(" ", 0, 0);
    QString text = QString(tr("GRUB is being installed on %1 device.")).arg(location);
    ui->outputLabel->setText(text);

    setConnections(timer, proc);
    QEventLoop loop;
    connect(proc, static_cast<void (QProcess::*)(int)>(&QProcess::finished), &loop, &QEventLoop::quit);

    // create a temp folder and mount dev sys proc
    QString path = getCmdOut("mktemp -d --tmpdir -p /tmp");
    cmd = QString("mount /dev/%1 %2 && mount -o bind /dev %2/dev && mount -o bind /sys %2/sys && mount -o bind /proc %2/proc").arg(root).arg(path);
    if (system(cmd.toUtf8()) == 0) {
        cmd = QString("chroot %1 grub-install --target=i386-pc --recheck --force /dev/%2").arg(path).arg(location);
        if (ui->grubEspButton->isChecked()) {
            system("test -d " + path.toUtf8() + "/boot/efi || mkdir " + path.toUtf8()  + "/boot/efi");
            if (system("mount /dev/" + location.toUtf8()  + " " + path.toUtf8() + "/boot/efi") != 0) {
                QMessageBox::critical(this, tr("Error"),
                                      tr("Could not mount ") + location + tr(" on /boot/efi"));
                setCursor(QCursor(Qt::ArrowCursor));
                ui->buttonApply->setEnabled(true);
                ui->buttonCancel->setEnabled(true);
                ui->progressBar->hide();
                ui->stackedWidget->setCurrentWidget(ui->selectionPage);
                return;
            }
            QString arch = getCmdOut("arch");
            if (arch == "i686") { // rename arch to match grub-install target
                arch = "i386";
            }
            QString release = getCmdOut("lsb_release -rs");
            cmd = QString("chroot %1 grub-install --target=%2-efi --efi-directory=/boot/efi --bootloader-id=MX%3 --recheck\"").arg(path).arg(arch).arg(release);
        }
        proc->start(cmd);
        loop.exec();
        // umount and clean temp folder
        system("mountpoint -q " + path.toUtf8() + "/boot/efi && umount " + path.toUtf8() + "/boot/efi");
        cmd = QString("umount %1/proc %1/sys %1/dev; umount %1; rmdir %1").arg(path);
        system(cmd.toUtf8());
    } else {
        QMessageBox::critical(this, tr("Error"),
                              tr("Could not set up chroot environment.\nPlease double-check the selected location."));
        setCursor(QCursor(Qt::ArrowCursor));
        ui->buttonApply->setEnabled(true);
        ui->buttonCancel->setEnabled(true);
        ui->progressBar->hide();
        ui->stackedWidget->setCurrentWidget(ui->selectionPage);
    }
}

void mxbootrepair::repairGRUB() {
    QString cmd;
    ui->progressBar->show();
    setCursor(QCursor(Qt::WaitCursor));
    ui->buttonCancel->setEnabled(false);
    ui->buttonApply->setEnabled(false);
    ui->stackedWidget->setCurrentWidget(ui->outputPage);
    QString location = QString(ui->grubBootCombo->currentText()).section(" ", 0, 0);
    ui->outputLabel->setText(tr("The GRUB configuration file (grub.cfg) is being rebuilt."));
    setConnections(timer, proc);
    // create a temp folder and mount dev sys proc
    QString path = getCmdOut("mktemp -d --tmpdir -p /mnt");
    cmd = QString("mount /dev/%1 %2 && mount -o bind /dev %2/dev && mount -o bind /sys %2/sys && mount -o bind /proc %2/proc").arg(location).arg(path);
    if (system(cmd.toUtf8()) == 0) {
        QEventLoop loop;
        connect(proc, static_cast<void (QProcess::*)(int)>(&QProcess::finished), &loop, &QEventLoop::quit);
        cmd = QString("chroot %1 update-grub").arg(path);
        proc->start(cmd);
        loop.exec();
    } else {
        QMessageBox::critical(this, tr("Error"),
                              tr("Could not set up chroot environment.\nPlease double-check the selected location."));
        setCursor(QCursor(Qt::ArrowCursor));
        ui->buttonApply->setEnabled(true);
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
    ui->buttonApply->setEnabled(false);
    ui->stackedWidget->setCurrentWidget(ui->outputPage);
    QString location = QString(ui->grubBootCombo->currentText()).section(" ", 0, 0);
    QString text = QString(tr("Backing up MBR or PBR from %1 device.")).arg(location);
    ui->outputLabel->setText(text);
    setConnections(timer, proc);
    QString cmd = "dd if=/dev/" + location + " of=" + filename + " bs=446 count=1";
    proc->start(cmd);
}

// try to guess partition to install GRUB
void mxbootrepair::guessPartition()
{
    if (ui->grubMbrButton->isChecked()) {
        // find first disk with Linux partitions
        for (int index = 0; index < ui->grubBootCombo->count(); index++) {
            QString drive = ui->grubBootCombo->itemText(index);
            if (system("lsblk -ln -o PARTTYPE /dev/" + drive.section(" ", 0 ,0).toUtf8() + \
                       "| grep -qEi '0x83|0fc63daf-8483-4772-8e79-3d69d8477de4|44479540-F297-41B2-9AF7-D131D5F0458A|4F68BCE3-E8CD-4DB1-96E7-FBCAF984B709'") == 0) {
                ui->grubBootCombo->setCurrentIndex(index);
                break;
            }
        }
    }
    // find first a partition with rootMX* label
    for (int index = 0; index < ui->rootCombo->count(); index++) {
        QString part = ui->rootCombo->itemText(index);
        if (system("lsblk -ln -o LABEL /dev/" + part.section(" ", 0 ,0).toUtf8() + "| grep -q rootMX") == 0) {
            ui->rootCombo->setCurrentIndex(index);
            // select the same location by default for GRUB and /boot
            if (ui->grubRootButton->isChecked()) {
                ui->grubBootCombo->setCurrentIndex(ui->rootCombo->currentIndex());
            }
            return;
        }
    }
    // it it cannot find rootMX*, look for Linux partitions
    for (int index = 0; index < ui->rootCombo->count(); index++) {
        QString part = ui->rootCombo->itemText(index);
        if (system("lsblk -ln -o PARTTYPE /dev/" + part.section(" ", 0 ,0).toUtf8() + \
                   "| grep -qEi '0x83|0fc63daf-8483-4772-8e79-3d69d8477de4|44479540-F297-41B2-9AF7-D131D5F0458A|4F68BCE3-E8CD-4DB1-96E7-FBCAF984B709'") == 0) {
            ui->rootCombo->setCurrentIndex(index);
            break;
        }
    }
    // use by default the same root and /boot partion for installing on root
    if (ui->grubRootButton->isChecked()) {
        ui->grubBootCombo->setCurrentIndex(ui->rootCombo->currentIndex());
    }
}

void mxbootrepair::restoreBR(QString filename) {
    ui->progressBar->show();
    setCursor(QCursor(Qt::WaitCursor));
    ui->buttonCancel->setEnabled(false);
    ui->buttonApply->setEnabled(false);
    ui->stackedWidget->setCurrentWidget(ui->outputPage);
    QString location = QString(ui->grubBootCombo->currentText()).section(" ", 0, 0);
    if (QMessageBox::warning(this, tr("Warning"),
                             tr("You are going to write the content of ") + filename + tr(" to ") + location + tr("\n\nAre you sure?"),
                             tr("Yes"), tr("No")) == 1){
        refresh();
        return;
    }
    QString text = QString(tr("Restoring MBR/PBR from backup to %1 device.")).arg(location);
    ui->outputLabel->setText(text);
    setConnections(timer, proc);
    QString cmd = "dd if=" + filename + " of=/dev/" + location + " bs=446 count=1";
    proc->start(cmd);
}

// select ESP GUI items
void mxbootrepair::setEspDefaults()
{
    // remove non-ESP partitions
    for (int index = 0; index < ui->grubBootCombo->count(); index++) {
        QString part = ui->grubBootCombo->itemText(index);
        if (system("lsblk -ln -o PARTTYPE /dev/" + part.section(" ", 0 ,0).toUtf8() + "| grep -qi c12a7328-f81f-11d2-ba4b-00a0c93ec93b") != 0) {
            ui->grubBootCombo->removeItem(index);
            index--;
        }
    }
    if (ui->grubBootCombo->count() == 0) {
        QMessageBox::critical(this, tr("Error"),
                              tr("Could not find EFI system partition (ESP) on any system disks. Please create an ESP and try again."));
        ui->buttonApply->setDisabled(true);
    }
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
    ui->buttonApply->setEnabled(true);
    if (exitCode == 0) {
        if (QMessageBox::information(this, tr("Success"),
                                     tr("Process finished with success.<p><b>Do you want to exit MX Boot Repair?</b>"),
                                     tr("Yes"), tr("No")) == 0){
            qApp->exit(0);
        }
    } else {
        QMessageBox::critical(this, tr("Error"),
                              tr("Process finished. Errors have occurred."));
    }
    ui->buttonApply->setText(tr("Back"));
    ui->buttonApply->setIcon(QIcon::fromTheme("go-previous"));
}

// set proc and timer connections
void mxbootrepair::setConnections(QTimer* timer, QProcess* proc) {
    timer->disconnect();
    proc->disconnect();
    connect(timer, &QTimer::timeout, this, &mxbootrepair::procTime);
    connect(proc, &QProcess::started, this, &mxbootrepair::procStart);
    connect(proc, static_cast<void (QProcess::*)(int)>(&QProcess::finished), this, &mxbootrepair::procDone);
    connect(proc, &QProcess::readyReadStandardOutput, this, &mxbootrepair::onStdoutAvailable);
}

// add list of devices to grubBootCombo
void mxbootrepair::addDevToList() {
    QString cmd = "/bin/bash -c \"lsblk -ln -o NAME,SIZE,LABEL,MODEL -d -e 2,11 -x NAME | grep -E '^x?[h,s,v].[a-z]|^mmcblk|^nvme'\"";
    proc->start(cmd);
    proc->waitForFinished();
    QString out = proc->readAllStandardOutput();
    ListDisk = out.split("\n", QString::SkipEmptyParts);

    cmd = "/bin/bash -c \"lsblk -ln -o NAME,SIZE,FSTYPE,MOUNTPOINT,LABEL -e 2,11 -x NAME | grep -E '^x?[h,s,v].[a-z][0-9]|^mmcblk[0-9]+p|^nvme[0-9]+n[0-9]+p'\"";
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

// enabled/disable GUI elements depending on MBR, Root or ESP selection
void mxbootrepair::targetSelection() {
    ui->grubBootCombo->clear();
    ui->rootCombo->setEnabled(true);
    ui->buttonApply->setEnabled(true);
    // add only disks
    if (ui->grubMbrButton->isChecked()) {
        ui->grubBootCombo->addItems(ListDisk);
        guessPartition();
        // add partitions if select root
    } else if (ui->grubRootButton->isChecked()) {
        ui->grubBootCombo->addItems(ListPart);
        guessPartition();
        // if Esp is checked, add partitions to Location combobox
    } else {
        ui->grubBootCombo->addItems(ListPart);
        guessPartition();
        setEspDefaults();
    }
}

// update output box on Stdout
void mxbootrepair::onStdoutAvailable() {
    QString out = ui->outputBox->toPlainText() + proc->readAllStandardOutput();
    ui->outputBox->setPlainText(out);
}

// Apply button clicked
void mxbootrepair::on_buttonApply_clicked() {
    // on first page
    if (ui->stackedWidget->currentIndex() == 0) {
        targetSelection();
        // Reinstall button selected
        if (ui->reinstallRadioButton->isChecked()) {
            ui->stackedWidget->setCurrentWidget(ui->selectionPage);
            ui->bootMethodGroup->setTitle(tr("Select Boot Method"));
            ui->grubInsLabel->setText(tr("Install on:"));
            ui->grubRootButton->setText(tr("root"));
            ui->rootLabel->show();
            ui->rootCombo->show();

            // Repair button selected
        } else if (ui->repairRadioButton->isChecked()) {
            ui->stackedWidget->setCurrentWidget(ui->selectionPage);
            ui->bootMethodGroup->setTitle(tr("Select GRUB location"));
            ui->grubInsLabel->hide();
            ui->grubRootButton->hide();
            ui->grubMbrButton->hide();
            ui->grubEspButton->hide();
            ui->grubRootButton->setChecked(true);
            on_grubRootButton_clicked();

            // Backup button selected
        } else if (ui->bakRadioButton->isChecked()) {
            ui->stackedWidget->setCurrentWidget(ui->selectionPage);
            ui->bootMethodGroup->setTitle(tr("Select Item to Back Up"));
            ui->grubInsLabel->setText("");
            ui->grubRootButton->setText("PBR");
            ui->grubEspButton->hide();
            // Restore backup button selected
        } else if (ui->restoreBakRadioButton->isChecked()) {
            ui->stackedWidget->setCurrentWidget(ui->selectionPage);
            ui->bootMethodGroup->setTitle(tr("Select Item to Restore"));
            ui->grubInsLabel->setText("");
            ui->grubRootButton->setText("PBR");
            ui->grubEspButton->hide();
        }

        // on selection page
    } else if (ui->stackedWidget->currentWidget() == ui->selectionPage) {
        if (ui->reinstallRadioButton->isChecked()) {
            installGRUB();
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
                       VERSION + "</p><p align=\"center\"><h3>" +
                       tr("Simple boot repair program for MX Linux") + "</h3></p><p align=\"center\"><a href=\"http://mxlinux.org\">http://mxlinux.org</a><br /></p><p align=\"center\">" +
                       tr("Copyright (c) MX Linux") + "<br /><br /></p>", 0, this);
    QPushButton *btnLicense = msgBox.addButton(tr("License"), QMessageBox::HelpRole);
    QPushButton *btnChangelog = msgBox.addButton(tr("Changelog"), QMessageBox::HelpRole);
    QPushButton *btnCancel = msgBox.addButton(tr("Cancel"), QMessageBox::NoRole);
    btnCancel->setIcon(QIcon::fromTheme("window-close"));

    msgBox.exec();

    if (msgBox.clickedButton() == btnLicense) {
        QString exec = "xdg-open";
        QString user = getCmdOut("logname");
        if (system("command -v mx-viewer") == 0) { // use mx-viewer if available
            exec = "mx-viewer";
        }
        QString cmd = "su " + user + " -c \"" + exec + " file:///usr/share/doc/mx-bootrepair/license.html\"&";
        system(cmd.toUtf8());
    } else if (msgBox.clickedButton() == btnChangelog) {
        QDialog *changelog = new QDialog(this);
        changelog->resize(600, 500);

        QTextEdit *text = new QTextEdit;
        text->setReadOnly(true);
        text->setText(getCmdOut("zless /usr/share/doc/" + QFileInfo(QCoreApplication::applicationFilePath()).fileName()  + "/changelog.gz"));

        QPushButton *btnClose = new QPushButton(tr("&Close"));
        btnClose->setIcon(QIcon::fromTheme("window-close"));
        connect(btnClose, &QPushButton::clicked, changelog, &QDialog::close);

        QVBoxLayout *layout = new QVBoxLayout;
        layout->addWidget(text);
        layout->addWidget(btnClose);
        changelog->setLayout(layout);
        changelog->exec();
    }
    this->show();
}

// Help button clicked
void mxbootrepair::on_buttonHelp_clicked() {
    QLocale locale;
    QString lang = locale.bcp47Name();

    QString url = "https://mxlinux.org/wiki/help-files/help-mx-boot-repair";
    if (lang.startsWith("fr")) {
        url = "https://mxlinux.org/wiki/help-files/help-r%C3%A9paration-d%E2%80%99amor%C3%A7age";
    }

    QString exec = "xdg-open";
    QString user = getCmdOut("logname");
    if (system("command -v mx-viewer") == 0) { // use mx-viewer if available
        exec = "mx-viewer";
    }
    QString cmd = "su " + user + " -c \"" + exec + " " + url + "\"&";
    system(cmd.toUtf8());
}

void mxbootrepair::on_grubMbrButton_clicked()
{
    targetSelection();
}

void mxbootrepair::on_grubRootButton_clicked()
{
    targetSelection();
}

void mxbootrepair::on_grubEspButton_clicked()
{
    targetSelection();
}

