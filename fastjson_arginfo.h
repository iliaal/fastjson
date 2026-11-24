/* This is a generated file, edit fastjson.stub.php instead.
 * Stub hash: ab6307fb222a0d5bdc64870c43f2de93f9dc4821 */

#include "zend_constants.h"

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_fastjson_version, 0, 0, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_MASK_EX(arginfo_fastjson_encode, 0, 1, MAY_BE_STRING|MAY_BE_FALSE)
	ZEND_ARG_TYPE_INFO(0, value, IS_MIXED, 0)
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, flags, IS_LONG, 0, "0")
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, depth, IS_LONG, 0, "512")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_fastjson_file_encode, 0, 2, _IS_BOOL, 0)
	ZEND_ARG_TYPE_INFO(0, filename, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO(0, value, IS_MIXED, 0)
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, flags, IS_LONG, 0, "0")
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, depth, IS_LONG, 0, "512")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_fastjson_decode, 0, 1, IS_MIXED, 0)
	ZEND_ARG_TYPE_INFO(0, json, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, associative, _IS_BOOL, 1, "null")
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, depth, IS_LONG, 0, "512")
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, flags, IS_LONG, 0, "0")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_fastjson_file_decode, 0, 1, IS_MIXED, 0)
	ZEND_ARG_TYPE_INFO(0, filename, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, associative, _IS_BOOL, 1, "null")
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, depth, IS_LONG, 0, "512")
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, flags, IS_LONG, 0, "0")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_fastjson_pointer_get, 0, 2, IS_MIXED, 0)
	ZEND_ARG_TYPE_INFO(0, json, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO(0, pointer, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, associative, _IS_BOOL, 1, "null")
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, depth, IS_LONG, 0, "512")
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, flags, IS_LONG, 0, "0")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_fastjson_pointer_exists, 0, 2, _IS_BOOL, 0)
	ZEND_ARG_TYPE_INFO(0, json, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO(0, pointer, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, flags, IS_LONG, 0, "0")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_MASK_EX(arginfo_fastjson_pointer_set, 0, 3, MAY_BE_STRING|MAY_BE_FALSE)
	ZEND_ARG_TYPE_INFO(0, json, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO(0, pointer, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO(0, value, IS_MIXED, 0)
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, depth, IS_LONG, 0, "512")
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, flags, IS_LONG, 0, "0")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_fastjson_merge_patch, 0, 2, IS_MIXED, 0)
	ZEND_ARG_TYPE_INFO(0, target, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO(0, patch, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, associative, _IS_BOOL, 1, "null")
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, depth, IS_LONG, 0, "512")
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, flags, IS_LONG, 0, "0")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_fastjson_validate, 0, 1, _IS_BOOL, 0)
	ZEND_ARG_TYPE_INFO(0, json, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, depth, IS_LONG, 0, "512")
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, flags, IS_LONG, 0, "0")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_fastjson_last_error, 0, 0, IS_LONG, 0)
ZEND_END_ARG_INFO()

#define arginfo_fastjson_last_error_msg arginfo_fastjson_version

#define arginfo_fastjson_last_error_pos arginfo_fastjson_last_error

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_fastjson_last_error_info, 0, 0, IS_ARRAY, 0)
ZEND_END_ARG_INFO()


#if (PHP_VERSION_ID >= 80400)
ZEND_FRAMELESS_FUNCTION(fastjson_version, 0);
static const zend_frameless_function_info frameless_function_infos_fastjson_version[] = {
	{ ZEND_FRAMELESS_FUNCTION_NAME(fastjson_version, 0), 0 },
	{ 0 },
};
#endif

#if (PHP_VERSION_ID >= 80400)
ZEND_FRAMELESS_FUNCTION(fastjson_decode, 1);
ZEND_FRAMELESS_FUNCTION(fastjson_decode, 2);
static const zend_frameless_function_info frameless_function_infos_fastjson_decode[] = {
	{ ZEND_FRAMELESS_FUNCTION_NAME(fastjson_decode, 1), 1 },
	{ ZEND_FRAMELESS_FUNCTION_NAME(fastjson_decode, 2), 2 },
	{ 0 },
};
#endif

#if (PHP_VERSION_ID >= 80400)
ZEND_FRAMELESS_FUNCTION(fastjson_pointer_get, 2);
ZEND_FRAMELESS_FUNCTION(fastjson_pointer_get, 3);
static const zend_frameless_function_info frameless_function_infos_fastjson_pointer_get[] = {
	{ ZEND_FRAMELESS_FUNCTION_NAME(fastjson_pointer_get, 2), 2 },
	{ ZEND_FRAMELESS_FUNCTION_NAME(fastjson_pointer_get, 3), 3 },
	{ 0 },
};
#endif

#if (PHP_VERSION_ID >= 80400)
ZEND_FRAMELESS_FUNCTION(fastjson_pointer_exists, 2);
ZEND_FRAMELESS_FUNCTION(fastjson_pointer_exists, 3);
static const zend_frameless_function_info frameless_function_infos_fastjson_pointer_exists[] = {
	{ ZEND_FRAMELESS_FUNCTION_NAME(fastjson_pointer_exists, 2), 2 },
	{ ZEND_FRAMELESS_FUNCTION_NAME(fastjson_pointer_exists, 3), 3 },
	{ 0 },
};
#endif

#if (PHP_VERSION_ID >= 80400)
ZEND_FRAMELESS_FUNCTION(fastjson_merge_patch, 2);
ZEND_FRAMELESS_FUNCTION(fastjson_merge_patch, 3);
static const zend_frameless_function_info frameless_function_infos_fastjson_merge_patch[] = {
	{ ZEND_FRAMELESS_FUNCTION_NAME(fastjson_merge_patch, 2), 2 },
	{ ZEND_FRAMELESS_FUNCTION_NAME(fastjson_merge_patch, 3), 3 },
	{ 0 },
};
#endif

#if (PHP_VERSION_ID >= 80400)
ZEND_FRAMELESS_FUNCTION(fastjson_validate, 1);
ZEND_FRAMELESS_FUNCTION(fastjson_validate, 2);
ZEND_FRAMELESS_FUNCTION(fastjson_validate, 3);
static const zend_frameless_function_info frameless_function_infos_fastjson_validate[] = {
	{ ZEND_FRAMELESS_FUNCTION_NAME(fastjson_validate, 1), 1 },
	{ ZEND_FRAMELESS_FUNCTION_NAME(fastjson_validate, 2), 2 },
	{ ZEND_FRAMELESS_FUNCTION_NAME(fastjson_validate, 3), 3 },
	{ 0 },
};
#endif

#if (PHP_VERSION_ID >= 80400)
ZEND_FRAMELESS_FUNCTION(fastjson_last_error, 0);
static const zend_frameless_function_info frameless_function_infos_fastjson_last_error[] = {
	{ ZEND_FRAMELESS_FUNCTION_NAME(fastjson_last_error, 0), 0 },
	{ 0 },
};
#endif

#if (PHP_VERSION_ID >= 80400)
ZEND_FRAMELESS_FUNCTION(fastjson_last_error_msg, 0);
static const zend_frameless_function_info frameless_function_infos_fastjson_last_error_msg[] = {
	{ ZEND_FRAMELESS_FUNCTION_NAME(fastjson_last_error_msg, 0), 0 },
	{ 0 },
};
#endif

#if (PHP_VERSION_ID >= 80400)
ZEND_FRAMELESS_FUNCTION(fastjson_last_error_pos, 0);
static const zend_frameless_function_info frameless_function_infos_fastjson_last_error_pos[] = {
	{ ZEND_FRAMELESS_FUNCTION_NAME(fastjson_last_error_pos, 0), 0 },
	{ 0 },
};
#endif

#if (PHP_VERSION_ID >= 80400)
ZEND_FRAMELESS_FUNCTION(fastjson_last_error_info, 0);
static const zend_frameless_function_info frameless_function_infos_fastjson_last_error_info[] = {
	{ ZEND_FRAMELESS_FUNCTION_NAME(fastjson_last_error_info, 0), 0 },
	{ 0 },
};
#endif

ZEND_FUNCTION(fastjson_version);
ZEND_FUNCTION(fastjson_encode);
ZEND_FUNCTION(fastjson_file_encode);
ZEND_FUNCTION(fastjson_decode);
ZEND_FUNCTION(fastjson_file_decode);
ZEND_FUNCTION(fastjson_pointer_get);
ZEND_FUNCTION(fastjson_pointer_exists);
ZEND_FUNCTION(fastjson_pointer_set);
ZEND_FUNCTION(fastjson_merge_patch);
ZEND_FUNCTION(fastjson_validate);
ZEND_FUNCTION(fastjson_last_error);
ZEND_FUNCTION(fastjson_last_error_msg);
ZEND_FUNCTION(fastjson_last_error_pos);
ZEND_FUNCTION(fastjson_last_error_info);

static const zend_function_entry ext_functions[] = {
#if (PHP_VERSION_ID >= 80400)
	ZEND_RAW_FENTRY("fastjson_version", zif_fastjson_version, arginfo_fastjson_version, 0, frameless_function_infos_fastjson_version, NULL)
#else
	ZEND_RAW_FENTRY("fastjson_version", zif_fastjson_version, arginfo_fastjson_version, 0)
#endif
	ZEND_FE(fastjson_encode, arginfo_fastjson_encode)
	ZEND_FE(fastjson_file_encode, arginfo_fastjson_file_encode)
#if (PHP_VERSION_ID >= 80400)
	ZEND_RAW_FENTRY("fastjson_decode", zif_fastjson_decode, arginfo_fastjson_decode, 0, frameless_function_infos_fastjson_decode, NULL)
#else
	ZEND_RAW_FENTRY("fastjson_decode", zif_fastjson_decode, arginfo_fastjson_decode, 0)
#endif
	ZEND_FE(fastjson_file_decode, arginfo_fastjson_file_decode)
#if (PHP_VERSION_ID >= 80400)
	ZEND_RAW_FENTRY("fastjson_pointer_get", zif_fastjson_pointer_get, arginfo_fastjson_pointer_get, 0, frameless_function_infos_fastjson_pointer_get, NULL)
#else
	ZEND_RAW_FENTRY("fastjson_pointer_get", zif_fastjson_pointer_get, arginfo_fastjson_pointer_get, 0)
#endif
#if (PHP_VERSION_ID >= 80400)
	ZEND_RAW_FENTRY("fastjson_pointer_exists", zif_fastjson_pointer_exists, arginfo_fastjson_pointer_exists, 0, frameless_function_infos_fastjson_pointer_exists, NULL)
#else
	ZEND_RAW_FENTRY("fastjson_pointer_exists", zif_fastjson_pointer_exists, arginfo_fastjson_pointer_exists, 0)
#endif
	ZEND_FE(fastjson_pointer_set, arginfo_fastjson_pointer_set)
#if (PHP_VERSION_ID >= 80400)
	ZEND_RAW_FENTRY("fastjson_merge_patch", zif_fastjson_merge_patch, arginfo_fastjson_merge_patch, 0, frameless_function_infos_fastjson_merge_patch, NULL)
#else
	ZEND_RAW_FENTRY("fastjson_merge_patch", zif_fastjson_merge_patch, arginfo_fastjson_merge_patch, 0)
#endif
#if (PHP_VERSION_ID >= 80400)
	ZEND_RAW_FENTRY("fastjson_validate", zif_fastjson_validate, arginfo_fastjson_validate, 0, frameless_function_infos_fastjson_validate, NULL)
#else
	ZEND_RAW_FENTRY("fastjson_validate", zif_fastjson_validate, arginfo_fastjson_validate, 0)
#endif
#if (PHP_VERSION_ID >= 80400)
	ZEND_RAW_FENTRY("fastjson_last_error", zif_fastjson_last_error, arginfo_fastjson_last_error, 0, frameless_function_infos_fastjson_last_error, NULL)
#else
	ZEND_RAW_FENTRY("fastjson_last_error", zif_fastjson_last_error, arginfo_fastjson_last_error, 0)
#endif
#if (PHP_VERSION_ID >= 80400)
	ZEND_RAW_FENTRY("fastjson_last_error_msg", zif_fastjson_last_error_msg, arginfo_fastjson_last_error_msg, 0, frameless_function_infos_fastjson_last_error_msg, NULL)
#else
	ZEND_RAW_FENTRY("fastjson_last_error_msg", zif_fastjson_last_error_msg, arginfo_fastjson_last_error_msg, 0)
#endif
#if (PHP_VERSION_ID >= 80400)
	ZEND_RAW_FENTRY("fastjson_last_error_pos", zif_fastjson_last_error_pos, arginfo_fastjson_last_error_pos, 0, frameless_function_infos_fastjson_last_error_pos, NULL)
#else
	ZEND_RAW_FENTRY("fastjson_last_error_pos", zif_fastjson_last_error_pos, arginfo_fastjson_last_error_pos, 0)
#endif
#if (PHP_VERSION_ID >= 80400)
	ZEND_RAW_FENTRY("fastjson_last_error_info", zif_fastjson_last_error_info, arginfo_fastjson_last_error_info, 0, frameless_function_infos_fastjson_last_error_info, NULL)
#else
	ZEND_RAW_FENTRY("fastjson_last_error_info", zif_fastjson_last_error_info, arginfo_fastjson_last_error_info, 0)
#endif
	ZEND_FE_END
};

static void register_fastjson_symbols(int module_number)
{
	REGISTER_LONG_CONSTANT("FASTJSON_ERROR_NONE", FASTJSON_ERROR_NONE, CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FASTJSON_ERROR_DEPTH", FASTJSON_ERROR_DEPTH, CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FASTJSON_ERROR_STATE_MISMATCH", FASTJSON_ERROR_STATE_MISMATCH, CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FASTJSON_ERROR_CTRL_CHAR", FASTJSON_ERROR_CTRL_CHAR, CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FASTJSON_ERROR_SYNTAX", FASTJSON_ERROR_SYNTAX, CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FASTJSON_ERROR_UTF8", FASTJSON_ERROR_UTF8, CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FASTJSON_ERROR_RECURSION", FASTJSON_ERROR_RECURSION, CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FASTJSON_ERROR_INF_OR_NAN", FASTJSON_ERROR_INF_OR_NAN, CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FASTJSON_ERROR_UNSUPPORTED_TYPE", FASTJSON_ERROR_UNSUPPORTED_TYPE, CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FASTJSON_ERROR_INVALID_PROPERTY_NAME", FASTJSON_ERROR_INVALID_PROPERTY_NAME, CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FASTJSON_ERROR_UTF16", FASTJSON_ERROR_UTF16, CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FASTJSON_ERROR_NON_BACKED_ENUM", FASTJSON_ERROR_NON_BACKED_ENUM, CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FASTJSON_DECODE_RELAXED", FASTJSON_DECODE_RELAXED, CONST_PERSISTENT);
}
