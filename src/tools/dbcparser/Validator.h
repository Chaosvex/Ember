/*
 * Copyright (c) 2014 - 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "Exception.h"
#include "Types.h"
#include "TypeUtils.h"
#include "TreeNode.h"
#include <functional>
#include <optional>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <regex>
#include <utility>
#include <cstddef>

namespace ember::dbc {

//todo - move
struct NameTester {
	NameTester() : regex_("^[A-Za-z_][A-Za-z_0-9]*$") { }

	void operator()(const std::string& name) const {
		if(cpp_keywords.contains(name)) {
			throw exception(name + " is a reserved word and cannot be used as an identifier");
		}

		if(!std::regex_match(name, regex_)) {
			throw exception(name + " is not a valid C++ identifier");
		}
	}

private:
	const std::regex regex_;
};

using TypeStore = std::vector<std::string>;

class Validator final {
public:
	enum Options {
		VAL_DEFAULT           = 1 << 0,
		VAL_SKIP_FOREIGN_KEYS = 1 << 1
	};

private:
	NameTester name_check_;
	TreeNode<std::string> root_;
	const types::Definitions* definitions_;
	std::vector<std::string> names_;
	Options options_;

	void validate_definition(const types::Base* def);
	void check_multiple_definitions(const types::Base* def);
	void check_key_types(const types::Field& def);
	void check_foreign_keys(const types::Field& def);
	std::optional<std::reference_wrapper<const types::Field>> locate_fk_parent(const std::string& parent);

	void build_type_tree();
	void recursive_type_parse(TreeNode<std::string>* parent, const types::Base* def);
	void map_struct_types(TreeNode<std::string>* parent, const types::Struct* def);
	void add_user_type(TreeNode<std::string>* node, const std::string& type);
	void validate_struct(const types::Struct* def, const TreeNode<std::string>* types);
	void validate_enum(const types::Enum* def);
	void check_dup_key_types(const types::Struct* def);
	void validate_enum_options(const types::Enum* def);
	void validate_enum_option_value(const std::string& type, const std::string& value);
	void check_field_types(const types::Struct* def, const TreeNode<std::string>* curr_def);
	bool recursive_ascent_field_type_check(const std::string& type, const TreeNode<std::string>* node,
	                                       const TreeNode<std::string>* prev_node = nullptr);
	const TreeNode<std::string>* locate_type_node(const std::string& name, const TreeNode<std::string>* node);
	template<typename T> void range_check(long long value);

	void print_type_tree(const TreeNode<std::string>* types, std::size_t depth = 0);

public:
	Validator() = default;
	void validate(const types::Definitions& definitions_, Options options = VAL_DEFAULT);
};

} // dbc, ember