dnl config.m4 for extension fastjson

PHP_ARG_ENABLE(fastjson, whether to enable fastjson support,
[  --enable-fastjson       Enable fastjson (yyjson-backed JSON) support])

PHP_ARG_ENABLE(fastjson-dev, whether to enable developer build flags,
[  --enable-fastjson-dev   Upgrade wrapper warnings to -Werror plus strict checks], no, no)

if test "$PHP_FASTJSON" != "no"; then

  PHP_VERSION_ID=$($PHP_CONFIG --vernum)
  if test "$PHP_VERSION_ID" -lt "80300"; then
    AC_MSG_ERROR([fastjson requires PHP 8.3.0 or later (found $PHP_VERSION_ID)])
  fi

  YYJSON_SRC_DIR=vendor/yyjson

  YYJSON_SOURCES="$YYJSON_SRC_DIR/yyjson.c"
  WRAPPER_SOURCES="fastjson.c fastjson_alloc.c fastjson_decode.c fastjson_encode.c fastjson_directwrite.c"

  dnl -Wall -Wextra are on by default so wrapper regressions get caught
  dnl in every local build; --enable-fastjson-dev upgrades to -Werror.
  dnl -Wno-unused-parameter silences PHP MINIT/MSHUTDOWN/MINFO macros
  dnl whose generated signatures take `type` and `module_number` params
  dnl we don't use; also covers yyjson static helpers that aren't
  dnl reachable depending on SIMD / float-conversion build config.
  dnl
  dnl -fvisibility=hidden keeps yyjson symbols out of the .so's dynamic
  dnl table; ZEND_GET_MODULE marks get_module with visibility("default")
  dnl so the loader still finds it. Prevents collisions with another
  dnl extension (or a future ext/json link) that also vendors yyjson.
  FASTJSON_CFLAGS="-fvisibility=hidden \
    -Wall -Wextra -Wno-unused-parameter -Wno-unused-function"

  dnl -Wshadow is intentionally NOT enabled; PHP's own headers declare
  dnl struct members named `zval` which shadow the zval typedef.
  if test "$PHP_FASTJSON_DEV" = "yes"; then
    FASTJSON_CFLAGS="$FASTJSON_CFLAGS -Werror -Wstrict-prototypes"
  fi

  PHP_NEW_EXTENSION(fastjson,
    $WRAPPER_SOURCES $YYJSON_SOURCES,
    $ext_shared,,
    $FASTJSON_CFLAGS)

  PHP_ADD_INCLUDE([$ext_srcdir/$YYJSON_SRC_DIR])
  PHP_ADD_BUILD_DIR([$ext_builddir/$YYJSON_SRC_DIR], 1)
fi
