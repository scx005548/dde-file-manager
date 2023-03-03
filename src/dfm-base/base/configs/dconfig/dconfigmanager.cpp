// SPDX-FileCopyrightText: 2022 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dconfigmanager.h"
#include "private/dconfigmanager_p.h"

#include <DConfig>

#include <QDebug>
#include <QSet>

static constexpr char kCfgAppId[] { "org.deepin.dde.file-manager" };

using namespace dfmbase;
DCORE_USE_NAMESPACE

#define DCONFIG_SUPPORTED (DTK_VERSION >= DTK_VERSION_CHECK(5, 5, 30, 0))

DConfigManager::DConfigManager(QObject *parent)
    : QObject(parent), d(new DConfigManagerPrivate(this))
{
    addConfig(kDefaultCfgPath);
}

DConfigManager *DConfigManager::instance()
{
    static DConfigManager ins;
    return &ins;
}

DConfigManager::~DConfigManager()
{
#if DCONFIG_SUPPORTED
    QWriteLocker locker(&d->lock);

    auto configs = d->configs.values();
    std::for_each(configs.begin(), configs.end(), [](DConfig *cfg) { delete cfg; });
    d->configs.clear();
#endif
}

bool DConfigManager::addConfig(const QString &config, QString *err)
{
#if DCONFIG_SUPPORTED
    QWriteLocker locker(&d->lock);

    if (d->configs.contains(config)) {
        if (err)
            *err = "config is already added";
        return false;
    }

    auto cfg = DConfig::create(kCfgAppId, config, "", this);
    if (!cfg) {
        if (err)
            *err = "cannot create config";
        return false;
    }

    if (!cfg->isValid()) {
        if (err)
            *err = "config is not valid";
        delete cfg;
        return false;
    }

    d->configs.insert(config, cfg);
    locker.unlock();
    connect(cfg, &DConfig::valueChanged, this, [=](const QString &key) { Q_EMIT valueChanged(config, key); });
#endif
    return true;
}

bool DConfigManager::removeConfig(const QString &config, QString *err)
{
#if DCONFIG_SUPPORTED
    QWriteLocker locker(&d->lock);

    if (d->configs.contains(config)) {
        delete d->configs[config];
        d->configs.remove(config);
    }
#endif
    return true;
}

QStringList DConfigManager::keys(const QString &config) const
{
#if DCONFIG_SUPPORTED
    QReadLocker locker(&d->lock);

    if (!d->configs.contains(config))
        return QStringList();

    return d->configs[config]->keyList();
#else
    return QStringList();
#endif
}

bool DConfigManager::contains(const QString &config, const QString &key) const
{
    return key.isEmpty() ? false : keys(config).contains(key);
}

QVariant DConfigManager::value(const QString &config, const QString &key, const QVariant &fallback) const
{
#if DCONFIG_SUPPORTED
    QReadLocker locker(&d->lock);

    if (d->configs.contains(config))
        return d->configs.value(config)->value(key, fallback);
    else
        qWarning() << "Config: " << config << "is not registered!!!";
    return fallback;
#else
    return QVariant();
#endif
}

void DConfigManager::setValue(const QString &config, const QString &key, const QVariant &value)
{
#if DCONFIG_SUPPORTED
    QReadLocker locker(&d->lock);

    if (d->configs.contains(config))
        d->configs.value(config)->setValue(key, value);
#endif
}

bool DConfigManager::validateConfigs(QStringList &invalidConfigs) const
{
#if DCONFIG_SUPPORTED
    QReadLocker locker(&d->lock);

    bool ret = true;
    for (auto iter = d->configs.cbegin(); iter != d->configs.cend(); ++iter) {
        bool valid = iter.value()->isValid();
        if (!valid)
            invalidConfigs << iter.key();
        ret &= valid;
    }
    return ret;
#else
    return true;
#endif
}