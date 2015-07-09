/* Copyright 2014 Endless Mobile, Inc. */

#ifndef EAM_UNINSTALL_H
#define EAM_UNINSTALL_H

#include "eam-transaction.h"

G_BEGIN_DECLS

#define EAM_TYPE_UNINSTALL (eam_uninstall_get_type())

#define EAM_UNINSTALL(o) \
  (G_TYPE_CHECK_INSTANCE_CAST ((o), EAM_TYPE_UNINSTALL, EamUninstall))

#define EAM_UNINSTALL_CLASS(k) \
  (G_TYPE_CHECK_CLASS_CAST((k), EAM_TYPE_UNINSTALL, EamUninstallClass))

#define EAM_IS_UNINSTALL(o) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((o), EAM_TYPE_UNINSTALL))

#define EAM_IS_UNINSTALL_CLASS(k) \
  (G_TYPE_CHECK_CLASS_TYPE ((k), EAM_TYPE_UNINSTALL))

#define EAM_UNINSTALL_GET_CLASS(o) \
  (G_TYPE_INSTANCE_GET_CLASS ((o), EAM_TYPE_UNINSTALL, EamUninstallClass))

typedef struct _EamUninstallClass EamUninstallClass;
typedef struct _EamUninstall EamUninstall;

struct _EamUninstall
{
  GObject parent;
};

struct _EamUninstallClass
{
  GObjectClass parent_class;
};

GType eam_uninstall_get_type (void) G_GNUC_CONST;

EamTransaction *eam_uninstall_new               (const gchar *appid);

void            eam_uninstall_set_force         (EamUninstall *uninstall,
                                                 gboolean      force);
void            eam_uninstall_set_prefix        (EamUninstall *uninstall,
                                                 const char   *prefix);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (EamUninstall, g_object_unref)

G_END_DECLS

#endif /* EAM_UNINSTALL_H */
