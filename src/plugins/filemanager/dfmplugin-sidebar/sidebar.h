/*
 * Copyright (C) 2021 Uniontech Software Technology Co., Ltd.
 *
 * Author:     zhangsheng<zhangsheng@uniontech.com>
 *
 * Maintainer: max-lv<lvwujun@uniontech.com>
 *             lanxuesong<lanxuesong@uniontech.com>
 *             xushitong<xushitong@uniontech.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef SIDEBAR_H
#define SIDEBAR_H

#include "dfmplugin_sidebar_global.h"

#include <dfm-framework/dpf.h>

DPSIDEBAR_BEGIN_NAMESPACE

class SideBarWidget;
class SideBar : public dpf::Plugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.deepin.plugin.filemanager" FILE "sidebar.json")

    DPF_EVENT_NAMESPACE(DPSIDEBAR_NAMESPACE)

    // slot events
    DPF_EVENT_REG_SLOT(slot_SetContextMenuEnable)   // TODO(xust) tmp solution, using GroupPolicy instead.

public:
    virtual void initialize() override;
    virtual bool start() override;
    virtual ShutdownFlag stop() override;

private slots:
    void onWindowOpened(quint64 windId);
    void onWindowClosed(quint64 winId);
};

DPSIDEBAR_END_NAMESPACE

#endif   // SIDEBAR_H
