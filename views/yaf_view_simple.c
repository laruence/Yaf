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

/* $Id: simple.c 329197 2013-01-18 05:55:37Z laruence $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "main/php_output.h"

#include "php_yaf.h"
#include "yaf_namespace.h"
#include "yaf_exception.h"
#include "yaf_loader.h"
#include "yaf_view.h"

#include "views/yaf_view_interface.h"
#include "views/yaf_view_simple.h"

zend_class_entry *yaf_view_simple_ce;

/** {{{ ARG_INFO */
ZEND_BEGIN_ARG_INFO_EX(yaf_view_simple_construct_arginfo, 0, 0, 1)
	ZEND_ARG_INFO(0, template_dir)
	ZEND_ARG_ARRAY_INFO(0, options, 1)
ZEND_END_ARG_INFO();

ZEND_BEGIN_ARG_INFO_EX(yaf_view_simple_get_arginfo, 0, 0, 0)
	ZEND_ARG_INFO(0, name)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(yaf_view_simple_isset_arginfo, 0, 0, 1)
	ZEND_ARG_INFO(0, name)
ZEND_END_ARG_INFO();

ZEND_BEGIN_ARG_INFO_EX(yaf_view_simple_assign_by_ref_arginfo, 0, 0, 2)
	ZEND_ARG_INFO(0, name)
	ZEND_ARG_INFO(1, value)
ZEND_END_ARG_INFO();

ZEND_BEGIN_ARG_INFO_EX(yaf_view_simple_eval_arginfo, 0, 0, 1)
	ZEND_ARG_INFO(0, tpl_str)
	ZEND_ARG_INFO(0, vars)
ZEND_END_ARG_INFO();

ZEND_BEGIN_ARG_INFO_EX(yaf_view_simple_clear_arginfo, 0, 0, 0)
	ZEND_ARG_INFO(0, name)
ZEND_END_ARG_INFO();
/* }}} */

#if ((PHP_MAJOR_VERSION == 5) && (PHP_MINOR_VERSION < 4))
/** {{{ static int yaf_view_simple_render_write(const char *str, uint str_length TSRMLS_DC)
*/
static int yaf_view_simple_render_write(const char *str, uint str_length TSRMLS_DC) {
	char *target;
	yaf_view_simple_buffer *buffer = YAF_G(buffer);

	if (!buffer->size) {
		buffer->size   = (str_length | VIEW_BUFFER_SIZE_MASK) + 1;
		buffer->len	   = str_length;
		buffer->buffer = emalloc(buffer->size);
		target = buffer->buffer;
	} else {
		size_t len = buffer->len + str_length;

		if (buffer->size < len + 1) {
			buffer->size   = (len | VIEW_BUFFER_SIZE_MASK) + 1;
			buffer->buffer = erealloc(buffer->buffer, buffer->size);
			if (!buffer->buffer) {
				php_error_docref(NULL TSRMLS_CC, E_ERROR, "Yaf output buffer collapsed");
			}
		}

		target = buffer->buffer + buffer->len;
		buffer->len = len;
	}

	memcpy(target, str, str_length);
	target[str_length] = '\0';

	return str_length;
}
/* }}} */
#endif

static int yaf_view_simple_valid_var_name(char *var_name, int len) /* {{{ */
{
	int i, ch;

	if (!var_name)
		return 0;

	/* These are allowed as first char: [a-zA-Z_\x7f-\xff] */
	ch = (int)((unsigned char *)var_name)[0];
	if (var_name[0] != '_' &&
			(ch < 65  /* A    */ || /* Z    */ ch > 90)  &&
			(ch < 97  /* a    */ || /* z    */ ch > 122) &&
			(ch < 127 /* 0x7f */ || /* 0xff */ ch > 255)
	   ) {
		return 0;
	}

	/* And these as the rest: [a-zA-Z0-9_\x7f-\xff] */
	if (len > 1) {
		for (i = 1; i < len; i++) {
			ch = (int)((unsigned char *)var_name)[i];
			if (var_name[i] != '_' &&
					(ch < 48  /* 0    */ || /* 9    */ ch > 57)  &&
					(ch < 65  /* A    */ || /* Z    */ ch > 90)  &&
					(ch < 97  /* a    */ || /* z    */ ch > 122) &&
					(ch < 127 /* 0x7f */ || /* 0xff */ ch > 255)
			   ) {
				return 0;
			}
		}
	}
	return 1;
}
/* }}} */

/** {{{ static int yaf_view_simple_extract(zval *tpl_vars, zval *vars TSRMLS_DC)
*/
static int yaf_view_simple_extract(zval *tpl_vars, zval *vars TSRMLS_DC) {
	zval *entry;
	zend_string *var_name;
	ulong num_key;

	if (tpl_vars && Z_TYPE_P(tpl_vars) == IS_ARRAY) {
	    ZEND_HASH_FOREACH_KEY_VAL(Z_ARRVAL_P(tpl_vars), num_key, var_name, entry) {
            if (var_name) {
                /* GLOBALS protection */
                if (var_name->len == sizeof("GLOBALS") && !strcmp(var_name->val, "GLOBALS")) {
                    continue;
                }

                if (var_name->len == sizeof("this")  && !strcmp(var_name->val, "this") && EG(scope) && EG(scope)->name->len != 0) {
                    continue;
                }


                if (yaf_view_simple_valid_var_name(var_name->val, var_name->len)) {
                    zend_set_local_var(var_name, entry, 1 TSRMLS_CC);
                }
            }
		} ZEND_HASH_FOREACH_END();
	}

	if (vars && Z_TYPE_P(vars) == IS_ARRAY) {
	    ZEND_HASH_FOREACH_KEY_VAL(Z_ARRVAL_P(vars), num_key, var_name, entry) {
	        if (var_name) {
                /* GLOBALS protection */
                if (var_name->len == sizeof("GLOBALS") && !strcmp(var_name->val, "GLOBALS")) {
                    continue;
                }

                if (var_name->len == sizeof("this")  && !strcmp(var_name->val, "this") && EG(scope) && EG(scope)->name->len != 0) {
                    continue;
                }

                if (yaf_view_simple_valid_var_name(var_name->val, var_name->len)) {
                    zend_set_local_var(var_name, entry, 1 TSRMLS_CC);
                }
            }
		} ZEND_HASH_FOREACH_END();
	}

	return 1;
}
/* }}} */

/** {{{ yaf_view_t * yaf_view_simple_instance(yaf_view_t *view, zval *tpl_dir, zval *options TSRMLS_DC)
*/
yaf_view_t * yaf_view_simple_instance(yaf_view_t *view, zval *tpl_dir, zval *options TSRMLS_DC) {
	zval *instance, tpl_vars;

	instance = view;
	if (ZVAL_IS_NULL(instance)) {
		object_init_ex(instance, yaf_view_simple_ce);
	}

	array_init(&tpl_vars);
	zend_update_property(yaf_view_simple_ce, instance, ZEND_STRL(YAF_VIEW_PROPERTY_NAME_TPLVARS), &tpl_vars TSRMLS_CC);
	zval_ptr_dtor(&tpl_vars);

	if (tpl_dir && Z_TYPE_P(tpl_dir) == IS_STRING) {
		if (IS_ABSOLUTE_PATH(Z_STRVAL_P(tpl_dir), Z_STRLEN_P(tpl_dir))) {
			zend_update_property(yaf_view_simple_ce, instance, ZEND_STRL(YAF_VIEW_PROPERTY_NAME_TPLDIR), tpl_dir TSRMLS_CC);
		} else {
			if (!view) {
				zval_ptr_dtor(instance);
			}
			yaf_trigger_error(YAF_ERR_TYPE_ERROR TSRMLS_CC, "Expects an absolute path for templates directory");
			return NULL;
		}
	} 

	if (options && IS_ARRAY == Z_TYPE_P(options)) {
		zend_update_property(yaf_view_simple_ce, instance, ZEND_STRL(YAF_VIEW_PROPERTY_NAME_OPTS), options TSRMLS_CC);
	}

	return instance;
}
/* }}} */

/** {{{ int yaf_view_simple_render(yaf_view_t *view, zval *tpl, zval * vars, zval *ret TSRMLS_DC)
*/
int yaf_view_simple_render(yaf_view_t *view, zval *tpl, zval * vars, zval *ret TSRMLS_DC) {
	zval *tpl_vars;
	char *script;
	uint len;
	//zend_array *new_symbol, *calling_symbol_table = zend_rebuild_symbol_table(TSRMLS_C);;

	if (IS_STRING != Z_TYPE_P(tpl)) {
		return 0;
	}

	ZVAL_NULL(ret);

	tpl_vars = zend_read_property(yaf_view_simple_ce, view, ZEND_STRL(YAF_VIEW_PROPERTY_NAME_TPLVARS), 1 TSRMLS_CC);

    //ZVAL_NEW_ARR(new_symbol);
	//zend_hash_init(&lling_symbol_table->ht, 0, NULL, ZVAL_PTR_DTOR, 0);

	(void)yaf_view_simple_extract(tpl_vars, vars TSRMLS_CC);

	if (php_output_start_user(NULL, 0, PHP_OUTPUT_HANDLER_STDFLAGS TSRMLS_CC) == FAILURE) {
		php_error_docref("ref.outcontrol" TSRMLS_CC, E_WARNING, "failed to create buffer");
		return 0;
	}

	if (IS_ABSOLUTE_PATH(Z_STRVAL_P(tpl), Z_STRLEN_P(tpl))) {
		script 	= Z_STRVAL_P(tpl);
		len 	= Z_STRLEN_P(tpl);

		if (yaf_loader_import(script, len + 1, 0 TSRMLS_CC) == 0) {
			php_output_end(TSRMLS_C);
			//if (calling_symbol_table) {
			//	zend_hash_destroy(&calling_symbol_table->ht);
			//}

			yaf_trigger_error(YAF_ERR_NOTFOUND_VIEW TSRMLS_CC, "Failed opening template %s: %s", script, strerror(errno));
			return 0;
		}
	} else {
		zval *tpl_dir = zend_read_property(yaf_view_simple_ce, view, ZEND_STRL(YAF_VIEW_PROPERTY_NAME_TPLDIR), 0 TSRMLS_CC);

		if (IS_STRING != Z_TYPE_P(tpl_dir)) {
			if (YAF_G(view_directory)) {
				len = spprintf(&script, 0, "%s%c%s", YAF_G(view_directory), DEFAULT_SLASH, Z_STRVAL_P(tpl));
			} else {
				php_output_end(TSRMLS_C);

				//if (calling_symbol_table) {
                //    zend_hash_destroy(&calling_symbol_table->ht);
				//}

				yaf_trigger_error(YAF_ERR_NOTFOUND_VIEW TSRMLS_CC,
						"Could not determine the view script path, you should call %s::setScriptPath to specific it",
						yaf_view_simple_ce->name);
				return 0;
			}
		} else {
			len = spprintf(&script, 0, "%s%c%s", Z_STRVAL_P(tpl_dir), DEFAULT_SLASH, Z_STRVAL_P(tpl));
		}

		if (yaf_loader_import(script, len + 1, 0 TSRMLS_CC) == 0) {
			php_output_end(TSRMLS_C);
			//if (calling_symbol_table) {
			//    zend_hash_destroy(&calling_symbol_table->ht);
			//}

			yaf_trigger_error(YAF_ERR_NOTFOUND_VIEW TSRMLS_CC, "Failed opening template %s: %s" , script, strerror(errno));
			efree(script);
			return 0;
		}
		efree(script);
	}

	//if (calling_symbol_table) {
	//        zend_hash_destroy(&calling_symbol_table->ht);
	//}

	if (php_output_get_contents(ret TSRMLS_CC) == FAILURE) {
		php_output_end(TSRMLS_C);
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Unable to fetch ob content");
		return 0;
	}

	if (php_output_discard(TSRMLS_C) != SUCCESS ) {
		return 0;
	}

	return 1;
}
/* }}} */

/** {{{ int yaf_view_simple_display(yaf_view_t *view, zval *tpl, zval * vars, zval *ret TSRMLS_DC)
*/
int yaf_view_simple_display(yaf_view_t *view, zval *tpl, zval *vars, zval *ret TSRMLS_DC) {
	zval *tpl_vars;
	char *script;
	uint len;
	zend_class_entry *old_scope;
	//zend_array *calling_symbol_table;

	if (IS_STRING != Z_TYPE_P(tpl)) {
		return 0;
	}

	//tpl_vars = zend_read_property(yaf_view_simple_ce, view, ZEND_STRL(YAF_VIEW_PROPERTY_NAME_TPLVARS), 0 TSRMLS_CC);
	//zend_hash_init(&calling_symbol_table->ht, 0, NULL, ZVAL_PTR_DTOR, 0);

	(void)yaf_view_simple_extract(tpl_vars, vars TSRMLS_CC);

	old_scope = EG(scope);
	EG(scope) = yaf_view_simple_ce;

	if (IS_ABSOLUTE_PATH(Z_STRVAL_P(tpl), Z_STRLEN_P(tpl))) {
		script 	= Z_STRVAL_P(tpl);
		len 	= Z_STRLEN_P(tpl);
		if (yaf_loader_import(script, len + 1, 0 TSRMLS_CC) == 0) {
			yaf_trigger_error(YAF_ERR_NOTFOUND_VIEW TSRMLS_CC, "Failed opening template %s: %s" , script, strerror(errno));
			EG(scope) = old_scope;
			//if (calling_symbol_table) {
            //    zend_hash_destroy(&calling_symbol_table->ht);
			//}
			return 0;
		}
	} else {
		zval *tpl_dir = zend_read_property(yaf_view_simple_ce, view, ZEND_STRL(YAF_VIEW_PROPERTY_NAME_TPLDIR), 0 TSRMLS_CC);

		if (IS_STRING != Z_TYPE_P(tpl_dir)) {
			if (YAF_G(view_directory)) {
				len = spprintf(&script, 0, "%s%c%s", YAF_G(view_directory), DEFAULT_SLASH, Z_STRVAL_P(tpl));
			} else {
				yaf_trigger_error(YAF_ERR_NOTFOUND_VIEW TSRMLS_CC,
						"Could not determine the view script path, you should call %s::setScriptPath to specific it", yaf_view_simple_ce->name);
				EG(scope) = old_scope;
				//if (calling_symbol_table) {
                //    zend_hash_destroy(&calling_symbol_table->ht);
				//}
				return 0;
			}
		} else {
			len = spprintf(&script, 0, "%s%c%s", Z_STRVAL_P(tpl_dir), DEFAULT_SLASH, Z_STRVAL_P(tpl));
		}

		if (yaf_loader_import(script, len + 1, 0 TSRMLS_CC) == 0) {
			yaf_trigger_error(YAF_ERR_NOTFOUND_VIEW TSRMLS_CC, "Failed opening template %s: %s", script, strerror(errno));
			efree(script);
			EG(scope) = old_scope;
			//if (calling_symbol_table) {
            //    zend_hash_destroy(&calling_symbol_table->ht);
			//}
			return 0;
		}
		efree(script);
	}

	EG(scope) = old_scope;
	//if (calling_symbol_table) {
    //    zend_hash_destroy(&calling_symbol_table->ht);
	//}

	return 1;
}
/* }}} */

/** {{{ int yaf_view_simple_eval(yaf_view_t *view, zval *tpl, zval * vars, zval *ret TSRMLS_DC)
*/
int yaf_view_simple_eval(yaf_view_t *view, zval *tpl, zval * vars, zval *ret TSRMLS_DC) {
	zval *tpl_vars;
	zend_array *calling_symbol_table;

	if (IS_STRING != Z_TYPE_P(tpl)) {
		return 0;
	}

	ZVAL_NULL(ret);

	tpl_vars = zend_read_property(yaf_view_simple_ce, view, ZEND_STRL(YAF_VIEW_PROPERTY_NAME_TPLVARS), 0 TSRMLS_CC);
	//zend_hash_init(&calling_symbol_table->ht, 0, NULL, ZVAL_PTR_DTOR, 0);

	(void)yaf_view_simple_extract(tpl_vars, vars TSRMLS_CC);

	if (php_output_start_user(NULL, 0, PHP_OUTPUT_HANDLER_STDFLAGS TSRMLS_CC) == FAILURE) {
		php_error_docref("ref.outcontrol" TSRMLS_CC, E_WARNING, "failed to create buffer");
		return 0;
	}

	if (Z_STRLEN_P(tpl)) {
		zval phtml;
		zend_op_array *new_op_array;
		char *eval_desc = zend_make_compiled_string_description("template code" TSRMLS_CC);
		char *tmp;
		
		/* eval require code mustn't be wrapped in opening and closing PHP tags */
		spprintf(&tmp, 0, "?>%s", Z_STRVAL_P(tpl));
		ZVAL_STRING(&phtml, tmp);
		efree(tmp);

		new_op_array = zend_compile_string(&phtml, eval_desc TSRMLS_CC);

		zval_dtor(&phtml);
		efree(eval_desc);

		if (new_op_array) {
			zval result;

			zend_execute(new_op_array, &result TSRMLS_CC);
			destroy_op_array(new_op_array TSRMLS_CC);
			efree(new_op_array);
			zval_ptr_dtor(&result);
		}
	}

	//if (calling_symbol_table) {
    //    zend_hash_destroy(&calling_symbol_table->ht);
	//}

	if (php_output_get_contents(ret TSRMLS_CC) == FAILURE) {
		php_output_end(TSRMLS_C);
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Unable to fetch ob content");
		return 0;
	}

	if (php_output_discard(TSRMLS_C) != SUCCESS ) {
		return 0;
	}
	return 1;
}
/* }}} */

/** {{{ int yaf_view_simple_assign_single(yaf_view_t *view, zend_string *name, zval *value TSRMLS_DC)
 */
int yaf_view_simple_assign_single(yaf_view_t *view, zend_string *name, zval *value TSRMLS_DC) {
	zval *tpl_vars = zend_read_property(yaf_view_simple_ce, view, ZEND_STRL(YAF_VIEW_PROPERTY_NAME_TPLVARS), 0 TSRMLS_CC);
	Z_TRY_ADDREF_P(value);
	if (zend_hash_update(Z_ARRVAL_P(tpl_vars), name, value) != NULL) {
		return 1;
	}
	return 0;
}
/* }}} */

/** {{{ int yaf_view_simple_assign_multi(yaf_view_t *view, zval *name, zval *value TSRMLS_DC)
 */
int yaf_view_simple_assign_multi(yaf_view_t *view, zval *value TSRMLS_DC) {
	zval *tpl_vars = zend_read_property(yaf_view_simple_ce, view, ZEND_STRL(YAF_VIEW_PROPERTY_NAME_TPLVARS), 0 TSRMLS_CC);
	if (Z_TYPE_P(value) == IS_ARRAY) {
		zend_hash_copy(Z_ARRVAL_P(tpl_vars), Z_ARRVAL_P(value), (copy_ctor_func_t) zval_add_ref);
		return 1;
	}
	return 0;
}
/* }}} */

/** {{{ void yaf_view_simple_clear_assign(yaf_view_t *view, zend_string *name TSRMLS_DC)
 */
void yaf_view_simple_clear_assign(yaf_view_t *view, zend_string *name TSRMLS_DC) {
	zval *tpl_vars = zend_read_property(yaf_view_simple_ce, view, ZEND_STRL(YAF_VIEW_PROPERTY_NAME_TPLVARS), 0 TSRMLS_CC);
	if (tpl_vars && Z_TYPE_P(tpl_vars) == IS_ARRAY) {
		if (name) {
			zend_symtable_del(Z_ARRVAL_P(tpl_vars), name);
		} else {
			zend_hash_clean(Z_ARRVAL_P(tpl_vars));
		}
	} 
}
/* }}} */

/** {{{ proto public Yaf_View_Simple::__construct(string $tpl_dir, array $options = NULL)
*/
PHP_METHOD(yaf_view_simple, __construct) {
	zval *tpl_dir, *options = NULL;
	zval rt, *self = getThis();

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z|a", &tpl_dir, &options) == FAILURE) {
		YAF_UNINITIALIZED_OBJECT(getThis());
		return;
	}

    if (!self) {
        ZVAL_NULL(&rt);
        self = &rt;
    }
	yaf_view_simple_instance(self, tpl_dir, options TSRMLS_CC);
}
/* }}} */

/** {{{ proto public Yaf_View_Simple::__isset($name)
*/
PHP_METHOD(yaf_view_simple, __isset) {
	zend_string *name;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "S", &name) == FAILURE) {
		return;
	} else {
		zval *tpl_vars = zend_read_property(yaf_view_simple_ce, getThis(), ZEND_STRL(YAF_VIEW_PROPERTY_NAME_TPLVARS), 1 TSRMLS_CC);
		RETURN_BOOL(zend_hash_exists(Z_ARRVAL_P(tpl_vars), name));
	}
}
/* }}} */

/** {{{ proto public Yaf_View_Simple::setScriptPath(string $tpl_dir)
*/
PHP_METHOD(yaf_view_simple, setScriptPath) {
	zval *tpl_dir;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &tpl_dir) == FAILURE) {
		return;
	}

	if (Z_TYPE_P(tpl_dir) == IS_STRING && IS_ABSOLUTE_PATH(Z_STRVAL_P(tpl_dir), Z_STRLEN_P(tpl_dir))) {
		zend_update_property(yaf_view_simple_ce, getThis(), ZEND_STRL(YAF_VIEW_PROPERTY_NAME_TPLDIR), tpl_dir TSRMLS_CC);
		RETURN_ZVAL(getThis(), 1, 0);
	}

	RETURN_FALSE;
}
/* }}} */

/** {{{ proto public Yaf_View_Simple::getScriptPath(void)
*/
PHP_METHOD(yaf_view_simple, getScriptPath) {
	zval *tpl_dir = zend_read_property(yaf_view_simple_ce, getThis(), ZEND_STRL(YAF_VIEW_PROPERTY_NAME_TPLDIR), 0 TSRMLS_CC);
	if (IS_STRING != Z_TYPE_P(tpl_dir) && YAF_G(view_directory)) {
		RETURN_STRING(YAF_G(view_directory));
	} 
	RETURN_ZVAL(tpl_dir, 1, 0);
}
/* }}} */

/** {{{ proto public Yaf_View_Simple::compose(string $script, zval *args)
*/
PHP_METHOD(yaf_view_simple, compose) {
	zend_string *script = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "S", &script) == FAILURE) {
		return;
	}

	if (!script) {
		RETURN_FALSE;
	}

}
/* }}} */

/** {{{ proto public Yaf_View_Simple::assign(mixed $value, mixed $value = null)
*/
PHP_METHOD(yaf_view_simple, assign) {
	uint argc = ZEND_NUM_ARGS();
	if (argc == 1) {
		zval *value;
		if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &value) == FAILURE) {
			return;
		}
		if (yaf_view_simple_assign_multi(getThis(), value TSRMLS_CC)) {
			RETURN_ZVAL(getThis(), 1, 0);
		}
	} else if (argc == 2) {
		zval *value;
		zend_string *name;
		if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "Sz", &name, &value) == FAILURE) {
			return;
		}
	    if (yaf_view_simple_assign_single(getThis(), name, value TSRMLS_CC)){
			RETURN_ZVAL(getThis(), 1, 0);
		}
	} else {
		WRONG_PARAM_COUNT;
	}

	RETURN_FALSE;
}
/* }}} */

/** {{{ proto public Yaf_View_Simple::assignRef(mixed $value, mixed $value)
*/
PHP_METHOD(yaf_view_simple, assignRef) {
	zend_string *name; 
	zval *value, *tpl_vars;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "Sz", &name, &value) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	tpl_vars = zend_read_property(yaf_view_simple_ce, getThis(), ZEND_STRL(YAF_VIEW_PROPERTY_NAME_TPLVARS), 1 TSRMLS_CC);

	Z_TRY_ADDREF_P(value);
	if (zend_hash_update(Z_ARRVAL_P(tpl_vars), name, value) != NULL) {
		RETURN_ZVAL(getThis(), 1, 0);
	}
	RETURN_FALSE;
}
/* }}} */

/** {{{ proto public Yaf_View_Simple::get($name)
*/
PHP_METHOD(yaf_view_simple, get) {
	zend_string *name = NULL;
	zval *tpl_vars, *ret;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|S", &name) == FAILURE) {
		return;
	}

	tpl_vars = zend_read_property(yaf_view_simple_ce, getThis(), ZEND_STRL(YAF_VIEW_PROPERTY_NAME_TPLVARS), 1 TSRMLS_CC);

	if (tpl_vars && Z_TYPE_P(tpl_vars) == IS_ARRAY) {
		if (name) {
			if ((ret = zend_hash_find(Z_ARRVAL_P(tpl_vars), name)) != NULL) {
				RETURN_ZVAL(ret, 1, 0);
			} 
		} else {
			RETURN_ZVAL(tpl_vars, 1, 0);
		}
	}

	RETURN_NULL();
}
/* }}} */

/** {{{ proto public Yaf_View_Simple::render(string $tpl, array $vars = NULL)
*/
PHP_METHOD(yaf_view_simple, render) {
	zval *tpl, *vars = NULL; /*, *tpl_vars;*/

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z|z", &tpl, &vars) == FAILURE) {
		return;
	}

	if (!yaf_view_simple_render(getThis(), tpl, vars, return_value TSRMLS_CC)) {
		RETVAL_FALSE;
	}
}
/* }}} */

/** {{{ proto public Yaf_View_Simple::eval(string $tpl_content, array $vars = NULL)
*/
PHP_METHOD(yaf_view_simple, eval) {
	zval *tpl, *vars = NULL; /*, *tpl_vars;*/

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z|z", &tpl, &vars) == FAILURE) {
		return;
	}

	/*tpl_vars = zend_read_property(yaf_view_simple_ce, getThis(), ZEND_STRL(YAF_VIEW_PROPERTY_NAME_TPLVARS), 1 TSRMLS_CC);*/
	if (!yaf_view_simple_eval(getThis(), tpl, vars, return_value TSRMLS_CC)) {
		RETURN_FALSE;
	}
}
/* }}} */

/** {{{ proto public Yaf_View_Simple::display(string $tpl, array $vars = NULL)
*/
PHP_METHOD(yaf_view_simple, display) {
	zval *tpl, *vars = NULL; /* , *tpl_vars*/

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z|z", &tpl, &vars) == FAILURE) {
		return;
	}

	/*tpl_vars = zend_read_property(yaf_view_simple_ce, getThis(), ZEND_STRL(YAF_VIEW_PROPERTY_NAME_TPLVARS), 0 TSRMLS_CC);*/
	if (!yaf_view_simple_display(getThis(), tpl, vars, return_value TSRMLS_CC)) {
		RETURN_FALSE;
	}

	RETURN_TRUE;
}
/* }}} */

/** {{{ proto public Yaf_View_Simple::clear(string $name)
*/
PHP_METHOD(yaf_view_simple, clear) {
	zend_string *name = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|S", &name) == FAILURE) {
		return;
	}
    
	yaf_view_simple_clear_assign(getThis(), name TSRMLS_CC);

	RETURN_ZVAL(getThis(), 1, 0);
}
/* }}} */

/** {{{ yaf_view_simple_methods
*/
zend_function_entry yaf_view_simple_methods[] = {
	PHP_ME(yaf_view_simple, __construct, yaf_view_simple_construct_arginfo, ZEND_ACC_CTOR|ZEND_ACC_FINAL|ZEND_ACC_PUBLIC)
	PHP_ME(yaf_view_simple, __isset, yaf_view_simple_isset_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(yaf_view_simple, get, yaf_view_simple_get_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(yaf_view_simple, assign, yaf_view_assign_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(yaf_view_simple, render, yaf_view_render_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(yaf_view_simple, eval,  yaf_view_simple_eval_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(yaf_view_simple, display, yaf_view_display_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(yaf_view_simple, assignRef, yaf_view_simple_assign_by_ref_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(yaf_view_simple, clear, yaf_view_simple_clear_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(yaf_view_simple, setScriptPath, yaf_view_setpath_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(yaf_view_simple, getScriptPath, yaf_view_getpath_arginfo, ZEND_ACC_PUBLIC)
	PHP_MALIAS(yaf_view_simple, __get, get, yaf_view_simple_get_arginfo, ZEND_ACC_PUBLIC)
	PHP_MALIAS(yaf_view_simple, __set, assign, yaf_view_assign_arginfo, ZEND_ACC_PUBLIC)
	{NULL, NULL, NULL}
};
/* }}} */

/** {{{ YAF_STARTUP_FUNCTION
*/
YAF_STARTUP_FUNCTION(view_simple) {
	zend_class_entry ce;

	YAF_INIT_CLASS_ENTRY(ce, "Yaf_View_Simple", "Yaf\\View\\Simple", yaf_view_simple_methods);
	yaf_view_simple_ce = zend_register_internal_class_ex(&ce, NULL TSRMLS_CC);

	zend_declare_property_null(yaf_view_simple_ce, ZEND_STRL(YAF_VIEW_PROPERTY_NAME_TPLVARS), ZEND_ACC_PROTECTED TSRMLS_CC);
	zend_declare_property_null(yaf_view_simple_ce, ZEND_STRL(YAF_VIEW_PROPERTY_NAME_TPLDIR),  ZEND_ACC_PROTECTED TSRMLS_CC);
	zend_declare_property_null(yaf_view_simple_ce, ZEND_STRL(YAF_VIEW_PROPERTY_NAME_OPTS),  ZEND_ACC_PROTECTED TSRMLS_CC);

	zend_class_implements(yaf_view_simple_ce TSRMLS_CC, 1, yaf_view_interface_ce);

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

