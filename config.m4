PHP_ARG_ENABLE(fastjson, whether to enable fastjson support,
[  --enable-fastjson       Enable fastjson (yyjson-backed JSON) support])

PHP_ARG_ENABLE(fastjson-dev, whether to enable developer build flags,
[  --enable-fastjson-dev   Upgrade wrapper warnings to -Werror plus strict checks], no, no)

if test "$PHP_FASTJSON" != "no"; then

  PHP_VERSION_ID=$($PHP_CONFIG --vernum)
  if test "$PHP_VERSION_ID" -lt "80100"; then
    AC_MSG_ERROR([fastjson requires PHP 8.1.0 or later (found $PHP_VERSION_ID)])
  fi

  YYJSON_SRC_DIR=vendor/yyjson

  YYJSON_SOURCES="$YYJSON_SRC_DIR/yyjson.c"
  WRAPPER_SOURCES="fastjson.c fastjson_alloc.c fastjson_decode.c fastjson_encode.c fastjson_directwrite.c fastjson_pointer_splice.c"

  dnl PHP lifecycle signatures and conditional yyjson helpers cause unused warnings.
  dnl Empty yyjson_api so its default-visibility attribute cannot override hidden.
  dnl This avoids collisions with other yyjson users; ZEND_GET_MODULE stays exported.
  FASTJSON_CFLAGS="-fvisibility=hidden -Dyyjson_api= \
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
