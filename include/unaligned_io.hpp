/**
 * Copyright 2014 Andrea Farruggia
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 * 		http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/

#ifndef UNALIGNED_IO_HPP
#define UNALIGNED_IO_HPP

#include <array>
#include <algorithm>
#include <limits>
#include <assert.h>
#include <common.hpp>
#include <copy_routines.hpp>

namespace unaligned_io {

extern std::array<unsigned int, 9> reader_masks;

struct shift_t {
	template <typename T>
	static void byte_shift(T &t)
	{
		t >>= 8U;
	}

	static void byte_shift(byte &t)
	{
		t = 0U;
	}
};

class writer {
private:
	byte *data;
	unsigned int bit_offset;

	template <typename T>
	void partial_write(T &t, unsigned int &bits) {
		size_t bits_written = std::min(8U - bit_offset, bits);
		// Take bits_written lower bits
		byte to_write = t & ((1U << bits_written) - 1U);
		*data = *data | (to_write << bit_offset);
		bit_offset += bits_written;
		if (bit_offset == 8U) {
			++data;
			bit_offset = 0U;
			assert(*data == 0U);
		}
		bits -= bits_written;
		t >>= bits_written;
	}

	template <typename T>
	void full_write(T &t, unsigned int &bits) {
		*data++ = (t & 0xFF);
		bits -= 8;
		shift_t::byte_shift(t);
	}

public:
	writer(byte *data) : data(data), bit_offset(0U)
	{
		assert(*data == 0U);
	}

	template <typename T>
	void write(T t, unsigned int bits)
	{
		assert(bits <= sizeof(t) * 8);
		partial_write(t, bits);
		while (bits >= 8) {
			full_write(t, bits);
		}
		partial_write(t, bits);
	}

	template <typename T>
	void write(T t)
	{
		write(t, sizeof(T) * 8);
	}

	byte *writing_head()
	{
		return data;
	}

	unsigned int offset()
	{
		return bit_offset;
	}

	void skip_bytes(unsigned int bytes)
	{
		data += bytes;
	}

	void skip_bits(unsigned int bits)
	{
		bit_offset += bits;
		data += bit_offset >> 3U;
		bit_offset &= 7U;
	}
};

class reader {
private:
	byte *data;
	unsigned int bit_offset;

	template <typename T>
	byte *partial_read(T &t, unsigned int &bits, byte *local_data, unsigned int &local_offset, byte &read)
	{
		read			= std::min(bits, 8U - local_offset);
		t				= (*local_data >> local_offset) & unaligned_io::reader_masks[read];
		auto new_offset = local_offset + read;
		local_data		+= new_offset >> 3U;
		local_offset	= new_offset & 0x7;
		bits			-= read;
		return local_data;
	}

	template <typename T>
	inline byte *read(T &t, unsigned int bits, byte *local_data, unsigned int &local_bit_offset)
	{
		static_assert(sizeof(t) <= sizeof(std::uint64_t), "unaligned_reader::read(): integers wider than 64 bits are not supported");
		// Take us to the boundary
		byte prefix = 0U, read = 0U;
		local_data = partial_read(prefix, bits, local_data, local_bit_offset, read);
		// Unaligned read from the boundary
		t =	*reinterpret_cast<std::uint64_t*>(local_data) & ((1ULL << bits) - 1);
		t <<= read;
		t |= prefix;
		local_bit_offset = (local_bit_offset + bits) & 7U;
		return local_data + (bits >> 3U);
	}

public:
	reader(byte *data) : data(data), bit_offset(0U)
	{

	}

	template <typename T>
	void peek(T &t, unsigned int bits)
	{
		unsigned int local_bit_offset = bit_offset;
		read(t, bits, data, local_bit_offset);
	}

	template <typename T>
	void peek(T &t)
	{
		peek(t, sizeof(t) * 8);
	}

	template <typename T>
	void read(T &t, unsigned int bits)
	{
		data = read(t, bits, data, bit_offset);
	}

	template <typename T>
	void read(T &t)
	{
		read(t, sizeof(t) * 8);
	}

	void skip_bits(unsigned int bits)
	{
		bit_offset += bits;
		data += bit_offset >> 3U;
		bit_offset = bit_offset & 7U;
	}

	void skip_bytes(unsigned int bytes)
	{
		data += bytes;
	}

	byte *reading_head()
	{
		return data;
	}

	unsigned int offset()
	{
		return bit_offset;
	}

};

namespace literal {

template <typename range_t, std::uint64_t start_>
class writer {
public:
	void write(
		const byte *run, std::uint_fast64_t len, unaligned_io::writer &write,
		std::uint32_t next_literal
	)
	{
		write.write(next_literal);
		assert(len > start_);
		range_t to_write = len - start_;
		write.write(to_write);
		// Divide the first character into two parts, to fit the first byte
		byte first = *run;
		auto first_width = write.offset(), second_width = 8U - first_width;
		byte first_part = first >> second_width;
		byte second_part = first & ((1U << second_width) - 1);
		// Write the first part, to get to byte boundary
		write.write(second_part, second_width);
		// Write every other characters directly
		assert(write.offset() == 0U);
		byte *data = write.writing_head();
		std::copy(run + 1, run + len, data);
		write.skip_bytes(len - 1);
		// Write the second part of the first character
		write.write(first_part, first_width);
	}

	std::uint64_t max_length()
	{
		return std::numeric_limits<range_t>::max() + start_;
	}

	unsigned int fixed_cost()
	{
		return (sizeof(range_t) + sizeof(std::uint32_t)) * 8;
	}

	unsigned int var_cost()
	{
		return 8U;
	}
};

template <typename range_t, std::uint64_t start_>
class reader {
public:
	void read(
		byte *dest, std::uint32_t &ret_length, std::uint32_t &next_literal,
		unaligned_io::reader &reader
	)
	{
		// Read next_literal and run length
		range_t length;
		reader.read(next_literal);
		reader.read(length);
		ret_length = length;
		ret_length += start_;
		// Find out the size of the first and second part
		auto first_width = reader.offset(), second_width = 8U - first_width;
		// Get the first part
		byte first_part = 0U, second_part = 0U;
		reader.read(second_part, second_width);
		// Get the reading head and copy the other characters
		byte *data = reader.reading_head();
		// TODO: cambiato qui
		u_copy_fast(dest + 1, data, ret_length - 1);
		// std::copy(data, data + ret_length - 1, dest + 1);
		reader.skip_bytes(ret_length - 1);
		// Finish getting the first character and write it
		reader.read(first_part, first_width);
		*dest = (first_part << second_width) | second_part;
	}
};

class single_writer {
public:
	void write(
		const byte *run, unsigned int len, unaligned_io::writer &write,
		std::uint32_t next_literal
	)
	{
		assert(len == 1U);
		write.write(next_literal);
		write.write(*run);
	}

	std::uint64_t max_length()
	{
		return 1U;
	}

	unsigned int fixed_cost()
	{
		return (sizeof(std::uint32_t)) * 8;
	}

	unsigned int var_cost()
	{
		return 8U;
	}
};

class single_reader {
public:
	void read(
		byte *dest, std::uint32_t &length, std::uint32_t &next_literal,
		unaligned_io::reader &reader
	)
	{
		// Read next_literal and run length
		length = 1U;
		reader.read(next_literal);
		reader.read(*dest);
	}
};

}

}

#endif // UNALIGNED_IO_HPP
