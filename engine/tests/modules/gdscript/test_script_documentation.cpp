/**************************************************************************/
/*  test_script_documentation.cpp                                         */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                          */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                        */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/**************************************************************************/

#include "tests/test_macros.h"

TEST_FORCE_LINK(test_script_documentation)

#include "modules/modules_enabled.gen.h"

#ifdef MODULE_GDSCRIPT_ENABLED

#include "core/object/class_db.h"

#include "modules/gdscript/gdscript.h"

namespace TestScriptDocumentation {

TEST_CASE("[Script] Structured documentation is exposed to scripts") {
	CHECK(ClassDB::has_method(SNAME("Script"), SNAME("get_script_documentation_list")));
	GDScriptLanguage::get_singleton()->init();

	Ref<GDScript> script;
	script.instantiate();
	script->set_path("res://test_script_documentation.gd");
	script->set_source_code(R"(## Test brief.
##
## Test description.
extends RefCounted

## Stored value.
var value: int = 7

## Greets a name.
func greet(name: String = "world") -> String:
	return name
)");
	const Error reload_error = script->reload();
	REQUIRE(reload_error == OK);
	if (reload_error != OK) {
		return;
	}

	Variant documentation_variant = script->call(SNAME("get_script_documentation_list"));
	REQUIRE(documentation_variant.get_type() == Variant::ARRAY);
	TypedArray<Dictionary> documentation = documentation_variant;
	REQUIRE(documentation.size() == 1);
	if (documentation.size() != 1) {
		return;
	}

	Dictionary class_documentation = documentation[0];
	CHECK(class_documentation["brief_description"] == String("Test brief."));
	CHECK(String(class_documentation["description"]).contains("Test description."));

	TypedArray<Dictionary> variables = class_documentation["variables"];
	REQUIRE(variables.size() == 1);
	Dictionary variable = variables[0];
	CHECK(variable["name"] == String("value"));
	CHECK(variable["type"] == String("int"));

	TypedArray<Dictionary> methods = class_documentation["methods"];
	REQUIRE(methods.size() == 1);
	Dictionary method = methods[0];
	CHECK(method["name"] == String("greet"));
	TypedArray<Dictionary> arguments = method["arguments"];
	REQUIRE(arguments.size() == 1);
	Dictionary argument = arguments[0];
	CHECK(argument["name"] == String("name"));
	CHECK(argument["type"] == String("String"));
}

} // namespace TestScriptDocumentation

#endif // MODULE_GDSCRIPT_ENABLED
