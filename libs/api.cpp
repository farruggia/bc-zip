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

#include <api.hpp>
#include <decompress.hpp>
#include <cmath>
#include <format.hpp>
#include <write_parsing.hpp>
#include <cstring>
#include <algorithm>
#include <meter_printer.hpp>
#include <generators.hpp>
#include <optimal_parser.hpp>

class allocate_parsing {
protected:
	std::unique_ptr<byte[]> data_stored;
	size_t stored_size;
public:

	allocate_parsing()
	{

	}

	virtual byte *alloc(size_t parsing_byte_size, size_t uncompressed_size) = 0;

	std::unique_ptr<byte[]> give_ownership(size_t *size = nullptr)
	{
		if (size != nullptr) {
			*size = stored_size;
		}
		return std::move(data_stored);
	}

	virtual ~allocate_parsing()
	{

	}
};

class raw_allocate : public allocate_parsing {
public:
	byte *alloc(size_t parsing_byte_size, size_t)
	{
		std::unique_ptr<byte[]> new_data(new byte[parsing_byte_size]);
		std::swap(new_data, data_stored);
		stored_size = parsing_byte_size;
		byte *to_ret = data_stored.get();
		std::fill(to_ret, to_ret + stored_size, 0U);
		return to_ret;
	}
};

class full_allocate : public allocate_parsing {
private:
	std::string encoder_name;
public:
	full_allocate(std::string encoder_name) 
		: encoder_name(encoder_name)
	{

	}

	byte *alloc(size_t parsing_byte_size, size_t uncompressed_size)
	{
		auto info = pack(encoder_name, uncompressed_size, parsing_byte_size);
		std::swap(data_stored, info.parsing);
		std::string enc_name;
		size_t uncomp_len;
		byte *to_ret;
		std::tie(enc_name, uncomp_len, to_ret) = unpack(data_stored.get());
		stored_size = info.data_len;
		return to_ret;
	}
};

class empty_delete {
public:
	template <typename T>
	void operator()(T *)
	{

	}
};

class bitoptimal_caller {
private:
	std::string encoder_name;
	byte *uncompressed;
	size_t uncomp_len;
	allocate_parsing *allocator;
public:

	bitoptimal_caller(
		std::string encoder_name, 
		byte *uncompressed, size_t uncomp_len, 
		allocate_parsing *allocator
	) : encoder_name(encoder_name), uncompressed(uncompressed), uncomp_len(uncomp_len), allocator(allocator)
	{

	}

	template <typename fsg_fact_t>
	void run()
	{
		// Get the cost_model
		encoders_ enc_container;
		auto cm = enc_container.get_cm(encoder_name);
		auto lit_win = enc_container.get_literal_len(encoder_name);

		// Get the text_info.
		// We encapsulate the uncompressed buffer in a text_info, using a void deleter 
		// to not mess with memory ownership.
		std::shared_ptr<byte> wrapper_text(uncompressed, empty_delete());
		text_info ti(wrapper_text, uncomp_len);

		// Allocate the FSG
		sa_instantiate sa;
		auto fsg = fsg_fact_t(ti, sa).instantiate(cm);

		// Launch the bit-optimal parser
		/* About *(fsg.get()): it's safe to delete a moved object. */
		double cost;
		auto solution = parse(ti, *(fsg.get()), lit_win, cm, &cost, empty_observer());

		// Get parsing length, in bytes, plus encoder overhead
		auto length = parsing_space(encoder_name, ITERS(solution));

		// Allocate space
		byte *output = allocator->alloc(length, uncomp_len);

		// Compress the parsing and put the content in there
		write_parsing(encoder_name, solution, length, ti, output);
	}
};

void bczip::decompress_buffer(const char *encoder_name, byte *compressed, byte *output, size_t uncompressed_size)
{
	decompress_raw(encoder_name, compressed, output, uncompressed_size);
}

std::unique_ptr<byte[]> bczip::decompress(byte *compressed, size_t *decompressed_size)
{
	auto o = decompress_full(compressed);
	if (decompressed_size != nullptr) {
		*decompressed_size = o.uncompressed_size;
	}
	return std::move(o.rep);
}

namespace bczip {
namespace impl {
std::unique_ptr<byte[]> compress(
	const char *encoder_name, byte *uncompressed, 
	size_t length, size_t *compressed_length, 
	allocate_parsing *allocator
)
{
	// Get suggested generator name
	auto dst_win = encoders_().get_cm(encoder_name).get_dst();
	auto generator = suggest_gen(get_kind(dst_win));

	// Invoke bitoptimal_caller through generator_
	bitoptimal_caller caller(encoder_name, uncompressed, length, allocator);
	generators_().call(generator, caller);

	return allocator->give_ownership(compressed_length);	
}
}
}

std::unique_ptr<byte[]>
bczip::compress_buffer(const char *encoder_name, byte *uncompressed, size_t length, size_t *compressed_length)
{
	raw_allocate allocate;
	return bczip::impl::compress(encoder_name, uncompressed, length, compressed_length, &allocate);
}

std::unique_ptr<byte[]> 
bczip::compress(const char *encoder_name, byte *uncompressed, size_t length, size_t *compressed_length)
{
	full_allocate allocate(encoder_name);
	return bczip::impl::compress(encoder_name, uncompressed, length, compressed_length, &allocate);
}

byte *bczip::extract_header(byte *parsing, char **encoder_name, std::uint32_t *file_size)
{
	byte *to_ret;
	std::uint32_t *size_ptr;
	std::tie(*encoder_name, size_ptr, to_ret) = ptr_unpack(parsing);
	*file_size = *size_ptr;
	return to_ret;
}

std::unique_ptr<byte[]> bczip::create_header(const char *encoder_name, std::uint32_t file_size, size_t *header_size)
{
	auto info = pack(encoder_name, file_size, 0U);
	*header_size = info.data_len;
	return std::move(info.parsing);
}

namespace {
enum run_mode {COMPRESSION, DECOMPRESSION};

class get_enlarged_size {
private:
	size_t answer;
	size_t query;
	run_mode mode;
public:
	get_enlarged_size(size_t query, run_mode mode)
		: answer(0U), query(query), mode(mode)
	{

	}

	template <typename enc_>
	void run()
	{
		typedef typename enc_::encoder enc_t;
		typedef typename enc_::decoder dec_t;
		answer = (mode == COMPRESSION) ? enc_t::data_len(query * 8) : query + dec_t::extra_read();
	}

	size_t get_answer()
	{
		return answer;
	}
};

size_t total_size(const char *encoder_name, size_t length, run_mode mode)
{
	get_enlarged_size getter(length, mode);
	encoders_().call(encoder_name, getter);
	return getter.get_answer();
}

size_t safe_compress_size(const char *encoder_name, size_t compressed_length)
{
	return total_size(encoder_name, compressed_length, COMPRESSION);
}

size_t safe_decompress_size(const char *encoder_name, size_t compressed_length)
{
	return total_size(encoder_name, compressed_length, DECOMPRESSION);
}
}

size_t bczip::safe_buffer_size(const char *encoder_name, size_t compressed_length)
{
	return std::max(
		safe_compress_size(encoder_name, compressed_length),
		safe_decompress_size(encoder_name, compressed_length)
	);
}