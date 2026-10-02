/**************************************************************************/
/*  gdscript_helper.cpp                                                   */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#include "gdscript_helper.h"

#ifdef TOOLS_ENABLED

#include "core/io/resource_loader.h"
#include "core/object/class_db.h"
#include "editor/gdscript_editor_language.h"

void GDScriptHelper::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_validate_code", "code", "script_path"), &GDScriptHelper::set_validate_code, DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("has_errors"), &GDScriptHelper::has_errors);
	ClassDB::bind_method(D_METHOD("get_errors"), &GDScriptHelper::get_errors);
	ClassDB::bind_method(D_METHOD("has_functions"), &GDScriptHelper::has_functions);
	ClassDB::bind_method(D_METHOD("get_functions"), &GDScriptHelper::get_functions);

	ClassDB::bind_method(D_METHOD("set_completion_code", "code", "script_path", "script_owner"), &GDScriptHelper::set_completion_code, DEFVAL(String()), DEFVAL(Variant()));
	ClassDB::bind_method(D_METHOD("has_completion_options"), &GDScriptHelper::has_completion_options);
	ClassDB::bind_method(D_METHOD("get_completion_options"), &GDScriptHelper::get_completion_options);
	ClassDB::bind_method(D_METHOD("has_completion_hint"), &GDScriptHelper::has_completion_hint);
	ClassDB::bind_method(D_METHOD("get_completion_hint"), &GDScriptHelper::get_completion_hint);
	ClassDB::bind_method(D_METHOD("is_completion_forced"), &GDScriptHelper::is_completion_forced);

	ClassDB::bind_method(D_METHOD("lookup_code", "code", "symbol", "script_path", "script_owner"), &GDScriptHelper::lookup_code, DEFVAL(String()), DEFVAL(Variant()));

	BIND_ENUM_CONSTANT(LOOKUP_RESULT_SCRIPT_LOCATION);
	BIND_ENUM_CONSTANT(LOOKUP_RESULT_CLASS);
	BIND_ENUM_CONSTANT(LOOKUP_RESULT_CLASS_CONSTANT);
	BIND_ENUM_CONSTANT(LOOKUP_RESULT_CLASS_PROPERTY);
	BIND_ENUM_CONSTANT(LOOKUP_RESULT_CLASS_METHOD);
	BIND_ENUM_CONSTANT(LOOKUP_RESULT_CLASS_SIGNAL);
	BIND_ENUM_CONSTANT(LOOKUP_RESULT_CLASS_ENUM);
	BIND_ENUM_CONSTANT(LOOKUP_RESULT_CLASS_TBD_GLOBALSCOPE);
	BIND_ENUM_CONSTANT(LOOKUP_RESULT_CLASS_ANNOTATION);
	BIND_ENUM_CONSTANT(LOOKUP_RESULT_LOCAL_CONSTANT);
	BIND_ENUM_CONSTANT(LOOKUP_RESULT_LOCAL_VARIABLE);
	BIND_ENUM_CONSTANT(LOOKUP_RESULT_MAX);
}

int GDScriptHelper::_get_completion_kind_priority(ScriptLanguage::CodeCompletionKind p_kind) {
	switch (p_kind) {
		case ScriptLanguage::CODE_COMPLETION_KIND_VARIABLE:
			return 0;
		case ScriptLanguage::CODE_COMPLETION_KIND_MEMBER:
			return 1;
		case ScriptLanguage::CODE_COMPLETION_KIND_FUNCTION:
			return 2;
		case ScriptLanguage::CODE_COMPLETION_KIND_ENUM:
			return 3;
		case ScriptLanguage::CODE_COMPLETION_KIND_SIGNAL:
			return 4;
		case ScriptLanguage::CODE_COMPLETION_KIND_CONSTANT:
			return 5;
		case ScriptLanguage::CODE_COMPLETION_KIND_CLASS:
			return 6;
		case ScriptLanguage::CODE_COMPLETION_KIND_NODE_PATH:
			return 7;
		case ScriptLanguage::CODE_COMPLETION_KIND_FILE_PATH:
			return 8;
		case ScriptLanguage::CODE_COMPLETION_KIND_PLAIN_TEXT:
			return 9;
		case ScriptLanguage::CODE_COMPLETION_KIND_KEYWORD:
			return 10;
		case ScriptLanguage::CODE_COMPLETION_KIND_MAX:
			return 11;
	}
	return 11;
}

bool GDScriptHelper::set_validate_code(const String &p_code, const String &p_path) {
	functions.clear();
	errors.clear();
	GDScriptEditorLanguage *language = GDScriptEditorLanguage::get_singleton();
	ERR_FAIL_NULL_V(language, false);
	return language->validate(p_code, p_path, &errors, nullptr, &functions, nullptr);
}

bool GDScriptHelper::has_errors() const {
	return !errors.is_empty();
}

TypedArray<Dictionary> GDScriptHelper::get_errors() const {
	TypedArray<Dictionary> result;
	for (const EditorLanguage::ScriptError &error_data : errors) {
		Dictionary error;
		error["line"] = error_data.start_line - 1;
		error["column"] = error_data.start_column - 1;
		error["message"] = error_data.message;
		result.push_back(error);
	}
	return result;
}

bool GDScriptHelper::has_functions() const {
	return !functions.is_empty();
}

Dictionary GDScriptHelper::get_functions() const {
	Dictionary result;
	for (const String &function : functions) {
		result[function.get_slice(":", 0)] = function.get_slice(":", 1).to_int() - 1;
	}
	return result;
}

Error GDScriptHelper::set_completion_code(const String &p_code, const String &p_path, Object *p_owner) {
	completion_options.clear();
	completion_forced = false;
	completion_hint.clear();

	GDScriptEditorLanguage *language = GDScriptEditorLanguage::get_singleton();
	ERR_FAIL_NULL_V(language, ERR_UNAVAILABLE);
	const Error error = language->complete_code(p_code, p_path, p_owner, &completion_options, completion_forced, completion_hint);
	completion_options.sort_custom<CompletionOptionComparator>();
	return error;
}

bool GDScriptHelper::has_completion_options() const {
	return !completion_options.is_empty();
}

TypedArray<Dictionary> GDScriptHelper::get_completion_options() const {
	TypedArray<Dictionary> result;
	for (const ScriptLanguage::CodeCompletionOption &completion_option : completion_options) {
		Dictionary option;
		option["type"] = completion_option.kind;
		option["display_text"] = completion_option.display;
		option["insert_text"] = completion_option.insert_text;
		option["default_value"] = completion_option.default_value;
		result.push_back(option);
	}
	return result;
}

bool GDScriptHelper::has_completion_hint() const {
	return !completion_hint.is_empty();
}

String GDScriptHelper::get_completion_hint() const {
	return completion_hint;
}

bool GDScriptHelper::is_completion_forced() const {
	return completion_forced;
}

Dictionary GDScriptHelper::lookup_code(const String &p_code, const String &p_symbol, const String &p_path, Object *p_owner) {
	Dictionary result;
	GDScriptEditorLanguage *language = GDScriptEditorLanguage::get_singleton();
	ERR_FAIL_NULL_V(language, result);

	EditorLanguage::LookupResult lookup_result;
	if (language->lookup_code(p_code, p_symbol, p_path, p_owner, lookup_result) != OK) {
		return result;
	}

	Ref<Resource> script;
	if (!lookup_result.script_path.is_empty() && ResourceLoader::exists(lookup_result.script_path)) {
		script = ResourceLoader::load(lookup_result.script_path);
	}
	result["type"] = static_cast<int>(lookup_result.type);
	result["script"] = script;
	result["class_name"] = lookup_result.class_name;
	result["class_member"] = lookup_result.class_member;
	result["class_path"] = lookup_result.script_path;
	result["location"] = lookup_result.location;
	return result;
}

#endif // TOOLS_ENABLED
