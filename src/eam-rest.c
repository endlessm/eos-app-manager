/* Copyright 2014 Endless Mobile, Inc. */

#include "eam-rest.h"
#include "eam-os.h"
#include "eam-config.h"

static const gchar *METHODS_V1_FORMAT[] = {
  "%s/api/v1/updates/%s?arch=%s",    /* ALL_UPDATES     */
  "%s/api/v1/updates/%s/%s?arch=%s", /* APP_UPDATE      */
  "%s/api/v1/updates/%s/%s",         /* APP_UPDATE_LINK */
  "%s/api/v1/updates/blob/%s",       /* APP_UPDATE_BLOB */
};

static gchar *
build_uri_v1 (EamRestMethod method, const gchar *saddr, va_list args)
{
  const gchar *osver = eam_os_get_version ();
  const gchar *osarch = eam_os_get_architecture ();

  switch (method) {
  case EAM_REST_API_V1_GET_ALL_UPDATES:{
    return g_strdup_printf (METHODS_V1_FORMAT[method], saddr, osver, osarch);
  }
  case EAM_REST_API_V1_GET_APP_UPDATES:{
    const gchar *appid = va_arg (args, const gchar *);
    if (appid)
      return g_strdup_printf (METHODS_V1_FORMAT[method], saddr, osver, appid,
                              osarch);

    break;
  }
  case EAM_REST_API_V1_GET_APP_UPDATE_LINK:{
    const gchar *appid = va_arg (args, const gchar *);
    const gchar *appver = va_arg (args, const gchar *);
    gchar *ret = NULL;
    if (appid) {
      ret = g_strdup_printf (METHODS_V1_FORMAT[method], saddr, osver, appid);
      if (ret) {
        /*
         * If the app version is specified, then we can build a full
         * endpoint url with /arch. Otherwise, we have to pass arch as a
         * parameter.
         */
        char *tmp = ret;
        if (appver)
          ret = g_strconcat (tmp, "/", appver, "/", osarch, NULL);
        else
          ret = g_strconcat (tmp, "?arch=", osarch, NULL);
        g_free (tmp);
      }
      return ret;
    }

    break;
  }
  case EAM_REST_API_V1_GET_APP_UPDATE_BLOB:{
    const gchar *hash = va_arg (args, const gchar *);
    if (hash)
      return g_strdup_printf (METHODS_V1_FORMAT[method], saddr, hash);

    break;
  }
  default:
    break;
  }

  return NULL;
}

/**
 * eam_rest_build_uri:
 * @method: The #EamRestMethod to construct
 * @...: The parameters of the rest method
 *
 * Construct the URI string given the @method
 *
 * Returns: a newly-allocated string.
 **/
gchar *
eam_rest_build_uri (EamRestMethod method, ...)
{
  va_list args;

  va_start (args, method);

  if (g_strcmp0 (eam_config_protver (), "v1") == 0)
    return build_uri_v1 (method, eam_config_saddr (), args);

  return NULL;
}
