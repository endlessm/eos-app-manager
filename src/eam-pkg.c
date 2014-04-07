/* Copyright 2014 Endless Mobile, Inc. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "eam-pkg.h"

G_DEFINE_BOXED_TYPE (EamPkg, eam_pkg, eam_pkg_copy, eam_pkg_free)

void
eam_pkg_free (EamPkg *pkg)
{
  g_free (pkg->id);
  g_free (pkg->name);

  if (pkg->version)
    eam_pkg_version_free (pkg->version);

  g_slice_free (EamPkg, pkg);
}

EamPkg *
eam_pkg_copy (EamPkg *pkg)
{
  EamPkg *copy = g_slice_new (EamPkg);
  copy->id = g_strdup (pkg->id);
  copy->name = g_strdup (pkg->name);
  copy->version = eam_pkg_version_copy (pkg->version);

  return copy;
}

static inline EamPkg *
create_pkg (gchar *id, gchar *name, const gchar *ver)
{
  EamPkgVersion *version = eam_pkg_version_new_from_string (ver);
  if (!version)
    return NULL;

  EamPkg *pkg = g_slice_new (EamPkg);
  pkg->id = id;
  pkg->name = name;
  pkg->version = version;

  return pkg;
}

static EamPkg *
eam_pkg_load_from_keyfile (GKeyFile *keyfile, GError **error)
{
  gchar *start_group, *ver, *id, *name;

  ver = id = name = NULL;

  start_group = g_key_file_get_start_group (keyfile);
  if (g_ascii_strcasecmp (start_group, "bundle"))
    goto bail;

  id = g_key_file_get_string (keyfile, start_group, "appId", error);
  if (!id)
    goto bail;

  name = g_key_file_get_string (keyfile, start_group, "appName", error);
  if (!name)
    goto bail;

  ver = g_key_file_get_string (keyfile, start_group, "codeVersion", error);
  if (!ver)
    goto bail;

  EamPkg *pkg = create_pkg (id, name, ver);

  g_free (start_group);
  g_free (ver); /* we don't need it anymore */

  return pkg;

bail:
  g_free (id);
  g_free (ver);
  g_free (name);
  g_free (start_group);

  return NULL;
}

/**
 * eam_pkg_new_from_keyfile:
 * @keyfile: the #GKeyFile to load
 * @error: return location for a #GError, or %NULL
 *
 * Creates a new #EamPkg based on a parsed key-file
 *
 * Returns: a new #EamPkg, or %NULL if the key-file is not valid
 */
EamPkg *
eam_pkg_new_from_keyfile (GKeyFile *keyfile, GError **error)
{
  g_return_val_if_fail (keyfile, NULL);

  return eam_pkg_load_from_keyfile (keyfile, error);
}

/**
 * eam_pkg_new_from_filename:
 * @filename: the bundle info file
 * @error: return location for a #GError, or %NULL
 *
 * Creates a new #EamPkg based on a bundle info file
 *
 * Returns: a new #EamPkg, or %NULL if the info file is not valid
 */
EamPkg *
eam_pkg_new_from_filename (const gchar *filename, GError **error)
{
  g_return_val_if_fail (filename, NULL);

  GKeyFile *keyfile = g_key_file_new ();
  EamPkg *pkg = NULL;

  if (g_key_file_load_from_file (keyfile, filename, G_KEY_FILE_NONE, error))
    pkg = eam_pkg_load_from_keyfile (keyfile, error);

  g_key_file_unref (keyfile);

  return pkg;
}

/**
 * eam_pkg_new_from_json_object:
 * @json: a #JsonObject struct
 *
 * Creates a new #EamPkg based on a JSON object
 *
 * Returns: a new #EamPkg, or %NULL if the JSON object is not valid.
 */
EamPkg *
eam_pkg_new_from_json_object (JsonObject *json)
{
  g_return_val_if_fail (json, NULL);

  const gchar *ver;
  gchar *id, *name;
  JsonNode *node;

  id = name = NULL;

  node = json_object_get_member (json, "appId");
  if (!node)
    goto bail;
  id = json_node_dup_string (node);

  node = json_object_get_member (json, "appName");
  if (!node)
    goto bail;
  name = json_node_dup_string (node);

  node = json_object_get_member (json, "codeVersion");
  if (!node)
    goto bail;
  ver = json_node_get_string (node);

  return create_pkg (id, name, ver);

bail:
  g_free (id);
  g_free (name);

  return NULL;
}
