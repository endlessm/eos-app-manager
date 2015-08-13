/* Copyright 2014 Endless Mobile, Inc. */

#ifndef EAM_SERVICE_H
#define EAM_SERVICE_H

#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define EAM_TYPE_SERVICE (eam_service_get_type())

#define EAM_SERVICE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), EAM_TYPE_SERVICE, EamService))

#define EAM_IS_SERVICE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EAM_TYPE_SERVICE))

#define EAM_SERVICE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), EAM_TYPE_SERVICE, EamServiceClass))

#define EAM_IS_SERVICE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), EAM_TYPE_SERVICE))

#define EAM_SERVICE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), EAM_TYPE_SERVICE, EamServiceClass))

typedef struct _EamService        EamService;
typedef struct _EamServiceClass   EamServiceClass;

struct _EamService {
  GObject parent;
};

struct _EamServiceClass {
  GObjectClass parent_class;
};

GType           eam_service_get_type                              (void) G_GNUC_CONST;

EamService     *eam_service_new                                   (void);

void            eam_service_dbus_register                         (EamService *service,
                                                                   GDBusConnection *connection);

guint           eam_service_get_idle                              (EamService *service);

gboolean        eam_service_load_authority                        (EamService *service,
                                                                   GError **error);

void            eam_service_pop_busy                              (EamService *service);
void            eam_service_reset_timer                           (EamService *service);
char *          eam_service_get_next_transaction_path             (EamService *service);

G_END_DECLS

#endif /* EAM_SERVICE_H */
