/*
 * Copyright (c) 2016 - 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <boost/asio/buffer.hpp>

#ifdef BUFFER_DEBUG
#include <utility>
#endif

namespace ember::spark::io {

template<typename BufferType>
class BufferSequence {
	const BufferType* buffer_;

public:
	BufferSequence(const BufferType& buffer) : buffer_(&buffer) { }

class const_iterator {
	using Node = typename BufferType::node_type;

public:
	const_iterator(const BufferType* buffer, const Node* curr_node)
		: buffer_(buffer), curr_node_(curr_node) {}

	const_iterator& operator++() {
		curr_node_ = curr_node_->next;
		return *this;
	}

	const_iterator operator++(int) {
		const_iterator current(*this);
		curr_node_ = curr_node_->next;
		return current;
	}

	boost::asio::const_buffer operator*() const {
		const auto buffer = buffer_->buffer_from_node(curr_node_);
		return boost::asio::const_buffer(buffer->read_data(), buffer->size());
	}

	bool operator==(const const_iterator& rhs) const {
		return curr_node_ == rhs.curr_node_;
	}

	bool operator!=(const const_iterator& rhs) const {
		return curr_node_ != rhs.curr_node_;
	}

	const_iterator& operator=(const_iterator&) = delete;

#ifdef BUFFER_DEBUG
	std::pair<const char*, std::size_t> get_buffer() {
		auto buffer = buffer_->buffer_from_node(curr_node_);
		return std::make_pair<char*, std::size_t>(
			const_cast<char*>(reinterpret_cast<const char*>(buffer->read_data())), buffer->size()
		);
	}
#endif

private:
	const BufferType* buffer_;
	const Node* curr_node_;
};

const_iterator begin() const {
	return const_iterator(buffer_, buffer_->root_.next);
}

const_iterator end() const {
	return const_iterator(buffer_, &buffer_->root_);
}

friend class const_iterator;
};

} // io, spark, ember