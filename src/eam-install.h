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

typedef enum {
  EAM_ACTION_INSTALL,       /* Install */
  EAM_ACTION_UPDATE,        /* Update downloading the complete bundle */
  EAM_ACTION_XDELTA_UPDATE, /* Update applying xdelta diff files */
} EamAction;

/* EamAction GType definition */
#define EAM_TYPE_ACTION	(eam_action_get_type())
GType eam_action_get_type (void) G_GNUC_CONST;

struct _EamInstall
{
  GObject parent;
};

struct _EamInstallClass
{
  GObjectClass parent_class;
};

GType           eam_install_get_type            (void) G_GNUC_CONST;

EamTransaction *eam_install_new                 (const gchar *appid);

EamTransaction *eam_install_new_from_version    (const gchar *appid,
                                                 const gchar *from_version);

const char *    eam_install_get_download_url    (EamInstall  *install);
const char *    eam_install_get_signature_url   (EamInstall  *install);
const char *    eam_install_get_bundle_hash     (EamInstall  *install);

void            eam_install_set_bundle_location (EamInstall  *install,
                                                 const char  *path);

const char *    eam_install_get_app_id          (EamInstall  *install);
const char *    eam_install_get_from_version    (EamInstall  *install);
EamAction       eam_install_get_action          (EamInstall  *install);

G_END_DECLS

#endif /* EAM_INSTALL_H */
