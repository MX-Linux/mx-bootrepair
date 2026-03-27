#include "about.h"

#include <QCoreApplication>
#include <QDialog>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QStringList>
#include <QTextDocument>
#include <QTextBrowser>
#include <QTextEdit>
#include <QUrl>
#include <QVBoxLayout>

namespace
{
void setupDocDialog(QDialog &dialog, QTextBrowser *browser, const QString &title, bool largeWindow)
{
    dialog.setWindowTitle(title);
    if (largeWindow) {
        dialog.setWindowFlags(Qt::Window);
        dialog.resize(1000, 800);
    } else {
        dialog.resize(700, 600);
    }

    browser->setOpenExternalLinks(true);

    auto *btnClose = new QPushButton(QObject::tr("&Close"), &dialog);
    btnClose->setIcon(QIcon::fromTheme(QStringLiteral("window-close")));
    QObject::connect(btnClose, &QPushButton::clicked, &dialog, &QDialog::close);

    auto *layout = new QVBoxLayout(&dialog);
    layout->addWidget(browser);
    layout->addWidget(btnClose);
}

void showHtmlDoc(const QString &url, const QString &title, bool largeWindow, const QString &prefixHtml = {})
{
    QDialog dialog;
    auto *browser = new QTextBrowser(&dialog);
    setupDocDialog(dialog, browser, title, largeWindow);

    const QUrl sourceUrl = QUrl::fromUserInput(url);
    const QString localPath = sourceUrl.isLocalFile() ? sourceUrl.toLocalFile() : url;
    if (!prefixHtml.isEmpty() && sourceUrl.isLocalFile() && QFileInfo::exists(localPath)) {
        QFile file(localPath);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            browser->document()->setBaseUrl(QUrl::fromLocalFile(localPath));
            browser->setHtml(prefixHtml + QString::fromUtf8(file.readAll()));
        } else {
            browser->setText(QObject::tr("Could not load %1").arg(url));
        }
    } else if (sourceUrl.isLocalFile() ? QFileInfo::exists(localPath) : QFileInfo::exists(url)) {
        browser->setSource(sourceUrl.isLocalFile() ? sourceUrl : QUrl::fromLocalFile(url));
    } else {
        browser->setText(QObject::tr("Could not load %1").arg(url));
    }

    dialog.exec();
}

QString loadChangelogText()
{
    const QString appName = QFileInfo(QCoreApplication::applicationFilePath()).fileName();
    const QStringList changelogPaths = {
        QStringLiteral("/usr/share/doc/mx-bootrepair/changelog.gz"),
        QStringLiteral("/usr/share/doc/mx-bootrepair/changelog"),
        QStringLiteral("/usr/share/doc/%1/changelog.gz").arg(appName),
        QStringLiteral("/usr/share/doc/%1/changelog").arg(appName),
    };

    for (const QString &path : changelogPaths) {
        if (!QFileInfo::exists(path)) {
            continue;
        }

        if (path.endsWith(QStringLiteral(".gz"))) {
            QProcess proc;
            proc.start(QStringLiteral("zcat"), {path}, QIODevice::ReadOnly);
            if (proc.waitForStarted(3000) && proc.waitForFinished(3000) && proc.exitStatus() == QProcess::NormalExit
                && proc.exitCode() == 0) {
                return QString::fromUtf8(proc.readAllStandardOutput());
            }
            continue;
        }

        QFile file(path);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            return QString::fromUtf8(file.readAll());
        }
    }

    return QObject::tr("Could not load changelog.");
}
} // namespace

void displayDoc(const QString &url, const QString &title, bool largeWindow)
{
    showHtmlDoc(url, title, largeWindow);
}

void displayHelpDoc(const QString &path, const QString &title)
{
    showHtmlDoc(path, title, true);
}

void displayAboutMsgBox(const QString &title, const QString &message, const QString &licenceUrl,
                        const QString &licenseTitle)
{
    const auto width = 600;
    const auto height = 500;
    QMessageBox msgBox(QMessageBox::NoIcon, title, message);
    auto *btnLicense = msgBox.addButton(QObject::tr("License"), QMessageBox::HelpRole);
    auto *btnChangelog = msgBox.addButton(QObject::tr("Changelog"), QMessageBox::HelpRole);
    auto *btnCancel = msgBox.addButton(QObject::tr("Cancel"), QMessageBox::NoRole);
    btnCancel->setIcon(QIcon::fromTheme(QStringLiteral("window-close")));

    msgBox.exec();

    if (msgBox.clickedButton() == btnLicense) {
        displayDoc(licenceUrl, licenseTitle);
    } else if (msgBox.clickedButton() == btnChangelog) {
        QDialog changelog;
        changelog.setWindowTitle(QObject::tr("Changelog"));
        changelog.resize(width, height);

        auto *text = new QTextEdit(&changelog);
        text->setReadOnly(true);
        text->setText(loadChangelogText());

        auto *btnClose = new QPushButton(QObject::tr("&Close"), &changelog);
        btnClose->setIcon(QIcon::fromTheme(QStringLiteral("window-close")));
        QObject::connect(btnClose, &QPushButton::clicked, &changelog, &QDialog::close);

        auto *layout = new QVBoxLayout(&changelog);
        layout->addWidget(text);
        layout->addWidget(btnClose);
        changelog.exec();
    }
}
