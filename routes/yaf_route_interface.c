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

/* $Id: interface.c 329197 2013-01-18 05:55:37Z laruence $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"

#include "php_yaf.h"

#include "yaf_namespace.h"
#include "yaf_exception.h"
#include "yaf_router.h"

#include "routes/yaf_route_interface.h"
#include "routes/yaf_route_static.h"
#include "routes/yaf_route_simple.h"
#include "routes/yaf_route_supervar.h"
#include "routes/yaf_route_regex.h"
#include "routes/yaf_route_rewrite.h"
#include "routes/yaf_route_map.h"

zend_class_entry *yaf_route_ce;

/* {{{ yaf_route_t * yaf_route_instance(yaf_route_t *this_ptr,  zval *config TSRMLS_DC)
 */
yaf_route_t * yaf_route_instance(yaf_route_t *this_ptr, zval *config TSRMLS_DC) {
	zval *match, *def, *map, *verify, *reverse, *pzval;
	yaf_route_t *instance = NULL;

	if (!config || IS_ARRAY != Z_TYPE_P(config)) {
		return NULL;
	}

	if ((pzval = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("type"))) == NULL
			|| IS_STRING != Z_TYPE_P(pzval)) {
		return NULL;
	}

	if (Z_STRLEN_P(pzval) == (sizeof("rewrite") - 1)
			&& strncasecmp(Z_STRVAL_P(pzval), "rewrite", sizeof("rewrite") - 1) == 0) {
		if ((match = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("match"))) == NULL
				|| Z_TYPE_P(match) != IS_STRING) {
			return NULL;
		}
		if ((def = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("route"))) == NULL
				|| Z_TYPE_P(def) != IS_ARRAY) {
			return NULL;
		}

		if ((verify = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("route"))) == NULL) {
            verify = NULL;
        }

        instance = yaf_route_rewrite_instance(this_ptr, match, def, verify? verify : NULL TSRMLS_CC);
	} else if (Z_STRLEN_P(pzval) == (sizeof("regex") - 1)
			&& strncasecmp(Z_STRVAL_P(pzval), "regex", sizeof("regex") - 1) == 0) {
		if ((match = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("match"))) == NULL || Z_TYPE_P(match) != IS_STRING) {
			return NULL;
		}
		if ((def = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("route"))) == NULL
				|| Z_TYPE_P(def) != IS_ARRAY) {
			return NULL;
		}
		if ((map = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("map"))) == NULL || Z_TYPE_P(map) != IS_ARRAY) {
			map = NULL;
		}

		if ((verify = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("route"))) == NULL) {
			verify = NULL;
		}

		if ((reverse = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("route"))) == NULL) {
			reverse = NULL;
		}

		instance = yaf_route_regex_instance(this_ptr, match, def, map? map : NULL, verify? verify : NULL, reverse? reverse : NULL TSRMLS_CC);
	} else if (Z_STRLEN_P(pzval) == (sizeof("map") - 1)
			&& strncasecmp(Z_STRVAL_P(pzval), "map", sizeof("map") - 1) == 0) {
		char *delimiter = NULL;
		uint delim_len  = 0;
		zend_bool controller_prefer = 0;
		
		if ((pzval = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("controllerPrefer"))) != NULL) {
			zval *tmp = pzval;
			Z_TRY_ADDREF_P(tmp);
			convert_to_boolean_ex(tmp);
			controller_prefer = Z_TYPE_P(tmp) == IS_TRUE ? 1 : 0;
			zval_ptr_dtor(tmp);
		}

		if ((pzval = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("delimiter"))) != NULL
				&& Z_TYPE_P(pzval) == IS_STRING) {
			delimiter = Z_STRVAL_P(pzval);
			delim_len = Z_STRLEN_P(pzval);
		}

		instance = yaf_route_map_instance(this_ptr, controller_prefer, delimiter, delim_len TSRMLS_CC);
	} else if (Z_STRLEN_P(pzval) == (sizeof("simple") - 1)
			&& strncasecmp(Z_STRVAL_P(pzval), "simple", sizeof("simple") - 1) == 0) {
		if ((match = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("module"))) == NULL
				|| Z_TYPE_P(match) != IS_STRING) {
			return NULL;
		}
		if ((def = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("controller"))) == NULL
				|| Z_TYPE_P(def) != IS_STRING) {
			return NULL;
		}
		if ((map = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("action"))) == NULL
				|| Z_TYPE_P(map) != IS_STRING) {
			return NULL;
		}

		instance = yaf_route_simple_instance(this_ptr, match, def, map TSRMLS_CC);
	} else if (Z_STRLEN_P(pzval) == (sizeof("supervar") - 1)
			&& strncasecmp(Z_STRVAL_P(pzval), "supervar", sizeof("supervar") - 1) == 0) {
		if ((match = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("varname"))) == NULL
				|| Z_TYPE_P(match) != IS_STRING) {
			return NULL;
		}

		instance = yaf_route_supervar_instance(this_ptr, match TSRMLS_CC);
	}

	return instance;
}
/* }}} */

/** {{{ yaf_route_methods
 */
zend_function_entry yaf_route_methods[] = {
	PHP_ABSTRACT_ME(yaf_route, route, yaf_route_route_arginfo)
	PHP_ABSTRACT_ME(yaf_route, assemble, yaf_route_assemble_arginfo)
    {NULL, NULL, NULL}
};
/* }}} */

/** {{{ YAF_STARTUP_FUNCTION
 */
YAF_STARTUP_FUNCTION(route) {
	zend_class_entry ce;

	YAF_INIT_CLASS_ENTRY(ce, "Yaf_Route_Interface", "Yaf\\Route_Interface", yaf_route_methods);
	yaf_route_ce = zend_register_internal_interface(&ce TSRMLS_CC);

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
