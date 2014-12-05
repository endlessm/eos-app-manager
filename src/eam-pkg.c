/* Copyright 2014 Endless Mobile, Inc. */

#include "config.h"

#include <glib/gi18n.h>

#include "eam-error.h"
#include "eam-pkg.h"

/**
 * EamPkg:
 *
 * Boxed struct that represents a Package, installed or to update.
 */
struct _EamPkg
{
  gchar *id;
  gchar *name;
  EamPkgVersion *version;
  gchar *locale;
  gboolean secondary;
};

G_DEFINE_BOXED_TYPE (EamPkg, eam_pkg, eam_pkg_copy, eam_pkg_free)

static const gchar *KEYS[] = { "app_id", "app_name", "version", "locale", "secondary_storage" };
static const gchar *JSON_KEYS[] = { "appId", "appName", "codeVersion", "Locale", "secondaryStorage" };
enum { APP_ID, APP_NAME, CODE_VERSION, LOCALE, SECONDARY_STORAGE };

void
eam_pkg_free (EamPkg *pkg)
{
  g_free (pkg->id);
  g_free (pkg->name);
  g_free (pkg->locale);

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
  copy->locale = g_strdup (pkg->locale);
  copy->secondary = pkg->secondary;

  return copy;
}

static inline EamPkg *
create_pkg (gchar *id,
            gchar *name,
            const gchar *ver,
            gchar *locale,
            gboolean secondary)
{
  EamPkgVersion *version = eam_pkg_version_new_from_string (ver);
  if (!version)
    return NULL;

  EamPkg *pkg = g_slice_new (EamPkg);
  pkg->id = id;
  pkg->name = name;
  pkg->version = version;
  pkg->locale = locale;
  pkg->secondary = secondary;

  return pkg;
}

#define GROUP   "Bundle"

static EamPkg *
eam_pkg_load_from_keyfile (GKeyFile *keyfile, GError **error)
{
  gchar *ver, *id, *name, *locale;
  gboolean secondary = FALSE;

  ver = id = name = locale = NULL;

  if (!g_key_file_has_group (keyfile, GROUP))
    goto bail;

  id = g_key_file_get_string (keyfile, GROUP, KEYS[APP_ID], error);
  if (!id)
    goto bail;

  name = g_key_file_get_string (keyfile, GROUP, KEYS[APP_NAME], error);
  if (!name)
    goto bail;

  ver = g_key_file_get_string (keyfile, GROUP, KEYS[CODE_VERSION], error);
  if (!ver)
    goto bail;

  /* "locale" is not required */
  locale = g_key_file_get_string (keyfile, GROUP, KEYS[LOCALE], NULL);

  /* "secondary_storage" is not required */
  secondary = g_key_file_get_boolean (keyfile, GROUP, KEYS[SECONDARY_STORAGE], NULL);

  EamPkg *pkg = create_pkg (id, name, ver, locale, secondary);

  g_free (ver); /* we don't need it anymore */

  return pkg;

bail:
  g_free (id);
  g_free (ver);
  g_free (name);
  g_free (locale);

  return NULL;
}

#undef GROUP

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
eam_pkg_new_from_json_object (JsonObject *json, GError **error)
{
  g_return_val_if_fail (json, NULL);

  const gchar *ver, *key;
  gchar *id, *name, *loc;
  JsonNode *node;
  gboolean secondary;

  id = name = loc = NULL;
  secondary = FALSE;

  key = JSON_KEYS[APP_ID];
  node = json_object_get_member (json, key);
  if (!node)
    goto bail;
  id = json_node_dup_string (node);

  key = JSON_KEYS[APP_NAME];
  node = json_object_get_member (json, key);
  if (!node)
    goto bail;
  name = json_node_dup_string (node);

  key = JSON_KEYS[CODE_VERSION];
  node = json_object_get_member (json, key);
  if (!node)
    goto bail;
  ver = json_node_get_string (node);

  /* "locale" is not required */
  key = JSON_KEYS[LOCALE];
  node = json_object_get_member (json, key);
  if (node)
    loc = json_node_dup_string (node);

  /* "secondaryStorage" is not required */
  key = JSON_KEYS[SECONDARY_STORAGE];
  node = json_object_get_member (json, key);
  if (node)
    secondary = json_node_get_boolean (node);

  return create_pkg (id, name, ver, loc, secondary);

bail:
  g_free (id);
  g_free (name);
  g_free (loc);

  g_set_error (error, EAM_ERROR, EAM_ERROR_INVALID_FILE,
    _("Key \"%s\" was not found"), key);

  return NULL;
}

const gchar *
eam_pkg_get_id (const EamPkg *pkg)
{
  return pkg->id;
}

const gchar *
eam_pkg_get_name (const EamPkg *pkg)
{
  return pkg->name;
}

EamPkgVersion *
eam_pkg_get_version (const EamPkg *pkg)
{
  return pkg->version;
}

const gchar *
eam_pkg_get_locale (const EamPkg *pkg)
{
  if (pkg->locale != NULL)
    return pkg->locale;

  return "All";
}

gboolean
eam_pkg_is_on_secondary_storage (const EamPkg *pkg)
{
  return pkg->secondary;
}
