#include "workspace_settings.hpp"
void WorkspaceSettings::open(const QString &path) {
    file_ = std::make_unique<QSettings>(path, QSettings::IniFormat);
    bool ok = false;
    const int version = file_->value("format/version", 1).toInt(&ok);
    supported_ = ok && version == 1;
    if (!supported_)
        return; // Never overwrite a future settings schema.
    state_.dark = file_->value("appearance/dark", true).toBool();
    state_.ticker = file_->value("overview/ticker").toString();
    const int sessions = file_->value("overview/sessions", 66).toInt();
    if (sessions == 0 || sessions == 22 || sessions == 66 || sessions == 252)
        state_.sessions = sessions;
    state_.comparison = file_->value("overview/comparison", false).toBool();
}
bool WorkspaceSettings::save() {
    if (!file_ || !supported_)
        return false;
    file_->setValue("format/version", 1);
    file_->setValue("appearance/dark", state_.dark);
    file_->setValue("overview/ticker", state_.ticker);
    file_->setValue("overview/sessions", state_.sessions);
    file_->setValue("overview/comparison", state_.comparison);
    file_->sync();
    return file_->status() == QSettings::NoError;
}
