#pragma once
#ifdef _WIN32
#include <QDir>
#include <QProcess>
// NTFS junctions do not require Developer Mode or symbolic-link privileges.
class TestJunction {
public:
    QString path;
    explicit TestJunction(QString value) : path(std::move(value)) {}
    bool create(const QString &target) {
        return QProcess::execute("cmd.exe", {"/D", "/C", "mklink", "/J",
            QDir::toNativeSeparators(path), QDir::toNativeSeparators(target)}) == 0;
    }
    ~TestJunction() { QDir().rmdir(path); } // Remove the junction, never its target tree.
};
#endif
