// Public polkit entry point. Mount roots, command lines and chroot paths are
// chosen by the engine in this process. The caller supplies only repair devices.
#include "core/bootrepair_engine.h"
#include "helper/device.h"

#include <QCoreApplication>
#include <QFile>
#include <QScopeGuard>
#include <QTextStream>

#include <sched.h>
#include <sys/mount.h>
#include <unistd.h>

namespace {
int fail(const QString &message)
{
    QTextStream(stderr) << message << '\n';
    return 1;
}

int transferRecord(const QString &action, const QString &device, QFile &input)
{
    QFile disk(device);
    if (action == "read-record") {
        if (!disk.open(QIODevice::ReadOnly)) return fail(disk.errorString());
        const QByteArray record = disk.read(446);
        if (record.size() != 446) return fail("Could not read the complete boot record");
        QTextStream(stdout) << record.toBase64() << '\n';
        return 0;
    }
    const QByteArray record = input.read(447);
    if (record.isEmpty() || record.size() > 446) return fail("Invalid boot record length");
    if (!disk.open(QIODevice::WriteOnly | QIODevice::ExistingOnly)
        || disk.write(record) != record.size() || !disk.flush()) return fail(disk.errorString());
    return 0;
}
}

int main(int argc, char **argv)
{
    if (geteuid() != 0) return fail("Administrator authorization is required");
    qputenv("PATH", "/usr/sbin:/usr/bin:/sbin:/bin");
    QCoreApplication app(argc, argv);
    QStringList args = app.arguments().mid(1);
    if (args.isEmpty()) return fail("Missing repair action");
    const QString action = args.takeFirst();
    if (action == "copy-log" && args.isEmpty()) return Cmd().copyLogAsRoot(QuietMode::Yes) ? 0 : 1;

    if (args.isEmpty()) return fail("Missing repair device");
    const QString root = RepairDevice::canonicalBlock(args.takeFirst());
    if (root.isEmpty()) return fail("Expected an existing block device under /dev");
    QFile input;
    if (!input.open(stdin, QIODevice::ReadOnly)) return fail(input.errorString());
    if ((action == "read-record" || action == "write-record") && args.isEmpty()) {
        return transferRecord(action, root, input);
    }
    BootRepairEngine engine(nullptr, BootRepairEngine::Execution::PrivilegedHelper);
    if (action == "is-luks" && args.isEmpty()) return engine.isLuks(root) ? 0 : 1;

    BootRepairOptions opt;
    opt.root = root;
    // Passphrases travel only on stdin. Reject excessive input before opening a device.
    opt.luksPassword = input.read(65537);
    const auto scrub = qScopeGuard([&] { opt.luksPassword.fill(0); });
    if (opt.luksPassword.size() > 65536) return fail("Passphrase exceeds the size limit");
    if (action == "check-passphrase" && args.isEmpty()) return engine.canUnlockLuks(root, opt.luksPassword) ? 0 : 1;

    if (action == "install" && args.size() == 4) {
        const QString target = args.takeFirst();
        if (target == "mbr") opt.target = GrubTarget::Mbr;
        else if (target == "root") opt.target = GrubTarget::Root;
        else if (target == "esp") opt.target = GrubTarget::Esp;
        else return fail("Invalid GRUB install target");
        const QString location = RepairDevice::canonicalBlock(args.takeFirst());
        if (location.isEmpty()) return fail("Invalid install device");
        opt.location = location.mid(5);
    } else if (!(action == "repair" && args.size() == 2) && !(action == "initramfs" && args.size() == 1)) {
        return fail("Unsupported repair action or arguments");
    }

    const QString boot = args.takeFirst();
    const QString esp = args.isEmpty() ? QString() : args.takeFirst();
    opt.bootDevice = RepairDevice::canonicalBlock(boot);
    opt.espDevice = RepairDevice::canonicalBlock(esp);
    if ((!boot.isEmpty() && opt.bootDevice.isEmpty()) || (!esp.isEmpty() && opt.espDevice.isEmpty())) {
        return fail("Invalid boot or EFI partition");
    }
    if (action == "install" && opt.target == GrubTarget::Esp && opt.espDevice != "/dev/" + opt.location) {
        return fail("EFI partition differs from the selected installation device");
    }

    // Keep temporary mounts local to this repair, including on shared-mount hosts.
    if (unshare(CLONE_NEWNS) != 0 || mount(nullptr, "/", nullptr, MS_REC | MS_PRIVATE, nullptr) != 0) {
        return fail("Could not create a private mount namespace");
    }
    QObject::connect(&engine, &BootRepairEngine::log, [](const QString &line) {
        QTextStream(stderr) << line << (line.endsWith('\n') ? "" : "\n");
    });
    if (action == "install") return engine.installGrub(opt) ? 0 : 1;
    if (action == "repair") return engine.repairGrub(opt) ? 0 : 1;
    return engine.regenerateInitramfs(opt) ? 0 : 1;
}
