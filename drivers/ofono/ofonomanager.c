/* @@@LICENSE
*
* Copyright (c) 2012 Simon Busch <morphis@gravedo.de>
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*
* LICENSE@@@ */

#include <string.h>
#include <errno.h>

#include <glib-object.h>
#include <gio/gio.h>

#include "utils.h"
#include "ofonomanager.h"
#include "ofonomodem.h"
#include "ofono-interface.h"

struct ofono_manager {
	OfonoInterfaceManager *remote;
	GList *modems;
	ofono_manager_modems_chanaged_cb modems_changed_cb;
	gpointer modems_changed_data;
	gulong modem_added_signal;
	gulong modem_removed_signal;
};

static void notify_modems_changed(struct ofono_manager *manager)
{
	if (manager->modems_changed_cb)
		manager->modems_changed_cb(manager->modems_changed_data);
}

static void modem_added_cb(OfonoInterfaceManager *object, const gchar *path, GVariant *properties, gpointer user_data)
{
	struct ofono_manager *manager = user_data;
	struct ofono_modem *modem = NULL;

	modem = ofono_modem_create(path);

	ofono_modem_ref(modem);
	manager->modems = g_list_append(manager->modems, modem);

	notify_modems_changed(manager);
}

static void modem_removed_cb(OfonoInterfaceManager *object, const gchar *path, gpointer user_data)
{
	struct ofono_manager *manager = user_data;
	struct ofono_modem *modem = NULL;
	GList *iter = NULL;
	const gchar *modem_path = NULL;
	bool found = false;

	for (iter = manager->modems; iter != NULL; iter = iter->next) {
		modem = (struct ofono_modem*)(iter->data);

		modem_path = ofono_modem_get_path(modem);
		if (g_strcmp0(modem_path, path) == 0) {
			found = true;
			break;
		}
	}

	if (found) {
		manager->modems = g_list_remove(manager->modems, modem);
		ofono_modem_unref(modem);
		notify_modems_changed(manager);
	}
}

static void get_modems_cb(GObject *source_object, GAsyncResult *res, gpointer user_data)
{
	struct ofono_manager *manager = user_data;
	GError *error = NULL;
	GVariant *modems = NULL;
	GVariant *child = NULL;
	gchar *path;
	int n = 0;
	struct ofono_modem *modem = NULL;
	gboolean success;

	success = ofono_interface_manager_call_get_modems_finish(manager->remote, &modems, res, &error);
	if (!success) {
		g_critical("Failed to retrieve list of available modems from manager: %s", error->message);
		g_error_free(error);
		goto done;
	}

	for (n = 0; n < g_variant_n_children(modems); n++) {
		child = g_variant_get_child_value(modems, n);

		path = g_variant_dup_string(g_variant_get_child_value(child, 0), NULL);

		modem = ofono_modem_create(path);
		manager->modems = g_list_append(manager->modems, modem);
	}

	notify_modems_changed(manager);

done:
	manager->modem_added_signal = g_signal_connect(G_OBJECT(manager->remote), "modem-added",
		G_CALLBACK(modem_added_cb), manager);

	manager->modem_removed_signal = g_signal_connect(G_OBJECT(manager->remote), "modem-removed",
		G_CALLBACK(modem_removed_cb), manager);
}

struct ofono_manager* ofono_manager_create(void)
{
	struct ofono_manager *manager;
	GError *error = NULL;

	manager = g_try_new0(struct ofono_manager, 1);
	if (!manager)
		return NULL;

	manager->modems = NULL;

	manager->remote = ofono_interface_manager_proxy_new_for_bus_sync(G_BUS_TYPE_SYSTEM,
							G_DBUS_PROXY_FLAGS_NONE, "org.ofono", "/", NULL, &error);
	if (error) {
		g_critical("Unable to initialize proxy for the org.ofono.Manager interface");
		g_error_free(error);
		g_free(manager);
		return NULL;
	}

	ofono_interface_manager_call_get_modems(manager->remote, NULL, get_modems_cb, manager);

	return manager;
}

void ofono_manager_free(struct ofono_manager *manager)
{
	if (!manager)
		return;

	if (manager->remote) {
		g_signal_handler_disconnect(manager->remote, manager->modem_added_signal);
		g_signal_handler_disconnect(manager->remote, manager->modem_removed_signal);
		g_object_unref(manager->remote);
	}

	g_list_free_full(manager->modems, (GDestroyNotify) ofono_modem_free);

	g_free(manager);
}

const GList* ofono_manager_get_modems(struct ofono_manager *manager)
{
	if (!manager)
		return NULL;

	return manager->modems;
}

void ofono_manager_set_modems_changed_callback(struct ofono_manager *manager, ofono_manager_modems_chanaged_cb cb, gpointer user_data)
{
	if (!manager)
		return;

	manager->modems_changed_cb = cb;
	manager->modems_changed_data = user_data;
}

// vim:ts=4:sw=4:noexpandtab
