/*****************************************************************************
 * mainwindow.cpp
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
#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QDebug>
#include <QFileDialog>
#include <QInputDialog>
#include <QScrollBar>

#include "about.h"

using namespace std::chrono_literals;

MainWindow::MainWindow(QWidget *parent)
    : QDialog(parent),
      ui(new Ui::MainWindow)
{
    qDebug().noquote() << QCoreApplication::applicationName() << "version:" << QCoreApplication::applicationVersion();

    ui->setupUi(this);
    shell = new Cmd(this);

    connect(ui->buttonAbout, &QPushButton::clicked, this, &MainWindow::buttonAbout_clicked);
    connect(ui->buttonApply, &QPushButton::clicked, this, &MainWindow::buttonApply_clicked);
    connect(ui->buttonCancel, &QPushButton::clicked, this, &MainWindow::reject);
    connect(ui->buttonHelp, &QPushButton::clicked, this, &MainWindow::buttonHelp_clicked);
    connect(ui->radioGrubEsp, &QPushButton::clicked, this, &MainWindow::targetSelection);
    connect(ui->radioGrubMbr, &QPushButton::clicked, this, &MainWindow::targetSelection);
    connect(ui->radioGrubRoot, &QPushButton::clicked, this, &MainWindow::targetSelection);
    connect(shell, &Cmd::outputAvailable, [](const QString &out) { qDebug() << out.trimmed(); });
    connect(shell, &Cmd::errorAvailable, [](const QString &out) { qWarning() << out.trimmed(); });

    setWindowFlags(Qt::Window); // for the close, min and max buttons
    refresh();
    addDevToList();
    ui->radioGrubEsp->setDisabled(!isUefi());
}

MainWindow::~MainWindow()
{
    Cmd().run("/usr/lib/mx-boot-repair/mxbr-lib copy_log", nullptr, nullptr, true, false);
    delete ui;
}

void MainWindow::refresh()
{
    disableOutput();
    ui->stackedWidget->setCurrentIndex(0);
    ui->radioReinstall->setFocus();
    ui->radioReinstall->setChecked(true);
    ui->outputBox->clear();
    ui->outputLabel->clear();
    ui->grubInsLabel->show();
    ui->radioGrubRoot->show();
    ui->radioGrubMbr->show();
    ui->radioGrubEsp->show();
    ui->rootLabel->hide();
    ui->comboRoot->hide();
    ui->buttonApply->setText(tr("Next"));
    ui->buttonApply->setIcon(QIcon::fromTheme("go-next"));
    ui->buttonApply->setEnabled(true);
    ui->buttonCancel->setEnabled(true);
    ui->comboRoot->setDisabled(false);
    setCursor(QCursor(Qt::ArrowCursor));
}

void MainWindow::installGRUB()
{
    ui->buttonCancel->setEnabled(false);
    ui->buttonApply->setEnabled(false);
    ui->stackedWidget->setCurrentWidget(ui->outputPage);

    const QString location = ui->comboLocation->currentText().section(' ', 0, 0);
    const QString text = tr("GRUB is being installed on %1 device.").arg(location);
    QString root = "/dev/" + ui->comboRoot->currentText().section(' ', 0, 0);

    const QString &luks = luksMapper(root);

    if (isMountedTo(root, "/")) { // on current root
        ui->outputLabel->setText(text);
        installGRUB(location, "/", luks);
        return;
    }

    if (!luks.isEmpty()) {
        if (!openLuks(root, luks)) {
            refresh();
            return;
        }
        root = "/dev/mapper/" + luks;
    }

    ui->outputLabel->setText(text);

    // for grub-install access UEFI NVRAM entries
    if (ui->radioGrubEsp->isChecked()) {
        // ... mount efivarfs if not already mounted
        shell->runAsRoot(
            "grep -sq ^efivarfs /proc/self/mounts || "
            "{ test -d /sys/firmware/efi/efivars && mount -t efivarfs efivarfs /sys/firmware/efi/efivars; }");
        // ...  remove dump-entries if exist to avoid "No space left on device" error
        shell->runAsRoot("ls -1 /sys/firmware/efi/efivars | grep -sq ^dump && rm /sys/firmware/efi/efivars/dump*");
    }
    if (mountChrootEnv(root)) {
        if (!checkAndMountPart(tmpdir.path(), "/boot")) {
            cleanupMountPoints(tmpdir.path(), luks);
            refresh();
            return;
        }
        installGRUB(location, tmpdir.path(), luks);
        return;
    } else {
        QMessageBox::critical(this, tr("Error"),
                              tr("Could not set up chroot environment.\n"
                                 "Please double-check the selected location."));
        setCursor(QCursor(Qt::ArrowCursor));
        ui->buttonApply->setEnabled(true);
        ui->buttonCancel->setEnabled(true);
        ui->stackedWidget->setCurrentWidget(ui->selectionPage);
    }
    cleanupMountPoints(tmpdir.path(), luks);
}

void MainWindow::installGRUB(const QString &location, const QString &path, const QString &luks)
{
    QString cmd
        = QStringLiteral("chroot %1 grub-install --target=i386-pc --recheck --force /dev/%2").arg(path, location);
    if (ui->radioGrubEsp->isChecked()) {
        shell->runAsRoot("test -d " + path + "/boot/efi || mkdir " + path + "/boot/efi");
        if (!checkAndMountPart(path, "/boot/efi")) {
            cleanupMountPoints(path, luks);
            refresh();
            return;
        }
        QString arch = shell->getCmdOut("arch");
        if (arch == "i686") { // rename arch to match grub-install target
            arch = "i386";
        }
        QString release
            = shell->getCmdOut(QStringLiteral("grep -oP '(?<=DISTRIB_RELEASE=).*' /etc/lsb-release")).left(2);
        cmd = QStringLiteral("chroot %1 grub-install --target=%2-efi --efi-directory=/boot/efi "
                             "--bootloader-id=MX%3 --force-extra-removable --recheck")
                  .arg(path, arch, release);
    }
    displayOutput();
    bool success = shell->runAsRoot(cmd);
    disableOutput();
    cleanupMountPoints(path, luks);
    displayResult(success);
}

void MainWindow::repairGRUB()
{
    ui->buttonCancel->setEnabled(false);
    ui->buttonApply->setEnabled(false);
    ui->stackedWidget->setCurrentWidget(ui->outputPage);
    QString part = "/dev/" + ui->comboLocation->currentText().section(' ', 0, 0);

    if (isMountedTo(part, "/")) { // on current root
        displayOutput();
        bool ok = shell->runAsRoot("update-grub");
        disableOutput();
        displayResult(ok);
        refresh();
        return;
    }

    const QString &luks = luksMapper(part);
    if (!luks.isEmpty()) {
        if (!openLuks(part, luks)) {
            refresh();
            return;
        }
        part = "/dev/mapper/" + luks;
    }

    ui->outputLabel->setText(tr("The GRUB configuration file (grub.cfg) is being rebuilt."));
    if (mountChrootEnv(part)) {
        if (!checkAndMountPart(tmpdir.path(), "/boot")) {
            cleanupMountPoints(tmpdir.path(), luks);
            refresh();
            return;
        }
        if (QFile::exists(tmpdir.path() + "/boot/efi") && !checkAndMountPart(tmpdir.path(), "/boot/efi")) {
            cleanupMountPoints(tmpdir.path(), luks);
            refresh();
            return;
        }
        displayOutput();
        bool success = shell->runAsRoot(QStringLiteral("chroot %1 update-grub").arg(tmpdir.path()));
        disableOutput();
        cleanupMountPoints(tmpdir.path(), luks);
        displayResult(success);
        return;
    } else {
        QMessageBox::critical(this, tr("Error"),
                              tr("Could not set up chroot environment.\nPlease double-check the selected location."));
        setCursor(QCursor(Qt::ArrowCursor));
        ui->buttonApply->setEnabled(true);
        ui->buttonCancel->setEnabled(true);
        ui->stackedWidget->setCurrentWidget(ui->selectionPage);
    }
    cleanupMountPoints(tmpdir.path(), luks);
}

void MainWindow::regenerateInitramfs()
{
    ui->buttonCancel->setEnabled(false);
    ui->buttonApply->setEnabled(false);
    ui->stackedWidget->setCurrentWidget(ui->outputPage);

    QString root = "/dev/" + ui->comboRoot->currentText().section(' ', 0, 0);
    ui->outputLabel->setText(tr("Generating initramfs images on: %1").arg(root));

    const QString &luks = luksMapper(root);

    if (isMountedTo(root, "/")) { // on current root
        displayOutput();
        const bool ok = shell->procAsRoot("update-initramfs", {"-c", "-v", "-k", "all"});
        disableOutput();
        displayResult(ok);
        return;
    }

    if (!luks.isEmpty()) {
        if (!openLuks(root, luks)) {
            refresh();
            return;
        }
        root = "/dev/mapper/" + luks;
    }

    if (mountChrootEnv(root)) {
        if (!checkAndMountPart(tmpdir.path(), "/boot")) {
            cleanupMountPoints(tmpdir.path(), luks);
            refresh();
            return;
        }
        if (QFile::exists(tmpdir.path() + "/boot/efi") && !checkAndMountPart(tmpdir.path(), "/boot/efi")) {
            cleanupMountPoints(tmpdir.path(), luks);
            refresh();
            return;
        }
        displayOutput();
        const bool success = shell->procAsRoot("chroot", {tmpdir.path(), "update-initramfs", "-c", "-v", "-k", "all"});
        disableOutput();
        cleanupMountPoints(tmpdir.path(), luks);
        displayResult(success);
        return;
    } else {
        QMessageBox::critical(this, tr("Error"),
                              tr("Could not set up chroot environment.\nPlease double-check the selected location."));
        setCursor(QCursor(Qt::ArrowCursor));
    }
    cleanupMountPoints(tmpdir.path(), luks);
    refresh();
}

void MainWindow::backupBR(const QString &filename)
{
    ui->buttonCancel->setEnabled(false);
    ui->buttonApply->setEnabled(false);
    ui->stackedWidget->setCurrentWidget(ui->outputPage);
    const QString location = ui->comboLocation->currentText().section(' ', 0, 0);
    const QString text = tr("Backing up MBR or PBR from %1 device.").arg(location);
    ui->outputLabel->setText(text);
    const QString cmd = "dd if=/dev/" + location + " of=" + filename + " bs=446 count=1";
    displayOutput();
    displayResult(shell->runAsRoot(cmd));
}

// umount and clean temp folder
void MainWindow::cleanupMountPoints(const QString &path, const QString &luks)
{
    if (path == "/") {
        return;
    }
    shell->runAsRoot("mountpoint -q " + path + "/boot/efi && umount " + path + "/boot/efi");
    shell->runAsRoot("mountpoint -q " + path + "/boot && umount -R " + path + "/boot");
    const QString cmd
        = QStringLiteral("mountpoint -q %1 && /bin/umount -R %1/run && /bin/umount -R"
                         " %1/proc && /bin/umount -R %1/sys && /bin/umount -R %1/dev && umount %1 && rmdir %1")
              .arg(path);
    shell->runAsRoot(cmd);
    if (!luks.isEmpty()) {
        shell->procAsRoot("cryptsetup", {"luksClose", luks});
    }
}

// try to guess partition to install GRUB
void MainWindow::guessPartition()
{
    if (ui->radioGrubMbr->isChecked()) {
        // find first disk with Linux partitions
        for (int index = 0; index < ui->comboLocation->count(); index++) {
            QString drive = ui->comboLocation->itemText(index);
            if (shell->run("lsblk -ln -o PARTTYPE /dev/" + drive.section(' ', 0, 0)
                           + "| grep -qEi "
                             "'0x83|0fc63daf-8483-4772-8e79-3d69d8477de4|44479540-F297-41B2-9AF7-D131D5F0458A|4F68BCE3-"
                             "E8CD-4DB1-96E7-FBCAF984B709'")) {
                ui->comboLocation->setCurrentIndex(index);
                break;
            }
        }
    }
    // find first a partition with rootMX* label
    for (int index = 0; index < ui->comboRoot->count(); index++) {
        QString part = ui->comboRoot->itemText(index);
        if (shell->run("lsblk -ln -o LABEL /dev/" + part.section(' ', 0, 0) + "| grep -q rootMX")) {
            ui->comboRoot->setCurrentIndex(index);
            // select the same location by default for GRUB and /boot
            if (ui->radioGrubRoot->isChecked()) {
                ui->comboLocation->setCurrentIndex(ui->comboRoot->currentIndex());
            }
            return;
        }
    }
    // it it cannot find rootMX*, look for Linux partitions
    for (int index = 0; index < ui->comboRoot->count(); index++) {
        QString part = ui->comboRoot->itemText(index);
        if (shell->run("lsblk -ln -o PARTTYPE /dev/" + part.section(' ', 0, 0)
                       + "| grep -qEi "
                         "'0x83|0fc63daf-8483-4772-8e79-3d69d8477de4|44479540-F297-41B2-9AF7-D131D5F0458A|4F68BCE3-"
                         "E8CD-4DB1-96E7-FBCAF984B709'")) {
            ui->comboRoot->setCurrentIndex(index);
            break;
        }
    }
    // use by default the same root and /boot partion for installing on root
    if (ui->radioGrubRoot->isChecked()) {
        ui->comboLocation->setCurrentIndex(ui->comboRoot->currentIndex());
    }
}

void MainWindow::restoreBR(const QString &filename)
{
    ui->buttonCancel->setEnabled(false);
    ui->buttonApply->setEnabled(false);
    ui->stackedWidget->setCurrentWidget(ui->outputPage);
    const QString location = ui->comboLocation->currentText().section(' ', 0, 0);
    const auto ans = QMessageBox::warning(this, tr("Warning"),
                                          tr("You are going to write the content of ") + filename + tr(" to ")
                                              + location + tr("\n\nAre you sure?"),
                                          QMessageBox::Yes, QMessageBox::No);
    if (ans != QMessageBox::Yes) {
        refresh();
        return;
    }
    const QString text = tr("Restoring MBR/PBR from backup to %1 device.").arg(location);
    ui->outputLabel->setText(text);
    const QString cmd = "dd if=" + filename + " of=/dev/" + location + " bs=446 count=1";
    displayOutput();
    displayResult(shell->runAsRoot(cmd));
}

// select ESP GUI items
void MainWindow::setEspDefaults()
{
    // remove non-ESP partitions
    for (int index = 0; index < ui->comboLocation->count(); index++) {
        const QString part = ui->comboLocation->itemText(index);
        if (!shell->run("lsblk -ln -o PARTTYPE /dev/" + part.section(' ', 0, 0)
                        + "| grep -qiE 'c12a7328-f81f-11d2-ba4b-00a0c93ec93b|0xef'")) {
            ui->comboLocation->removeItem(index);
            index--;
        }
    }
    if (ui->comboLocation->count() == 0) {
        QMessageBox::critical(this, tr("Error"),
                              tr("Could not find EFI system partition (ESP) "
                                 "on any system disks. Please create an ESP and try again."));
        ui->buttonApply->setDisabled(true);
    }
}

QString MainWindow::selectPart(const QString &path, const QString &mountpoint)
{
    // read /etc/fstab on mounted path
    QFile file(path + "/etc/fstab");
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "Count not find /etc/fstab file on specified root partition";
    }

    while (!file.atEnd()) {
        const QString &line = file.readLine().simplified();
        if (line.isEmpty() || line.startsWith('#')) {
            continue; // Empty line or comment
        }

        const QStringList &fields = line.split(' ');
        if (fields.count() < 2) {
            continue;
        }
        // return device for /boot mount point
        if (fields.at(1) == mountpoint) {
            const QString &device = fields.at(0);
            QString cmd = "readlink -e \"$(echo " + device
                          + " | sed -r 's:((PART)?(UUID|LABEL))=:\\L/dev/disk/by-\\1/:g; s:[\\\"]::g;')\"";
            if (shell->runAsRoot(cmd)) {
                qDebug() << "Found partition:" << device;
                return device;
            } else {
                qDebug() << "Unknown partition:" << device;
            }
        }
    }
    file.close();

    QInputDialog dialog;
    QStringList partitions = ListPart;
    partitions.removeAll(ui->comboRoot->currentText());
    dialog.setComboBoxItems(partitions);
    dialog.setLabelText(tr("Select %1 location:").arg(mountpoint));
    dialog.setWindowTitle(this->windowTitle());

    if (dialog.exec() == QDialog::Accepted) {
        const QString &selected = dialog.textValue().simplified();
        qDebug() << "Selected entry: " << selected;
        const QString partition = "/dev/" + selected.split(' ').at(0);
        qDebug() << "Selected partition: " << partition;
        return partition;
    }
    return {};
}

void MainWindow::procStart()
{
    setCursor(QCursor(Qt::BusyCursor));
}

void MainWindow::procDone()
{
    ui->progressBar->setRange(0, 100);
    ui->progressBar->setValue(100);
    setCursor(QCursor(Qt::ArrowCursor));
    ui->buttonCancel->setEnabled(true);
    ui->buttonApply->setEnabled(true);
    ui->buttonApply->setText(tr("Back"));
    ui->buttonApply->setIcon(QIcon::fromTheme("go-previous"));
}

void MainWindow::displayOutput()
{
    ui->progressBar->setRange(0, 0);
    connect(shell, &Cmd::started, this, &MainWindow::procStart);
    connect(shell, &Cmd::outputAvailable, this, &MainWindow::outputAvailable);
    connect(shell, &Cmd::errorAvailable, this, &MainWindow::outputAvailable);
    connect(shell, &Cmd::done, this, &MainWindow::procDone);
}

void MainWindow::displayResult(bool success)
{
    if (success) {
        const auto ans = QMessageBox::information(
            this, tr("Success"), tr("Process finished with success.<p><b>Do you want to exit MX Boot Repair?</b>"),
            QMessageBox::Yes, QMessageBox::No);
        if (ans == QMessageBox::Yes) {
            QApplication::exit(EXIT_SUCCESS);
        }
    } else {
        QMessageBox::critical(this, tr("Error"), tr("Process finished. Errors have occurred."));
    }
}

void MainWindow::disableOutput()
{
    disconnect(shell, &Cmd::started, this, &MainWindow::procStart);
    disconnect(shell, &Cmd::outputAvailable, this, &MainWindow::outputAvailable);
    disconnect(shell, &Cmd::errorAvailable, this, &MainWindow::outputAvailable);
    disconnect(shell, &Cmd::done, this, &MainWindow::procDone);
}

QString MainWindow::luksMapper(const QString &part)
{
    QString mapper;
    if (!shell->procAsRoot("cryptsetup", {"isLuks", part})) {
        return {};
    }
    if (!shell->procAsRoot("cryptsetup", {"luksUUID", part}, &mapper)) {
        return {};
    }
    return "luks-" + mapper;
}
bool MainWindow::openLuks(const QString &part, const QString &mapper)
{
    bool ok = false;
    QByteArray &&pass = QInputDialog::getText(this, this->windowTitle(),
                                              tr("Enter password to unlock %1 encrypted partition:").arg(part),
                                              QLineEdit::Password, QString(), &ok)
                            .toUtf8();
    if (ok) {
        ok = !pass.isEmpty();
    }
    if (ok) {
        ok = shell->procAsRoot("cryptsetup", {"luksOpen", part, mapper, "-"}, nullptr, &pass);
    }
    pass.fill(0xA5);
    if (!ok) {
        QMessageBox::critical(this, tr("Error"), tr("Sorry, could not open %1 LUKS container").arg(part));
    }
    return ok;
}

bool MainWindow::isUefi()
{
    QDir dir("/sys/firmware/efi/efivars");
    return dir.exists() && !dir.entryList(QDir::NoDotAndDotDot | QDir::AllEntries).isEmpty();
}

// add list of devices to comboLocation
void MainWindow::addDevToList()
{
    QString cmd("lsblk -ln -o NAME,SIZE,LABEL,MODEL -d -e 2,11 -x NAME | grep -E '^x?[h,s,v].[a-z]|^mmcblk|^nvme'");
    ListDisk = shell->getCmdOut(cmd).split('\n', Qt::SkipEmptyParts);

    cmd = "lsblk -ln -o NAME,SIZE,FSTYPE,MOUNTPOINT,LABEL -e 2,11 -x NAME | grep -E "
          "'^x?[h,s,v].[a-z][0-9]|^mmcblk[0-9]+p|^nvme[0-9]+n[0-9]+p'";
    ListPart = shell->getCmdOut(cmd).split('\n', Qt::SkipEmptyParts);
    ui->comboRoot->clear();
    ui->comboRoot->addItems(ListPart);

    ui->comboLocation->clear();
    // add only disks
    if (ui->radioGrubMbr->isChecked()) {
        ui->comboLocation->addItems(ListDisk);
    } else { // add partition
        ui->comboLocation->addItems(ListPart);
    }
}

// enabled/disable GUI elements depending on MBR, Root or ESP selection
void MainWindow::targetSelection()
{
    ui->comboLocation->clear();
    ui->comboRoot->setEnabled(true);
    ui->buttonApply->setEnabled(true);
    // add only disks
    if (ui->radioGrubMbr->isChecked()) {
        ui->comboLocation->addItems(ListDisk);
        guessPartition();
        // add partitions if select root
    } else if (ui->radioGrubRoot->isChecked()) {
        ui->comboLocation->addItems(ListPart);
        guessPartition();
        // if Esp is checked, add partitions to Location combobox
    } else {
        ui->comboLocation->addItems(ListPart);
        guessPartition();
        setEspDefaults();
    }
}

void MainWindow::outputAvailable(const QString &output)
{
    //    if (output.contains('\r')) {
    //        ui->outputBox->moveCursor(QTextCursor::Up, QTextCursor::KeepAnchor);
    //        ui->outputBox->moveCursor(QTextCursor::EndOfLine, QTextCursor::KeepAnchor);
    //    }
    ui->outputBox->insertPlainText(output);
    ui->outputBox->verticalScrollBar()->setValue(ui->outputBox->verticalScrollBar()->maximum());
}

void MainWindow::buttonApply_clicked()
{
    const int currentIndex = ui->stackedWidget->currentIndex();
    if (currentIndex == 0) {
        targetSelection();
        ui->stackedWidget->setCurrentWidget(ui->selectionPage);

        if (ui->radioReinstall->isChecked()) {
            ui->bootMethodGroup->setTitle(tr("Select Boot Method"));
            ui->locationLabel->setText(tr("Location:"));
            ui->grubInsLabel->setText(tr("Install on:"));
            ui->radioGrubRoot->setText(tr("root"));
            ui->rootLabel->show();
            ui->comboRoot->show();
        } else if (ui->raidoRepair->isChecked()) {
            ui->bootMethodGroup->setTitle(tr("Select GRUB location"));
            ui->locationLabel->setText(tr("Select root location:"));
            ui->grubInsLabel->hide();
            ui->radioGrubRoot->hide();
            ui->radioGrubMbr->hide();
            ui->radioGrubEsp->hide();
            ui->radioGrubRoot->setChecked(true);
            targetSelection();
        } else if (ui->radioRegenerateInitramfs->isChecked()) {
            ui->bootMethodGroup->setTitle(tr("Select initramfs options"));
            ui->locationLabel->hide();
            ui->comboLocation->hide();
            ui->grubInsLabel->hide();
            ui->radioGrubRoot->hide();
            ui->radioGrubMbr->hide();
            ui->radioGrubEsp->hide();
            ui->rootLabel->show();
            ui->comboRoot->show();
        } else if (ui->radioBak->isChecked()) {
            ui->bootMethodGroup->setTitle(tr("Select Item to Back Up"));
            ui->grubInsLabel->clear();
            ui->radioGrubRoot->setText("PBR");
            ui->radioGrubEsp->hide();
        } else if (ui->radioRestoreBak->isChecked()) {
            ui->bootMethodGroup->setTitle(tr("Select Item to Restore"));
            ui->grubInsLabel->clear();
            ui->radioGrubRoot->setText("PBR");
            ui->radioGrubEsp->hide();
        }
        ui->buttonApply->setText(tr("Apply"));
        ui->buttonApply->setIcon(QIcon::fromTheme("dialog-ok"));
    } else if (currentIndex == ui->stackedWidget->indexOf(ui->selectionPage)) {
        if (ui->radioReinstall->isChecked()) {
            if (ui->comboLocation->currentText().isEmpty()) {
                QMessageBox::critical(this, tr("Error"), tr("No location was selected."));
                return;
            }
            if (ui->comboRoot->currentText().isEmpty()) {
                QMessageBox::critical(this, tr("Error"),
                                      tr("Please select the root partition of the system you want to fix."));
                return;
            }
            installGRUB();
        } else if (ui->raidoRepair->isChecked()) {
            if (ui->comboLocation->currentText().isEmpty()) {
                QMessageBox::critical(this, tr("Error"), tr("No location was selected."));
                return;
            }
            repairGRUB();
        } else if (ui->radioRegenerateInitramfs->isChecked()) {
            if (ui->comboRoot->currentText().isEmpty()) {
                QMessageBox::critical(this, tr("Error"),
                                      tr("Please select the root partition of the system you want to fix."));
                return;
            }
            regenerateInitramfs();
        } else if (ui->radioBak->isChecked()) {
            QString filename = QFileDialog::getSaveFileName(this, tr("Select backup file name"));
            if (filename.isEmpty()) {
                QMessageBox::critical(this, tr("Error"), tr("No file was selected."));
                return;
            }
            backupBR(filename);
        } else if (ui->radioRestoreBak->isChecked()) {
            QString filename = QFileDialog::getOpenFileName(this, tr("Select MBR or PBR backup file"));
            if (filename.isEmpty()) {
                QMessageBox::critical(this, tr("Error"), tr("No file was selected."));
                return;
            }
            restoreBR(filename);
        }
    } else if (currentIndex == ui->stackedWidget->indexOf(ui->outputPage)) {
        refresh();
    } else {
        QApplication::exit(EXIT_SUCCESS);
    }
}

void MainWindow::buttonAbout_clicked()
{
    this->hide();
    displayAboutMsgBox(
        tr("About %1").arg(QApplication::applicationDisplayName()),
        "<p align=\"center\"><b><h2>" + QApplication::applicationDisplayName() + "</h2></b></p><p align=\"center\">"
            + tr("Version: ") + QCoreApplication::applicationVersion() + "</p><p align=\"center\"><h3>"
            + tr("Simple boot repair program for MX Linux")
            + R"(</h3></p><p align="center"><a href="http://mxlinux.org">http://mxlinux.org</a><br /></p><p align="center">)"
            + tr("Copyright (c) MX Linux") + "<br /><br /></p>",
        "/usr/share/doc/mx-bootrepair/license.html", tr("%1 License").arg(QApplication::applicationDisplayName()));
    this->show();
}

void MainWindow::buttonHelp_clicked()
{
    QLocale locale;
    const QString lang = locale.bcp47Name();

    QString url("/usr/share/doc/mx-bootrepair/mx-boot-repair.html");
    if (lang.startsWith(QLatin1String("fr"))) {
        url = "https://mxlinux.org/wiki/help-files/help-r%C3%A9paration-d%E2%80%99amor%C3%A7age";
    }
    displayDoc(url, tr("%1 Help").arg(this->windowTitle()));
}

bool MainWindow::isMountedTo(const QString &volume, const QString &mount)
{
    QString points;
    if (!shell->proc("lsblk", {"-nro", "MOUNTPOINTS", volume}, &points)) {
        shell->proc("lsblk", {"-nro", "MOUNTPOINT", volume}, &points);
    }
    return points.split('\n', Qt::SkipEmptyParts).contains(mount);
}

bool MainWindow::checkAndMountPart(const QString &path, const QString &mountpoint)
{
    if (!shell->runAsRoot("test -n \"$(ls -A " + path + mountpoint + ")\"")) {
        const QString part = selectPart(path, mountpoint);
        if (!shell->runAsRoot("mount " + part + ' ' + path + mountpoint)) {
            QMessageBox::critical(this, tr("Error"), tr("Sorry, could not mount %1 partition").arg(mountpoint));
            return false;
        }
    }
    return true;
}

bool MainWindow::mountChrootEnv(const QString &path)
{
    // create a temp folder and mount dev sys proc
    if (!tmpdir.isValid()) {
        QMessageBox::critical(this, tr("Error"), tr("Could not create a temporary folder"));
        return false;
    }
    // create a temp folder and mount dev sys proc; mount run as tmpfs
    if (!QFile::exists(tmpdir.path())) {
        shell->runAsRoot(QStringLiteral("mkdir -p %1").arg(tmpdir.path()));
    }

    QString cmd = QStringLiteral("/bin/mount %1 %2 && /bin/mount --rbind --make-rslave /dev %2/dev && "
                                 "/bin/mount --rbind --make-rslave /sys %2/sys && /bin/mount --rbind /proc %2/proc && "
                                 "/bin/mount -t tmpfs -o "
                                 "size=100m,nodev,mode=755 tmpfs %2/run && /bin/mkdir %2/run/udev && "
                                 "/bin/mount --rbind /run/udev %2/run/udev")
                      .arg(path, tmpdir.path());
    return shell->runAsRoot(cmd);
}
