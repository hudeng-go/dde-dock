#include <QDir>
#include <QLibrary>
#include <QPluginLoader>
//#include <QFileSystemWatcher>

#include "dockpluginproxy.h"
#include "dockpluginmanager.h"
#include "interfaces/dockplugininterface.h"

DockPluginManager::DockPluginManager(QObject *parent) :
    QObject(parent)
{
    m_settingFrame = new PluginsSettingFrame;

    m_searchPaths << "/usr/lib/dde-dock/plugins/";

    //    m_watcher = new QFileSystemWatcher(this);
    //    m_watcher->addPaths(m_searchPaths);

    //    connect(m_watcher, &QFileSystemWatcher::fileChanged, this, &DockPluginManager::watchedFileChanged);
    //    connect(m_watcher, &QFileSystemWatcher::directoryChanged, this, &DockPluginManager::watchedDirectoryChanged);
}

void DockPluginManager::initAll()
{
    foreach (QString path, m_searchPaths) {
        QDir pluginsDir(path);

        foreach (QString fileName, pluginsDir.entryList(QDir::Files)) {
            QString pluginPath = pluginsDir.absoluteFilePath(fileName);

            this->loadPlugin(pluginPath);
        }
    }

    foreach (DockPluginProxy * proxy, m_proxies.values()) {
        connect(proxy, &DockPluginProxy::configurableChanged, [=](const QString &id) {
            if (proxy->plugin()->configurable(id)) {
                m_settingFrame->onPluginAdd(proxy->plugin()->enabled(id),
                                            id,
                                            proxy->plugin()->getName(id),
                                            proxy->plugin()->getIcon(id));
            }
            else {
                m_settingFrame->onPluginRemove(id);
            }
        });
        connect(proxy, &DockPluginProxy::enabledChanged, [=](const QString &id) {
            m_settingFrame->onPluginEnabledChanged(id, proxy->plugin()->enabled(id));
        });
        connect(proxy, &DockPluginProxy::titleChanged, [=](const QString &id) {
            m_settingFrame->onPluginTitleChanged(id, proxy->plugin()->getName(id));
        });

        proxy->plugin()->init(proxy);
    }

    initSettingWindow();
}

void DockPluginManager::onPluginsSetting(int y)
{
    m_settingFrame->move(QCursor::pos().x(), y - m_settingFrame->height());
    m_settingFrame->show();
}

// public slots
void DockPluginManager::onDockModeChanged(Dock::DockMode newMode, Dock::DockMode oldMode)
{
    if (newMode == oldMode)
        return;

    for (DockPluginProxy * proxy : m_proxies) {
        DockPluginInterface * plugin = proxy->plugin();
        plugin->changeMode(newMode, oldMode);
    }

    //reanchor systray-plugin
    AbstractDockItem *sysItem = m_sysPlugins.key(SYSTRAY_PLUGIN_ID);
    m_sysPlugins.remove(sysItem);
    emit itemRemoved(sysItem);
    handleSysPluginAdd(sysItem, SYSTRAY_PLUGIN_ID);
}

// private methods
DockPluginProxy * DockPluginManager::loadPlugin(const QString &path)
{
    // check the file type
    if (!QLibrary::isLibrary(path)) return NULL;

    QPluginLoader * pluginLoader = new QPluginLoader(path);

    // check the apiVersion the plugin uses
    double apiVersion = pluginLoader->metaData()["MetaData"].toObject()["api_version"].toDouble();
    if (apiVersion != PLUGIN_API_VERSION) return NULL;


    QObject *plugin = pluginLoader->instance();

    if (plugin) {
        DockPluginInterface * interface = qobject_cast<DockPluginInterface*>(plugin);

        if (interface) {
            qDebug() << "Plugin loaded: " << path;

            DockPluginProxy * proxy = new DockPluginProxy(pluginLoader, interface);
            if (proxy) {
                m_proxies[path] = proxy;
//                m_watcher->addPath(path);
                connect(proxy, &DockPluginProxy::itemAdded, this, &DockPluginManager::onPluginItemAdded);
                connect(proxy, &DockPluginProxy::itemRemoved, this, &DockPluginManager::onPluginItemRemoved);
                connect(m_settingFrame, &PluginsSettingFrame::checkedChanged, [=](QString uuid, bool checked){
                    //NOTE:one sender, multi receiver
                    if (interface->ids().indexOf(uuid) != -1) {
                        interface->setEnabled(uuid, checked);
                    }
                });

                return proxy;
            }
        } else {
            qWarning() << "Load plugin failed(failed to convert) " << path;
        }
    } else {
        qWarning() << "Load plugin failed" << pluginLoader->errorString();
    }

    return NULL;
}

void DockPluginManager::unloadPlugin(const QString &path)
{
    if (m_proxies.contains(path)) {
        DockPluginProxy * proxy = m_proxies.take(path);
        delete proxy;
    }
}

void DockPluginManager::initSettingWindow()
{
    foreach (DockPluginProxy *proxy, m_proxies.values()) {
        QStringList ids = proxy->plugin()->ids();
        foreach (QString uuid, ids) {
            if (proxy->plugin()->configurable(uuid)){
                m_settingFrame->onPluginAdd(proxy->plugin()->enabled(uuid),
                                            uuid,
                                            proxy->plugin()->getName(uuid),
                                            proxy->plugin()->getIcon(uuid));
            }
        }
    }
}

void DockPluginManager::onPluginItemAdded(AbstractDockItem *item, QString uuid)
{
    DockPluginProxy *proxy = qobject_cast<DockPluginProxy *>(sender());
    if (!proxy)
        return;

    if (proxy->isSystemPlugin())
        handleSysPluginAdd(item, uuid);
    else
        handleNormalPluginAdd(item, uuid);
}

void DockPluginManager::onPluginItemRemoved(AbstractDockItem *item, QString)
{
    m_sysPlugins.remove(item);
    m_normalPlugins.remove(item);

    emit itemRemoved(item);
    item->setVisible(false);
    item->deleteLater();
}

// private slots
void DockPluginManager::watchedFileChanged(const QString & file)
{
    qDebug() << "DockPluginManager::watchedFileChanged" << file;
    this->unloadPlugin(file);

    if (QFile::exists(file)) {
        DockPluginProxy * proxy = loadPlugin(file);

        if (proxy) proxy->plugin()->init(proxy);
    }
}

void DockPluginManager::watchedDirectoryChanged(const QString & directory)
{
    qDebug() << "DockPluginManager::watchedDirectoryChanged" << directory;
    // we just need to take care of the situation that new files pop up in
    // our watched directory.
    QDir targetDir(directory);
    foreach (QString fileName, targetDir.entryList(QDir::Files)) {
        QString absPath = targetDir.absoluteFilePath(fileName);
        if (!m_proxies.contains(absPath)) {
            DockPluginProxy * proxy = loadPlugin(absPath);

            if (proxy) proxy->plugin()->init(proxy);
        }
    }
}

AbstractDockItem *DockPluginManager::sysPluginItem(QString id)
{
    int si = m_sysPlugins.values().indexOf(id);

    if (si != -1)
        return m_sysPlugins.keys().at(si);
    else
        return NULL;
}

void DockPluginManager::handleSysPluginAdd(AbstractDockItem *item, QString uuid)
{
    if (!item || m_sysPlugins.values().indexOf(uuid) != -1)
        return;

    m_sysPlugins.insert(item, uuid);

    if (uuid == SYSTRAY_PLUGIN_ID) {
        if (m_dockModeData->getDockMode() != Dock::FashionMode) {
            emit itemAppend(item);
        }
        else {
            emit itemInsert(sysPluginItem(DATETIME_PLUGIN_ID), item);
        }
    }
    else {
        emit itemInsert(NULL, item);
    }
}

void DockPluginManager::handleNormalPluginAdd(AbstractDockItem *item, QString uuid)
{
    if (!item || m_normalPlugins.values().indexOf(uuid) != -1)
        return;

    if (m_dockModeData->getDockMode() != Dock::FashionMode) {
        //Normal plug next date plugins to ensure trayicon displayed in the far left
        emit itemInsert(sysPluginItem(DATETIME_PLUGIN_ID), item);
    }
    else {
        //Normal plug placed in the far left on Fashion Mode
        emit itemAppend(item);
    }

    m_normalPlugins.insert(item, uuid);

}
