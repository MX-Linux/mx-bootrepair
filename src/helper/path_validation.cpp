#include "helper/path_validation.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <cerrno>
#include <fcntl.h>
#include <linux/openat2.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

namespace HelperPath
{
bool isValidAbsolutePath(const QString &path)
{
    return QDir::isAbsolutePath(path) && !path.contains(QLatin1Char('\n')) && !path.contains(QChar::Null);
}

Descriptor::~Descriptor()
{
    if (fd_ >= 0) ::close(fd_);
}

QString Descriptor::procPath() const
{
    // Host tools run in another process while this descriptor stays open.
    return QStringLiteral("/proc/%1/fd/%2").arg(getpid()).arg(fd_);
}

TargetRoot::TargetRoot(const QString &path)
    : root_(isValidAbsolutePath(path) ? ::open(QFile::encodeName(path).constData(),
                                               O_PATH | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC) : -1)
{
}

Descriptor TargetRoot::open(const QString &path, int flags) const
{
    if (!root_ || !isValidAbsolutePath(path)) {
        errno = EINVAL;
        return Descriptor();
    }
    struct open_how how {};
    how.flags = static_cast<unsigned int>(flags | O_CLOEXEC);
    how.resolve = RESOLVE_IN_ROOT | RESOLVE_NO_MAGICLINKS;
    return Descriptor(static_cast<int>(syscall(SYS_openat2, root_.get(), QFile::encodeName(path).constData(),
                                               &how, sizeof(how))));
}

Descriptor TargetRoot::directory(const QString &path, bool create) const
{
    if (!isValidAbsolutePath(path)) {
        errno = EINVAL;
        return Descriptor();
    }
    if (create && path != "/") {
        const QFileInfo info(path);
        const auto parent = directory(info.path(), true);
        if (!parent) return Descriptor();
        if (::mkdirat(parent.get(), QFile::encodeName(info.fileName()).constData(), 0755) != 0 && errno != EEXIST)
            return Descriptor();
    }
    return open(path, O_PATH | O_DIRECTORY);
}

bool TargetRoot::enter() const
{
    return root_ && ::fchdir(root_.get()) == 0 && ::chroot(".") == 0 && ::chdir("/") == 0;
}
}
