/* eam-install.h: Installation transaction
 *
 * This file is part of eos-app-manager.
 * Copyright 2014  Endless Mobile Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef EAM_INSTALL_H
#define EAM_INSTALL_H

#include "eam-transaction.h"

G_BEGIN_DECLS

#define EAM_TYPE_INSTALL (eam_install_get_type())

#define EAM_INSTALL(o) \
  (G_TYPE_CHECK_INSTANCE_CAST ((o), EAM_TYPE_INSTALL, EamInstall))

#define EAM_INSTALL_CLASS(k) \
  (G_TYPE_CHECK_CLASS_CAST((k), EAM_TYPE_INSTALL, EamInstallClass))

#define EAM_IS_INSTALL(o)        \
  (G_TYPE_CHECK_INSTANCE_TYPE ((o), EAM_TYPE_INSTALL))

#define EAM_IS_INSTALL_CLASS(k) \
  (G_TYPE_CHECK_CLASS_TYPE ((k), EAM_TYPE_INSTALL))

#define EAM_INSTALL_GET_CLASS(o) \
  (G_TYPE_INSTANCE_GET_CLASS ((o), EAM_TYPE_INSTALL, EamInstallClass))

typedef struct _EamInstallClass        EamInstallClass;
typedef struct _EamInstall        EamInstall;

struct _EamInstall
{
  GObject parent;
};

struct _EamInstallClass
{
  GObjectClass parent_class;
};

GType eam_install_get_type (void) G_GNUC_CONST;

EamTransaction *        eam_install_new                 (const char *appid);

void                    eam_install_set_prefix          (EamInstall *install,
                                                         const char *path);
void                    eam_install_set_bundle_file     (EamInstall *install,
                                                         const char *path);
void                    eam_install_set_signature_file  (EamInstall *install,
                                                         const char *path);

const char *            eam_install_get_app_id          (EamInstall *install);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (EamInstall, g_object_unref)

G_END_DECLS

#endif /* EAM_INSTALL_H */
