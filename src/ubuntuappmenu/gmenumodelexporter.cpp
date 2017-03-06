/*
 * Copyright (C) 2016 Canonical, Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License version 3, as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranties of MERCHANTABILITY,
 * SATISFACTORY QUALITY, or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

// Local
#include "gmenumodelexporter.h"
#include "registry.h"
#include "logging.h"

#include <QDebug>

#include <functional>

namespace {

// Derive an action name from the label by removing spaces and Capitilizing the words.
// Also remove mnemonics from the label.
inline QString getActionString(QString label)
{
    QRegExp re("\\W");
    label = label.replace(QRegExp("(&|_)"), "");
    QStringList parts = label.split(re, QString::SkipEmptyParts);

    QString result;
    Q_FOREACH(const QString& part, parts) {
        result += part[0].toUpper();
        result += part.right(part.length()-1);
    }
    return result;
}

static void activate_cb(GSimpleAction *action, GVariant *, gpointer user_data)
{
    qCDebug(ubuntuappmenu, "Activate menu action '%s'", g_action_get_name(G_ACTION(action)));
    auto item = static_cast<UbuntuPlatformMenuItem*>(user_data);
    item->activated();
}

static uint s_menuId = 0;

#define MENU_OBJECT_PATH "/com/ubuntu/Menu/%1"

} // namespace


UbuntuMenuBarExporter::UbuntuMenuBarExporter(UbuntuPlatformMenuBar * bar)
    : UbuntuGMenuModelExporter(bar)
{
    qCDebug(ubuntuappmenu, "UbuntuMenuBarExporter::UbuntuMenuBarExporter");

    connect(bar, &UbuntuPlatformMenuBar::structureChanged, this, [this]() {
        m_structureTimer.start();
    });
    connect(&m_structureTimer, &QTimer::timeout, this, [this, bar]() {
        clear();
        Q_FOREACH(QPlatformMenu *platformMenu, bar->menus()) {
            GMenuItem* item = createSubmenu(platformMenu, nullptr);
            if (item) {
                g_menu_append_item(m_gmainMenu, item);
                g_object_unref(item);
            }
        }
    });

    connect(bar, &UbuntuPlatformMenuBar::ready, this, [this]() {
        exportModels();
    });
}

UbuntuMenuBarExporter::~UbuntuMenuBarExporter()
{
    qCDebug(ubuntuappmenu, "UbuntuMenuBarExporter::~UbuntuMenuBarExporter");
}

UbuntuMenuExporter::UbuntuMenuExporter(UbuntuPlatformMenu *menu)
    : UbuntuGMenuModelExporter(menu)
{
    qCDebug(ubuntuappmenu, "UbuntuMenuExporter::UbuntuMenuExporter");

    connect(menu, &UbuntuPlatformMenu::structureChanged, this, [this]() {
        m_structureTimer.start();
    });
    connect(&m_structureTimer, &QTimer::timeout, this, [this, menu]() {
        clear();
        addSubmenuItems(menu, m_gmainMenu);
    });
    addSubmenuItems(menu, m_gmainMenu);
}

UbuntuMenuExporter::~UbuntuMenuExporter()
{
    qCDebug(ubuntuappmenu, "UbuntuMenuExporter::~UbuntuMenuExporter");
}

UbuntuGMenuModelExporter::UbuntuGMenuModelExporter(QObject *parent)
    : QObject(parent)
    , m_connection(nullptr)
    , m_gmainMenu(g_menu_new())
    , m_gactionGroup(g_simple_action_group_new())
    , m_exportedModel(0)
    , m_exportedActions(0)
    , m_menuPath(QStringLiteral(MENU_OBJECT_PATH).arg(s_menuId++))
{
    m_structureTimer.setSingleShot(true);
    m_structureTimer.setInterval(0);
}

UbuntuGMenuModelExporter::~UbuntuGMenuModelExporter()
{
    unexportModels();
    clear();

    g_object_unref(m_gmainMenu);
    g_object_unref(m_gactionGroup);
}

// Clear the menu and actions that have been created.
void UbuntuGMenuModelExporter::clear()
{
    Q_FOREACH(const QMetaObject::Connection& connection, m_propertyConnections) {
        QObject::disconnect(connection);
    }

    g_menu_remove_all(m_gmainMenu);

    Q_FOREACH(const QByteArray& action, m_actions) {
        g_action_map_remove_action(G_ACTION_MAP(m_gactionGroup), action.constData());
    }
    m_actions.clear();
}

static const gchar introspection_xml[] =
  "<node>"
  "  <interface name='qtubuntu.actions.extra'>"
  "    <method name='aboutToShow'>"
  "      <arg type='t' name='tag' direction='in'/>"
  "    </method>"
  "  </interface>"
  "</node>";

static void handle_method_call (GDBusConnection       *,
                    const gchar           *,
                    const gchar           *,
                    const gchar           *,
                    const gchar           *method_name,
                    GVariant              *parameters,
                    GDBusMethodInvocation *invocation,
                    gpointer               user_data)
{

    if (g_strcmp0 (method_name, "aboutToShow") == 0)
    {
        UbuntuGMenuModelExporter *obj = (UbuntuGMenuModelExporter *)user_data;
        guint64 tag;
        g_variant_get (parameters, "(t)", &tag);
        obj->aboutToShow(tag);

        g_dbus_method_invocation_return_value (invocation, NULL);
    }
}


static const GDBusInterfaceVTable interface_vtable =
{
  handle_method_call,
  NULL,
  NULL,
  NULL
};


// Export the model on dbus
void UbuntuGMenuModelExporter::exportModels()
{
    GError *error = nullptr;
    m_connection = g_bus_get_sync (G_BUS_TYPE_SESSION, nullptr, &error);
    if (!m_connection) {
        qCWarning(ubuntuappmenu, "Failed to retreive session bus - %s", error ? error->message : "unknown error");
        g_error_free (error);
        return;
    }

    QByteArray menuPath(m_menuPath.toUtf8());

    if (m_exportedModel == 0) {
        m_exportedModel = g_dbus_connection_export_menu_model(m_connection, menuPath.constData(), G_MENU_MODEL(m_gmainMenu), &error);
        if (m_exportedModel == 0) {
            qCWarning(ubuntuappmenu, "Failed to export menu - %s", error ? error->message : "unknown error");
            g_error_free (error);
            error = nullptr;
        } else {
            qCDebug(ubuntuappmenu, "Exported menu on %s", g_dbus_connection_get_unique_name(m_connection));
        }
    }

    if (m_exportedActions == 0) {
        m_exportedActions = g_dbus_connection_export_action_group(m_connection, menuPath.constData(), G_ACTION_GROUP(m_gactionGroup), &error);
        if (m_exportedActions == 0) {
            qCWarning(ubuntuappmenu, "Failed to export actions - %s", error ? error->message : "unknown error");
            g_error_free (error);
            error = nullptr;
        } else {
            qCDebug(ubuntuappmenu, "Exported actions on %s", g_dbus_connection_get_unique_name(m_connection));
        }
    }

    GDBusNodeInfo *introspection_data = g_dbus_node_info_new_for_xml (introspection_xml, NULL);
    auto res = g_dbus_connection_register_object (m_connection, menuPath.constData(),
                            introspection_data->interfaces[0],
                            &interface_vtable,
                            this,
                            nullptr,
                            &error
    );
    qDebug() << "res" << res;
    // TODO do something with result value
}

void UbuntuGMenuModelExporter::aboutToShow(quint64 tag)
{
    UbuntuPlatformMenu* gplatformMenu = m_submenusWithTag.value(tag);
    if (!gplatformMenu) {
        qWarning() << "Got an aboutToShow call with an unknown tag";
        return;
    }

    gplatformMenu->aboutToShow();
}

// Unexport the model
void UbuntuGMenuModelExporter::unexportModels()
{
    GError *error = nullptr;
    if (!m_connection) {
        qCWarning(ubuntuappmenu, "Failed to retreive session bus - %s", error ? error->message : "unknown error");
        return;
    }

    if (m_exportedModel != 0) {
        g_dbus_connection_unexport_menu_model(m_connection, m_exportedModel);
        m_exportedModel = 0;
    }
    if (m_exportedActions != 0) {
        g_dbus_connection_unexport_action_group(m_connection, m_exportedActions);
        m_exportedActions = 0;
    }
    g_object_unref(m_connection);
    m_connection = nullptr;
}

// Create a submenu for the given platform menu.
// Returns a gmenuitem entry for the menu, which must be cleaned up using g_object_unref.
// If forItem is suplied, use it's label.
GMenuItem *UbuntuGMenuModelExporter::createSubmenu(QPlatformMenu *platformMenu, UbuntuPlatformMenuItem *forItem)
{
    UbuntuPlatformMenu* gplatformMenu = static_cast<UbuntuPlatformMenu*>(platformMenu);
    if (!gplatformMenu) return nullptr;

    GMenu* menu = g_menu_new();

    QByteArray label;
    if (forItem) {
        label = UbuntuPlatformMenuItem::get_text(forItem).toUtf8();
    } else {
        label = UbuntuPlatformMenu::get_text(gplatformMenu).toUtf8();
    }

    addSubmenuItems(gplatformMenu, menu);

    GMenuItem* gmenuItem = g_menu_item_new_submenu(label.constData(), G_MENU_MODEL(menu));
    const quint64 tag = gplatformMenu->tag();
    if (tag != 0) {
        g_menu_item_set_attribute_value(gmenuItem, "qtubuntu-tag", g_variant_new_uint64 (tag));
        m_submenusWithTag.insert(gplatformMenu->tag(), gplatformMenu);
        connect(gplatformMenu, &UbuntuPlatformMenu::destroyed,
                this, [this, tag] { m_submenusWithTag.remove(tag); });
    }
    g_object_unref(menu);

    connect(gplatformMenu, &UbuntuPlatformMenu::structureChanged, this, [this, gplatformMenu, menu]
        {
            // TODO clean stuff?
            // TODO hide behind a timer so that it doesn't get called a bazillion times
            g_menu_remove_all(menu);

            addSubmenuItems(gplatformMenu, menu);
        });


    return gmenuItem;
}

// Add a platform menu's items to the given gmenu.
// The items are inserted into menus sections, split by the menu separators.
void UbuntuGMenuModelExporter::addSubmenuItems(UbuntuPlatformMenu* gplatformMenu, GMenu* menu)
{
    auto iter = gplatformMenu->menuItems().begin();
    auto lastSectionStart = iter;
    // Iterate through all the menu items adding sections when a separator is found.
    for (; iter != gplatformMenu->menuItems().end(); ++iter) {
        UbuntuPlatformMenuItem* gplatformMenuItem = static_cast<UbuntuPlatformMenuItem*>(*iter);
        if (!gplatformMenuItem) continue;

        // don't add a section until we have separator
        if (UbuntuPlatformMenuItem::get_separator(gplatformMenuItem)) {
            if (lastSectionStart != gplatformMenu->menuItems().begin()) {
                GMenuItem* section = createSection(lastSectionStart, iter);
                g_menu_append_item(menu, section);
                g_object_unref(section);
            }
            lastSectionStart = iter + 1;
        } else if (lastSectionStart == gplatformMenu->menuItems().begin()) {
            processItemForGMenu(gplatformMenuItem, menu);
        }
    }

    // Add the last section
    if (lastSectionStart != gplatformMenu->menuItems().begin() &&
            lastSectionStart != gplatformMenu->menuItems().end()) {
        GMenuItem* gsectionItem = createSection(lastSectionStart, gplatformMenu->menuItems().end());
        g_menu_append_item(menu, gsectionItem);
        g_object_unref(gsectionItem);
    }
}

// Create and return a gmenu item for the given platform menu item.
// Returned GMenuItem must be cleaned up using g_object_unref
GMenuItem *UbuntuGMenuModelExporter::createMenuItem(QPlatformMenuItem *platformMenuItem)
{
    UbuntuPlatformMenuItem* gplatformMenuItem = static_cast<UbuntuPlatformMenuItem*>(platformMenuItem);
    if (!gplatformMenuItem) return nullptr;

    QByteArray label(UbuntuPlatformMenuItem::get_text(gplatformMenuItem).toUtf8());
    QByteArray actionLabel(getActionString(UbuntuPlatformMenuItem::get_text(gplatformMenuItem)).toUtf8());
    QByteArray shortcut(UbuntuPlatformMenuItem::get_shortcut(gplatformMenuItem).toString(QKeySequence::NativeText).toUtf8());

    GMenuItem* gmenuItem = g_menu_item_new(label.constData(), nullptr);
    g_menu_item_set_attribute(gmenuItem, "accel", "s", shortcut.constData());
    g_menu_item_set_detailed_action(gmenuItem, ("unity." + actionLabel).constData());

    addAction(actionLabel, gplatformMenuItem);

    return gmenuItem;
}

// Create a menu section for a section of separated menu items.
// Returned GMenuItem must be cleaned up using g_object_unref
GMenuItem *UbuntuGMenuModelExporter::createSection(QList<QPlatformMenuItem *>::const_iterator iter, QList<QPlatformMenuItem *>::const_iterator end)
{
    GMenu* gsectionMenu = g_menu_new();
    for (; iter != end; ++iter) {
        processItemForGMenu(*iter, gsectionMenu);
    }
    GMenuItem* gsectionItem = g_menu_item_new_section("", G_MENU_MODEL(gsectionMenu));
    g_object_unref(gsectionMenu);
    return gsectionItem;
}

// Add the given platform menu item to the menu.
// If it has an attached submenu, then create and add the submenu.
void UbuntuGMenuModelExporter::processItemForGMenu(QPlatformMenuItem *platformMenuItem, GMenu *gmenu)
{
    UbuntuPlatformMenuItem* gplatformMenuItem = static_cast<UbuntuPlatformMenuItem*>(platformMenuItem);
    if (!gplatformMenuItem) return;

    GMenuItem* gmenuItem = gplatformMenuItem->menu() ? createSubmenu(gplatformMenuItem->menu(), gplatformMenuItem) :
                                                       createMenuItem(gplatformMenuItem);
    if (gmenuItem) {
        g_menu_append_item(gmenu, gmenuItem);
        g_object_unref(gmenuItem);
    }
}

// Create and add an action for a menu item.
void UbuntuGMenuModelExporter::addAction(const QByteArray &name, UbuntuPlatformMenuItem *gplatformMenuItem)
{
    disconnect(gplatformMenuItem, &UbuntuPlatformMenuItem::checkedChanged, this, 0);
    disconnect(gplatformMenuItem, &UbuntuPlatformMenuItem::enabledChanged, this, 0);

    if (m_actions.contains(name)) {
        g_action_map_remove_action(G_ACTION_MAP(m_gactionGroup), name.constData());
        m_actions.remove(name);
    }

    bool checkable = UbuntuPlatformMenuItem::get_checkable(gplatformMenuItem);

    GSimpleAction* action = nullptr;
    if (checkable) {
        bool checked = UbuntuPlatformMenuItem::get_checked(gplatformMenuItem);
        action = g_simple_action_new_stateful(name.constData(), nullptr, g_variant_new_boolean(checked));

        std::function<void(bool)> updateChecked = [gplatformMenuItem, action](bool checked) {
            auto type = g_action_get_state_type(G_ACTION(action));
            if (type && g_variant_type_equal(type, G_VARIANT_TYPE_BOOLEAN)) {
                g_simple_action_set_state(action, g_variant_new_boolean(checked ? TRUE : FALSE));
            }
        };
        // save the connection to disconnect in UbuntuGMenuModelExporter::clear()
        m_propertyConnections << connect(gplatformMenuItem, &UbuntuPlatformMenuItem::checkedChanged, this, updateChecked);
    } else {
        action = g_simple_action_new(name.constData(), nullptr);
    }

    // Enabled update
    std::function<void(bool)> updateEnabled = [gplatformMenuItem, action](bool enabled) {
        GValue value = G_VALUE_INIT;
        g_value_init (&value, G_TYPE_BOOLEAN);
        g_value_set_boolean(&value, enabled ? TRUE : FALSE);
        g_object_set_property(G_OBJECT(action), "enabled", &value);
    };
    updateEnabled(UbuntuPlatformMenuItem::get_enabled(gplatformMenuItem));
    // save the connection to disconnect in UbuntuGMenuModelExporter::clear()
    m_propertyConnections << connect(gplatformMenuItem, &UbuntuPlatformMenuItem::enabledChanged, this, updateEnabled);

    g_signal_connect(action, "activate", G_CALLBACK(activate_cb), gplatformMenuItem);

    m_actions.insert(name);
    g_action_map_add_action(G_ACTION_MAP(m_gactionGroup), G_ACTION(action));
    g_object_unref(action);
}
