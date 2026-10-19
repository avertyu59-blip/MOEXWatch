#pragma once
#include <QSettings>
#include <QString>
#include <memory>
struct WorkspaceState {
    bool dark{true};
    QString ticker;
    int sessions{66};
    bool comparison{};
};
class WorkspaceSettings {
  public:
    void open(const QString &path);
    WorkspaceState &state() { return state_; }
    const WorkspaceState &state() const { return state_; }
    bool save();

  private:
    WorkspaceState state_;
    std::unique_ptr<QSettings> file_;
    bool supported_{true};
};
