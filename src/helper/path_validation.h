#pragma once

#include <QString>

namespace HelperPath
{
bool isValidAbsolutePath(const QString &path);

class Descriptor
{
public:
    explicit Descriptor(int fd = -1) : fd_(fd) {}
    ~Descriptor();
    Descriptor(const Descriptor &) = delete;
    Descriptor &operator=(const Descriptor &) = delete;
    explicit operator bool() const { return fd_ >= 0; }
    int get() const { return fd_; }
    QString procPath() const;

private:
    int fd_;
};

class TargetRoot
{
public:
    explicit TargetRoot(const QString &path);
    Descriptor open(const QString &path, int flags) const;
    Descriptor directory(const QString &path, bool create = false) const;
    bool enter() const;

private:
    Descriptor root_;
};
}
