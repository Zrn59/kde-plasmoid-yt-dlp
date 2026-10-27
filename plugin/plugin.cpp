// SPDX-License-Identifier: MIT

#include <QQmlEngine>
#include <QQmlExtensionPlugin>

#include <KLocalizedString>

#include "ytdlpbackend.h"
#include "ytdlpactivemodel.h"

class YtdlpPlugin : public QQmlExtensionPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QQmlExtensionInterface_iid)

public:
    void registerTypes(const char *uri) override
    {
        if (KLocalizedString::applicationDomain().isEmpty()) {
            KLocalizedString::setApplicationDomain("com.zrn59.ytdlp");
        }
        qmlRegisterType<YtdlpBackend>(uri, 1, 0, "YtdlpBackend");
        qmlRegisterUncreatableType<YtdlpActiveModel>(
            uri,
            1,
            0,
            "YtdlpActiveModel",
            QStringLiteral("Provided by YtdlpBackend"));
    }
};

#include "plugin.moc"
