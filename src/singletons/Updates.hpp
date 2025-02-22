#pragma once

#include <pajlada/signals/signal.hpp>
#include <QString>

namespace chatterino {

class Updates
{
    Updates();

public:
    enum Status {
        None,
        Searching,
        UpdateAvailable,
        NoUpdateAvailable,
        SearchFailed,
        Downloading,
        DownloadFailed,
        WriteFileFailed,
    };

    // fourtf: don't add this class to the application class
    static Updates &instance();

    void checkForUpdates();
    const QString &getCurrentVersion() const;
    const QString &getOnlineVersion() const;
    void installUpdates();
    Status getStatus() const;

    bool shouldShowUpdateButton() const;
    bool isError() const;
    bool isDowngrade() const;

    pajlada::Signals::Signal<Status> statusUpdated;

private:
    QString currentVersion_;
    QString onlineVersion_;
    Status status_ = None;
    bool isDowngrade_{};

    QString updateExe_;
    QString updatePortable_;
    QString updateGuideLink_;

    void setStatus_(Status status);
};

}  // namespace chatterino
