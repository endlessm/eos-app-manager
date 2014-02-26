/* Copyright 2014 Endless Mobile, Inc. */

#ifndef EAM_DBUS_SERVER_H
#define EAM_DBUS_SERVER_H

#include <glib-object.h>

G_BEGIN_DECLS

#define EAM_TYPE_DBUS_SERVER (eam_dbus_server_get_type())

#define EAM_DBUS_SERVER(o) \
  (G_TYPE_CHECK_INSTANCE_CAST ((o), EAM_TYPE_DBUS_SERVER, EamDbusServer))

#define EAM_DBUS_SERVER_CLASS(k) \
  (G_TYPE_CHECK_CLASS_CAST((k), EAM_TYPE_DBUS_SERVER, EamDbusServerClass))

#define EAM_IS_DBUS_SERVER(o) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((o), EAM_TYPE_DBUS_SERVER))

#define EAM_IS_DBUS_SERVER_CLASS(k) \
  (G_TYPE_CHECK_CLASS_TYPE ((k), EAM_TYPE_DBUS_SERVER))

#define EAM_DBUS_SERVER_GET_CLASS(o) \
  (G_TYPE_INSTANCE_GET_CLASS ((o), EAM_TYPE_DBUS_SERVER, EamDbusServerClass))

typedef struct _EamDbusServerClass EamDbusServerClass;
typedef struct _EamDbusServer EamDbusServer;

struct _EamDbusServer {
  GObject parent;
};

struct _EamDbusServerClass {
  GObjectClass parent_class;
};

GType          eam_dbus_server_get_type                          (void) G_GNUC_CONST;

EamDbusServer *eam_dbus_server_new                               (void);

gboolean       eam_dbus_server_run                               (EamDbusServer *server);

void           eam_dbus_server_quit                              (EamDbusServer *server);

G_END_DECLS

#endif /* EAM_DBUS_SERVER_H */
