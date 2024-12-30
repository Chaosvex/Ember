/*
 * Copyright (c) 2016 - 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once 

#include <gsl/gsl_util>
#include <iomanip>
#include <sstream>
#include <span>
#include <cstddef>
#include <cctype>
#include <cmath>

namespace ember::util {

template<typename T>
inline std::string format_packet(const T* packet, std::size_t size,
                                 unsigned int columns = 16) {
	auto data = reinterpret_cast<const unsigned char*>(packet);
	auto rows = static_cast<std::size_t>(std::ceil(size / static_cast<double>(columns)));
	std::stringstream buffer;
	std::size_t offset = 0u;

	for(std::size_t i = 0; i != rows; ++i) {
		buffer << std::hex << std::setw(4) << std::setfill('0') << i * columns << "   ";

		for(std::size_t j = 0; j < columns; ++j) {
			buffer << std::hex << std::setfill('0');

			if(j + offset < size) {
				buffer << std::setw(2) << gsl::narrow_cast<unsigned>(data[j + offset]) << " ";
			} else {
				buffer << "   ";
			}
		}

		buffer << "     ";

		for(std::size_t j = 0; j < columns; ++j) {
			if(j + offset < size) {
				buffer << gsl::narrow_cast<char>(std::isprint(data[j + offset])? data[j + offset] : '.');
			} else {
				buffer << " ";
			}
		}

		offset += columns;

		if(i != rows - 1) {
			buffer << "\n";
		}
	}

	return buffer.str();
}

template<typename T>
inline std::string format_packet(std::span<const T> packet, unsigned int columns = 16) {
	return format_packet(packet.data(), packet.size_bytes(), columns);
}

} // util, ember