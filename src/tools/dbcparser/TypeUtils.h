/*
 * Copyright (c) 2014 - 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "Types.h"
#include <array>
#include <optional>
#include <utility>
#include <string>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <string_view>
#include <stdexcept>

namespace ember::dbc {

using TypeComponents = std::pair<std::string, std::optional<unsigned int>>;
using ComponentCache = std::unordered_map<std::string, TypeComponents>;

TypeComponents extract_components(const std::string& type);
std::string pascal_to_underscore(std::string name);
types::Base* locate_type_base(const types::Struct& base, const std::string& type_name);

const extern std::unordered_map<std::string_view, std::pair<std::string_view, bool>> type_map;
const extern std::unordered_map<std::string_view, int> type_size_map;
const extern std::unordered_set<std::string_view> cpp_keywords;
const extern std::array<std::string_view, 8> string_ref_loc_regions;


template<typename T>
void walk_dbc_fields(T& visitor, const types::Struct* dbc, const types::Base* parent, ComponentCache* ccache = nullptr) {
	for(auto f : dbc->fields) {
		TypeComponents components;

		if(ccache) {
			if(auto it = ccache->find(f.underlying_type); it != ccache->end()) {
				components = it->second;
			} else {
				components = extract_components(f.underlying_type);	
				ccache->insert_or_assign(f.underlying_type, components);
			}
		} else {
			components = extract_components(f.underlying_type);
		}

		if(type_map.contains(components.first)) {
			visitor.visit(&f, parent);
		} else {
			const auto found = locate_type_base(*dbc, components.first);

			if(!found) {
				throw std::runtime_error("Unknown field type encountered, " + f.underlying_type);
			}

			visitor.visit(&f, parent);
		}
	}
}

class TypeMetrics final : public types::TypeVisitor {
public:
	unsigned int fields = 0;
	unsigned int record_size = 0;

	void visit(const types::Struct* type, const types::Field* parent) override {
		walk_dbc_fields(*this, type, parent);
	}

	void visit(const types::Enum* type) override {
		++fields;
		record_size += type_size_map.at(type->underlying_type);
	}

	void visit(const types::Field* type, const types::Base* parent) override {
		auto components = extract_components(type->underlying_type);
		int scalar_size = 0;

		if(auto it = type_size_map.find(components.first); it != type_size_map.end()) {
			scalar_size = it->second;
		} else {
			const auto base = locate_type_base(static_cast<const types::Struct&>(*type->parent), components.first);

			if(!base) {
				throw std::runtime_error("Unable to locate base type");
			}
			
			if(base->type == types::Type::ENUM) {
				scalar_size = type_size_map.at(static_cast<const types::Enum*>(base)->underlying_type);
			} else if(base->type == types::Type::STRUCT) {
				visit(static_cast<const types::Struct*>(base), type);
				return;
			}
		}

		// handle arrays
		unsigned int additional_fields = 1;

		if(components.first == "string_ref_loc") {
			additional_fields = 9;
		}

		if(components.second) {
			additional_fields *= *components.second;
			record_size += (scalar_size * (*components.second));
		} else {
			record_size += scalar_size;
		}

		fields += additional_fields;
	}
};

} // dbc, ember