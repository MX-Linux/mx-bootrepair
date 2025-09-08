/*****************************************************************************
 * mainwindow.cpp
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
    engine = new BootRepairEngine(this);

    connect(ui->buttonAbout, &QPushButton::clicked, this, &MainWindow::buttonAbout_clicked);
    connect(ui->buttonApply, &QPushButton::clicked, this, &MainWindow::buttonApply_clicked);
    connect(ui->buttonCancel, &QPushButton::clicked, this, &MainWindow::reject);
    connect(ui->buttonHelp, &QPushButton::clicked, this, &MainWindow::buttonHelp_clicked);
    connect(ui->radioGrubEsp, &QPushButton::clicked, this, &MainWindow::targetSelection);
    connect(ui->radioGrubMbr, &QPushButton::clicked, this, &MainWindow::targetSelection);
    connect(ui->radioGrubRoot, &QPushButton::clicked, this, &MainWindow::targetSelection);
    // Cmd logging is handled via BootRepairEngine::log when displaying output

    setWindowFlags(Qt::Window); // for the close, min and max buttons
    refresh();
    addDevToList();
    ui->radioGrubEsp->setDisabled(!isUefi());
}

MainWindow::~MainWindow()
{
    Cmd().runAsRoot("/usr/lib/mx-boot-repair/mxbr-lib copy_log", nullptr, nullptr, true);
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

    BootRepairOptions opt;
    opt.location = ui->comboLocation->currentText().section(' ', 0, 0);
    opt.root = "/dev/" + ui->comboRoot->currentText().section(' ', 0, 0);
    opt.target = ui->radioGrubEsp->isChecked() ? GrubTarget::Esp
                                               : (ui->radioGrubRoot->isChecked() ? GrubTarget::Root : GrubTarget::Mbr);

    ui->outputLabel->setText(tr("GRUB is being installed on %1 device.").arg(opt.location));

    if (!engine->isMounted(opt.root, "/")) {
        if (engine->isLuks(opt.root)) {
            bool ok = false;
            const QByteArray pass
                = QInputDialog::getText(this, this->windowTitle(),
                                        tr("Enter password to unlock %1 encrypted partition:").arg(opt.root),
                                        QLineEdit::Password, QString(), &ok)
                      .toUtf8();
            if (!ok) {
                refresh();
                return;
            }
            opt.luksPassword = pass;
        }
        const QString bootDev = engine->resolveFstabDevice(opt.root, "/boot", opt.luksPassword);
        opt.bootDevice = !bootDev.isEmpty() ? bootDev : selectPartFromList("/boot");
        if (opt.target == GrubTarget::Esp) {
            const QString espDev = engine->resolveFstabDevice(opt.root, "/boot/efi", opt.luksPassword);
            opt.espDevice = !espDev.isEmpty() ? espDev : selectPartFromList("/boot/efi");
        }
    }

    displayOutput();
    const bool success = engine->installGrub(opt);
    disableOutput();
    displayResult(success);
}

// legacy installGRUB helper removed; engine handles execution

void MainWindow::repairGRUB()
{
    ui->buttonCancel->setEnabled(false);
    ui->buttonApply->setEnabled(false);
    ui->stackedWidget->setCurrentWidget(ui->outputPage);
    const QString root = "/dev/" + ui->comboLocation->currentText().section(' ', 0, 0);
    ui->outputLabel->setText(tr("The GRUB configuration file (grub.cfg) is being rebuilt."));
    BootRepairOptions opt;
    opt.root = root;
    if (!engine->isMounted(opt.root, "/")) {
        if (engine->isLuks(opt.root)) {
            bool ok = false;
            const QByteArray pass
                = QInputDialog::getText(this, this->windowTitle(),
                                        tr("Enter password to unlock %1 encrypted partition:").arg(opt.root),
                                        QLineEdit::Password, QString(), &ok)
                      .toUtf8();
            if (!ok) {
                refresh();
                return;
            }
            opt.luksPassword = pass;
        }
        opt.bootDevice = engine->resolveFstabDevice(opt.root, "/boot", opt.luksPassword);
        opt.espDevice = engine->resolveFstabDevice(opt.root, "/boot/efi", opt.luksPassword);
    }
    displayOutput();
    const bool success = engine->repairGrub(opt);
    disableOutput();
    displayResult(success);
}

void MainWindow::regenerateInitramfs()
{
    ui->buttonCancel->setEnabled(false);
    ui->buttonApply->setEnabled(false);
    ui->stackedWidget->setCurrentWidget(ui->outputPage);

    BootRepairOptions opt;
    opt.root = "/dev/" + ui->comboRoot->currentText().section(' ', 0, 0);
    ui->outputLabel->setText(tr("Generating initramfs images on: %1").arg(opt.root));
    if (!engine->isMounted(opt.root, "/")) {
        if (engine->isLuks(opt.root)) {
            bool ok = false;
            const QByteArray pass
                = QInputDialog::getText(this, this->windowTitle(),
                                        tr("Enter password to unlock %1 encrypted partition:").arg(opt.root),
                                        QLineEdit::Password, QString(), &ok)
                      .toUtf8();
            if (!ok) {
                refresh();
                return;
            }
            opt.luksPassword = pass;
        }
        opt.bootDevice = engine->resolveFstabDevice(opt.root, "/boot", opt.luksPassword);
    }
    displayOutput();
    const bool success = engine->regenerateInitramfs(opt);
    disableOutput();
    displayResult(success);
}

void MainWindow::backupBR(const QString &filename)
{
    ui->buttonCancel->setEnabled(false);
    ui->buttonApply->setEnabled(false);
    ui->stackedWidget->setCurrentWidget(ui->outputPage);
    const QString location = ui->comboLocation->currentText().section(' ', 0, 0);
    const QString text = tr("Backing up MBR or PBR from %1 device.").arg(location);
    ui->outputLabel->setText(text);
    displayOutput();
    BootRepairOptions opt;
    opt.location = location;
    opt.backupPath = filename;
    const bool ok = engine->backup(opt);
    disableOutput();
    displayResult(ok);
}

// try to guess partition to install GRUB
void MainWindow::guessPartition()
{
    if (ui->radioGrubMbr->isChecked()) {
        // find first disk with Linux partitions
        for (int index = 0; index < ui->comboLocation->count(); index++) {
            QString drive = ui->comboLocation->itemText(index);
            if (engine->isLinuxPartitionType(drive.section(' ', 0, 0))) {
                ui->comboLocation->setCurrentIndex(index);
                break;
            }
        }
    }
    // find first a partition with rootMX* label
    for (int index = 0; index < ui->comboRoot->count(); index++) {
        QString part = ui->comboRoot->itemText(index);
        if (engine->labelContains(part.section(' ', 0, 0), "rootMX")) {
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
        if (engine->isLinuxPartitionType(part.section(' ', 0, 0))) {
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
    displayOutput();
    BootRepairOptions opt;
    opt.location = location;
    opt.backupPath = filename;
    const bool ok = engine->restore(opt);
    disableOutput();
    displayResult(ok);
}

// select ESP GUI items
void MainWindow::setEspDefaults()
{
    // remove non-ESP partitions
    for (int index = 0; index < ui->comboLocation->count(); index++) {
        const QString part = ui->comboLocation->itemText(index);
        if (!engine->isEspPartition(part.section(' ', 0, 0))) {
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

QString MainWindow::selectPartFromList(const QString &mountpoint)
{
    QInputDialog dialog;
    QStringList partitions = ListPart;
    partitions.removeAll(ui->comboRoot->currentText());
    dialog.setComboBoxItems(partitions);
    dialog.setLabelText(tr("Select %1 location:").arg(mountpoint));
    dialog.setWindowTitle(this->windowTitle());

    if (dialog.exec() == QDialog::Accepted) {
        const QString &selected = dialog.textValue().simplified();
        const QString partition = "/dev/" + selected.split(' ').at(0);
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
    connect(engine, &BootRepairEngine::log, this, &MainWindow::outputAvailable);
    connect(engine, &BootRepairEngine::finished, this, &MainWindow::procDone);
    procStart();
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
    disconnect(engine, &BootRepairEngine::log, this, &MainWindow::outputAvailable);
    disconnect(engine, &BootRepairEngine::finished, this, &MainWindow::procDone);
}

// luks helpers now handled in engine

bool MainWindow::isUefi()
{
    QDir dir("/sys/firmware/efi/efivars");
    return dir.exists() && !dir.entryList(QDir::NoDotAndDotDot | QDir::AllEntries).isEmpty();
}

// add list of devices to comboLocation
void MainWindow::addDevToList()
{
    ListDisk = engine->listDisks();
    ListPart = engine->listPartitions();
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
    // echo to terminal/log via Qt's message handler
    qInfo().noquote() << output;
    // ensure visible with newlines in the GUI
    const bool endsNl = !output.isEmpty() && (output.endsWith('\n') || output.endsWith("\r\n"));
    ui->outputBox->insertPlainText(endsNl ? output : output + '\n');
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
    return engine->isMounted(volume, mount);
}

// chroot mounting now handled in engine
