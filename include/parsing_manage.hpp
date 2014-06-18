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

#ifndef __PARSING_MANAGE_HPP
#define __PARSING_MANAGE_HPP

#include <common.hpp>
#include <write_parsing.hpp>
#include <memory>

struct parsing {
	byte *begin;
	/* Compressed length, in bytes */
	size_t comp_len;
	/* Decompressed length, in bytes */
	size_t orig_len;
};

class shared_parsing {
private:
	std::shared_ptr<byte> compressed;
	byte *begin;
	size_t comp_len;
	size_t orig_len;
public:
	shared_parsing(std::shared_ptr<byte> parsing, byte *begin, size_t comp_len, size_t orig_len)
		: 	compressed(parsing), begin(begin), comp_len(comp_len), orig_len (orig_len )
	{

	}

	shared_parsing(std::shared_ptr<byte> parsing, size_t comp_len, size_t orig_len)
		: shared_parsing(parsing, parsing.get(), comp_len, orig_len)
	{

	}

	byte *ptr()
	{
		return begin;
	}

	size_t compressed_size()
	{
		return comp_len;
	}

	size_t uncompressed_size()
	{
		return orig_len;
	}

	parsing get_parsing()
	{
		auto p_len = comp_len - (begin - compressed.get());
		return {begin, p_len, orig_len};
	}
};

/**
 * compress: start of compressed file (including header fields)
 * compress_length: compressed file length, in bytes.
 */
parsing get_parsing(byte *compress, size_t compress_length);

parsing get_parsing(compressed_file &cf);

std::shared_ptr<byte> dup_parsing(parsing &in, parsing &out);

#endif