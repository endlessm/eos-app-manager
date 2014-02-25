/* Copyright 2014 Endless Mobile, Inc. */

/* Based on dpkg:
 *
 * Copyright © 1995 Ian Jackson <ian@chiark.greenend.org.uk>
 * Copyright © 2006-2012 Guillem Jover <guillem@debian.org>
 */

#ifndef EAM_VERSION_H
#define EAM_VERSION_H

#include <glib-object.h>
#include <glib.h>

G_BEGIN_DECLS

typedef struct _EamPkgVersion EamPkgVersion;
typedef enum _EamRelation EamRelation;

struct _EamPkgVersion {
  guint epoch;
  gchar *version;
  gchar *revision;
};

enum _EamRelation {
  EAM_RELATION_NONE = 0,
  EAM_RELATION_EQ = 1UL << 0,
  EAM_RELATION_LT = 1UL << 1,
  EAM_RELATION_LE = EAM_RELATION_LT | EAM_RELATION_EQ,
  EAM_RELATION_GT = 1UL << 2,
  EAM_RELATION_GE = EAM_RELATION_GT | EAM_RELATION_EQ,
};

EamPkgVersion  *eam_pkg_version_new                              (void);

EamPkgVersion  *eam_pkg_version_new_from_string                  (const gchar *string);

void            eam_pkg_version_free                             (EamPkgVersion *version);

EamPkgVersion  *eam_pkg_version_copy                             (const EamPkgVersion *version);

gboolean        eam_pkg_version_parse                            (EamPkgVersion *version,
                                                                  const gchar *string);

gint            eam_pkg_version_compare                          (const EamPkgVersion *a,
                                                                  const EamPkgVersion *b);

gboolean        eam_pkg_version_relate                           (const EamPkgVersion *a,
                                                                  EamRelation rel,
                                                                  const EamPkgVersion *b);

G_END_DECLS

#endif /* EAM_VERSION_H */
