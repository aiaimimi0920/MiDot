/**************************************************************************/
/*  gdscript_helper.h                                                     */
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

#pragma once

#ifdef TOOLS_ENABLED

#include "core/object/editor_language.h"
#include "core/object/ref_counted.h"

class GDScriptHelper : public RefCounted {
	GDCLASS(GDScriptHelper, RefCounted);

	List<String> functions;
	List<EditorLanguage::ScriptError> errors;
	List<ScriptLanguage::CodeCompletionOption> completion_options;
	String completion_hint;
	bool completion_forced = false;

	static int _get_completion_kind_priority(ScriptLanguage::CodeCompletionKind p_kind);

	struct CompletionOptionComparator {
		_FORCE_INLINE_ bool operator()(const ScriptLanguage::CodeCompletionOption &p_left, const ScriptLanguage::CodeCompletionOption &p_right) const {
			if (p_left.location != p_right.location) {
				return p_left.location < p_right.location;
			}
			if (p_left.kind != p_right.kind) {
				return _get_completion_kind_priority(p_left.kind) < _get_completion_kind_priority(p_right.kind);
			}
			return p_left.display < p_right.display;
		}
	};

protected:
	static void _bind_methods();

public:
	enum LookupResultType {
		LOOKUP_RESULT_SCRIPT_LOCATION,
		LOOKUP_RESULT_CLASS,
		LOOKUP_RESULT_CLASS_CONSTANT,
		LOOKUP_RESULT_CLASS_PROPERTY,
		LOOKUP_RESULT_CLASS_METHOD,
		LOOKUP_RESULT_CLASS_SIGNAL,
		LOOKUP_RESULT_CLASS_ENUM,
		LOOKUP_RESULT_CLASS_TBD_GLOBALSCOPE,
		LOOKUP_RESULT_CLASS_ANNOTATION,
		LOOKUP_RESULT_LOCAL_CONSTANT,
		LOOKUP_RESULT_LOCAL_VARIABLE,
		LOOKUP_RESULT_MAX,
	};

	bool set_validate_code(const String &p_code, const String &p_path = String());
	bool has_errors() const;
	TypedArray<Dictionary> get_errors() const;
	bool has_functions() const;
	Dictionary get_functions() const;

	Error set_completion_code(const String &p_code, const String &p_path = String(), Object *p_owner = nullptr);
	bool has_completion_options() const;
	TypedArray<Dictionary> get_completion_options() const;
	bool has_completion_hint() const;
	String get_completion_hint() const;
	bool is_completion_forced() const;

	Dictionary lookup_code(const String &p_code, const String &p_symbol, const String &p_path = String(), Object *p_owner = nullptr);
};

VARIANT_ENUM_CAST(GDScriptHelper::LookupResultType);

#endif // TOOLS_ENABLED
