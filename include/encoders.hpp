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

#ifndef __ENCODERS_
#define __ENCODERS_

#include <memory>
#include <limits>
#include <assert.h>
#include <stdint.h>
#include <stdexcept>
#include <cstddef>
#include <array>

#include <common.hpp>
#include <cost_model.hpp>
#include <copy_routines.hpp>
#include <factory.hpp>
#include <unaligned_io.hpp>

struct enc_cost_info {
	std::vector<unsigned int> dst;
	std::vector<unsigned int> dstcst;
	std::vector<unsigned int> len;
	std::vector<unsigned int> lencst;
};

namespace lzopt {

class base_encoder {
public:
	// The length of the output array: uncompressed text length in
	// decompression, compressed size in compression
	unsigned int get_len() { return text_len; }

protected:
	// Set and read by get/set_len() accessor functions
	std::uint32_t text_len;

	void set_len(std::uint32_t tlen) { text_len = tlen; }
	virtual ~base_encoder() { }
};

/* BYTE-ALIGNED SINGLE CHAR WRITING ROUTINES */
inline byte *sym_write(byte *data, byte c, std::uint32_t next)
{
	*data++ = c;
	*reinterpret_cast<std::uint32_t*>(data) = next;
	return data + sizeof(next);
}

inline byte *sym_read(byte *data, byte *c, std::uint32_t &next)
{
	*c = *data++;
	next = *reinterpret_cast<std::uint32_t*>(data);
	return data + sizeof(next);
}

inline size_t sym_cost()
{
	return 40u; // char (8 bit) + next (32 bit)
}

/*************** BYTE-ALIGNED LITERAL STRING WRITING ROUTINES *********/
/**
 * @brief Interface to a byte-aligned plain encoder.
 * It's not meant to be used, for performance matters. Just use it as a reference.
 */
class literal_encoder {
public:
	/**
	 * @brief encode a literal phrase of length ell, copying from source and writing it to dest.
	 * @param ell
	 *		Length of match (in bytes)
	 * @param source
	 *		Pointer to the first character to be copied
	 * @param dest
	 *		Pointer to the first byte of the encoded phrase
	 * @param next
	 *		Number of phrases between this and the next occurrence of a literal run
	 * @return
	 *		The pointer to the first unwritten byte of dest
	 */
	virtual byte *encode(unsigned int ell, byte *source, byte *dest, std::uint32_t next) = 0;
	/**
	 * @brief decode a literal phrase from dest and writes it into source
	 * @param ell
	 *		Literal length (output parameter)
	 * @param source
	 *		Pointer to the first byte of the literal run
	 * @param dest
	 *		Pointer to the first byte to be written
	 * @param next
	 *		Number of phrases between this and the next occurrence of a literal run (output parameter)
	 * @return
	 *		The pointer to the first unread byte of source
	 */
	virtual byte *decode(unsigned int &ell, byte *source, byte *dest, std::uint32_t &next) = 0;

	/**
	 * @brief Returns the max length encodable by this encoder.
	 * @return
	 *		The max length (in byte) encodable by the encoder.
	 */
	virtual size_t max_length() = 0;

	/**
	 * @brief The cost of a literal run can be given as c + \ell d, where \ell is the run length and
	 *		c and d are constants. This function returns c, the fixed cost.
	 * @return
	 *		The fixed cost (in bits).
	 */
	virtual unsigned int fixed_cost() = 0;

	/**
	 * @brief The cost of a literal run can be given as c + \ell d, where \ell is the run length and
	 *		c and d are constants. This function returns d, the variable cost.
	 * @return
	 *		The variable cost (in bits).
	 */
	virtual unsigned int var_cost() = 0;
};

template <typename T>
class literal_write : public literal_encoder {
private:
	typedef T length_type;
public:
	byte *encode(unsigned int length, byte *data, byte *start, std::uint32_t next)
	{
		assert(length <= max_length());
		*reinterpret_cast<T*>(data) = static_cast<T>(length - 1);
		data +=	sizeof(T);
		data =	std::copy(start, start + length, data);
		*reinterpret_cast<std::uint32_t*>(data) = next;
		return data + sizeof(std::uint32_t);
	}

	byte *decode(unsigned int &length, byte *data, byte *dest, std::uint32_t &next)
	{
		length = *reinterpret_cast<T*>(data) + 1;
		data += sizeof(T);
		// TODO: cambiato qui
		u_copy_fast(dest, data, length);
		// std::copy(data, data + length, dest);
		data += length;
		next = *reinterpret_cast<std::uint32_t*>(data);
		return data += sizeof(std::uint32_t);
	}

	unsigned int fixed_cost()
	{
		return 8U * sizeof(length_type) + 32U;
	}

	unsigned int var_cost()
	{
		return 8;
	}

	size_t max_length()
	{
		return std::numeric_limits<length_type>::max() + 1;
	}
};

class char_write : public literal_encoder {
public:
	byte *encode(unsigned int ell, byte *dest, byte *source, std::uint32_t next)
	{
		assert(ell <= max_length());
		return sym_write(dest, *source, next);
	}

	byte *decode(unsigned int &ell, byte *dest, byte *source, std::uint32_t &next)
	{
		ell = 1;
		return sym_read(dest, source, next);
	}

	unsigned int fixed_cost()
	{
		return 40U;
	}

	unsigned int var_cost()
	{
		return 0U;
	}

	size_t max_length()
	{
		return 1U;
	}
};

/*********************** HYBRID ENCODER/DECODER FUNCTIONS ************************/
namespace hybrid {

/** From distance flag to a 32-bit integer which "selects" the content */
extern const unsigned int masks[4];
/** From length flag to a 32-bit integer which "selects" the content */
extern const unsigned int l_masks[2];

inline byte *dst_encode(std::uint32_t to_encode, byte *write)
{
	assert(to_encode > 0);
	--to_encode;
	assert(to_encode <= (1 << 30));
	unsigned char tag;
	if (to_encode < (1 << 6))
		tag = 0;
	else if (to_encode < (1 << 14))
		tag = 1;
	else if (to_encode < (1 << 22))
		tag = 2;
	else {
		assert(to_encode < (1 << 30));
		tag = 3;
	}
	*write = tag;
	*write++ |= static_cast<byte>((to_encode << 2) & 0xFF);
	for (unsigned int i = 0; i < tag; i++)
		*write++ = to_encode >> (6 + 8 * i) & 0xFF;
	return write;
}

inline byte *len_encode(std::uint32_t to_encode, byte *write)
{
	assert(to_encode > 0);
	--to_encode;
	assert(to_encode <= (1 << 15));
	const unsigned char tag = (to_encode < (1 << 7)) ? 0 : 1;
	*write = tag;
	*write++ |= static_cast<byte>((to_encode << 1) & 0xFF);
	if (tag)
		*write++ = (to_encode >> 7) & 0xFF;
	return write;
}

inline byte *dst_decode(byte * const read, std::uint32_t *value)
{
	const int tag = *read & 0x3;
	// unaligned read
	*value = 1 + (((*reinterpret_cast<std::uint32_t*>(read)) >> 2) & masks[tag]);
	return read + tag + 1;
}

inline byte *len_decode(byte * const read, std::uint32_t *value)
{
	const int tag = *read & 0x1;
	//const unsigned int mask = tag ? 0x7FFF : 0x7F;
	*value = 1 + (((*reinterpret_cast<uint16_t*>(read)) >> 1) & l_masks[tag]);
	return read + tag + 1;
}


/* HYBRID CODER IMPLEMENTATION */
/**
 * The encoder class.
 * @param lit_t
 *		The type of the literal run encoder
 */
template <typename lit_t>
class encoder : public base_encoder {
private:
	// The data where we put the encoding -- we don't own it.
	byte *data;
	// The pointer to the ``original'' pointer
	byte *start_data;
	// The literal writer
	lit_t lit_write;
public:

	typedef lit_t literal_encoder_t;

	encoder(byte* data_, size_t data_size) : data(data_)
	{
		set_len(data_size);
		start_data = data;
	}

	inline void encode(std::uint32_t dst, std::uint32_t len)
	{
		assert(data - start_data < get_len());
		data = lzopt::hybrid::dst_encode(dst, data);
		assert(data - start_data < get_len());
		data = lzopt::hybrid::len_encode(len, data);
	}

	inline void encode(byte *literal_run, unsigned int ell, std::uint32_t next)
	{
		data = lit_write.encode(ell, data, literal_run, next);
	}

	/**
	 * @brief Gets the amount of data (in BYTES) needed to store a parsing of length parsing_length (in BITS)
	 * @param parsing_length
	 *		The parsing length, in bits
	 * @return
	 *		The amount of of data (in BYTES) needed to store the parsing.
	 */
	static size_t data_len(size_t parsing_length)
	{
		return std::ceil(parsing_length / 8) + 8;
	}

	static enc_cost_info get_info()
	{
		std::vector<unsigned int> dst =  {(1 << 6), (1 << 14), (1 << 22), (1 << 30)};
		std::vector<unsigned int> dstcst = {8, 16, 24, 32};
		std::vector<unsigned int> len = {(1 << 7), (1 << 15)};
		std::vector<unsigned int> lencst = {8, 16};
		
		return {dst, dstcst, len, lencst};
	}

	static cost_model get_cm()
	{

		enc_cost_info info = get_info();

		class_info dst_class(info.dst.begin(), info.dst.end(), info.dstcst.begin(), info.dstcst.end());
		class_info len_class(info.len.begin(), info.len.end(), info.lencst.begin(), info.lencst.end());
		lit_t writer;
		return cost_model(dst_class, len_class, writer.fixed_cost(), writer.var_cost());
	}

	static size_t get_literal_len()
	{
		lit_t writer;
		return writer.max_length();
	}

	virtual ~encoder()
	{

	}
};

template <typename lit_type>
class decoder : public base_encoder {
private:
	// The data being decoded -- we don't own it.
	byte *data;
	lit_type lit_read;

public:
	decoder(byte *array, size_t text_length) : data(array)
	{
		set_len(text_length);
	}

	inline void decode(std::uint32_t &dst, std::uint32_t &len)
	{
		data = lzopt::hybrid::dst_decode(data, &dst);
		data = lzopt::hybrid::len_decode(data, &len);
	}

	inline void decode(byte *str, std::uint32_t &len, std::uint32_t &next)
	{
		data = lit_read.decode(len, data, str, next);
	}

	// Returns the amount of extra memory to be reserved for allocating the
	// file
	static size_t extra_read()
	{
		return 8U;
	}
};


} // Namespace lzopt::hybrid

typedef lzopt::literal_write<std::uint8_t> lit_8;
typedef lzopt::literal_write<std::uint16_t> lit_16;
typedef lzopt::literal_write<std::uint32_t> lit_32;

struct hybrid_coder_8 {
	typedef lzopt::hybrid::encoder<lit_8> encoder;
	typedef lzopt::hybrid::decoder<lit_8> decoder;
	static std::string name()
	{
		return "hybrid-8";
	}
};

struct hybrid_coder_16 {
	typedef lzopt::hybrid::encoder<lit_16> encoder;
	typedef lzopt::hybrid::decoder<lit_16> decoder;
	static std::string name()
	{
		return "hybrid-16";
	}
};

struct hybrid_coder_32 {
	typedef lzopt::hybrid::encoder<lit_32> encoder;
	typedef lzopt::hybrid::decoder<lit_32> decoder;
	static std::string name()
	{
		return "hybrid-32";
	}
};

struct hybrid_coder_1 {
	typedef lzopt::hybrid::encoder<char_write> encoder;
	typedef lzopt::hybrid::decoder<char_write> decoder;
	static std::string name()
	{
		return "hybrid";
	}
};

} // Namespace lzopt

/************************* SODA09 DESCRIPTION *********************************/
namespace soda09 {

	struct soda09_dst {
		static std::array<unsigned int, 7> cost_classes;
		static std::array<unsigned int, 6> binary_width;
		static std::array<unsigned int, 6> decode_mask;
	};

	struct soda09_len {
		static std::array<unsigned int, 16> cost_classes;
		static std::array<unsigned int, 15> binary_width;
		static std::array<unsigned int, 15> decode_mask;
	};
	struct cost_classes {
		typedef soda09_dst dst;
		typedef soda09_len len;
	};
}

/************************* NIBBLE DESCRIPTION *********************************/
namespace nibble {
	struct class_desc {
		static std::array<unsigned int, 11> cost_classes;
		static std::array<unsigned int, 10> binary_width;
		static std::array<unsigned int, 10> decode_mask;
	};

	struct cost_classes {
		typedef class_desc dst;
		typedef class_desc len;
	};
}

/************************** GAMMA-LIKE ENCODER STUB ***************************/
template <typename T>
void unary_suffix_length(std::uint64_t integer, T &length)
{
	length = __builtin_ctz(integer);
}


namespace gamma_like {

template <typename encoding>
void encode(std::uint_fast32_t to_encode, unaligned_io::writer &writer)
{
	assert(to_encode <= encoding::cost_classes.back());
	// First: find the cost class
	auto cost_ptr = std::lower_bound(ITERS(encoding::cost_classes), to_encode);
	auto class_id = std::distance(encoding::cost_classes.begin(), cost_ptr);
	// Second: write the proper number of 0, followed by a 1.
	writer.write(1 << (class_id - 1), class_id);
	// Third: write the remainder in binary representation
	to_encode -= cost_ptr[-1];
	writer.write(to_encode - 1, encoding::binary_width[class_id - 1]);
}

template <typename encoding>
std::uint32_t decode(unaligned_io::reader &reader)
{
	// First: read a whole 64-bit integer
	std::uint64_t d_word = 0U;
	unsigned int cost_class;
	reader.peek(d_word);
	// Find out the cost class
	unary_suffix_length(d_word, cost_class);
	// Advance the binary buffer
	reader.skip_bits(1 + cost_class + encoding::binary_width[cost_class]);
	// Get the integer
	return 1 + (((d_word >> (cost_class + 1)) & encoding::decode_mask[cost_class]) + encoding::cost_classes[cost_class]);
	// Second: skip those 1's, read the proper number of bits and append to the remainder
}

template <typename enc_inf, typename lit_write>
class encoder : public lzopt::base_encoder {
private:
	unaligned_io::writer writer;
	byte *start_data;
	size_t data_size;
	lit_write lit_writer;
	typedef typename enc_inf::dst dst_ci;
	typedef typename enc_inf::len len_ci;

	template <typename cost_class_t>
	static class_info get_ci()
	{
		using std::vector;
		auto &width = cost_class_t::cost_classes;
		auto &bin_cost = cost_class_t::binary_width;
		vector<unsigned int> dsts(std::next(width.begin(), 1), width.end());
		vector<unsigned int> costs(bin_cost.begin(), bin_cost.end());
		for (auto i = 0U; i < costs.size(); i++) {
			costs[i] += i + 1;
		}
		return class_info(ITERS(dsts), ITERS(costs));
	}

public:
	encoder(byte *data_, size_t data_size)
		: writer(data_), start_data(data_), data_size(data_size)
	{
		set_len(data_size);
	}

	inline void encode(std::uint32_t dst, std::uint32_t len)
	{
		assert(dst > 0);
		assert(len > 0);
		gamma_like::encode<dst_ci>(dst, writer);
		gamma_like::encode<len_ci>(len, writer);
	}

	inline void encode(byte *literal_run, unsigned int ell, std::uint32_t next)
	{
		lit_writer.write(literal_run, ell, writer, next);
	}

	/**
	 * @brief Gets the amount of data (in BYTES) needed to store a parsing of length parsing_length (in BITS)
	 * @param parsing_length
	 *		The parsing length, in bits
	 * @return
	 *		The amount of of data (in BYTES) needed to store the parsing.
	 */
	static size_t data_len(size_t parsing_length)
	{
		return std::ceil(parsing_length / 8) + 8;
	}

	static enc_cost_info get_info()
	{
		class_info ci_dst = get_ci<dst_ci>(), ci_len = get_ci<len_ci>();
		std::vector<unsigned int> dst 	 = ci_dst.win;
		std::vector<unsigned int> len 	 = ci_len.win;

		std::vector<unsigned int> dstcst(ci_dst.costs.size());
		std::vector<unsigned int> lencst(ci_len.costs.size());
		std::copy(ITERS(ci_dst.costs), dstcst.begin());
		std::copy(ITERS(ci_len.costs), lencst.begin());
		return {dst, dstcst, len, lencst};
	}

	static cost_model get_cm()
	{
		class_info dst = get_ci<dst_ci>(), len = get_ci<len_ci>();
		lit_write lw;
		auto fix_cost = lw.fixed_cost();
		auto var_cost = lw.var_cost();
		return cost_model(dst, len, fix_cost, var_cost);
	}

	static size_t get_literal_len()
	{
		return lit_write().max_length();
	}

	virtual ~encoder()
	{

	}
};

template <typename enc_inf, typename lit_read>
class decoder : public lzopt::base_encoder {
private:
	unaligned_io::reader reader;
	lit_read lit_reader;
	typedef typename enc_inf::cost_classes::dst dst_ci;
	typedef typename enc_inf::cost_classes::len len_ci;
public:
	decoder(byte *array, size_t text_length) : reader(array)
	{
		set_len(text_length);
	}

	inline void decode(std::uint32_t &dst, std::uint32_t &len)
	{
		dst = gamma_like::decode<dst_ci>(reader);
		len = gamma_like::decode<len_ci>(reader);
	}

	inline void decode(byte *str, std::uint32_t &len, std::uint32_t &next)
	{
		len = next = 0U;
		lit_reader.read(str, len, next, reader);
	}

	// Returns the amount of extra memory to be reserved for allocating the
	// file
	static size_t extra_read()
	{
		return 8U;
	}
};

}


/************************** PROPER IMPLEMENTATION *****************************/
struct soda09_coder_1 {
	typedef gamma_like::encoder<soda09::cost_classes, unaligned_io::literal::single_writer> encoder;
	typedef gamma_like::decoder<soda09::cost_classes, unaligned_io::literal::single_reader> decoder;
	static std::string name()
	{
		return "soda09";
	}
};

struct soda09_coder_8 {
	typedef gamma_like::encoder<soda09::cost_classes, unaligned_io::literal::writer<std::uint8_t, 0U>> encoder;
	typedef gamma_like::decoder<soda09::cost_classes, unaligned_io::literal::reader<std::uint8_t, 0U>> decoder;
	static std::string name()
	{
		return "soda09_8";
	}
};

struct soda09_coder_16 {
	typedef gamma_like::encoder<soda09::cost_classes, unaligned_io::literal::writer<std::uint16_t, 0U>> encoder;
	typedef gamma_like::decoder<soda09::cost_classes, unaligned_io::literal::reader<std::uint16_t, 0U>> decoder;
	static std::string name()
	{
		return "soda09_16";
	}
};

struct soda09_coder_8U {
	typedef gamma_like::encoder<soda09::cost_classes, unaligned_io::literal::writer<std::uint8_t, 1U>> encoder;
	typedef gamma_like::decoder<soda09::cost_classes, unaligned_io::literal::reader<std::uint8_t, 1U>> decoder;
	static std::string name()
	{
		return "soda09_8U";
	}
};

struct soda09_coder_16U {
	typedef gamma_like::encoder<soda09::cost_classes, unaligned_io::literal::writer<std::uint16_t, 1U>> encoder;
	typedef gamma_like::decoder<soda09::cost_classes, unaligned_io::literal::reader<std::uint16_t, 1U>> decoder;
	static std::string name()
	{
		return "soda09_16U";
	}
};


struct nibble4_coder_1 {
	typedef gamma_like::encoder<nibble::cost_classes, unaligned_io::literal::single_writer> encoder;
	typedef gamma_like::decoder<nibble::cost_classes, unaligned_io::literal::single_reader> decoder;
	static std::string name()
	{
		return "nibble4";
	}
};

struct nibble4_coder_8 {
	typedef gamma_like::encoder<nibble::cost_classes, unaligned_io::literal::writer<std::uint8_t, 0U>> encoder;
	typedef gamma_like::decoder<nibble::cost_classes, unaligned_io::literal::reader<std::uint8_t, 0U>> decoder;
	static std::string name()
	{
		return "nibble4_8";
	}
};

struct nibble4_coder_16 {
	typedef gamma_like::encoder<soda09::cost_classes, unaligned_io::literal::writer<std::uint16_t, 0U>> encoder;
	typedef gamma_like::decoder<soda09::cost_classes, unaligned_io::literal::reader<std::uint16_t, 0U>> decoder;
	static std::string name()
	{
		return "nibble4_16";
	}
};

struct nibble4_coder_8U {
	typedef gamma_like::encoder<nibble::cost_classes, unaligned_io::literal::writer<std::uint8_t, 1U>> encoder;
	typedef gamma_like::decoder<nibble::cost_classes, unaligned_io::literal::reader<std::uint8_t, 1U>> decoder;
	static std::string name()
	{
		return "nibble4_8U";
	}
};

struct nibble4_coder_16U {
	typedef gamma_like::encoder<soda09::cost_classes, unaligned_io::literal::writer<std::uint16_t, 1U>> encoder;
	typedef gamma_like::decoder<soda09::cost_classes, unaligned_io::literal::reader<std::uint16_t, 1U>> decoder;
	static std::string name()
	{
		return "nibble4_16U";
	}
};

/**
 * Defines a container of encoders.
 * The list of included encoders is encoded in the template parameters.
 * The class supports two operations:
 * - Returns the list of names associated to contained encoders;
 * - Given a name, a template and a base class B, returns an instantiation of
 *   the template with the encoder corresponding to the given name, in the
 *   form of a unique_ptr<B>.
 */
template <typename ...T>
struct encoders;

template <typename T, typename... U>
struct encoders<T, U...>
{
	// Get the names
	void get_names(std::vector<std::string> &names)
	{
		names.push_back(T::name());
		encoders<U...>().get_names(names);
	}

	// Istantiate something with T
	template <typename V, typename Factory_>
	std::unique_ptr<V> instantiate(std::string name, const Factory_ &factory)
	{
		if (name == T::name()) {
			return factory.template get_instance<T>();
		} else {
			return encoders<U...>().template instantiate<V, Factory_>(name, factory);
		}
	}

	// Get the cost model of an encoder
	cost_model get_cm(std::string name)
	{
		if (name == T::name()) {
			return T::encoder::get_cm();
		} else {
			return encoders<U...>().get_cm(name);
		}
	}

	// Get the cost model of an encoder
	enc_cost_info get_info(std::string name)
	{
		if (name == T::name()) {
			return T::encoder::get_info();
		} else {
			return encoders<U...>().get_info(name);
		}
	}

	// Get the literal infos of a generator
	size_t get_literal_len(std::string name)
	{
		if (name == T::name()) {
			return T::encoder::get_literal_len();
		} else {
			return encoders<U...>().get_literal_len(name);
		}
	}

	// Invoke a functor with the proper type
	template <typename Functor>
	void call(std::string name, Functor &&func)
	{
		if (name == T::name()) {
			func.template run<T>();
		} else {
			return encoders<U...>().template call<Functor>(name, std::forward<Functor>(func));
		}
	}

};

// Didn't find it
template <>
struct encoders<>
{
	void get_names(std::vector<std::string> &names) {
	}

	std::string error(std::string name)
	{
		return "No encoder named" + name;
	}

	template <typename V, typename Factory_>
	std::unique_ptr<V> instantiate(std::string name, const Factory_ &)
	{
		throw std::logic_error(error(name));
	}

	size_t get_literal_len(std::string name)
	{
		throw std::logic_error(error(name));
	}

	cost_model get_cm(std::string name)
	{
		throw std::logic_error(error(name));
	}

	enc_cost_info get_info(std::string name)
	{
		throw std::logic_error(error(name));
	}

	template <typename Functor>
	void call(std::string name, Functor &&)
	{
		throw std::logic_error(error(name));
	}

};

// Defines a container with the list of all known encoders.
typedef encoders<
	lzopt::hybrid_coder_1, lzopt::hybrid_coder_8, lzopt::hybrid_coder_16,
	soda09_coder_1, soda09_coder_8, soda09_coder_16, soda09_coder_8U, soda09_coder_16U,
	nibble4_coder_1, nibble4_coder_8, nibble4_coder_16, nibble4_coder_8U, nibble4_coder_16U
> encoders_;
#endif
