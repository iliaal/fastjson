/* This is a generated file, edit fastjson.stub.php instead.
 * Stub hash: b77edc093149f37ade85cdfebbe5fe0392cc9045 */

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

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_fastjson_validate, 0, 1, _IS_BOOL, 0)
	ZEND_ARG_TYPE_INFO(0, json, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, depth, IS_LONG, 0, "512")
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, flags, IS_LONG, 0, "0")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_fastjson_last_error, 0, 0, IS_LONG, 0)
ZEND_END_ARG_INFO()

#define arginfo_fastjson_last_error_msg arginfo_fastjson_version

ZEND_FUNCTION(fastjson_version);
ZEND_FUNCTION(fastjson_encode);
ZEND_FUNCTION(fastjson_file_encode);
ZEND_FUNCTION(fastjson_decode);
ZEND_FUNCTION(fastjson_file_decode);
ZEND_FUNCTION(fastjson_validate);
ZEND_FUNCTION(fastjson_last_error);
ZEND_FUNCTION(fastjson_last_error_msg);

static const zend_function_entry ext_functions[] = {
	ZEND_FE(fastjson_version, arginfo_fastjson_version)
	ZEND_FE(fastjson_encode, arginfo_fastjson_encode)
	ZEND_FE(fastjson_file_encode, arginfo_fastjson_file_encode)
	ZEND_FE(fastjson_decode, arginfo_fastjson_decode)
	ZEND_FE(fastjson_file_decode, arginfo_fastjson_file_decode)
	ZEND_FE(fastjson_validate, arginfo_fastjson_validate)
	ZEND_FE(fastjson_last_error, arginfo_fastjson_last_error)
	ZEND_FE(fastjson_last_error_msg, arginfo_fastjson_last_error_msg)
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
}
