#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gio/gio.h>

#include "shared-values.h"
#include "hud.interface.h"
#include "hud-dbus.h"

struct _HudDbusPrivate {
	GDBusConnection * bus;
	GCancellable * bus_lookup;
	guint bus_registration;
};

#define HUD_DBUS_GET_PRIVATE(o) \
(G_TYPE_INSTANCE_GET_PRIVATE ((o), HUD_DBUS_TYPE, HudDbusPrivate))

static void hud_dbus_class_init (HudDbusClass *klass);
static void hud_dbus_init       (HudDbus *self);
static void hud_dbus_dispose    (GObject *object);
static void hud_dbus_finalize   (GObject *object);

static void bus_got_cb          (GObject *object, GAsyncResult * res, gpointer user_data);
static void bus_method          (GDBusConnection *connection,
                                 const gchar *sender,
                                 const gchar *object_path,
                                 const gchar *interface_name,
                                 const gchar *method_name,
                                 GVariant *parameters,
                                 GDBusMethodInvocation *invocation,
                                 gpointer user_data);
static GVariant * get_suggestions (const gchar * query);
static void execute_query (const gchar * query);


G_DEFINE_TYPE (HudDbus, hud_dbus, G_TYPE_OBJECT);
static GDBusNodeInfo * node_info = NULL;
static GDBusInterfaceInfo * iface_info = NULL;
static GDBusInterfaceVTable bus_vtable = {
	method_call: bus_method,
	get_property: NULL,
	set_property: NULL,
};

static void
hud_dbus_class_init (HudDbusClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (HudDbusPrivate));

	object_class->dispose = hud_dbus_dispose;
	object_class->finalize = hud_dbus_finalize;

	if (node_info == NULL) {
		GError * error = NULL;

		node_info = g_dbus_node_info_new_for_xml(hud_interface, &error);
		if (error != NULL) {
			g_error("Unable to parse HUD interface: %s", error->message);
			g_error_free(error);
		}
	}

	if (node_info != NULL && iface_info == NULL) {
		iface_info = g_dbus_node_info_lookup_interface(node_info, DBUS_IFACE);
		if (iface_info == NULL) {
			g_error("Unable to find interface '" DBUS_IFACE "'");
		}
	}

	return;
}

static void
hud_dbus_init (HudDbus *self)
{
	self->priv = HUD_DBUS_GET_PRIVATE(self);

	self->priv->bus = NULL;
	self->priv->bus_lookup = NULL;
	self->priv->bus_registration = 0;

	self->priv->bus_lookup = g_cancellable_new();
	g_bus_get(G_BUS_TYPE_SESSION, self->priv->bus_lookup, bus_got_cb, self);

	return;
}

static void
hud_dbus_dispose (GObject *object)
{
	HudDbus * self = HUD_DBUS(object);
	g_return_if_fail(self != NULL);

	if (self->priv->bus_lookup != NULL) {
		g_cancellable_cancel(self->priv->bus_lookup);
		g_object_unref(self->priv->bus_lookup);
		self->priv->bus_lookup = NULL;
	}

	if (self->priv->bus_registration != 0) {
		g_dbus_connection_unregister_object(self->priv->bus, self->priv->bus_registration);
		self->priv->bus_registration = 0;
	}

	if (self->priv->bus != NULL) {
		g_object_unref(self->priv->bus);
		self->priv->bus = NULL;
	}

	G_OBJECT_CLASS (hud_dbus_parent_class)->dispose (object);
	return;
}

static void
hud_dbus_finalize (GObject *object)
{

	G_OBJECT_CLASS (hud_dbus_parent_class)->finalize (object);
	return;
}

HudDbus *
hud_dbus_new (void)
{
	return g_object_new(HUD_DBUS_TYPE, NULL);
}

static void
bus_got_cb (GObject *object, GAsyncResult * res, gpointer user_data)
{
	GError * error = NULL;
	HudDbus * self = HUD_DBUS(user_data);
	GDBusConnection * bus;

	bus = g_bus_get_finish(res, &error);
	if (error != NULL) {
		g_critical("Unable to get bus: %s", error->message);
		g_error_free(error);
		return;
	}

	self->priv->bus = bus;

	/* Register object */
	self->priv->bus_registration = g_dbus_connection_register_object(bus,
	                                                /* path */       DBUS_PATH,
	                                                /* interface */  iface_info,
	                                                /* vtable */     &bus_vtable,
	                                                /* userdata */   self,
	                                                /* destroy */    NULL,
	                                                /* error */      &error);

	return;
}

static void
bus_method (GDBusConnection *connection, const gchar *sender, const gchar *object_path, const gchar *interface_name, const gchar *method_name, GVariant *parameters, GDBusMethodInvocation *invocation, gpointer user_data)
{
	if (g_strcmp0(method_name, "GetSuggestions") == 0) {
		GVariant * ret = NULL;
		gchar * query = NULL;

		g_variant_get(parameters, "(s)", &query);

		ret = get_suggestions(query);

		g_dbus_method_invocation_return_value(invocation, ret);
		g_free(query);
	} else if (g_strcmp0(method_name, "ExecuteQuery") == 0) {
		gchar * query = NULL;

		g_variant_get(parameters, "(s)", &query);

		execute_query(query);
		
		g_dbus_method_invocation_return_value(invocation, NULL);
		g_free(query);
	}

	return;
}

static GVariant *
get_suggestions (const gchar * query)
{
	GVariantBuilder ret;
	GVariantBuilder suggests;

	g_variant_builder_init(&ret, G_VARIANT_TYPE_TUPLE);

	g_variant_builder_add_value(&ret, g_variant_new_string("New Document"));

	g_variant_builder_init(&suggests, G_VARIANT_TYPE_ARRAY);

	g_variant_builder_add_value(&suggests, g_variant_new_string("Suggestion 1"));
	g_variant_builder_add_value(&suggests, g_variant_new_string("Suggestion 2"));
	g_variant_builder_add_value(&suggests, g_variant_new_string("Suggestion 3"));
	g_variant_builder_add_value(&suggests, g_variant_new_string("Suggestion 4"));
	g_variant_builder_add_value(&suggests, g_variant_new_string("Suggestion 5"));

	g_variant_builder_add_value(&ret, g_variant_builder_end(&suggests));
	return g_variant_builder_end(&ret);
}

static void
execute_query (const gchar * query)
{
	g_debug("Execute: %s", query);

	return;
}
