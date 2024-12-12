/*
* Copyright (c) 2003 Ladislav Zezula
* Copyright (c) 2024 Ember
* 
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
* 
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
* 
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/

// Most of this file is extracted (and somewhat modernised) from StormLib and is
// seemingly based on a reverse engineered version of PKWare's Data Compression Library
// from the 90s. Most open source implementations weren't able to correctly decompress
// the data and I'm not spending the time to reverse engineer an obsolete format 
// (other than MPQ, seemingly)
// ttcomp is a small library that *does* work correctly but it's LGPLd, so no good here, 
// (not going to dynamic link it) which is a shame because it's much smaller and
// leaner than PKLib
// ttcomp for reference:
// http://fileformats.archiveteam.org/wiki/TTCOMP

// Original for reference:
// https://github.com/ladislav-zezula/StormLib/blob/master/src/SCompression.cpp

#include <mpq/Compression.h>
#include <mpq/Exception.h>
#include <pklib/pklib.h>
#include <memory>
#include <stdexcept>
#include <cstring>

namespace ember::mpq {

struct CallbackInfo {
	std::span<const std::byte> input_buffer;
	std::size_t in_offset;
	std::span<std::byte> output_buffer;
	std::size_t out_offset;
};

// Function loads data from the input buffer. Used by Pklib's "implode"
// and "explode" function as user-defined callback
// Returns number of bytes loaded
//
//   char* buf          - Pointer to a buffer where to store loaded data
//   unsigned int* size - Max. number of bytes to read
//   void* param        - User data, parameter of implode/explode
//
// implode/explode should terminate when it returns zero 
// (based on original SDK sample code I managed to dig up, which this is based on)
static unsigned int read_input_data(char* buf, unsigned int* size, void* param) {
	auto info = static_cast<CallbackInfo*>(param);
	auto max_read_size = info->input_buffer.size() - info->in_offset;
	auto read_size = *size;

	// Check the case when not enough data available
	if(read_size > max_read_size) {
		read_size = max_read_size;
	}

	// Load data and increment offsets
	std::memcpy(buf, info->input_buffer.data() + info->in_offset, read_size);
	info->in_offset += read_size;
	return read_size;
}

// Function for storing output data. Used by Pklib's "implode" and "explode"
// as user-defined callback
//
//   char* buf          - Pointer to data to be written
//   unsigned int* size - Number of bytes to write
//   void* param        - User data, parameter of implode/explode
static void write_output_data(char* buf, unsigned int* size, void* param) {
	auto info = static_cast<CallbackInfo*>(param);
	auto max_write_size = info->output_buffer.size_bytes() - info->out_offset;
	auto write_size = *size;

	if(!max_write_size && write_size) {
		throw exception("cannot decompress: output buffer full");
	}

	// Check the case when not enough space in the output buffer
	if(write_size > max_write_size) {
		write_size = max_write_size;
	}

	// Write output data and increment offsets
	std::memcpy(info->output_buffer.data() + info->out_offset, buf, write_size);
	info->out_offset += write_size;
}

std::expected<std::size_t, int> decompress_pklib(std::span<const std::byte> input,
												 std::span<std::byte> output) {
	CallbackInfo info {
		.input_buffer   = input,
		.output_buffer  = output,
	};

	auto work_buf = std::make_unique<TDcmpStruct>();

	if(auto ret = explode(read_input_data, write_output_data, work_buf.get(), &info) != CMP_NO_ERROR) {
		return std::unexpected(ret);
	}

	return info.out_offset;
}

} // mpq, ember