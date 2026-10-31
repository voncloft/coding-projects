#include <QQmlExtensionPlugin>
#include <qqml.h>

#include "caldavmodel.h"
#include "configstore.h"

class RadicaleAgendaPlugin : public QQmlExtensionPlugin {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QQmlExtensionInterface_iid FILE "radicaleagenda_plugin.json")

public:
    void registerTypes(const char *uri) override {
        qmlRegisterType<CalDavModel>(uri, 1, 0, "CalDavModel");
        qmlRegisterType<ConfigStore>(uri, 1, 0, "ConfigStore");
    }
};

#include "plugin.moc"
