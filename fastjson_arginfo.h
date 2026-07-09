/* This is a generated file, edit fastjson.stub.php instead.
 * Stub hash: 25719bb93d86a9737e891f8e3e36228a5b4af0b7 */

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
	ZEND_FE(fastjson_version, arginfo_fastjson_version)
	ZEND_FE(fastjson_encode, arginfo_fastjson_encode)
	ZEND_FE(fastjson_file_encode, arginfo_fastjson_file_encode)
	ZEND_FE(fastjson_decode, arginfo_fastjson_decode)
	ZEND_FE(fastjson_file_decode, arginfo_fastjson_file_decode)
	ZEND_FE(fastjson_pointer_get, arginfo_fastjson_pointer_get)
	ZEND_FE(fastjson_pointer_exists, arginfo_fastjson_pointer_exists)
	ZEND_FE(fastjson_pointer_set, arginfo_fastjson_pointer_set)
	ZEND_FE(fastjson_merge_patch, arginfo_fastjson_merge_patch)
	ZEND_FE(fastjson_validate, arginfo_fastjson_validate)
	ZEND_FE(fastjson_last_error, arginfo_fastjson_last_error)
	ZEND_FE(fastjson_last_error_msg, arginfo_fastjson_last_error_msg)
	ZEND_FE(fastjson_last_error_pos, arginfo_fastjson_last_error_pos)
	ZEND_FE(fastjson_last_error_info, arginfo_fastjson_last_error_info)
	ZEND_FE_END
};

static void register_fastjson_symbols(int module_number)
{
	REGISTER_LONG_CONSTANT("FASTJSON_ERROR_NONE", FASTJSON_ERROR_NONE, CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FASTJSON_ERROR_DEPTH", FASTJSON_ERROR_DEPTH, CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FASTJSON_ERROR_SYNTAX", FASTJSON_ERROR_SYNTAX, CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FASTJSON_ERROR_UTF8", FASTJSON_ERROR_UTF8, CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FASTJSON_ERROR_RECURSION", FASTJSON_ERROR_RECURSION, CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FASTJSON_ERROR_INF_OR_NAN", FASTJSON_ERROR_INF_OR_NAN, CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FASTJSON_ERROR_UNSUPPORTED_TYPE", FASTJSON_ERROR_UNSUPPORTED_TYPE, CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FASTJSON_ERROR_NON_BACKED_ENUM", FASTJSON_ERROR_NON_BACKED_ENUM, CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FASTJSON_DECODE_RELAXED", FASTJSON_DECODE_RELAXED, CONST_PERSISTENT);
}
