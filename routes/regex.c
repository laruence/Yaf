/*
  +----------------------------------------------------------------------+
  | Yet Another Framework                                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author: Xinchen Hui  <laruence@php.net>                              |
  +----------------------------------------------------------------------+
 */

/* $Id: regex.c 327549 2012-09-09 03:02:48Z laruence $ */

#include "ext/standard/php_string.h"

zend_class_entry *yaf_route_regex_ce;

/** {{{ ARG_INFO
 */
ZEND_BEGIN_ARG_INFO_EX(yaf_route_regex_construct_arginfo, 0, 0, 2)
	ZEND_ARG_INFO(0, match)
	ZEND_ARG_ARRAY_INFO(0, route, 0)
	ZEND_ARG_ARRAY_INFO(0, map, 1)
	ZEND_ARG_INFO(0, verify)
ZEND_END_ARG_INFO()
/* }}} */

/** {{{ yaf_route_t * yaf_route_regex_instance(yaf_route_t *this_ptr, zval *route, zval *def, zval *map, zval *verify TSRMLS_DC)
 */
yaf_route_t * yaf_route_regex_instance(yaf_route_t *this_ptr, zval *route, zval *def, zval *map, zval *verify TSRMLS_DC) {
	yaf_route_t	*instance;

	if (this_ptr) {
		instance = this_ptr;
	} else {
		MAKE_STD_ZVAL(instance);
		object_init_ex(instance, yaf_route_regex_ce);
	}

	zend_update_property(yaf_route_regex_ce, instance, ZEND_STRL(YAF_ROUTE_PROPETY_NAME_MATCH), route TSRMLS_CC);
	zend_update_property(yaf_route_regex_ce, instance, ZEND_STRL(YAF_ROUTE_PROPETY_NAME_ROUTE), def TSRMLS_CC);
	zend_update_property(yaf_route_regex_ce, instance, ZEND_STRL(YAF_ROUTE_PROPETY_NAME_MAP), map TSRMLS_CC);

	if (!verify) {
		zend_update_property_null(yaf_route_regex_ce, instance, ZEND_STRL(YAF_ROUTE_PROPETY_NAME_VERIFY) TSRMLS_CC);
	} else {
		zend_update_property(yaf_route_regex_ce, instance, ZEND_STRL(YAF_ROUTE_PROPETY_NAME_VERIFY), verify TSRMLS_CC);
	}

	return instance;
}
/* }}} */

/** {{{ static zval * yaf_route_regex_match(yaf_route_t *router, char *uir, int len TSRMLS_DC)
 */
static zval * yaf_route_regex_match(yaf_route_t *route, char *uir, int len TSRMLS_DC) {
	zval *match;
	pcre_cache_entry *pce_regexp;

	if (!len) {
		return NULL;
	}

	match = zend_read_property(yaf_route_regex_ce, route, ZEND_STRL(YAF_ROUTE_PROPETY_NAME_MATCH), 1 TSRMLS_CC);

	if ((pce_regexp = pcre_get_compiled_regex_cache(Z_STRVAL_P(match), Z_STRLEN_P(match) TSRMLS_CC)) == NULL) {
		return NULL;
	} else {
		zval *matches, *subparts, *map;

		MAKE_STD_ZVAL(matches);
		MAKE_STD_ZVAL(subparts);
		ZVAL_NULL(subparts);

		map = zend_read_property(yaf_route_regex_ce, route, ZEND_STRL(YAF_ROUTE_PROPETY_NAME_MAP), 1 TSRMLS_CC);

		php_pcre_match_impl(pce_regexp, uir, len, matches, subparts /* subpats */,
				0/* global */, 0/* ZEND_NUM_ARGS() >= 4 */, 0/*flags PREG_OFFSET_CAPTURE*/, 0/* start_offset */ TSRMLS_CC);

		if (!Z_LVAL_P(matches)) {
			zval_ptr_dtor(&matches);
			zval_ptr_dtor(&subparts);
			return NULL;
		} else {
			zval  *ret, **name, **ppzval;
			char	*key = NULL;
			uint	len  = 0;
			ulong	idx	 = 0;
			HashTable 	*ht;

			MAKE_STD_ZVAL(ret);
			array_init(ret);

			ht = Z_ARRVAL_P(subparts);
			for(zend_hash_internal_pointer_reset(ht);
					zend_hash_has_more_elements(ht) == SUCCESS;
					zend_hash_move_forward(ht)) {

				if (zend_hash_get_current_data(ht, (void**)&ppzval) == FAILURE) {
					continue;
				}

				if (zend_hash_get_current_key_ex(ht, &key, &len, &idx, 0, NULL) == HASH_KEY_IS_LONG) {
					if (zend_hash_index_find(Z_ARRVAL_P(map), idx, (void **)&name) == SUCCESS) {
						Z_ADDREF_P(*ppzval);
						zend_hash_update(Z_ARRVAL_P(ret), Z_STRVAL_PP(name), Z_STRLEN_PP(name) + 1, (void **)ppzval, sizeof(zval *), NULL);
					}
				} else {
					Z_ADDREF_P(*ppzval);
					zend_hash_update(Z_ARRVAL_P(ret), key, len, (void **)ppzval, sizeof(zval *), NULL);
				}
			}

			zval_ptr_dtor(&matches);
			zval_ptr_dtor(&subparts);
			return ret;
		}
	}

	return NULL;
}
/* }}} */

/** {{{ int yaf_route_regex_route(yaf_route_t *router, yaf_request_t *request TSRMLS_DC)
 */
int yaf_route_regex_route(yaf_route_t *router, yaf_request_t *request TSRMLS_DC) {
	char *request_uri;
	zval *args, *base_uri, *zuri;

	zuri 	 = zend_read_property(yaf_request_ce, request, ZEND_STRL(YAF_REQUEST_PROPERTY_NAME_URI), 1 TSRMLS_CC);
	base_uri = zend_read_property(yaf_request_ce, request, ZEND_STRL(YAF_REQUEST_PROPERTY_NAME_BASE), 1 TSRMLS_CC);

	if (base_uri && IS_STRING == Z_TYPE_P(base_uri)
			&& !strncasecmp(Z_STRVAL_P(zuri), Z_STRVAL_P(base_uri), Z_STRLEN_P(base_uri))) {
		request_uri  = estrdup(Z_STRVAL_P(zuri) + Z_STRLEN_P(base_uri));
	} else {
		request_uri  = estrdup(Z_STRVAL_P(zuri));
	}

	if (!(args = yaf_route_regex_match(router, request_uri, strlen(request_uri) TSRMLS_CC))) {
		efree(request_uri);
		return 0;
	} else {
		zval **module, **controller, **action, *routes;

		routes = zend_read_property(yaf_route_regex_ce, router, ZEND_STRL(YAF_ROUTE_PROPETY_NAME_ROUTE), 1 TSRMLS_CC);
		if (zend_hash_find(Z_ARRVAL_P(routes), ZEND_STRS("module"), (void **)&module) == SUCCESS) {
			zend_update_property(yaf_request_ce, request, ZEND_STRL(YAF_REQUEST_PROPERTY_NAME_MODULE), *module TSRMLS_CC);
		}

		if (zend_hash_find(Z_ARRVAL_P(routes), ZEND_STRS("controller"), (void **)&controller) == SUCCESS) {
			zend_update_property(yaf_request_ce, request, ZEND_STRL(YAF_REQUEST_PROPERTY_NAME_CONTROLLER), *controller TSRMLS_CC);
		}

		if (zend_hash_find(Z_ARRVAL_P(routes), ZEND_STRS("action"), (void **)&action) == SUCCESS) {
			zend_update_property(yaf_request_ce, request, ZEND_STRL(YAF_REQUEST_PROPERTY_NAME_ACTION), *action TSRMLS_CC);
		}

		(void)yaf_request_set_params_multi(request, args TSRMLS_CC);
		zval_ptr_dtor(&args);
		efree(request_uri);
	}

	return 1;
}
/* }}} */

/** {{{ zval * yaf_route_regex_assemble(yaf_route_t *this_ptr, zval *mvc, zval *query TSRMLS_DC)
 */
zval * yaf_route_regex_assemble(yaf_route_t *this_ptr, zval *mvc, zval *query TSRMLS_DC) {
	char 	tvalue[1024];
	uint 	tvalue_len = 0;
	zval 	*verify, *uri = NULL;

	MAKE_STD_ZVAL(uri);

	verify = zend_read_property(yaf_route_regex_ce, this_ptr, ZEND_STRL(YAF_ROUTE_PROPETY_NAME_VERIFY), 1 TSRMLS_CC);

	do {	
		char tsprintf[1024];
		char *t_str;
		uint tlen = 0;
		zval **tmp;

		if (!verify) {
			yaf_trigger_error(YAF_ERR_TYPE_ERROR TSRMLS_CC, "%s", "Undefined the 'verify' parameter for this route");
			break;
		}

		if (zend_hash_find(Z_ARRVAL_P(mvc), ZEND_STRS(YAF_ROUTE_VAR_NAME_MODULE), (void **)&tmp) == SUCCESS) {
			t_str = php_str_to_str(Z_STRVAL_P(verify), Z_STRLEN_P(verify),
							ZEND_STRL(YAF_ROUTE_ASSEMBLE_MOUDLE_FORMAT),
							Z_STRVAL_PP(tmp), Z_STRLEN_PP(tmp), &tlen);
			ZVAL_STRING(verify, t_str, 1);
		}		

		if (zend_hash_find(Z_ARRVAL_P(mvc), ZEND_STRS(YAF_ROUTE_VAR_NAME_CONTROLLER), (void **)&tmp) == SUCCESS) {
			t_str = php_str_to_str(Z_STRVAL_P(verify), Z_STRLEN_P(verify),
							ZEND_STRL(YAF_ROUTE_ASSEMBLE_CONTROLLER_FORMAT),
							Z_STRVAL_PP(tmp), Z_STRLEN_PP(tmp), &tlen);
			ZVAL_STRING(verify, t_str, 1);
		}		

		if (zend_hash_find(Z_ARRVAL_P(mvc), ZEND_STRS(YAF_ROUTE_VAR_NAME_ACTION), (void **)&tmp) == SUCCESS) {
			t_str = php_str_to_str(Z_STRVAL_P(verify), Z_STRLEN_P(verify),
							ZEND_STRL(YAF_ROUTE_ASSEMBLE_ACTION_FORMAT),
							Z_STRVAL_PP(tmp), Z_STRLEN_PP(tmp), &tlen);
			ZVAL_STRING(verify, t_str, 1);
		}		

		if (tlen) {
			memcpy(&tvalue[tvalue_len], t_str, tlen);
			tvalue_len += tlen;
		} 
		
		if ( IS_ARRAY == Z_TYPE_P(query)) {
			uint key_type, key_len;
			char *key;
			ulong key_idx;
			uint i = 0;

			for (zend_hash_internal_pointer_reset(Z_ARRVAL_P(query));
					zend_hash_get_current_data(Z_ARRVAL_P(query), (void **)&tmp) == SUCCESS;
					zend_hash_move_forward(Z_ARRVAL_P(query))) {

				if (IS_STRING == Z_TYPE_PP(tmp)
						&& HASH_KEY_IS_STRING == zend_hash_get_current_key_ex(Z_ARRVAL_P(query), &key, &key_len, &key_idx, 0, NULL)) {

					tlen = strlen("=&") + key_len + Z_STRLEN_PP(tmp);
					if (i == 0) {
						tlen = snprintf(tsprintf, tlen + 1, "?%s=%s", key, Z_STRVAL_PP(tmp));
					} else {
						tlen = snprintf(tsprintf, tlen + 1, "&%s=%s", key, Z_STRVAL_PP(tmp));
					}
					if (tlen) {
						memcpy(&tvalue[tvalue_len], tsprintf, strlen(tsprintf));
						tvalue_len += tlen;
					}
				}
				i++;
			}
		}

		tvalue[tvalue_len] = '\0';
		ZVAL_STRING(uri, tvalue, 1);
		return uri;	
	} while (0);

	ZVAL_NULL(uri);
	return uri;
}

/** {{{ proto public Yaf_Route_Regex::route(string $uri)
 */
PHP_METHOD(yaf_route_regex, route) {
	yaf_route_t	*route;
	yaf_request_t *request;

	route = getThis();

	RETVAL_FALSE;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "O", &request, yaf_request_ce) == FAILURE) {
		return;
	}

	if (!request || IS_OBJECT != Z_TYPE_P(request)
			|| !instanceof_function(Z_OBJCE_P(request), yaf_request_ce TSRMLS_CC)) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Expects a %s instance",  yaf_request_ce->name);
		RETURN_FALSE;
	}

	RETURN_BOOL(yaf_route_regex_route(route, request TSRMLS_CC));
}
/** }}} */

/** {{{ proto public Yaf_Route_Regex::__construct(string $match, array $route, array $map = NULL, array $verify = NULL)
 */
PHP_METHOD(yaf_route_regex, __construct) {
	zval 		*match, *route, *map, *verify = NULL;
	yaf_route_t	*self = getThis();

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "zaa|z", &match, &route, &map, &verify) ==  FAILURE) {
		YAF_UNINITIALIZED_OBJECT(getThis());
		return;
	}

	if (IS_STRING != Z_TYPE_P(match) || !Z_STRLEN_P(match)) {
		YAF_UNINITIALIZED_OBJECT(getThis());
		yaf_trigger_error(YAF_ERR_TYPE_ERROR TSRMLS_CC, "Expects a valid string as the first parameter", yaf_route_regex_ce->name);
		RETURN_FALSE;
	}

	if (ZEND_NUM_ARGS() == 4 && IS_STRING != Z_TYPE_P(verify)) {
		YAF_UNINITIALIZED_OBJECT(getThis());
		yaf_trigger_error(YAF_ERR_TYPE_ERROR TSRMLS_CC, "Expects a string as verify parmater",  yaf_route_regex_ce->name);
		RETURN_FALSE;
	}

	(void)yaf_route_regex_instance(self, match, route, map, verify TSRMLS_CC);

	if (self) {
		RETURN_ZVAL(self, 1, 0);
	}

	RETURN_FALSE;
}
/** }}} */

/** {{{ proto public Yaf_Route_regex::assemble(array $mvc[, array $query = NULL])
*/
PHP_METHOD(yaf_route_regex, assemble) {
	zval 	*mvc, *query, *return_uri = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "a|a", &mvc, &query) == FAILURE) {
		return;
	} else {
		return_uri = yaf_route_regex_assemble(getThis(), mvc, query TSRMLS_CC);
		RETURN_ZVAL(return_uri, 0, 1);
	}
}
/* }}} */

/** {{{ yaf_route_regex_methods
 */
zend_function_entry yaf_route_regex_methods[] = {
	PHP_ME(yaf_route_regex, __construct, yaf_route_regex_construct_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
	PHP_ME(yaf_route_regex, route, yaf_route_route_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(yaf_route_regex, assemble, yaf_route_assemble_arginfo, ZEND_ACC_PUBLIC)
    {NULL, NULL, NULL}
};
/* }}} */

/** {{{ YAF_STARTUP_FUNCTION
 */
YAF_STARTUP_FUNCTION(route_regex) {
	zend_class_entry ce;
	YAF_INIT_CLASS_ENTRY(ce, "Yaf_Route_Regex", "Yaf\\Route\\Regex", yaf_route_regex_methods);
	yaf_route_regex_ce = zend_register_internal_class_ex(&ce, yaf_route_ce, NULL TSRMLS_CC);
	zend_class_implements(yaf_route_regex_ce TSRMLS_CC, 1, yaf_route_ce);
	yaf_route_regex_ce->ce_flags |= ZEND_ACC_FINAL_CLASS;

	zend_declare_property_null(yaf_route_regex_ce, ZEND_STRL(YAF_ROUTE_PROPETY_NAME_MATCH),  ZEND_ACC_PROTECTED TSRMLS_CC);
	zend_declare_property_null(yaf_route_regex_ce, ZEND_STRL(YAF_ROUTE_PROPETY_NAME_ROUTE),  ZEND_ACC_PROTECTED TSRMLS_CC);
	zend_declare_property_null(yaf_route_regex_ce, ZEND_STRL(YAF_ROUTE_PROPETY_NAME_MAP),    ZEND_ACC_PROTECTED TSRMLS_CC);
	zend_declare_property_null(yaf_route_regex_ce, ZEND_STRL(YAF_ROUTE_PROPETY_NAME_VERIFY), ZEND_ACC_PROTECTED TSRMLS_CC);

	return SUCCESS;
}
/* }}} */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
