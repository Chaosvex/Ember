/*
 * Copyright (c) 2021 - 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "SchemaParser.h"
#include "Utility.h"
#include "inja/inja.hpp"
#include <flatbuffers/reflection.h>
#include <chrono>
#include <format>
#include <fstream>
#include <stdexcept>

using namespace nlohmann;

namespace ember {

SchemaParser::SchemaParser(std::filesystem::path templates_dir, std::filesystem::path output_dir)
	: tpl_path_(std::move(templates_dir)),
      out_path_(std::move(output_dir)) {
}

void SchemaParser::verify(std::span<const std::uint8_t> buffer) {
	flatbuffers::Verifier verifier(buffer.data(), buffer.size());

	if(!reflection::VerifySchemaBuffer(verifier)) {
		throw std::runtime_error("Schema verification failed!");
	}
}

std::vector<std::uint8_t> SchemaParser::load_file(const std::filesystem::path& path) {
	std::vector<std::uint8_t> buffer;
	std::ifstream file(path, std::ios::in | std::ios::binary);

	if(!file.is_open()) {
		throw std::runtime_error(std::format("Unable to open, {}", path.string()));
	}

	const auto size = std::filesystem::file_size(path);
	buffer.resize(static_cast<std::size_t>(size));

	if(!file.read(reinterpret_cast<char*>(buffer.data()), buffer.size())) {
		throw std::runtime_error(std::format("Unable to read, {}", path.string()));
	}

	return buffer;
}

void SchemaParser::generate(const std::filesystem::path& path) {
	const auto buffer = load_file(path);
	generate(buffer);
}

void SchemaParser::generate(std::span<const std::uint8_t> buffer) {
	verify(buffer);

	const auto& schema = reflection::GetSchema(buffer.data());
	const auto services = schema->services();

	for(const auto& service : *services) {
		process(schema, service);
	}
}

void SchemaParser::process(const reflection::Schema* schema, const reflection::Service* service) {
	const auto time = std::chrono::system_clock::now();
	const std::chrono::year_month_day date = std::chrono::floor<std::chrono::days>(time);
	const auto bare_name = remove_fbs_ns(service->name()->str());
	const auto gen_file = fbs_to_name(service->declaration_file()->str());

	if(!schema->root_table()) {
		throw std::runtime_error("Missing root table");
	}

	json data;
	data["fbs_name"] = gen_file;
	data["year"] = static_cast<int>(date.year());
	data["name"] = bare_name;
	data["handlers"] = {};
	data["root"] = remove_fbs_ns(schema->root_table()->name()->str());

	for(auto call : *service->calls()) {
		const auto& name_str = call->name()->str();
		const auto& req_str = call->request()->name()->str();
		const auto& resp_str = call->response()->name()->str();


		data["handlers"].push_back(
			{
				{"call", name_str },
				{"name", name_str },
				{"request_ns", to_cpp_ns(req_str)},
				{"request_sn", remove_fbs_ns(req_str)},
				{"request", remove_fbs_ns(req_str)},
				{"response_ns", to_cpp_ns(resp_str)},
				{"response_sn", remove_fbs_ns(resp_str)},
				{"response", remove_fbs_ns(resp_str)},
			}
		);
	}
	
	// Service
	inja::Environment env;
	auto tpl = env.parse_template(tpl_path_.string() + "Service.h_");

	auto path = std::format(
		"{}/{}ServiceStub.h", out_path_.string(), data["name"].get<std::string>()
	);

	env.write(tpl, data, path);

	// Client
	tpl = env.parse_template(tpl_path_.string() + "Client.h_");

	path = std::format(
		"{}/{}ClientStub.h", out_path_.string(), data["name"].get<std::string>()
	);

	env.write(tpl, data, path);
}

} // ember