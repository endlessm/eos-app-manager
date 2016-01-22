/* eam-uninstall.h: Uninstall transaction
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

#pragma once
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
                                                 const char   *path);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (EamUninstall, g_object_unref)

G_END_DECLS
