/*
 * Copyright (c) 2014 - 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <shared/utility/StringHash.h>
#include <dbcreader/Loader.h>
#include <dbcreader/Storage.h>
#include <boost/unordered/unordered_flat_map.hpp>
#include <concepts>
#include <functional>
#include <memory>

namespace ember::dbc {

class DiskLoader final : public Loader {
	using DBCLoadFunc = std::function<void(Storage&, const std::string&)>;
	using LogCB = std::function<void(const std::string&)>;

	const LogCB log_cb_;
	const std::string dir_path_;
	boost::unordered_flat_map<std::string, DBCLoadFunc, StringHash, std::equal_to<>> dbc_map;

public:
	DiskLoader(std::string dir_path, LogCB log_cb = [](const std::string&){});
	~DiskLoader() = default;

	Storage load(std::convertible_to<std::string_view> auto&& ...whitelist) const {
		std::initializer_list<std::string_view> list { whitelist... };
		return load({ list.begin(), list.end() });
	}

	Storage load() const override;
	Storage load(std::span<const std::string_view> whitelist) const override;
};

} // dbc, ember