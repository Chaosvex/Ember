/*
 * Copyright (c) 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <protocol/Packet.h>
#include <type_traits>
#include <shared/utility/polyfill/start_lifetime_as>

namespace ember::protocol {

/*
 * SmartMessage allows for fast message access by selectively
 * skipping the usual deserialisation step for POD types
 * where all fields are correctly aligned for the target
 * architecture. This relies on the types being generated at
 * the same time as the core.
 * 
 * The provided buffer is presumed to be suitably aligned
 * (buffer % alignof(T) == 0) but runtime validation
 * for every message would negate performance benefits,
 * so we'll fall back to deserialisation for platforms
 * where an unaligned access could cause a crash.
 * 
 * When multiple messages are loaded into a single buffer
 * without padding, such as buffered client streams, it's
 * possible that an occasional unaligned access may occur.
 * Such cases should be profiled to gauge the benefits of
 * using this wrapper.
 * 
 * The API has been designed for compatibility with existing
 * deserialisation functionality in the core, allowing the
 * usual message types to be selectively wrapped.
 * 
 * SmartMessage objects should always be considered to be
 * non-owning, even though this may not always be the case.
 */
template<typename MessageType>
class SmartMessage final {
	constexpr static bool allow_unaligned = true; // todo, temp

	MessageType _message;
	MessageType* _view = &_message;

	template<typename Reader>
	consteval static bool can_create_view() {
		return std::is_same<typename Reader::contiguous_type, spark::io::is_contiguous>::value
			&& std::is_same<typename MessageType::aligned, is_aligned>::value
			&& std::is_standard_layout<T>::value
			&& std::is_trivial<T>::value;
	}

public:
	template<typename Reader>
	[[nodiscard]] State read_from_stream(Reader& stream) {
		if constexpr(can_create_view<Reader>() && allow_unaligned) {
			_view = std::start_lifetime_as<MessageType>(stream.buffer());
			stream.skip(sizeof(MessageType));
			return stream? State::DONE : State::ERRORED;
		} else {
			return _message.read_from_stream(stream);
		}
	}

	MessageType* operator->() {
		return _view;
	}

	operator const MessageType&() const {
		return *_view;
	}

	operator MessageType() const {
		return *_view;
	}

	SmartMessage() = default;

	/*
	 * We don't want to allow copying or moving as
	 * the type still contains a potentially unused
	 * message object (expensive). Instead, functions
	 * should accept a reference to the message type,
	 * allowing a SmartMessage<T> to decay to a const T&.
	 */
	SmartMessage& operator=(SmartMessage&) = delete;
	SmartMessage& operator=(SmartMessage&&) = delete;
	SmartMessage(SmartMessage&) = delete;
	SmartMessage(SmartMessage&&) = delete;
};

} // protocol, ember