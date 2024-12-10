/*
 * Copyright (c) 2014, 2015 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <memory>
#include <vector>

namespace ember::dbc {

template<typename T>
struct TreeNode {
	T t;
	TreeNode<T>* parent = nullptr;
	std::vector<std::unique_ptr<TreeNode<T>>> children;
};

} // dbc, ember