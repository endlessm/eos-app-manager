/* Copyright 2014 Endless Mobile, Inc. */

#ifndef EAM_SERVICE_H
#define EAM_SERVICE_H

#include <glib-object.h>
#include <gio/gio.h>

#include "eam-pkgdb.h"

G_BEGIN_DECLS

/**
 * EamServiceError:
 * @EAM_SERIVCE_ERROR_BUSY: The serivice is currently busy with other task.
 * @EAM_SERVICE_ERROR_PKG_UNKNOWN: Requested package is unknown.
 * @EAM_SERVICE_ERROR_UNIMPLEMENTED: Asked for an unimplemented feature.
 *
 * These constants identify all the available errors managed by
 * the Endless Application Manager Service.
 */
typedef enum {
  EAM_SERVICE_ERROR_BUSY = 1,
  EAM_SERVICE_ERROR_PKG_UNKNOWN,
  EAM_SERVICE_ERROR_UNIMPLEMENTED,
  EAM_SERVICE_ERROR_AUTHORIZATION,
  EAM_SERVICE_ERROR_NOT_AUTHORIZED,
} EamServiceError;

#define EAM_SERVICE_ERROR eam_service_error_quark ()

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

GQuark          eam_service_error_quark                           (void);

GType           eam_service_get_type                              (void) G_GNUC_CONST;

EamService     *eam_service_new                                   (EamPkgdb *db);

void            eam_service_dbus_register                         (EamService *service,
								   GDBusConnection *connection);

guint           eam_service_get_idle                              (EamService *service);

gboolean        eam_service_load_authority                        (EamService *service,
                                                                   GError **error);

G_END_DECLS

#endif /* EAM_SERVICE_H */
