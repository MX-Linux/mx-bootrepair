#include "cli/controller.h"

#include <QCoreApplication>
#include <QTextStream>
#include <QCommandLineParser>
#include <limits>

#include "core/bootrepair_engine.h"

namespace {
// Returns index >= 0, -1 for back, -2 for quit
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
        if (line.compare("q", Qt::CaseInsensitive) == 0) { out << '\n'; return -2; }
        const int idx = line.toInt(&ok);
        if (ok && idx >= 0 && idx < items.size()) { out << '\n'; return idx; }
        out << QObject::tr("Invalid index. Try again: ") << Qt::flush;
    }
}

// Returns in [min,max], min-1 for back (if allowBack), or INT_MIN for quit
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
        if (line.compare("q", Qt::CaseInsensitive) == 0) { out << '\n'; return std::numeric_limits<int>::min(); } // quit
        const int val = line.toInt(&ok);
        if (ok && val >= min && val <= max) { out << '\n'; return val; }
        out << QObject::tr("Invalid selection. Enter a number ") << min << "-" << max << ": " << Qt::flush;
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
    QCommandLineOption verboseOpt("verbose", "Enable verbose output");
    QCommandLineOption quietOpt({"q", "quiet"}, "Suppress non-error output");

    parser.addOptions({dryRunOpt, nonIntOpt, actionOpt, targetOpt, locationOpt, rootOpt, bootDevOpt, espDevOpt, pathOpt, forceOpt, verboseOpt, quietOpt});
    QStringList args = QCoreApplication::arguments();
    // Strip launcher-only flags handled by the GUI entry point
    args.erase(std::remove_if(args.begin(), args.end(), [](const QString& a){ return a == "-c" || a == "--cli"; }), args.end());
    parser.process(args);

    // Validate conflicting options
    if (parser.isSet(verboseOpt) && parser.isSet(quietOpt)) {
        out << "Error: --verbose and --quiet options are mutually exclusive\n";
        return 2;
    }

    auto normalizeDev = [](const QString& dev, bool requireDevPrefix) -> QString {
        if (dev.isEmpty()) return dev;
        if (requireDevPrefix) {
            return dev.startsWith("/dev/") ? dev : "/dev/" + dev;
        } else {
            return dev.startsWith("/dev/") ? dev.mid(5) : dev;
        }
    };

    auto validateDevicePath = [&out](const QString& dev, const QString& name) -> bool {
        if (dev.isEmpty()) return true; // Empty is valid (optional)
        if (dev.startsWith("/dev/")) {
            // Check if it looks like a valid device path
            const QString devName = dev.mid(5);
            if (devName.isEmpty() || devName.contains('/')) {
                out << "Error: Invalid " << name << " device path: " << dev << "\n";
                return false;
            }
        } else {
            // Check if it looks like a valid device name
            if (dev.contains('/') || dev.isEmpty()) {
                out << "Error: Invalid " << name << " device name: " << dev << "\n";
                return false;
            }
        }
        return true;
    };

    const bool nonInteractive = parser.isSet(nonIntOpt) || parser.isSet(actionOpt);
    if (nonInteractive) {
        const QString action = parser.value(actionOpt).toLower();
        if (action.isEmpty()) { out << "Error: --action is required in non-interactive mode\n"; return 2; }

        // Validate action
        const QStringList validActions = {"install", "repair", "initramfs", "backup", "restore", "update-grub", "regenerate-initramfs"};
        if (!validActions.contains(action)) {
            out << "Error: Invalid action '" << action << "'. Valid actions: " << validActions.join(", ") << "\n";
            return 2;
        }

        BootRepairOptions opt;
        opt.dryRun = parser.isSet(dryRunOpt);
        const QString target = parser.value(targetOpt).toLower();
        if (action == "install") {
            if (target == "mbr") opt.target = GrubTarget::Mbr;
            else if (target == "esp") opt.target = GrubTarget::Esp;
            else if (target == "root") opt.target = GrubTarget::Root;
            else { out << "Error: --target must be mbr|esp|root for install\n"; return 2; }

            // Validate target-specific requirements
            if (target == "mbr" && parser.value(locationOpt).isEmpty()) {
                out << "Error: --location is required for MBR target\n"; return 2;
            }
            if ((target == "esp" || target == "root") && parser.value(locationOpt).isEmpty()) {
                out << "Error: --location is required for ESP/Root target\n"; return 2;
            }

            opt.location = normalizeDev(parser.value(locationOpt), /*requireDevPrefix*/ false);
            opt.root = normalizeDev(parser.value(rootOpt), /*requireDevPrefix*/ true);
            opt.bootDevice = normalizeDev(parser.value(bootDevOpt), /*requireDevPrefix*/ true);
            opt.espDevice = normalizeDev(parser.value(espDevOpt), /*requireDevPrefix*/ true);

            // Validate device paths
            if (!validateDevicePath(opt.location, "location") ||
                !validateDevicePath(opt.root, "root") ||
                !validateDevicePath(opt.bootDevice, "boot-device") ||
                !validateDevicePath(opt.espDevice, "esp-device")) {
                return 2;
            }

            if (opt.location.isEmpty() || opt.root.isEmpty()) {
                out << "Error: --location and --root are required for install\n"; return 2;
            }
            const bool ok = engine.installGrub(opt); return ok ? 0 : 1;
        } else if (action == "repair" || action == "update-grub") {
            opt.root = normalizeDev(parser.value(rootOpt), /*requireDevPrefix*/ true);
            opt.bootDevice = normalizeDev(parser.value(bootDevOpt), /*requireDevPrefix*/ true);
            opt.espDevice = normalizeDev(parser.value(espDevOpt), /*requireDevPrefix*/ true);

            // Validate device paths
            if (!validateDevicePath(opt.root, "root") ||
                !validateDevicePath(opt.bootDevice, "boot-device") ||
                !validateDevicePath(opt.espDevice, "esp-device")) {
                return 2;
            }

            if (opt.root.isEmpty()) { out << "Error: --root is required for repair\n"; return 2; }
            const bool ok = engine.repairGrub(opt); return ok ? 0 : 1;
        } else if (action == "initramfs" || action == "regenerate-initramfs") {
            opt.root = normalizeDev(parser.value(rootOpt), /*requireDevPrefix*/ true);

            // Validate device path
            if (!validateDevicePath(opt.root, "root")) {
                return 2;
            }

            if (opt.root.isEmpty()) { out << "Error: --root is required for initramfs\n"; return 2; }
            const bool ok = engine.regenerateInitramfs(opt); return ok ? 0 : 1;
        } else if (action == "backup") {
            opt.location = normalizeDev(parser.value(locationOpt), /*requireDevPrefix*/ false);
            opt.backupPath = parser.value(pathOpt);

            // Validate device path
            if (!validateDevicePath(opt.location, "location")) {
                return 2;
            }

            if (opt.location.isEmpty() || opt.backupPath.isEmpty()) { out << "Error: --location and --backup-path required for backup\n"; return 2; }
            const bool ok = engine.backup(opt); return ok ? 0 : 1;
        } else if (action == "restore") {
            opt.location = normalizeDev(parser.value(locationOpt), /*requireDevPrefix*/ false);
            opt.backupPath = parser.value(pathOpt);

            // Validate device path
            if (!validateDevicePath(opt.location, "location")) {
                return 2;
            }

            if (opt.location.isEmpty() || opt.backupPath.isEmpty()) { out << "Error: --location and --backup-path required for restore\n"; return 2; }
            if (!parser.isSet(forceOpt)) { out << "Refusing to restore without --force confirmation.\n"; return 2; }
            const bool ok = engine.restore(opt); return ok ? 0 : 1;
        } else {
            out << "Unknown action: " << action << "\n"; return 2;
        }
    }

    bool first = true;
    for (;;) {
        if (!first) { out << '\n'; out.flush(); }
        first = false;
        out << QObject::tr("MX Boot Repair (CLI)") << '\n';
        out << QObject::tr("1) Install GRUB") << '\n'
            << QObject::tr("2) Repair GRUB (update-grub)") << '\n'
            << QObject::tr("3) Regenerate initramfs") << '\n'
            << QObject::tr("4) Backup MBR/PBR") << '\n'
            << QObject::tr("5) Restore MBR/PBR") << '\n'
            << QObject::tr("q) Quit") << '\n';
        out << QObject::tr("Select action [1-5 or 'q' to quit]: ") << Qt::flush;
        const QString actionStr = in.readLine().trimmed();
        if (actionStr.compare("q", Qt::CaseInsensitive) == 0) return 0;
        bool okAction = false;
        const int action = actionStr.toInt(&okAction);
        if (!okAction || action < 1 || action > 5) {
            out << QObject::tr("Invalid selection. Enter 1-5 or 'q' to quit.") << '\n';
            continue;
        }

        if (action == 1) {
            out << QObject::tr("Target: 0) MBR  1) ESP  2) Root") << '\n';
            const int target = askInt(0, 2, QObject::tr("Select target"), in, out, /*allowBack*/ true);
            if (target == std::numeric_limits<int>::min()) return 0; // 'q' to quit
            if (target < 0) continue; // 'b' to go back
        const QStringList disks = engine.listDisks();
        const QStringList parts = engine.listPartitions();
        if (parts.isEmpty()) {
            out << QObject::tr("No partitions found. Returning to main menu.") << '\n';
            continue;
        }
        if (disks.isEmpty() && target == 0) {
            out << QObject::tr("No disks found. Returning to main menu.") << '\n';
            continue;
        }
        int locIdx = -1;
        if (target == 0) {
            locIdx = askIndex(disks, QObject::tr("Select disk for MBR (e.g., sda)"), in, out, /*allowBack*/ true);
            if (locIdx == -2) return 0; // quit
            if (locIdx == -1) continue; // back
        } else {
            locIdx = askIndex(parts, QObject::tr("Select partition for GRUB (e.g., sda1)"), in, out, /*allowBack*/ true);
            if (locIdx == -2) return 0; // quit
            if (locIdx == -1) continue; // back
        }
        const QString location = (target == 0 ? disks.at(locIdx) : parts.at(locIdx)).split(' ').first();
        const int rootIdx = askIndex(parts, QObject::tr("Select root partition of installed system"), in, out, /*allowBack*/ true);
        if (rootIdx == -2) return 0; // quit
        if (rootIdx == -1) continue; // back
        const QString root = "/dev/" + parts.at(rootIdx).split(' ').first();

        BootRepairOptions opt;
        opt.target = (target == 0 ? GrubTarget::Mbr : (target == 1 ? GrubTarget::Esp : GrubTarget::Root));
        opt.location = location;
        opt.root = root;
        opt.dryRun = parser.isSet(dryRunOpt);

            const bool ok = engine.installGrub(opt);
            Q_UNUSED(ok);
            continue;
        }
        if (action == 2) {
        const QStringList parts = engine.listPartitions();
            if (parts.isEmpty()) { out << QObject::tr("No partitions found. Returning to main menu.") << '\n'; continue; }
            const int rootIdx = askIndex(parts, QObject::tr("Select root partition to repair"), in, out, /*allowBack*/ true);
            if (rootIdx == -2) return 0; // quit
            if (rootIdx == -1) continue; // back
        BootRepairOptions opt;
        opt.root = "/dev/" + parts.at(rootIdx).split(' ').first();
        opt.dryRun = parser.isSet(dryRunOpt);
        const bool ok = engine.repairGrub(opt);
        Q_UNUSED(ok);
        continue;
        }
        if (action == 3) {
        const QStringList parts = engine.listPartitions();
            if (parts.isEmpty()) { out << QObject::tr("No partitions found. Returning to main menu.") << '\n'; continue; }
            const int rootIdx = askIndex(parts, QObject::tr("Select root partition to regenerate initramfs"), in, out, /*allowBack*/ true);
            if (rootIdx == -2) return 0; // quit
            if (rootIdx == -1) continue; // back
        BootRepairOptions opt;
        opt.root = "/dev/" + parts.at(rootIdx).split(' ').first();
        opt.dryRun = parser.isSet(dryRunOpt);
        const bool ok = engine.regenerateInitramfs(opt);
        Q_UNUSED(ok);
        continue;
        }
        if (action == 4) {
        const QStringList disks = engine.listDisks();
            if (disks.isEmpty()) { out << QObject::tr("No disks found. Returning to main menu.") << '\n'; continue; }
            const int diskIdx = askIndex(disks, QObject::tr("Select disk to back up MBR/PBR from"), in, out, /*allowBack*/ true);
            if (diskIdx == -2) return 0; // quit
            if (diskIdx == -1) continue; // back
            out << QObject::tr("Output file path (or 'b' to go back, 'q' to quit): ") << Qt::flush;
            const QString outPath = in.readLine().trimmed();
            if (outPath.compare("q", Qt::CaseInsensitive) == 0) return 0;
            if (outPath.compare("b", Qt::CaseInsensitive) == 0) continue;
        BootRepairOptions opt;
        opt.location = disks.at(diskIdx).split(' ').first();
        opt.backupPath = outPath;
        opt.dryRun = parser.isSet(dryRunOpt);
        const bool ok = engine.backup(opt);
        Q_UNUSED(ok);
        continue;
        }
        if (action == 5) {
        const QStringList disks = engine.listDisks();
            if (disks.isEmpty()) { out << QObject::tr("No disks found. Returning to main menu.") << '\n'; continue; }
            const int diskIdx = askIndex(disks, QObject::tr("Select disk to restore MBR/PBR to"), in, out, /*allowBack*/ true);
            if (diskIdx == -2) return 0; // quit
            if (diskIdx == -1) continue; // back
            out << QObject::tr("Input backup file path (or 'b' to go back, 'q' to quit): ") << Qt::flush;
            const QString inPath = in.readLine().trimmed();
            if (inPath.compare("q", Qt::CaseInsensitive) == 0) return 0;
            if (inPath.compare("b", Qt::CaseInsensitive) == 0) continue;
        BootRepairOptions opt;
        opt.location = disks.at(diskIdx).split(' ').first();
        opt.backupPath = inPath;
        out << QObject::tr("WARNING: This will overwrite the first 446 bytes of /dev/") << opt.location << QObject::tr(". Continue? [y/N]: ")
            << Qt::flush;
        const QString ans = in.readLine().trimmed().toLower();
        const QString yWord = QObject::tr("yes", "confirmation input: full word").trimmed().toLower();
        const QString yLetter = QObject::tr("y", "confirmation input: single-letter yes").trimmed().toLower();
        const bool isYes = (ans == QStringLiteral("y") || ans == QStringLiteral("yes")
                            || (!yLetter.isEmpty() && ans == yLetter)
                            || (!yWord.isEmpty() && ans == yWord));
        if (!isYes) { out << QObject::tr("Cancelled.") << '\n'; continue; }
        opt.dryRun = parser.isSet(dryRunOpt);
        const bool ok = engine.restore(opt);
        Q_UNUSED(ok);
        continue;
        }

        out << QObject::tr("Unknown selection") << '\n';
        continue;
    }
}
