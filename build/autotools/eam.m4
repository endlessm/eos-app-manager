dnl eam_ADD_CFLAGS
dnl

AC_DEFUN([eam_ADD_CFLAGS],
[
  AC_MSG_CHECKING([to see if compiler understands $1])

  save_CFLAGS="$CFLAGS"
  CFLAGS="$CFLAGS $1"

  AC_TRY_COMPILE([ ], [], [flag_ok=yes], [flag_ok=no])
  CFLAGS="$save_CFLAGS"

  if test "X$flag_ok" = Xyes ; then
    m4_ifvaln([$2],[$2],[CFLAGS="$CFLAGS $1"])
    true
  else
    m4_ifvaln([$3],[$3])
    true
  fi
  AC_MSG_RESULT([$flag_ok])
])
