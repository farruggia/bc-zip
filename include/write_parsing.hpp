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

#ifndef WRITE_PARSING_HPP
#define WRITE_PARSING_HPP

#include <assert.h>
#include <edges.hpp>
#include <base_fsg.hpp>
#include <encoders.hpp>
#include <format.hpp>

template <typename ret_t = size_t, typename T>
ret_t parsing_length(const T begin, const T end, const cost_model &cm)
{
	ret_t size = ret_t();
	auto it = begin;
	auto vend = std::prev(end, 1);
	auto length = 0U;
	while (it < vend) {
		const edge_t &edge = *it;
		size += cm.edge_cost(edge);
		length += edge.ell;
		std::advance(it, edge.ell);
	}
	size += length * cm.cost_per_char();
	return size;
}

class encoder_overhead_getter {
private:
	size_t p_len;
public:

	encoder_overhead_getter(size_t p_len) : p_len(p_len)
	{

	}

	template <typename enc_>
	void run()
	{
		typedef typename enc_::encoder enc_t;
		// Encoder wants the length in bits
		p_len = enc_t::data_len(p_len * 8);
	}

	size_t get_len()
	{
		return p_len;
	}
};

/**
 * Gets the total parsing space (parsing + encoder overhead)
 * \param 	enc_name 	Encoder name
 * \param 	begin		Parsing iterator to first element
 * \param 	end 		Parsing iterator to one-past-end
 * \return	Total parsing space, IN BYTES
 */
template <typename T>
size_t parsing_space(std::string enc_name, const T begin, const T end)
{
	encoders_ encoders_container;
	auto cm = encoders_container.get_cm(enc_name);
	auto p_len = parsing_length(begin, end, cm);
	encoder_overhead_getter getter(p_len);
	encoders_().call(enc_name, getter);
	return getter.get_len();
}

class base_writer {
public:
	/**
	 * @brief Write a parsing, computing parsing length
	 * @param file_name
	 *		Where to write the parsing
	 * @return
	 *		The parsing size
	 */
	virtual size_t write(std::string file_name, text_info ti, std::vector<edge_t> &solution) = 0;
	/**
	 * @brief Write a parsing, trusting solution cost as parsing length
	 * @param file_name
	 *		Where to write the parsing
	 * @return
	 *		The parsing size
	 */
	virtual size_t write(std::string file_name, text_info ti, std::vector<edge_t> &solution, size_t size) = 0;

	virtual ~base_writer()
	{

	}
};

struct compressed_file {
	std::unique_ptr<byte[]> data;
	size_t total_size; // Bytes
	size_t parsing_size; // Bytes
};

/**
 * Gets the compressed rep. of the parsing.
 * \param 	sol 			Parsing, in "edge" format
 * \param 	parsing_length 	Length of compressed parsing, IN BYTES
 * \param 	ti 				Uncompressed text
 * \param 	output 			Start of compressed rep. (memory must be allocated and zeroed)
 */
template <typename enc_>
void write_parsing(const std::vector<edge_t> &sol, size_t parsing_length, text_info ti, byte *output)
{
	typedef typename enc_::encoder enc_t;
	// (2): instantiate the encoder
	enc_t enc(output, parsing_length);

	// (3): encode each phrase and return it
	uint32_t j, nextliteral = 0;
	byte *text = ti.text.get();
	auto length = ti.len;
	// Encode phrases
	// Bit 0 + single char 1 <dist, length>
	for (uint32_t i = 0; i < length;) {
		auto &edge = sol[i];
		assert(!edge.invalid());
		if (edge.kind() == PLAIN) {
			// # of phrases up to next literal
			nextliteral = 0;
			for (j = i + edge.ell; j < length; nextliteral++) {
				if (sol[j].kind() == PLAIN){
					break;
				} else {
					j += sol[j].ell;
				}
			}
			if (j >= length) {
				nextliteral++;
			}
			enc.encode(text + i, edge.ell, nextliteral);
			i += edge.ell;
		} else {
			assert(edge.kind() == REGULAR);
			unsigned int d = sol[i].d, ell = sol[i].ell;
			enc.encode(d, ell);
			i += ell;
		}
	}	
}

class generic_parsing_writer {
private:
	const std::vector<edge_t> &sol;
	size_t parsing_length;
	text_info ti;
	byte *output;
public:
	generic_parsing_writer(
		const std::vector<edge_t> &sol, size_t parsing_length, text_info ti, byte *output
	) : sol(sol), parsing_length(parsing_length), ti(ti), output(output)
	{

	}

	template <typename enc_>
	void run()
	{
		write_parsing<enc_>(sol, parsing_length, ti, output);
	}
};

void write_parsing(
	std::string enc_name, const std::vector<edge_t> &sol, size_t parsing_length, text_info ti, byte *output
);

/**
 * Gets the compressed rep. of the parsing.
 * Template parameters:
 *	- enc_:				Encoder used to compress (must be the same used while parsing)
 * Parameters:
 *  - sol:				The parsing
 *	- parsing_length:	Length of parsing IN BITS
 *  - ti:				text_info of source file
 *
 * OUTPUTS: a triple (V1, V2, V3), where:
 *	- V1:				Compressed parsing
 *  - V2:				Length of V1 (includes also encoder name and other informations)
 *	- V3:				Compressed length (excludes encoder name etc.)
 */
template <typename enc_>
compressed_file write_parsing(const std::vector<edge_t> &sol, size_t parsing_length, text_info ti)
{
	typedef typename enc_::encoder enc_t;
	// (1): Pack the header
	size_t byte_parsing_length = enc_t::data_len(parsing_length);
	std::uint32_t length = sol.size() - 1;

	pack_info p_info = pack(enc_::name(), length, byte_parsing_length);
	std::unique_ptr<byte[]> data_holder = std::move(p_info.parsing);
	size_t data_len = p_info.data_len;

	byte *data = std::get<2>(unpack(data_holder.get()));

	// (2): write the parsing
	write_parsing<enc_>(sol, byte_parsing_length, ti, data);

	// (3): return it
	return { std::move(data_holder), data_len, byte_parsing_length };
}

template <typename enc_>
class parsing_writer : public base_writer {
private:
	typedef typename enc_::encoder enc_t;

public:

	parsing_writer()
	{

	}

	size_t write(std::string filename, text_info ti, std::vector<edge_t> &sol)
	{
		return write(filename, ti, sol, parsing_length(sol.begin(), sol.end(), enc_t::get_cm()));
	}

	size_t write(std::string filename, text_info ti, std::vector<edge_t> &sol, size_t parsing_length)
	{
		// (1) get the parsing
		auto comp_file = write_parsing<enc_>(sol, parsing_length, ti);
		std::unique_ptr<byte[]> parsing = std::move(comp_file.data);
		size_t data_len = comp_file.total_size, clen = comp_file.parsing_size;

		// (2) Write the parsing on file
		std::ofstream out_file;
		open_file(out_file, filename.c_str());
		write_file<byte>(out_file, parsing.get(), static_cast<std::streamsize>(data_len));

		return clen;
	}

	size_t write_noconv(std::string file_name, text_info ti, std::vector<edge_t> &solution, size_t size)
	{
		throw std::logic_error("Write_noconv not implemented yet");
	}

};

class writer_factory
{
public:
	writer_factory()
	{

	}

	template <typename enc_>
	std::unique_ptr<base_writer> get_instance() const
	{
		return make_unique<parsing_writer<enc_>>();
	}
};

void write_parsing(std::vector<edge_t> &sol, text_info ti, std::string file_name, std::string encoder_name);
compressed_file write_parsing(const std::vector<edge_t> &sol, text_info ti, std::string encoder_name);
compressed_file write_parsing(const std::vector<edge_t> &sol, text_info ti, std::string encoder_name, cost_model cm);

#endif // WRITE_PARSING_HPP
