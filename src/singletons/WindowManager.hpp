#pragma once

#include "common/Channel.hpp"
#include "common/FlagsEnum.hpp"
#include "common/Singleton.hpp"
#include "common/WindowDescriptors.hpp"
#include "pajlada/settings/settinglistener.hpp"
#include "widgets/splits/SplitContainer.hpp"

#include <memory>

namespace chatterino {

class Settings;
class Paths;
class Window;
class SplitContainer;
class ChannelView;

enum class MessageElementFlag : int64_t;
using MessageElementFlags = FlagsEnum<MessageElementFlag>;
enum class WindowType;

enum class SettingsDialogPreference;
class FramelessEmbedWindow;

class WindowManager final : public Singleton
{
public:
    static const QString WINDOW_LAYOUT_FILENAME;

    WindowManager();
    ~WindowManager() override;

    static void encodeTab(SplitContainer *tab, bool isSelected,
                          QJsonObject &obj);
    static void encodeChannel(IndirectChannel channel, QJsonObject &obj);
    static void encodeFilters(Split *split, QJsonArray &arr);
    static IndirectChannel decodeChannel(const SplitDescriptor &descriptor);

    void showSettingsDialog(
        QWidget *parent,
        SettingsDialogPreference preference = SettingsDialogPreference());

    // Show the account selector widget at point
    void showAccountSelectPopup(QPoint point);

    // Tell a channel (or all channels if channel is nullptr) to redo their
    // layout
    void layoutChannelViews(Channel *channel = nullptr);

    // Force all channel views to redo their layout
    // This is called, for example, when the emote scale or timestamp format has
    // changed
    void forceLayoutChannelViews();
    void repaintVisibleChatWidgets(Channel *channel = nullptr);
    void repaintGifEmotes();

    Window &getMainWindow();
    Window &getSelectedWindow();
    Window &createWindow(WindowType type, bool show = true,
                         QWidget *parent = nullptr);

    // Use this method if you want to open a "new" channel in a popup. If you want to popup an
    // existing Split or SplitContainer, consider using Split::popup() or SplitContainer::popup().
    Window &openInPopup(ChannelPtr channel);

    void select(Split *split);
    void select(SplitContainer *container);
    /**
     * Scrolls to the message in a split that's not
     * a mentions view and focuses the split.
     *
     * @param message Message to scroll to.
     */
    void scrollToMessage(const MessagePtr &message);

    QPoint emotePopupPos();
    void setEmotePopupPos(QPoint pos);

    virtual void initialize(Settings &settings, Paths &paths) override;
    virtual void save() override;
    void closeAll();

    int getGeneration() const;
    void incGeneration();

    MessageElementFlags getWordFlags();
    void updateWordTypeMask();

    // Sends an alert to the main window
    // It reads the `longAlert` setting to decide whether the alert will expire
    // or not
    void sendAlert();

    // Queue up a save in the next 10 seconds
    // If a save was already queued up, we reset the to happen in 10 seconds
    // again
    void queueSave();

    /// Signals
    pajlada::Signals::NoArgSignal gifRepaintRequested;

    // This signal fires whenever views rendering a channel, or all views if the
    // channel is a nullptr, need to redo their layout
    pajlada::Signals::Signal<Channel *> layoutRequested;

    pajlada::Signals::NoArgSignal wordFlagsChanged;

    // This signal fires every 100ms and can be used to trigger random things that require a recheck.
    // It is currently being used by the "Tooltip Preview Image" system to recheck if an image is ready to be rendered.
    pajlada::Signals::NoArgSignal miscUpdate;

    pajlada::Signals::Signal<Split *> selectSplit;
    pajlada::Signals::Signal<SplitContainer *> selectSplitContainer;
    pajlada::Signals::Signal<const MessagePtr &> scrollToMessageSignal;

private:
    static void encodeNodeRecursively(SplitContainer::Node *node,
                                      QJsonObject &obj);

    // Load window layout from the window-layout.json file
    WindowLayout loadWindowLayoutFromFile() const;

    // Apply a window layout for this window manager.
    void applyWindowLayout(const WindowLayout &layout);

    // Contains the full path to the window layout file, e.g. /home/pajlada/.local/share/Chatterino/Settings/window-layout.json
    const QString windowLayoutFilePath;

    bool initialized_ = false;

    QPoint emotePopupPos_;

    std::atomic<int> generation_{0};

    std::vector<Window *> windows_;

    std::unique_ptr<FramelessEmbedWindow> framelessEmbedWindow_;
    Window *mainWindow_{};
    Window *selectedWindow_{};

    MessageElementFlags wordFlags_{};
    pajlada::SettingListener wordFlagsListener_;

    QTimer *saveTimer;
    QTimer miscUpdateTimer_;
};

}  // namespace chatterino
