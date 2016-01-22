/* eam-update.h: Update transaction
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

#define EAM_TYPE_UPDATE (eam_update_get_type())

#define EAM_UPDATE(o) \
  (G_TYPE_CHECK_INSTANCE_CAST ((o), EAM_TYPE_UPDATE, EamUpdate))

#define EAM_UPDATE_CLASS(k) \
  (G_TYPE_CHECK_CLASS_CAST((k), EAM_TYPE_UPDATE, EamUpdateClass))

#define EAM_IS_UPDATE(o)        \
  (G_TYPE_CHECK_INSTANCE_TYPE ((o), EAM_TYPE_UPDATE))

#define EAM_IS_UPDATE_CLASS(k) \
  (G_TYPE_CHECK_CLASS_TYPE ((k), EAM_TYPE_UPDATE))

#define EAM_UPDATE_GET_CLASS(o) \
  (G_TYPE_INSTANCE_GET_CLASS ((o), EAM_TYPE_UPDATE, EamUpdateClass))

typedef struct _EamUpdateClass        EamUpdateClass;
typedef struct _EamUpdate        EamUpdate;

struct _EamUpdate
{
  GObject parent;
};

struct _EamUpdateClass
{
  GObjectClass parent_class;
};

GType eam_update_get_type (void) G_GNUC_CONST;

EamTransaction *        eam_update_new                  (const gchar    *appid);

void                    eam_update_set_source_prefix    (EamUpdate      *update,
                                                         const char     *path);
void                    eam_update_set_target_prefix    (EamUpdate      *update,
                                                         const char     *path);
void                    eam_update_set_bundle_file      (EamUpdate      *update,
                                                         const char     *path);
void                    eam_update_set_signature_file   (EamUpdate      *update,
                                                         const char     *path);

const char *            eam_update_get_app_id           (EamUpdate  *update);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (EamUpdate, g_object_unref)

G_END_DECLS
