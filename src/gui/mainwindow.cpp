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

#include <QApplication>
#include <QDebug>
#include <QEventLoop>
#include <QFileDialog>
#include <QInputDialog>
#include <QScrollBar>

#include "about.h"
#include "core/app_init.h"

using namespace std::chrono_literals;

namespace
{
void setBusyCursor()
{
    if (QApplication::overrideCursor()) {
        QApplication::changeOverrideCursor(QCursor(Qt::BusyCursor));
    } else {
        QApplication::setOverrideCursor(QCursor(Qt::BusyCursor));
    }
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
}

void clearBusyCursor()
{
    while (QApplication::overrideCursor()) {
        QApplication::restoreOverrideCursor();
    }
}

bool isPreferredRootCandidate(BootRepairEngine *engine, const QString &part)
{
    const QString device = part.section(' ', 0, 0);
    const QString fstype = engine->filesystemType(device).trimmed();
    if (fstype.compare(QStringLiteral("exfat"), Qt::CaseInsensitive) == 0) {
        return false;
    }

    const QString label = engine->partitionLabel(device).trimmed();
    if (label.compare(QStringLiteral("boot"), Qt::CaseInsensitive) == 0) {
        return false;
    }

    return true;
}
} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QDialog(parent),
      progressTimer(new QTimer(this)),
      ui(new Ui::MainWindow)
{
    qDebug().noquote() << QCoreApplication::applicationName() << "version:" << QCoreApplication::applicationVersion();

    ui->setupUi(this);
    engine = new BootRepairEngine(this);
    progressTimer->setInterval(250);

    connect(progressTimer, &QTimer::timeout, this, [this] {
        const int currentValue = ui->progressBar->value();
        if (currentValue >= 90) {
            return;
        }

        const int increment = qMax(1, (95 - currentValue) / 10);
        ui->progressBar->setValue(qMin(90, currentValue + increment));
        ui->progressBar->repaint();
    });

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
    if (isUefi()) {
        ui->radioGrubEsp->setChecked(true);
        ui->radioGrubMbr->setChecked(false);
    }
    ui->radioGrubEsp->setDisabled(!isUefi());
    addDevToList();
    guessPartition();
}

MainWindow::~MainWindow()
{
    if (AppInit::shouldPersistLog()) {
        Cmd().copyLogAsRoot(QuietMode::Yes);
    }
    delete ui;
}

void MainWindow::refresh()
{
    disableOutput();
    progressTimer->stop();
    ui->stackedWidget->setCurrentIndex(0);
    ui->radioReinstall->setFocus();
    ui->radioReinstall->setChecked(true);
    ui->outputBox->clear();
    ui->outputLabel->clear();
    ui->progressBar->setRange(0, 100);
    ui->progressBar->setValue(0);
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
    clearBusyCursor();
    setCursor(QCursor(Qt::ArrowCursor));
}

bool MainWindow::handleElevationFailure()
{
    if (!engine->lastFailureWasElevation()) {
        return false;
    }

    QMessageBox::critical(this, tr("Authorization Failed"), tr("Could not obtain administrator privileges."));
    refresh();
    return true;
}

void MainWindow::installGRUB()
{
    BootRepairOptions opt;
    opt.location = ui->comboLocation->currentText().section(' ', 0, 0);
    opt.root = "/dev/" + ui->comboRoot->currentText().section(' ', 0, 0);
    opt.target = ui->radioGrubEsp->isChecked() ? GrubTarget::Esp
                                               : (ui->radioGrubRoot->isChecked() ? GrubTarget::Root : GrubTarget::Mbr);

    if (!engine->isMounted(opt.root, "/")) {
        const bool isLuks = engine->isLuks(opt.root);
        if (handleElevationFailure()) {
            return;
        }
        if (isLuks) {
            bool ok = false;
            const QByteArray pass
                = QInputDialog::getText(this, this->windowTitle(),
                                        tr("Enter password to unlock %1 encrypted partition:").arg(opt.root),
                                        QLineEdit::Password, QString(), &ok)
                      .toUtf8();
            if (!ok) {
                return;
            }
            setBusyCursor();
            if (!engine->canUnlockLuks(opt.root, pass)) {
                if (handleElevationFailure()) {
                    return;
                }
                clearBusyCursor();
                QMessageBox::critical(this, tr("Error"),
                                      tr("Could not unlock %1. Please check the password and try again.")
                                          .arg(opt.root));
                return;
            }
            opt.luksPassword = pass;
        }
        opt.bootDevice = engine->resolveFstabDevice(opt.root, "/boot", opt.luksPassword);
        if (handleElevationFailure()) {
            return;
        }
        if (opt.target == GrubTarget::Esp) {
            const QString espDev = engine->resolveFstabDevice(opt.root, "/boot/efi", opt.luksPassword);
            if (handleElevationFailure()) {
                return;
            }
            opt.espDevice = !espDev.isEmpty() ? espDev : "/dev/" + opt.location;
        }
    } else if (opt.target == GrubTarget::Esp) {
        opt.espDevice = "/dev/" + opt.location;
    }

    ui->buttonCancel->setEnabled(false);
    ui->buttonApply->setEnabled(false);
    ui->stackedWidget->setCurrentWidget(ui->outputPage);
    ui->outputLabel->setText(tr("GRUB is being installed on %1 device.").arg(opt.location));
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

    displayOutput();
    const bool success = engine->installGrub(opt);
    disableOutput();
    displayResult(success);
}

// legacy installGRUB helper removed; engine handles execution

void MainWindow::repairGRUB()
{
    const QString root = "/dev/" + ui->comboLocation->currentText().section(' ', 0, 0);
    BootRepairOptions opt;
    opt.root = root;
    if (!engine->isMounted(opt.root, "/")) {
        const bool isLuks = engine->isLuks(opt.root);
        if (handleElevationFailure()) {
            return;
        }
        if (isLuks) {
            bool ok = false;
            const QByteArray pass
                = QInputDialog::getText(this, this->windowTitle(),
                                        tr("Enter password to unlock %1 encrypted partition:").arg(opt.root),
                                        QLineEdit::Password, QString(), &ok)
                      .toUtf8();
            if (!ok) {
                return;
            }
            setBusyCursor();
            if (!engine->canUnlockLuks(opt.root, pass)) {
                if (handleElevationFailure()) {
                    return;
                }
                clearBusyCursor();
                QMessageBox::critical(this, tr("Error"),
                                      tr("Could not unlock %1. Please check the password and try again.")
                                          .arg(opt.root));
                return;
            }
            opt.luksPassword = pass;
        }
        opt.bootDevice = engine->resolveFstabDevice(opt.root, "/boot", opt.luksPassword);
        if (handleElevationFailure()) {
            return;
        }
        opt.espDevice = engine->resolveFstabDevice(opt.root, "/boot/efi", opt.luksPassword);
        if (handleElevationFailure()) {
            return;
        }
    }
    ui->buttonCancel->setEnabled(false);
    ui->buttonApply->setEnabled(false);
    ui->stackedWidget->setCurrentWidget(ui->outputPage);
    ui->outputLabel->setText(tr("The GRUB configuration file (grub.cfg) is being rebuilt."));
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    displayOutput();
    const bool success = engine->repairGrub(opt);
    disableOutput();
    displayResult(success);
}

void MainWindow::regenerateInitramfs()
{
    BootRepairOptions opt;
    opt.root = "/dev/" + ui->comboRoot->currentText().section(' ', 0, 0);
    if (!engine->isMounted(opt.root, "/")) {
        const bool isLuks = engine->isLuks(opt.root);
        if (handleElevationFailure()) {
            return;
        }
        if (isLuks) {
            bool ok = false;
            const QByteArray pass
                = QInputDialog::getText(this, this->windowTitle(),
                                        tr("Enter password to unlock %1 encrypted partition:").arg(opt.root),
                                        QLineEdit::Password, QString(), &ok)
                      .toUtf8();
            if (!ok) {
                return;
            }
            setBusyCursor();
            if (!engine->canUnlockLuks(opt.root, pass)) {
                if (handleElevationFailure()) {
                    return;
                }
                clearBusyCursor();
                QMessageBox::critical(this, tr("Error"),
                                      tr("Could not unlock %1. Please check the password and try again.")
                                          .arg(opt.root));
                return;
            }
            opt.luksPassword = pass;
        }
        opt.bootDevice = engine->resolveFstabDevice(opt.root, "/boot", opt.luksPassword);
        if (handleElevationFailure()) {
            return;
        }
    }
    ui->buttonCancel->setEnabled(false);
    ui->buttonApply->setEnabled(false);
    ui->stackedWidget->setCurrentWidget(ui->outputPage);
    ui->outputLabel->setText(tr("Generating initramfs images on: %1").arg(opt.root));
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
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
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
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
        if (isPreferredRootCandidate(engine, part) && engine->labelContains(part.section(' ', 0, 0), "rootMX")) {
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
        if (isPreferredRootCandidate(engine, part) && engine->isLinuxPartitionType(part.section(' ', 0, 0))) {
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
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
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
        return;
    }
    // Pre-select the ESP currently mounted at /boot/efi
    const QString mountedEsp = engine->mountSource("/boot/efi");
    if (!mountedEsp.isEmpty()) {
        const QString devName = mountedEsp.startsWith("/dev/") ? mountedEsp.mid(5) : mountedEsp;
        for (int i = 0; i < ui->comboLocation->count(); ++i) {
            if (ui->comboLocation->itemText(i).section(' ', 0, 0) == devName) {
                ui->comboLocation->setCurrentIndex(i);
                break;
            }
        }
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
    progressTimer->start();
    setBusyCursor();
}

void MainWindow::procDone()
{
    progressTimer->stop();
    ui->progressBar->setRange(0, 100);
    ui->progressBar->setValue(100);
    ui->progressBar->repaint();
    clearBusyCursor();
    setCursor(QCursor(Qt::ArrowCursor));
    ui->buttonCancel->setEnabled(true);
    ui->buttonApply->setEnabled(true);
    ui->buttonApply->setText(tr("Back"));
    ui->buttonApply->setIcon(QIcon::fromTheme("go-previous"));
}

void MainWindow::displayOutput()
{
    ui->progressBar->setRange(0, 100);
    ui->progressBar->setValue(5);
    ui->progressBar->repaint();
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
        if (engine->lastFailureWasElevation()) {
            QMessageBox::critical(this, tr("Authorization Failed"),
                                  tr("Could not obtain administrator privileges."));
            refresh();
            return;
        }
        QMessageBox::critical(this, tr("Error"), tr("Process finished. Errors have occurred."));
    }
}

void MainWindow::disableOutput()
{
    progressTimer->stop();
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

    const int lineCount = qMax(1, output.count('\n'));
    const int increment = qMin(12, qMax(4, lineCount * 2));
    const int nextValue = qMin(95, ui->progressBar->value() + increment);
    ui->progressBar->setValue(nextValue);
    ui->progressBar->repaint();
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
    QString helpPath = QStringLiteral("/usr/share/doc/mx-bootrepair/mx-boot-repair.html");
    if (qEnvironmentVariable("LANG").startsWith(QLatin1String("fr"))) {
        helpPath = QStringLiteral("/usr/share/doc/mx-bootrepair/mx-boot-repair-fr.html");
    }

    displayHelpDoc(helpPath, tr("%1 Help").arg(this->windowTitle()));
}

bool MainWindow::isMountedTo(const QString &volume, const QString &mount)
{
    return engine->isMounted(volume, mount);
}

// chroot mounting now handled in engine
