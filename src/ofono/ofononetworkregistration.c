/* @@@LICENSE
*
* Copyright (c) 2012 networkon Busch <morphis@gravedo.de>
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
#include "ofonobase.h"
#include "ofononetworkregistration.h"
#include "ofono-interface.h"

struct ofono_network_registration {
	gchar *path;
	OfonoInterfaceNetworkRegistration *remote;
	struct ofono_base *base;
	int ref_count;
	enum ofono_network_registration_mode mode;
	enum ofono_network_status status;
	unsigned int location_area_code;
	unsigned int cell_id;
	gchar *mcc;
	gchar *mnc;
	enum ofono_network_technology technology;
	gchar *operator_name;
	unsigned int strength;
	gchar *base_station;
	ofono_property_changed_cb status_changed_cb;
	void *status_changed_data;
	ofono_property_changed_cb strength_changed_cb;
	void *strength_changed_data;
};

static enum ofono_network_registration_mode parse_ofono_network_registration_mode(const gchar *mode)
{
	if (g_str_equal(mode, "auto"))
		return OFONO_NETWORK_REGISTRATION_MODE_AUTO;
	else if (g_str_equal(mode, "auto-only"))
		return OFONO_NETWORK_REGISTRATION_MODE_AUTO_ONLY;
	else if (g_str_equal(mode, "manual"))
		return OFONO_NETWORK_REGISTRATION_MODE_MANUAL;

	return OFONO_NETWORK_REGISTRATION_MODE_UNKNOWN;
}

static enum ofono_network_status parse_ofono_network_status(const gchar *status)
{
	if (g_str_equal(status, "unregistered"))
		return OFONO_NETWORK_REGISTRATION_STATUS_UNREGISTERED;
	else if (g_str_equal(status, "registered"))
		return OFONO_NETWORK_REGISTRATION_STATUS_REGISTERED;
	else if (g_str_equal(status, "searching"))
		return OFONO_NETWORK_REGISTRATION_STATUS_SEARCHING;
	else if (g_str_equal(status, "denied"))
		return OFONO_NETWORK_REGISTRATION_STATUS_DENIED;
	else if (g_str_equal(status, "unknown"))
		return OFONO_NETWORK_REGISTRATION_STATUS_UNKNOWN;
	else if (g_str_equal(status, "roaming"))
		return OFONO_NETWORK_REGISTRATION_STATUS_ROAMING;

	return OFONO_NETWORK_REGISTRATION_STATUS_UNKNOWN;
}

static enum ofono_network_technology parse_ofono_network_technology(const gchar *technology)
{
	if (g_str_equal(technology, "gsm"))
		return OFONO_NETWORK_TECHNOLOGOY_GSM;
	else if (g_str_equal(technology, "edge"))
		return OFONO_NETWORK_TECHNOLOGOY_EDGE;
	else if (g_str_equal(technology, "umts"))
		return OFONO_NETWORK_TECHNOLOGOY_UMTS;
	else if (g_str_equal(technology, "hspa"))
		return OFONO_NETWORK_TECHNOLOGOY_HSPA;
	else if (g_str_equal(technology, "lte"))
		return OFONO_NETWORK_TECHNOLOGOY_LTE;

	return OFONO_NETWORK_TECHNOLOGOY_UNKNOWN;
}

static void update_property(const gchar *name, GVariant *value, void *user_data)
{
	struct ofono_network_registration *netreg = user_data;

	g_message("[NetworkRegistration:%s] property %s changed", netreg->path, name);

	if (g_str_equal(name, "Mode"))
		netreg->mode = parse_ofono_network_registration_mode(g_variant_dup_string(value, NULL));
	else if (g_str_equal(name, "Status")) {
		netreg->status = parse_ofono_network_status(g_variant_dup_string(value, NULL));
		if (netreg->status_changed_cb != NULL)
			netreg->status_changed_cb(netreg->status_changed_data);
	}
	else if (g_str_equal(name, "LocationAreaCode"))
		netreg->location_area_code = g_variant_get_uint16(value);
	else if (g_str_equal(name, "CellId"))
		netreg->cell_id = g_variant_get_uint32(value);
	else if (g_str_equal(name, "MobileCountryCode"))
		netreg->mcc = g_variant_dup_string(value, NULL);
	else if (g_str_equal(name, "MobileNetworkCode"))
		netreg->mnc = g_variant_dup_string(value, NULL);
	else if (g_str_equal(name, "Technology"))
		netreg->technology = parse_ofono_network_technology(g_variant_dup_string(value, NULL));
	else if (g_str_equal(name, "Name"))
		netreg->operator_name = g_variant_dup_string(value, NULL);
	else if (g_str_equal(name, "Strength")) {
		netreg->strength = g_variant_get_byte(value);
		if (netreg->strength_changed_cb != NULL)
			netreg->strength_changed_cb(netreg->strength_changed_data);
	}
	else if (g_str_equal(name, "BaseStation"))
		netreg->base_station = g_variant_dup_string(value, NULL);
}

struct ofono_base_funcs netreg_base_funcs = {
	.update_property = update_property,
	.set_property = ofono_interface_network_registration_call_set_property,
	.set_property_finish = ofono_interface_network_registration_call_set_property_finish,
	.get_properties = ofono_interface_network_registration_call_get_properties,
	.get_properties_finish = ofono_interface_network_registration_call_get_properties_finish
};

struct ofono_network_registration* ofono_network_registration_create(const gchar *path)
{
	struct ofono_network_registration *netreg;
	GError *error = NULL;

	netreg = g_try_new0(struct ofono_network_registration, 1);
	if (!netreg)
		return NULL;

	netreg->remote = ofono_interface_network_registration_proxy_new_for_bus_sync(G_BUS_TYPE_SYSTEM,
							G_DBUS_PROXY_FLAGS_NONE, "org.ofono", path, NULL, &error);
	if (error) {
		g_error("Unable to initialize proxy for the org.ofono.network interface");
		g_error_free(error);
		g_free(netreg);
		return NULL;
	}

	netreg->path = g_strdup(path);

	netreg->base = ofono_base_create(&netreg_base_funcs, netreg->remote, netreg);

	return netreg;
}

void ofono_network_registration_ref(struct ofono_network_registration *netreg)
{
	if (!netreg)
		return;

	__sync_fetch_and_add(&netreg->ref_count, 1);
}

void ofono_network_registration_unref(struct ofono_network_registration *netreg)
{
	if (!netreg)
		return;

	if (__sync_sub_and_fetch(&netreg->ref_count, 1))
		return;

	ofono_network_registration_free(netreg);
}

void ofono_network_registration_free(struct ofono_network_registration *netreg)
{
	if (!netreg)
		return;

	if (netreg->remote)
		g_object_unref(netreg->remote);

	g_free(netreg);
}

const gchar* ofono_network_registration_get_path(struct ofono_network_registration *netreg)
{
	if (!netreg)
		return NULL;

	return netreg->path;
}

void ofono_network_registration_register_status_changed_handler(struct ofono_network_registration *netreg, ofono_property_changed_cb cb, void *data)
{
	if (!netreg)
		return NULL;

	netreg->status_changed_cb = cb;
	netreg->status_changed_data = data;
}

void ofono_network_registration_register_strength_changed_handler(struct ofono_network_registration *netreg, ofono_property_changed_cb cb, void *data)
{
	if (!netreg)
		return NULL;

	netreg->strength_changed_cb = cb;
	netreg->strength_changed_data = data;
}

enum ofono_network_registration_mode ofono_network_registration_get_mode(struct ofono_network_registration *netreg)
{
	if (!netreg)
		return OFONO_NETWORK_REGISTRATION_MODE_UNKNOWN;

	return netreg->mode;
}

enum ofono_network_status ofono_network_registration_get_status(struct ofono_network_registration *netreg)
{
	if (!netreg)
		return OFONO_NETWORK_REGISTRATION_STATUS_UNKNOWN;

	return netreg->status;
}

unsigned int ofono_network_registration_get_strength(struct ofono_network_registration *netreg)
{
	if (!netreg)
		return 0;

	return netreg->strength;
}

unsigned int ofono_network_registration_get_location_area_code(struct ofono_network_registration *netreg)
{
	if (!netreg)
		return 0;

	return netreg->location_area_code;
}

unsigned int ofono_network_registration_get_cell_id(struct ofono_network_registration *netreg)
{
	if (!netreg)
		return 0;

	return netreg->cell_id;
}

const gchar* ofono_network_registration_get_mcc(struct ofono_network_registration *netreg)
{
	if (!netreg)
		return NULL;

	return netreg->mcc;
}

const gchar* ofono_network_registration_get_mnc(struct ofono_network_registration *netreg)
{
	if (!netreg)
		return NULL;

	return netreg->mnc;
}

enum ofono_network_technology ofono_network_registration_get_technology(struct ofono_network_registration *netreg)
{
	if (!netreg)
		return OFONO_NETWORK_TECHNOLOGOY_UNKNOWN;

	return netreg->technology;
}

const gchar* ofono_network_registration_get_operator_name(struct ofono_network_registration *netreg)
{
	if (!netreg)
		return NULL;

	return netreg->operator_name;
}

const gchar* ofono_network_registration_get_base_station(struct ofono_network_registration *netreg)
{
	if (!netreg)
		return NULL;

	return netreg->base_station;
}

// vim:ts=4:sw=4:noexpandtab