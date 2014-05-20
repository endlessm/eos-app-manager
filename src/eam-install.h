/* Copyright 2014 Endless Mobile, Inc. */

#ifndef EAM_INSTALL_H
#define EAM_INSTALL_H

#include "eam-transaction.h"

G_BEGIN_DECLS

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

EamInstall     *eam_install_new_with_scripts                       (const gchar *appid,
                                                                    const gchar *scriptdir,
                                                                    const gchar *rollbackdir);

void            eam_install_run_async                              (EamInstall *install,
                                                                    GCancellable *cancellable,
                                                                    GAsyncReadyCallback callback,
                                                                    gpointer data);

gboolean        eam_install_finish                                 (EamInstall *install,
                                                                    GAsyncResult *res,
                                                                    GError **error);

G_END_DECLS

#endif /* EAM_INSTALL_H */
