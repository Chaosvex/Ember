/*
 * Copyright (c) 2019 - 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "SQLDMLGenerator.h"
#include "TypeUtils.h"
#include "DBCHeader.h"
#include <logger/Logger.h>
#include <spark/buffers/pmr/BinaryStream.h>
#include <spark/buffers/DynamicBuffer.h>
#include <spark/buffers/pmr/BufferAdaptor.h>
#include <gsl/gsl_util>
#include <fstream>
#include <vector>
#include <sstream>
#include <filesystem>
#include <cstddef>
#include <cstdint>

namespace ember::dbc {

class DMLPrinter final : public types::TypeVisitor {
	spark::io::pmr::BinaryStream& stream_;
	std::span<const std::byte> data_;
	const std::size_t string_block_index_;
	ComponentCache ccache_;
	std::vector<std::string> values_;

public:
	DMLPrinter(spark::io::pmr::BinaryStream& stream, std::span<std::byte> data,
	           std::size_t string_block_index, ComponentCache& ccache)
		: stream_(stream),
		  data_(data),
		  string_block_index_(string_block_index),
		  ccache_(ccache) { }

	void visit(const types::Struct* structure, const types::Field* parent) override {
		walk_dbc_fields(*this, structure, parent);
	}

	void visit(const types::Enum* enumeration) override {
		// we don't care about enums at the moment
	}

	void check_string_offset(const std::size_t offset) {
		if(offset >= data_.size()) {
			throw std::runtime_error("Attempting to out of bounds string block read!");
		}
	
		for(auto i = offset; i < data_.size(); ++i) {
			if(data_[i] == std::byte(0)) {
				return;
			}
		}
	
		throw std::runtime_error("Potential string block overrun.");
	}

	std::string read_string(std::size_t offset) {
		offset += string_block_index_;
		check_string_offset(offset);
		std::string string = reinterpret_cast<const char*>(&data_[offset]);
		return string;
	}

	void generate_column(const std::string& type) {
		if(type == "int8") {
			std::int8_t val = 0;
			stream_ >> val;
			values_.emplace_back(std::to_string(val));
		} else if(type == "uint8") {
			std::uint8_t val = 0;
			stream_ >> val;
			values_.emplace_back(std::to_string(val));
		} else if(type == "int16") {
			std::int16_t val = 0;
			stream_ >> val;
			values_.emplace_back(std::to_string(val));
		} else if(type == "uint16") {
			std::uint16_t val = 0;
			stream_ >> val;
			values_.emplace_back(std::to_string(val));
		} else if(type == "uint32") {
			std::uint32_t val = 0;
			stream_ >> val;
			values_.emplace_back(std::to_string(val));
		} else if(type == "int32" || type == "bool32") {
			std::int32_t val = 0;
			stream_ >> val;
			values_.emplace_back(std::to_string(val));
		} else if(type == "bool") {
			std::uint8_t val = 0;
			stream_ >> val;
			values_.emplace_back(std::to_string(val));
		} else if(type == "float") {
			float val = 0.f;
			stream_ >> val;
			values_.emplace_back(std::to_string(val));
		} else if(type == "double") {
			double val = 0.0;
			stream_ >> val;
			values_.emplace_back(std::to_string(val));
		} else if(type == "string_ref") {
			std::uint32_t offset = 0;
			stream_ >> offset;
			std::stringstream qstr;
			qstr << std::quoted(read_string(offset));
			values_.emplace_back(qstr.str());
		} else if(type == "string_ref_loc") {
			std::uint32_t offset, flags = 0;

			for(const auto& loc : string_ref_loc_regions) {
				stream_ >> offset;
				std::stringstream qstr;
				qstr << std::quoted(read_string(offset));
				values_.emplace_back(qstr.str());
			}

			stream_ >> flags;
			values_.emplace_back(std::to_string(flags));
		} else {
			throw std::runtime_error("Unhandled type, " + type);
		}
	}

	void visit(const types::Field* field, const types::Base* parent) override {
		const auto& type = field->underlying_type;
		TypeComponents components;

		if(auto it = ccache_.find(type); it != ccache_.end()) {
			components = it->second;
		} else {
			components = extract_components(type);	
			ccache_[type] = components;
		}

		// handle primitive types
		if(type_map.contains(components.first)) {
			auto i = 0u;

			do {
				generate_column(components.first);
				 ++i;
			} while(components.second && i < *components.second);
		} else { // handle user-defined types
			const auto found = locate_type_base(static_cast<types::Struct&>(*field->parent), components.first);

			if(!found) {
				throw std::runtime_error("Unable to locate type base");
			}

			if(found->type == types::Type::STRUCT) {
				if(components.second) {
					for(auto i = 0u; i < *components.second; ++i) {
						visit(static_cast<const types::Struct*>(found), field);
					}
				} else {
					visit(static_cast<const types::Struct*>(found), field);
				}
			} else if(found->type == types::Type::ENUM) {
				const auto type = static_cast<const types::Enum*>(found);
				auto i = 0u;

				do {
					generate_column(type->underlying_type);
					++i;
				} while(components.second && i < *components.second);
			} else {
				throw std::runtime_error("Unhandled type");
			}
		}
	}

	std::string record() {
		std::stringstream record;
		bool first = true;

		for(const auto& field : values_) {
			if(!first) {
				record << ", ";
			}

			first = false;
			record << field;
		}

		return record.str();
	}
};

void write_dbc_dml(const types::Struct& dbc, std::ofstream& out, std::vector<std::byte> data) {
	if(dbc.fields.empty()) {
		LOG_WARN_GLOB << "Skipping SQL generation for " << dbc.name << " - table cannot have no columns" << LOG_SYNC;
		return;
	}

	spark::io::pmr::BufferAdaptor buffer(data);
	spark::io::pmr::BinaryStream stream(buffer);

	Header header;
	stream >> header.magic;
	stream >> header.records;
	stream >> header.fields;
	stream >> header.record_size;
	stream >> header.string_block_size;

	LOG_DEBUG_GLOB << dbc.name << ": " << "records: " << header.records << ", record size: "
	               << header.record_size << ", fields: " << header.fields << ", string block size: "
	               << header.string_block_size << LOG_SYNC;

	TypeMetrics metrics;
	walk_dbc_fields(metrics, &dbc, dbc.parent);
	validate_dbc(dbc.name, header, metrics.record_size, metrics.fields, data.size());

	const std::size_t string_block_index = DBC_HEADER_SIZE + (header.record_size * header.records);
	const auto MAX_RECORDS_PER_INSERT = 100u; // keep the server from rejecting large packets/hitting query timeouts
	ComponentCache ccache;

	for(auto i = 0u; i < header.records; i += MAX_RECORDS_PER_INSERT) {
		bool first = true;
		const auto& real_name = dbc.alias.empty()? dbc.name : dbc.alias;
		out << "INSERT INTO `" << pascal_to_underscore(real_name) << "` VALUES\n";
	
		for(auto j = 0; j < MAX_RECORDS_PER_INSERT && j + i < header.records; ++j) {
			DMLPrinter printer(stream, data, string_block_index, ccache);

			if(!first) {
				out << ",\n";
			}

			first = false;
			walk_dbc_fields(printer, &dbc, dbc.parent, &ccache);
			out << "(" << printer.record() << ")";
		}
		
		out << ";\n\n";
	}
}

std::vector<std::byte> load_dbc(const std::string& filename) {
	const auto size = std::filesystem::file_size(filename);
	std::ifstream file(filename, std::ios::binary);

	if(!file) {
		throw std::runtime_error("Unable to open DBC for reading, " + filename);
	}

	std::vector<std::byte> data(gsl::narrow<std::size_t>(size));
	file.read(reinterpret_cast<char*>(data.data()), data.size());
	return data;
}

void generate_sql_dml(const types::Definitions& defs, const std::string& out_path) {
	std::ofstream file(out_path + "dbc_data.sql");

	if(!file) {
		throw std::runtime_error("Unable to open file for SQL DML generation");
	}
	
	for(const auto& def : defs) {
		if(def->type != types::Type::STRUCT) {
			continue;
		}

		LOG_DEBUG_GLOB << "Generating SQL DML for " << def->name << LOG_ASYNC;
		auto data = load_dbc(def->name + ".dbc");
		write_dbc_dml(static_cast<const types::Struct&>(*def.get()), file, std::move(data));
		LOG_DEBUG_GLOB << "Completed SQL DML generation for " << def->name << LOG_ASYNC;
	}

	LOG_INFO_GLOB << "Completed SQL DML generation" << LOG_ASYNC;
}

} // dbc, ember