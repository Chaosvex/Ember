/*
 * Copyright (c) 2015 - 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "LoginHandler.h"
#include <utility>

namespace ember {

class LoginHandlerBuilder final {
	log::Logger& logger_;
	const Patcher& patcher_;
	const RealmList& realm_list_;
	const dal::UserDAO& user_dao_;
	const AccountClient &acct_svc_;
	const Survey& survey_;
	const IntegrityData& bin_data_;
	Metrics& metrics_;
	bool locale_enforce_;
	bool integrity_enforce_;
	bool verified_email_;

public:
	LoginHandlerBuilder(log::Logger& logger, const Patcher& patcher, const Survey& survey,
	                    const IntegrityData& exe_data, const dal::UserDAO& user_dao,
	                    const AccountClient& acct_svc, const RealmList& realm_list,
	                    Metrics& metrics, bool locale_enforce, bool integrity_enforce,
	                    bool verified_email)
	                    : logger_(logger), patcher_(patcher), user_dao_(user_dao),
	                      acct_svc_(acct_svc), realm_list_(realm_list), metrics_(metrics),
	                      survey_(survey), bin_data_(exe_data), locale_enforce_(locale_enforce),
	                      integrity_enforce_(integrity_enforce), verified_email_(verified_email) {}

	LoginHandler create(std::string source) const {
		return { user_dao_, acct_svc_, patcher_, bin_data_, survey_, logger_, realm_list_,
		         std::move(source), metrics_, locale_enforce_, integrity_enforce_, verified_email_ };
	}
};

} // ember