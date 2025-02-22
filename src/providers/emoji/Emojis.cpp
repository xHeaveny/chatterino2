#include "providers/emoji/Emojis.hpp"

#include "Application.hpp"
#include "common/QLogging.hpp"
#include "messages/Emote.hpp"
#include "singletons/Settings.hpp"

#include <boost/variant.hpp>
#include <QFile>
#include <rapidjson/error/en.h>
#include <rapidjson/error/error.h>
#include <rapidjson/rapidjson.h>

#include <memory>

namespace chatterino {
namespace {

    auto toneNames = std::map<QString, QString>{
        {"1F3FB", "tone1"}, {"1F3FC", "tone2"}, {"1F3FD", "tone3"},
        {"1F3FE", "tone4"}, {"1F3FF", "tone5"},
    };

    void parseEmoji(const std::shared_ptr<EmojiData> &emojiData,
                    const rapidjson::Value &unparsedEmoji,
                    QString shortCode = QString())
    {
        std::array<uint32_t, 9> unicodeBytes;

        struct {
            bool apple;
            bool google;
            bool twitter;
            bool facebook;
        } capabilities;

        if (!shortCode.isEmpty())
        {
            emojiData->shortCodes.push_back(shortCode);
        }
        else
        {
            const auto &shortCodes = unparsedEmoji["short_names"];
            for (const auto &_shortCode : shortCodes.GetArray())
            {
                emojiData->shortCodes.emplace_back(_shortCode.GetString());
            }
        }

        rj::getSafe(unparsedEmoji, "non_qualified",
                    emojiData->nonQualifiedCode);
        rj::getSafe(unparsedEmoji, "unified", emojiData->unifiedCode);

        rj::getSafe(unparsedEmoji, "has_img_apple", capabilities.apple);
        rj::getSafe(unparsedEmoji, "has_img_google", capabilities.google);
        rj::getSafe(unparsedEmoji, "has_img_twitter", capabilities.twitter);
        rj::getSafe(unparsedEmoji, "has_img_facebook", capabilities.facebook);

        if (capabilities.apple)
        {
            emojiData->capabilities.insert("Apple");
        }
        if (capabilities.google)
        {
            emojiData->capabilities.insert("Google");
        }
        if (capabilities.twitter)
        {
            emojiData->capabilities.insert("Twitter");
        }
        if (capabilities.facebook)
        {
            emojiData->capabilities.insert("Facebook");
        }

        QStringList unicodeCharacters;
        if (!emojiData->nonQualifiedCode.isEmpty())
        {
            unicodeCharacters =
                emojiData->nonQualifiedCode.toLower().split('-');
        }
        else
        {
            unicodeCharacters = emojiData->unifiedCode.toLower().split('-');
        }
        if (unicodeCharacters.length() < 1)
        {
            return;
        }

        int numUnicodeBytes = 0;

        for (const QString &unicodeCharacter : unicodeCharacters)
        {
            unicodeBytes.at(numUnicodeBytes++) =
                QString(unicodeCharacter).toUInt(nullptr, 16);
        }

        emojiData->value =
            QString::fromUcs4(unicodeBytes.data(), numUnicodeBytes);
    }

    // getToneNames takes a tones and returns their names in the same order
    // The format of the tones is: "1F3FB-1F3FB" or "1F3FB"
    // The output of the tone names is: "tone1-tone1" or "tone1"
    QString getToneNames(const QString &tones)
    {
        auto toneParts = tones.split('-');
        QStringList toneNameResults;
        for (const auto &tonePart : toneParts)
        {
            auto toneNameIt = toneNames.find(tonePart);
            if (toneNameIt == toneNames.end())
            {
                qDebug() << "Tone with key" << tonePart
                         << "does not exist in tone names map";
                continue;
            }

            toneNameResults.append(toneNameIt->second);
        }

        assert(!toneNameResults.isEmpty());

        return toneNameResults.join('-');
    }

}  // namespace

void Emojis::load()
{
    this->loadEmojis();

    this->sortEmojis();

    this->loadEmojiSet();
}

void Emojis::loadEmojis()
{
    // Current version: https://github.com/iamcal/emoji-data/blob/v14.0.0/emoji.json (Emoji version 14.0 (2022))
    QFile file(":/emoji.json");
    file.open(QFile::ReadOnly);
    QTextStream s1(&file);
    QString data = s1.readAll();
    rapidjson::Document root;
    rapidjson::ParseResult result = root.Parse(data.toUtf8(), data.length());

    if (result.Code() != rapidjson::kParseErrorNone)
    {
        qCWarning(chatterinoEmoji)
            << "JSON parse error:" << rapidjson::GetParseError_En(result.Code())
            << "(" << result.Offset() << ")";
        return;
    }

    for (const auto &unparsedEmoji : root.GetArray())
    {
        auto emojiData = std::make_shared<EmojiData>();
        parseEmoji(emojiData, unparsedEmoji);

        for (const auto &shortCode : emojiData->shortCodes)
        {
            this->emojiShortCodeToEmoji_.insert(shortCode, emojiData);
            this->shortCodes.emplace_back(shortCode);
        }

        this->emojiFirstByte_[emojiData->value.at(0)].append(emojiData);

        this->emojis.insert(emojiData->unifiedCode, emojiData);

        if (unparsedEmoji.HasMember("skin_variations"))
        {
            for (const auto &skinVariation :
                 unparsedEmoji["skin_variations"].GetObject())
            {
                auto toneName = getToneNames(skinVariation.name.GetString());
                const auto &variation = skinVariation.value;

                auto variationEmojiData = std::make_shared<EmojiData>();

                parseEmoji(variationEmojiData, variation,
                           emojiData->shortCodes[0] + "_" + toneName);

                this->emojiShortCodeToEmoji_.insert(
                    variationEmojiData->shortCodes[0], variationEmojiData);
                this->shortCodes.push_back(variationEmojiData->shortCodes[0]);

                this->emojiFirstByte_[variationEmojiData->value.at(0)].append(
                    variationEmojiData);

                this->emojis.insert(variationEmojiData->unifiedCode,
                                    variationEmojiData);
            }
        }
    }
}

void Emojis::sortEmojis()
{
    for (auto &p : this->emojiFirstByte_)
    {
        std::stable_sort(p.begin(), p.end(),
                         [](const auto &lhs, const auto &rhs) {
                             return lhs->value.length() > rhs->value.length();
                         });
    }

    auto &p = this->shortCodes;
    std::stable_sort(p.begin(), p.end(), [](const auto &lhs, const auto &rhs) {
        return lhs < rhs;
    });
}

void Emojis::loadEmojiSet()
{
#ifndef CHATTERINO_TEST
    getSettings()->emojiSet.connect([this](const auto &emojiSet) {
#else
    const QString emojiSet = "twitter";
#endif
        this->emojis.each([=](const auto &name,
                              std::shared_ptr<EmojiData> &emoji) {
            QString emojiSetToUse = emojiSet;
            // clang-format off
            static std::map<QString, QString> emojiSets = {
                // JSDELIVR
                // {"Twitter", "https://cdn.jsdelivr.net/npm/emoji-datasource-twitter@4.0.4/img/twitter/64/"},
                // {"Facebook", "https://cdn.jsdelivr.net/npm/emoji-datasource-facebook@4.0.4/img/facebook/64/"},
                // {"Apple", "https://cdn.jsdelivr.net/npm/emoji-datasource-apple@5.0.1/img/apple/64/"},
                // {"Google", "https://cdn.jsdelivr.net/npm/emoji-datasource-google@4.0.4/img/google/64/"},
                // {"Messenger", "https://cdn.jsdelivr.net/npm/emoji-datasource-messenger@4.0.4/img/messenger/64/"},

                // OBRODAI
                {"Twitter", "https://pajbot.com/static/emoji-v2/img/twitter/64/"},
                {"Facebook", "https://pajbot.com/static/emoji-v2/img/facebook/64/"},
                {"Apple", "https://pajbot.com/static/emoji-v2/img/apple/64/"},
                {"Google", "https://pajbot.com/static/emoji-v2/img/google/64/"},

                // Cloudflare+B2 bucket
                // {"Twitter", "https://chatterino2-emoji-cdn.pajlada.se/file/c2-emojis/emojis-v1/twitter/64/"},
                // {"Facebook", "https://chatterino2-emoji-cdn.pajlada.se/file/c2-emojis/emojis-v1/facebook/64/"},
                // {"Apple", "https://chatterino2-emoji-cdn.pajlada.se/file/c2-emojis/emojis-v1/apple/64/"},
                // {"Google", "https://chatterino2-emoji-cdn.pajlada.se/file/c2-emojis/emojis-v1/google/64/"},
            };
            // clang-format on

            if (emoji->capabilities.count(emojiSetToUse) == 0)
            {
                emojiSetToUse = "Twitter";
            }

            QString code = emoji->unifiedCode.toLower();
            QString urlPrefix =
                "https://pajbot.com/static/emoji-v2/img/twitter/64/";
            auto it = emojiSets.find(emojiSetToUse);
            if (it != emojiSets.end())
            {
                urlPrefix = it->second;
            }
            QString url = urlPrefix + code + ".png";
            emoji->emote = std::make_shared<Emote>(Emote{
                EmoteName{emoji->value}, ImageSet{Image::fromUrl({url}, 0.35)},
                Tooltip{":" + emoji->shortCodes[0] + ":<br/>Emoji"}, Url{}});
        });
#ifndef CHATTERINO_TEST
    });
#endif
}

std::vector<boost::variant<EmotePtr, QString>> Emojis::parse(
    const QString &text)
{
    auto result = std::vector<boost::variant<EmotePtr, QString>>();
    int lastParsedEmojiEndIndex = 0;

    for (auto i = 0; i < text.length(); ++i)
    {
        const QChar character = text.at(i);

        if (character.isLowSurrogate())
        {
            continue;
        }

        auto it = this->emojiFirstByte_.find(character);
        if (it == this->emojiFirstByte_.end())
        {
            // No emoji starts with this character
            continue;
        }

        const auto &possibleEmojis = it.value();

        int remainingCharacters = text.length() - i - 1;

        std::shared_ptr<EmojiData> matchedEmoji;

        int matchedEmojiLength = 0;

        for (const std::shared_ptr<EmojiData> &emoji : possibleEmojis)
        {
            int emojiExtraCharacters = emoji->value.length() - 1;
            if (emojiExtraCharacters > remainingCharacters)
            {
                // It cannot be this emoji, there's not enough space for it
                continue;
            }

            bool match = true;

            for (int j = 1; j < emoji->value.length(); ++j)
            {
                if (text.at(i + j) != emoji->value.at(j))
                {
                    match = false;

                    break;
                }
            }

            if (match)
            {
                matchedEmoji = emoji;
                matchedEmojiLength = emoji->value.length();

                break;
            }
        }

        if (matchedEmojiLength == 0)
        {
            continue;
        }

        int currentParsedEmojiFirstIndex = i;
        int currentParsedEmojiEndIndex = i + (matchedEmojiLength);

        int charactersFromLastParsedEmoji =
            currentParsedEmojiFirstIndex - lastParsedEmojiEndIndex;

        if (charactersFromLastParsedEmoji > 0)
        {
            // Add characters inbetween emojis
            result.emplace_back(text.mid(lastParsedEmojiEndIndex,
                                         charactersFromLastParsedEmoji));
        }

        // Push the emoji as a word to parsedWords
        result.emplace_back(matchedEmoji->emote);

        lastParsedEmojiEndIndex = currentParsedEmojiEndIndex;

        i += matchedEmojiLength - 1;
    }

    if (lastParsedEmojiEndIndex < text.length())
    {
        // Add remaining characters
        result.emplace_back(text.mid(lastParsedEmojiEndIndex));
    }

    return result;
}

QString Emojis::replaceShortCodes(const QString &text)
{
    QString ret(text);
    auto it = this->findShortCodesRegex_.globalMatch(text);

    int32_t offset = 0;

    while (it.hasNext())
    {
        auto match = it.next();

        auto capturedString = match.captured();

        QString matchString =
            capturedString.toLower().mid(1, capturedString.size() - 2);

        auto emojiIt = this->emojiShortCodeToEmoji_.constFind(matchString);

        if (emojiIt == this->emojiShortCodeToEmoji_.constEnd())
        {
            continue;
        }

        auto emojiData = emojiIt.value();

        ret.replace(offset + match.capturedStart(), match.capturedLength(),
                    emojiData->value);

        offset += emojiData->value.size() - match.capturedLength();
    }

    return ret;
}

}  // namespace chatterino
