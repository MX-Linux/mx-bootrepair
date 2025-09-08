#include "cli/controller.h"

#include <QCoreApplication>
#include <QTextStream>
#include <QCommandLineParser>

#include "core/bootrepair_engine.h"

namespace {
int askIndex(const QStringList& items, const QString& prompt, QTextStream& in, QTextStream& out, bool allowBack)
{
    for (int i = 0; i < items.size(); ++i) {
        out << i << ") " << items.at(i) << '\n';
    }
    if (allowBack) {
        out << prompt << " [index, or 'b' to go back]: " << Qt::flush;
    } else {
        out << prompt << " [index]: " << Qt::flush;
    }
    for (;;) {
        bool ok = false;
        const QString line = in.readLine().trimmed();
        if (allowBack && (line.compare("b", Qt::CaseInsensitive) == 0)) { out << '\n'; return -1; }
        const int idx = line.toInt(&ok);
        if (ok && idx >= 0 && idx < items.size()) { out << '\n'; return idx; }
        out << "Invalid index. Try again: " << Qt::flush;
    }
}

int askInt(int min, int max, const QString& prompt, QTextStream& in, QTextStream& out, bool allowBack = false)
{
    if (allowBack) {
        out << prompt << " [" << min << "-" << max << ", or 'b' to go back]: " << Qt::flush;
    } else {
        out << prompt << " [" << min << "-" << max << "]: " << Qt::flush;
    }
    for (;;) {
        bool ok = false;
        const QString line = in.readLine().trimmed();
        if (allowBack && (line.compare("b", Qt::CaseInsensitive) == 0)) { out << '\n'; return min - 1; } // back
        const int val = line.toInt(&ok);
        if (ok && val >= min && val <= max) { out << '\n'; return val; }
        out << "Invalid selection. Enter a number " << min << "-" << max << ": " << Qt::flush;
    }
}
}

CliController::CliController(QObject* parent) : QObject(parent) {}

int CliController::run()
{
    QTextStream out(stdout);
    QTextStream in(stdin);

    BootRepairEngine engine;
    // Log everything to file via Qt logging, but only print command OUTPUT to terminal (not the command lines)
    QObject::connect(&engine, &BootRepairEngine::log, &engine, [&](const QString& s) {
        qInfo().noquote() << s; // always to log file
        if (!(s.startsWith("$ ") || s.startsWith("# ") || s.startsWith("[dry-run]"))) {
            out << s; if (!s.endsWith('\n')) out << '\n'; out.flush();
        }
    });
    QObject::connect(&engine, &BootRepairEngine::finished, &engine, [&](bool ok) {
        out << (ok ? "Command completed successfully." : "Command failed.") << '\n';
        out.flush();
    });

    // Non-interactive mode via flags
    QCommandLineParser parser;
    parser.setApplicationDescription("MX Boot Repair CLI");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption dryRunOpt({"d", "dry-run"}, "Print actions without executing");
    QCommandLineOption nonIntOpt({"n", "non-interactive"}, "Do not prompt; require flags");
    QCommandLineOption actionOpt("action", "Action: install, repair, initramfs, backup, restore", "name");
    QCommandLineOption targetOpt("target", "Target for install: mbr, esp, root", "name");
    QCommandLineOption locationOpt("location", "Device for target (e.g., sda, sda1)", "dev");
    QCommandLineOption rootOpt("root", "Root partition (e.g., /dev/sda2)", "dev");
    QCommandLineOption bootDevOpt("boot-device", "Partition to mount at /boot in chroot", "dev");
    QCommandLineOption espDevOpt("esp-device", "Partition to mount at /boot/efi in chroot", "dev");
    QCommandLineOption pathOpt("backup-path", "Path for backup/restore image", "path");
    QCommandLineOption forceOpt({"f", "force"}, "Skip confirmations (for restore)");

    parser.addOptions({dryRunOpt, nonIntOpt, actionOpt, targetOpt, locationOpt, rootOpt, bootDevOpt, espDevOpt, pathOpt, forceOpt});
    QStringList args = QCoreApplication::arguments();
    // Strip launcher-only flags handled by the GUI entry point
    args.erase(std::remove_if(args.begin(), args.end(), [](const QString& a){ return a == "-c" || a == "--cli"; }), args.end());
    parser.process(args);

    auto normalizeDev = [](const QString& dev, bool requireDevPrefix) -> QString {
        if (dev.isEmpty()) return dev;
        if (requireDevPrefix) {
            return dev.startsWith("/dev/") ? dev : "/dev/" + dev;
        } else {
            return dev.startsWith("/dev/") ? dev.mid(5) : dev;
        }
    };

    const bool nonInteractive = parser.isSet(nonIntOpt) || parser.isSet(actionOpt);
    if (nonInteractive) {
        const QString action = parser.value(actionOpt).toLower();
        if (action.isEmpty()) { out << "Error: --action is required in non-interactive mode\n"; return 2; }

        BootRepairOptions opt;
        opt.dryRun = parser.isSet(dryRunOpt);
        const QString target = parser.value(targetOpt).toLower();
        if (action == "install") {
            if (target == "mbr") opt.target = GrubTarget::Mbr;
            else if (target == "esp") opt.target = GrubTarget::Esp;
            else if (target == "root") opt.target = GrubTarget::Root;
            else { out << "Error: --target must be mbr|esp|root for install\n"; return 2; }

            opt.location = normalizeDev(parser.value(locationOpt), /*requireDevPrefix*/ false);
            opt.root = normalizeDev(parser.value(rootOpt), /*requireDevPrefix*/ true);
            opt.bootDevice = normalizeDev(parser.value(bootDevOpt), /*requireDevPrefix*/ true);
            opt.espDevice = normalizeDev(parser.value(espDevOpt), /*requireDevPrefix*/ true);
            if (opt.location.isEmpty() || opt.root.isEmpty()) {
                out << "Error: --location and --root are required for install\n"; return 2;
            }
            const bool ok = engine.installGrub(opt); return ok ? 0 : 1;
        } else if (action == "repair" || action == "update-grub") {
            opt.root = normalizeDev(parser.value(rootOpt), /*requireDevPrefix*/ true);
            opt.bootDevice = normalizeDev(parser.value(bootDevOpt), /*requireDevPrefix*/ true);
            opt.espDevice = normalizeDev(parser.value(espDevOpt), /*requireDevPrefix*/ true);
            if (opt.root.isEmpty()) { out << "Error: --root is required for repair\n"; return 2; }
            const bool ok = engine.repairGrub(opt); return ok ? 0 : 1;
        } else if (action == "initramfs" || action == "regenerate-initramfs") {
            opt.root = normalizeDev(parser.value(rootOpt), /*requireDevPrefix*/ true);
            if (opt.root.isEmpty()) { out << "Error: --root is required for initramfs\n"; return 2; }
            const bool ok = engine.regenerateInitramfs(opt); return ok ? 0 : 1;
        } else if (action == "backup") {
            opt.location = normalizeDev(parser.value(locationOpt), /*requireDevPrefix*/ false);
            opt.backupPath = parser.value(pathOpt);
            if (opt.location.isEmpty() || opt.backupPath.isEmpty()) { out << "Error: --location and --backup-path required for backup\n"; return 2; }
            const bool ok = engine.backup(opt); return ok ? 0 : 1;
        } else if (action == "restore") {
            opt.location = normalizeDev(parser.value(locationOpt), /*requireDevPrefix*/ false);
            opt.backupPath = parser.value(pathOpt);
            if (opt.location.isEmpty() || opt.backupPath.isEmpty()) { out << "Error: --location and --backup-path required for restore\n"; return 2; }
            if (!parser.isSet(forceOpt)) { out << "Refusing to restore without --force confirmation.\n"; return 2; }
            const bool ok = engine.restore(opt); return ok ? 0 : 1;
        } else {
            out << "Unknown action: " << action << "\n"; return 2;
        }
    }

    for (;;) {
        out << "MX Boot Repair (CLI)\n";
        out << "0) Exit\n1) Install GRUB\n2) Repair GRUB (update-grub)\n3) Regenerate initramfs\n4) Backup MBR/PBR\n5) Restore MBR/PBR\n";
        const int action = askInt(0, 5, "Select action", in, out);
        if (action == 0) return 0;

        if (action == 1) {
            out << "Target: 0) MBR  1) ESP  2) Root  3) Back\n";
            const int target = askInt(0, 3, "Select target", in, out);
            if (target == 3) continue; // back to main menu
        const QStringList disks = engine.listDisks();
        const QStringList parts = engine.listPartitions();
        if (parts.isEmpty()) {
            out << "No partitions found. Returning to main menu.\n";
            continue;
        }
        if (disks.isEmpty() && target == 0) {
            out << "No disks found. Returning to main menu.\n";
            continue;
        }
        int locIdx = -1;
        if (target == 0) {
            locIdx = askIndex(disks, "Select disk for MBR (e.g., sda)", in, out, /*allowBack*/ true);
            if (locIdx == -1) continue;
        } else {
            locIdx = askIndex(parts, "Select partition for GRUB (e.g., sda1)", in, out, /*allowBack*/ true);
            if (locIdx == -1) continue;
        }
        const QString location = (target == 0 ? disks.at(locIdx) : parts.at(locIdx)).split(' ').first();
        const int rootIdx = askIndex(parts, "Select root partition of installed system", in, out, /*allowBack*/ true);
        if (rootIdx == -1) continue;
        const QString root = "/dev/" + parts.at(rootIdx).split(' ').first();

        BootRepairOptions opt;
        opt.target = (target == 0 ? GrubTarget::Mbr : (target == 1 ? GrubTarget::Esp : GrubTarget::Root));
        opt.location = location;
        opt.root = root;
        opt.dryRun = parser.isSet(dryRunOpt);

            const bool ok = engine.installGrub(opt);
            return ok ? 0 : 1;
        }
        if (action == 2) {
        const QStringList parts = engine.listPartitions();
            if (parts.isEmpty()) { out << "No partitions found. Returning to main menu.\n"; continue; }
            const int rootIdx = askIndex(parts, "Select root partition to repair", in, out, /*allowBack*/ true);
            if (rootIdx == -1) continue;
        BootRepairOptions opt;
        opt.root = "/dev/" + parts.at(rootIdx).split(' ').first();
        opt.dryRun = parser.isSet(dryRunOpt);
        const bool ok = engine.repairGrub(opt);
        return ok ? 0 : 1;
        }
        if (action == 3) {
        const QStringList parts = engine.listPartitions();
            if (parts.isEmpty()) { out << "No partitions found. Returning to main menu.\n"; continue; }
            const int rootIdx = askIndex(parts, "Select root partition to regenerate initramfs", in, out, /*allowBack*/ true);
            if (rootIdx == -1) continue;
        BootRepairOptions opt;
        opt.root = "/dev/" + parts.at(rootIdx).split(' ').first();
        opt.dryRun = parser.isSet(dryRunOpt);
        const bool ok = engine.regenerateInitramfs(opt);
        return ok ? 0 : 1;
        }
        if (action == 4) {
        const QStringList disks = engine.listDisks();
            if (disks.isEmpty()) { out << "No disks found. Returning to main menu.\n"; continue; }
            const int diskIdx = askIndex(disks, "Select disk to back up MBR/PBR from", in, out, /*allowBack*/ true);
            if (diskIdx == -1) continue;
            out << "Output file path (leave empty to go back): " << Qt::flush;
            const QString outPath = in.readLine().trimmed();
            if (outPath.isEmpty()) continue;
        BootRepairOptions opt;
        opt.location = disks.at(diskIdx).split(' ').first();
        opt.backupPath = outPath;
        opt.dryRun = parser.isSet(dryRunOpt);
        const bool ok = engine.backup(opt);
        return ok ? 0 : 1;
        }
        if (action == 5) {
        const QStringList disks = engine.listDisks();
            if (disks.isEmpty()) { out << "No disks found. Returning to main menu.\n"; continue; }
            const int diskIdx = askIndex(disks, "Select disk to restore MBR/PBR to", in, out, /*allowBack*/ true);
            if (diskIdx == -1) continue;
            out << "Input backup file path (leave empty to go back): " << Qt::flush;
            const QString inPath = in.readLine().trimmed();
            if (inPath.isEmpty()) continue;
        BootRepairOptions opt;
        opt.location = disks.at(diskIdx).split(' ').first();
        opt.backupPath = inPath;
        out << "WARNING: This will overwrite the first 446 bytes of /dev/" << opt.location << ". Continue? [y/N]: "
            << Qt::flush;
        const QString ans = in.readLine().trimmed().toLower();
        if (ans != "y" && ans != "yes") return 1;
        opt.dryRun = parser.isSet(dryRunOpt);
        const bool ok = engine.restore(opt);
        return ok ? 0 : 1;
        }

        out << "Unknown selection\n";
        return 1;
    }
}
