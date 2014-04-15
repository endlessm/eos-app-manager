/* Copyright 2014 Endless Mobile, Inc. */

#ifndef EAM_INSTALL_H
#define EAM_INSTALL_H

#include "eam-transaction.h"

G_BEGIN_DECLS

/**
 * EamInstallError:
 * @EAM_INSTALL_ERROR_PROTOCOL_ERROR: Invalid URI.
 * @EAM_INSTALL_ERROR_INVALID_FILE: Invalid install file.
 * @EAM_INSTALL_ERROR_NO_NETWORK: The network is not available.
 *
 * These constants identify all the available errors managed by
 * the Endless Application Manager Install.
 */
typedef enum {
  EAM_INSTALL_ERROR_PROTOCOL_ERROR = 1,
  EAM_INSTALL_ERROR_INVALID_FILE,
  EAM_INSTALL_ERROR_NO_NETWORK,
} EamInstallError;

#define EAM_INSTALL_ERROR eam_install_error_quark ()

#define EAM_TYPE_INSTALL (eam_install_get_type())

#define EAM_INSTALL(o) \
  (G_TYPE_CHECK_INSTANCE_CAST ((o), EAM_TYPE_INSTALL, EamInstall))

#define EAM_INSTALL_CLASS(k) \
  (G_TYPE_CHECK_CLASS_CAST((k), EAM_TYPE_INSTALL, EamInstallClass))

#define EAM_IS_INSTALL(o)	\
  (G_TYPE_CHECK_INSTANCE_TYPE ((o), EAM_TYPE_INSTALL))

#define EAM_IS_INSTALL_CLASS(k) \
  (G_TYPE_CHECK_CLASS_TYPE ((k), EAM_TYPE_INSTALL))

#define EAM_INSTALL_GET_CLASS(o) \
  (G_TYPE_INSTANCE_GET_CLASS ((o), EAM_TYPE_INSTALL, EamInstallClass))

typedef struct _EamInstallClass	EamInstallClass;
typedef struct _EamInstall	EamInstall;

struct _EamInstall
{
  GObject parent;
};

struct _EamInstallClass
{
  GObjectClass parent_class;
};

GType           eam_install_get_type                               (void) G_GNUC_CONST;

EamTransaction *eam_install_new                                    (const gchar *appid,
                                                                    const gchar *version);

G_END_DECLS

#endif /* EAM_INSTALL_H */
