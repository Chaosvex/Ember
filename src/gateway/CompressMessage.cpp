/*
 * Copyright (c) 2018 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "CompressMessage.h"
#include <spark/buffers/pmr/BinaryStream.h>
#include <spark/buffers/DynamicBuffer.h>
#include <zlib.h>
#include <cstdint>
#include <cstddef>

namespace ember::gateway {

/*
 * To think about: state of the buffers if compression fails.
 * Use temporary buffers to avoid a bad state?
 */
int compress_message(const spark::io::pmr::Buffer& in, spark::io::pmr::Buffer& out, int compression_level) {
	return Z_ERRNO;
}

// temp testing function, do not use yet
int compress_message(const auto& packet, spark::io::pmr::Buffer& out, int compression_level) {
	constexpr std::size_t BLOCK_SIZE = 64;
	spark::io::DynamicBuffer<4096> temp_buffer;
	spark::io::pmr::BinaryStream stream(temp_buffer);
	spark::io::pmr::BinaryStream out_stream(out);

	std::uint8_t in_buff[BLOCK_SIZE];
	std::uint8_t out_buff[BLOCK_SIZE];

	z_stream z_stream{};
	z_stream.next_in = in_buff;
	z_stream.next_out = out_buff;
	deflateInit(&z_stream, compression_level);

	stream << packet->opcode << packet;
	out_stream << std::uint16_t(0) << protocol::ServerOpcode::SMSG_COMPRESSED_UPDATE_OBJECT;

	while(!stream.empty()) {
		z_stream.avail_in = stream.size() >= BLOCK_SIZE? BLOCK_SIZE : stream.size();
		stream.get(in_buff, z_stream.avail_in);

		const auto flush = stream.empty()? Z_FINISH : Z_NO_FLUSH;

		do {
			z_stream.avail_out = BLOCK_SIZE;
			const int ret = deflate(&z_stream, flush);

			if(ret != Z_OK && ret != Z_STREAM_END) {
				return ret;
			}

			out.write(out_buff, BLOCK_SIZE - z_stream.avail_out);

			if(flush == Z_FINISH && ret == Z_OK) {
				continue;
			}
		} while(z_stream.avail_out == 0);


		out.write(out_buff, BLOCK_SIZE - z_stream.avail_out);
	}

	return deflateEnd(&z_stream);
}

} // gateway, ember