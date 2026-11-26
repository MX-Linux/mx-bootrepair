#include "cli/validation.h"
#include "core/bootrepair_engine.h"
#include "core/cmd.h"
#include "helper/path_validation.h"

#include <QCoreApplication>
#include <QProcess>
#include <QTemporaryDir>
#include <QTextStream>

namespace
{
int failures = 0;

void check(bool condition, const QString &message)
{
    if (!condition) {
        QTextStream(stderr) << "FAIL: " << message << '\n';
        ++failures;
    }
}

void testCmd()
{
    Cmd command;
    QString output;
    check(command.proc("/bin/printf", {"%s", "hello"}, &output, nullptr, QuietMode::Yes),
          "Cmd should report a successful process");
    check(output == "hello", "Cmd should capture standard output");

    const QByteArray input("input-data\n");
    check(command.proc("/bin/cat", {}, &output, &input, QuietMode::Yes),
          "Cmd should write process input");
    check(output == "input-data", "Cmd should capture echoed input");
    check(!command.proc("/bin/false", {}, nullptr, nullptr, QuietMode::Yes),
          "Cmd should report a non-zero exit status");
}

void testBootRepairEngine()
{
    check(BootRepairEngine::efiPlatform("x86_64", 32) == "i386", "32-bit x86 firmware selects IA32 GRUB");
    check(BootRepairEngine::efiPlatform("i686", 64) == "x86_64", "64-bit x86 firmware selects x64 GRUB");
    check(BootRepairEngine::efiPlatform("i386", 0) == "i386", "missing firmware size uses the kernel family");
    check(BootRepairEngine::efiPlatform("aarch64", 64) == "arm64", "ARM64 uses the GRUB platform spelling");
    check(BootRepairEngine::matchesEspPartitionType("c12a7328-f81f-11d2-ba4b-00a0c93ec93b"),
          "GPT ESP type should match");
    check(BootRepairEngine::matchesEspPartitionType("0xEF"), "MBR ESP type should match case-insensitively");
    check(!BootRepairEngine::matchesEspPartitionType("0x83"), "Linux MBR type should not match ESP");

    check(BootRepairEngine::matchesLinuxPartitionType("0fc63daf-8483-4772-8e79-3d69d8477de4"),
          "GPT Linux filesystem type should match");
    check(BootRepairEngine::matchesLinuxPartitionType("0x83"), "MBR Linux type should match");
    check(!BootRepairEngine::matchesLinuxPartitionType("c12a7328-f81f-11d2-ba4b-00a0c93ec93b"),
          "ESP type should not match Linux");

    BootRepairEngine engine;
    check(engine.isMounted("/dev/mxbr-test-device-that-does-not-exist", "/") == MountState::QueryFailed,
          "A failed lsblk query should return MountState::QueryFailed");
}

void testPartitionInfoParsing()
{
    const PartitionInfo info = BootRepairEngine::parsePartitionInfo(
        QStringLiteral(R"(PARTTYPE="0fc63daf-8483-4772-8e79-3d69d8477de4" FSTYPE="ext4" LABEL="rootMX")"));
    check(info.partType == QStringLiteral("0fc63daf-8483-4772-8e79-3d69d8477de4"),
          "PARTTYPE should be extracted from a single lsblk -P row");
    check(info.fsType == QStringLiteral("ext4"), "FSTYPE should be extracted from a single lsblk -P row");
    check(info.label == QStringLiteral("rootMX"), "LABEL should be extracted from a single lsblk -P row");

    // lsblk hex-escapes unsafe LABEL/FSTYPE characters (e.g. an embedded quote) as \xHH;
    // this regex previously had a raw-string-literal bug that silently failed to build.
    const PartitionInfo escaped = BootRepairEngine::parsePartitionInfo(
        QStringLiteral(R"(PARTTYPE="" FSTYPE="ext4" LABEL="root\x22MX")"));
    check(escaped.label == QStringLiteral("root\"MX"),
          "a hex-escaped quote in LABEL should be unescaped back to a literal quote");

    // Documents a known limitation: lsblk recurses into child partitions for a whole-disk
    // device, returning one row per partition. parsePartitionInfo collapses every row into
    // a single struct, keeping only the LAST row's fields - it must only be used on a
    // single partition's output, never on a whole disk's multi-row output.
    const PartitionInfo multiRow = BootRepairEngine::parsePartitionInfo(
        QStringLiteral("PARTTYPE=\"\" FSTYPE=\"\" LABEL=\"\"\n"
                       "PARTTYPE=\"c12a7328-f81f-11d2-ba4b-00a0c93ec93b\" FSTYPE=\"vfat\" LABEL=\"EFI\"\n"));
    check(multiRow.label == QStringLiteral("EFI"),
          "multi-row input keeps only the last row (documented limitation, not a supported use)");
}

void testCliValidation()
{
    check(CliValidation::normalizeDevice("sda2", true) == "/dev/sda2",
          "root devices should gain the /dev prefix");
    check(CliValidation::normalizeDevice("/dev/sda", false) == "sda",
          "target devices should lose the /dev prefix");
    check(CliValidation::isValidDevice("/dev/nvme0n1p2"), "a normal device path should be valid");
    check(!CliValidation::isValidDevice("/dev/disk/by-id/example"), "nested device paths should be rejected");
    check(!CliValidation::isValidDevice("sda/2"), "device names containing a slash should be rejected");
    check(CliValidation::isValidDevice(QString()), "an empty device should be considered valid (optional)");
    check(CliValidation::normalizeDevice(QString(), true).isEmpty(),
          "an empty device should stay empty regardless of the /dev prefix requirement");
}

void testHelperPaths()
{
    QTemporaryDir root;
    check(root.isValid(), "temporary helper root should be created");
    check(HelperPath::isValidRootPath(root.path()), "an existing absolute helper root should be valid");
    check(!HelperPath::isValidRootPath("relative"), "a relative helper root should be rejected");
    check(HelperPath::isValidAbsolutePath("/etc/fstab"), "an absolute path should be valid");
    check(!HelperPath::isValidAbsolutePath("/etc/fstab\nother"), "paths containing newlines should be rejected");
    check(HelperPath::isSafeChildPath(root.path(), "/etc/fstab"),
          "a target-root-relative path should remain below the root");
    check(!HelperPath::isSafeChildPath(root.path(), "/../../etc/passwd"),
          "path traversal outside the target root should be rejected");
    check(!HelperPath::isSafeChildPath(QStringLiteral("/mxbr-test-root-that-does-not-exist"), "/etc/fstab"),
          "a root path that doesn't resolve to a canonical path must fail closed, not be treated as unrestricted");
    check(HelperPath::isSafeChildPath(QString(), "/etc/fstab"),
          "an empty root (no chroot) should allow any absolute path");
    check(!HelperPath::isSafeChildPath(QString(), "relative/path"),
          "an empty root (no chroot) should still reject a non-absolute path");
}

void testCliParser()
{
    auto run = [](const QStringList &arguments, const QByteArray &expectedText) {
        QProcess process;
        process.start(QStringLiteral(TEST_CLI_PATH), arguments);
        check(process.waitForFinished(10000), "CLI validation process should finish");
        check(process.exitStatus() == QProcess::NormalExit && process.exitCode() == 2,
              "invalid CLI arguments should return exit code 2");
        check(process.readAllStandardOutput().contains(expectedText),
              QStringLiteral("CLI output should contain: %1").arg(QString::fromUtf8(expectedText)));
    };

    run({"--non-interactive", "--action", "unknown"}, "Invalid action");
    run({"--non-interactive", "--action", "repair", "--root", "/dev/sda/2"}, "Invalid root device path");
    run({"--action", "install", "--target", "esp", "--location", "sda1", "--root", "sda2",
         "--esp-device", "/dev/sdb1"}, "conflicts");
    run({"--non-interactive", "--verbose", "--quiet"}, "mutually exclusive");
}
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    testCmd();
    testBootRepairEngine();
    testPartitionInfoParsing();
    testCliValidation();
    testHelperPaths();
    testCliParser();

    if (failures == 0) {
        QTextStream(stdout) << "All tests passed\n";
    }
    return failures == 0 ? 0 : 1;
}
