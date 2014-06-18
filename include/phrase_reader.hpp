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

#ifndef PHRASE_READER_HPP
#define PHRASE_READER_HPP

#include <stdint.h>
#include <memory>
#include <common.hpp>
#include <decompress.hpp>

namespace lzopt {

class i_phrase_reader {
public:

	virtual void next(std::uint32_t &dst, std::uint32_t &len) = 0;

	virtual bool end() = 0;

	virtual byte *get_buffer() = 0;

	virtual size_t text_size() = 0;

	virtual std::uint32_t get_next() = 0;

	virtual void adjust(int i) = 0;

	virtual ~i_phrase_reader()
	{

	}
};

template <typename enc_>
class phrase_reader : public i_phrase_reader {
	size_t text_len;
	typename enc_::decoder dec;
	typedef typename enc_::encoder enc;
	unsigned int text_pos;
	std::uint32_t next_literal;
	std::vector<byte> data_buffer;
public:

	/**
	 * @brief Instantiate the phrase reader
	 * @param data
	 *		Pointer to the compressed rep.
	 * @param length
	 *		Length of uncompressed data
	 */
	phrase_reader(byte *data, size_t length)
		: 	text_len(length), dec(data, text_len), text_pos(0U), 
			next_literal(0U), data_buffer(enc::get_literal_len())
	{

	}

	void next(std::uint32_t &dst, std::uint32_t &len)
	{
		if (next_literal-- > 0U) {
			dec.decode(dst, len);
		} else {
			dst = 0U;
			dec.decode(data_buffer.data(), len, next_literal);
		}
		text_pos += len;
	}

	bool end()
	{
		return text_pos >= text_len;
	}

	byte *get_buffer()
	{
		return data_buffer.data();
	}

	size_t text_size()
	{
		return text_len;
	}

	std::uint32_t get_next()
	{
		return next_literal;
	}

	unsigned int get_text_pos()
	{
		return text_pos;
	}

	void adjust(int i) {
		text_pos += i;
	}

	void adjust_next(std::uint32_t next)
	{
		next_literal = next;
	}
};

class pr_factory {
private:
	byte *data;
	size_t length;
public:
	pr_factory(byte *data, size_t length) : data(data), length(length)
	{

	}

	template <typename T>
	std::unique_ptr<i_phrase_reader> get_instance() const
	{
		return make_unique<phrase_reader<T>>(data, length);
	}
};

std::unique_ptr<i_phrase_reader> get_phrase_reader(byte *compressed_file);

std::unique_ptr<i_phrase_reader> get_phrase_reader(std::string encoder_name, byte *buffer, size_t text_len);

}

#endif // PHRASE_READER_HPP
