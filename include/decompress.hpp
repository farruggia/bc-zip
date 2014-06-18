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

#ifndef DECOMPRESS_HPP
#define DECOMPRESS_HPP

#include <string>
#include <iostream>
#include <chrono>

#include <utilities.hpp>
#include <cmd_parse.hpp>
#include <encoders.hpp>
#include <factory.hpp>
#include <io.hpp>

#include <sys/mman.h>

struct fast_copy {
	template <typename T>
	void operator()(T *dest, T *src, size_t len)
	{
		copy_fast(dest, src, len);
	}
};

class base_decompress {
public:
	virtual void run(byte *input, byte *output, size_t size, std::uint64_t *dec_time) = 0;

	virtual ~base_decompress()
	{
	}
};

template <typename enc_, typename copier_ = fast_copy>
class decompress : public base_decompress {
public:
	void run(byte *input, byte *output, size_t size, std::uint64_t *dec_time = nullptr)
	{
		typedef typename enc_::decoder dec_t;
		dec_t decoder(input, size);
		copier_ copy;

//		const byte *start = output;
		byte *end = output + size;

		auto time_1 = std::chrono::high_resolution_clock::now();
		uint32_t dist, len, nextliteral;
		decoder.decode(output, len, nextliteral);
		output += len;

//		std::cout << size << std::endl;

		while (output < end) {
			if (nextliteral > 0) {
				decoder.decode(dist, len);

//				std::cout << dist << "\t" << len << std::endl;

				assert(output + len <= end);
//				assert(output - dist >= start);
				copy(output, output - dist, len);
				output += len;
				nextliteral--;
			} else {
				decoder.decode(output, len, nextliteral);
				assert(output + len <= end);
				assert(len > 0);
				output += len;
			}
		}
		auto time_2 = std::chrono::high_resolution_clock::now();
		auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(time_2 - time_1).count();
		if (dec_time != nullptr) {
			*dec_time = elapsed;
		}
	}
};

template <typename copier_>
class dec_fact {
public:
	template <typename enc_>
	std::unique_ptr<base_decompress> get_instance() const
	{
		return make_unique<decompress<enc_, copier_>>();
	}
};

std::uint32_t extract_size(byte *comp, byte **new_ptr = nullptr);

template <typename copy_ = fast_copy>
std::uint64_t decompress_raw(const char *encoder, byte *parsing, byte *output, size_t uncompressed_size)
{
	dec_fact<copy_> fact;
	auto decompressor = encoders_().instantiate<base_decompress, dec_fact<copy_>>(encoder, fact);

	std::uint64_t elapsed;
	decompressor->run(parsing, output, uncompressed_size, &elapsed);
	return elapsed;
}

typedef struct _decompress_output {
	std::unique_ptr<byte[]> rep;
	std::string enc_name;
	// In bytes
	size_t uncompressed_size;
	// In nanoseconds
	std::uint64_t dec_time;
} dec_output;

dec_output decompress_full(byte *data);

std::uint64_t decompress_file(const char *tool_name, int argc, char **argv, std::ostream &out = std::cout);

#endif // DECOMPRESS_HPP
