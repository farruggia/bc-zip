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

#ifndef __API__
#define __API__

#include <common.hpp>
#include <memory>
#include <cstdint>

#include <impl/api_impl.hpp>

/**
 * NOTES ABOUT THE API
 * The LZ77 factorization of a text T is denoted as a parsing of T.
 * A parsing may be compressed with an integer encoder to obtain a compressed representation of T.
 * However, in order to decompress the binary representation of that compressed parsing, two 
 * informations are needed:
 * -) The name of the encoder used to compress the parsing;
 * -) The length of T.
 * Thus, a compressed file is composed of two sections: an HEADER and a BODY.
 * The HEADER contains the encoder name and the length of the text; the BODY contains the 
 * compressed parsing plus a few bytes needed to allow efficient decompression (the exact amount depends
 * on the encoder used to compress the parsing).
 * Functions decompress and compress operates with full compressed files (that is, HEADER + BODY), while
 * decompress_buffer and compress_buffer operates directly with the BODY. The latter two are useful in
 * a number of situations, such as implementing parallel compressors which "glue" together parsings of
 * entire blocks of T. However, some care is required to use those functions: read carefully the 
 * documentation before using them to avoid hard-to-spot bugs.
 * If compress_buffer is used to obtain a compressed representation of T, the appropriate header must be
 * placed in front of that representation in order to obtain a compressed file. Use function create_header
 * to create such header. To get back informations contained therein, use extract_header.
 * 
 * In this implementation of LZ77, a phrase may be a copy <distance, length>, or a run of characters, that is,
 * a literal phrase.
 * In order to disambiguate between these two kind of phrases, the first phrase in the parsing must be
 * a literal phrase, and each literal phrase is followed by a 32-bit integer which denotes the number of
 * non-literal phrases which follows it in the parsing before the next literal.
 * For example, a valid parsing of "bananaX" is <ban, 1> <2, 3> <X, 0>; the "1" in <ban, 1> is the 
 * "next-literal" field (because only <2,3> divides <ban, 1> from the next literal phrase, <X, 0>).
 * This format implies that glueing together different parsings from different blocks may require to "fix"
 * those nextliteral fields. Function "fix_parsing" does exactly that. It takes as argument, among others, the
 * "glued" parsing (or, in general, a parsing which may have ill-defined nextliteral fields) and a list of
 * unsigned integers which represent the correct sequence of nextliteral fields. For example, if parsing
 * <ban, 42> <2, 3> <X, 5> and nextliteral list [1, 0] is given as argument, it returns the parsing
 * <ban, 1> <2, 3> <X, 0>. On the other hand, if list [42, 84] is given as argument, it returns parsing
 * <ban, 42> <2, 3> <X, 84>, which is incorrect. Thus, it is crucial to properly compute the list of nextliteral
 * fields to the function in order to obtain a correct parsing.
 *
 * Please consult api_test.cpp for some examples of how to use of this API.
 */

namespace bczip {

/**
 * Uncompress a RAW buffer (i.e., without headers)
 * \param	encoder_name	Encoder used to compress the data
 * \param 	compressed		Pointer to compressed data (without headers at the beginning - just the parsing)
 * \param 	output			Pointer to start of decompressed data. Sufficient memory should have been 
 							allocated prior to the invocation of this function.
 							Invoke safe_buffer_size to know exactly how much memory to allocate
 							for this buffer.
 * @uncompressed_size: 	Size of uncompressed size (in bytes)
 */
void decompress_buffer(const char *encoder_name, byte *compressed, byte *output, size_t uncompressed_size);

/**
 * Even if a compressed parsing may be allocated in just T bits, some additional C extra bytes must be allocated
 * for a containing buffer in order to safely reading or writing it. This is due to some peculiarities in the
 * implementation of encoders and decoders, which may access some extra bytes past the parsing end.
 * More simply: if a parsing must be written to a buffer B via the compress_buffer function, and the parsing has
 * length T (in bytes), this function gives the number of bytes which must be allocated for B, in bytes, that is,
 * T + C.
 * Symmetrically, if a compressed parsing of length T is stored somewhere (in a file or in an another buffer,
 * for example), and this parsing must be decoded via the decompress_buffer function, it must be placed in a 
 * buffer of length at least T + C, which is what is returned by this function.
 * \param 	encoder_name 		Name of encoder used in compression
 * \param	compressed_length	Parsing length, in bytes
 * \return 	Length of containing buffer, in bytes
 */
size_t safe_buffer_size(const char *encoder_name, size_t compressed_length);

/**
 * Uncompress a compressed buffer (headers + parsing)
 * \param 	compressed			Start of compressed buffer
 * \param 	decompressed_size	When not null, will contain the size of the decompressed output
 * \return	Pointer to decompressed output
 */
std::unique_ptr<byte[]> decompress(byte *compressed, size_t *decompressed_size = nullptr);

/**
 * Compress a text into a raw buffer (i.e., without headers at the beginning)
 * \param 	encoder_name		The integer encoder name (for a list, invoke bc-zip with command "list")
 * \param	uncompressed 		Pointer to data to be compressed
 * \param	length 				Length of data to be compressed
 * \param	compressed_length	If not null, will contain the length of compressed output
 * \return	Pointer to compressed output
 */
std::unique_ptr<byte[]>
compress_buffer(
	const char *encoder_name, byte *uncompressed, size_t length, size_t *compressed_length = nullptr
);

/**
 * Compress a text into a full buffer (i.e., WITH headers at the beginning)
 * \param 	encoder_name		The integer encoder name (for a list, invoke bc-zip with command "list")
 * \param	uncompressed 		Pointer to data to be compressed
 * \param	length 				Length of data to be compressed
 * \param	compressed_length	If not null, will contain the length of compressed output
 * \return	Pointer to compressed output
 */
std::unique_ptr<byte[]>
compress(
	const char *encoder_name, byte *uncompressed, size_t length, size_t *compressed_length = nullptr
);

/**
 * Extract the header from a full buffer.
 * \param	parsing			Pointer to parsing
 * \param	encoder_name	After the invocation, pointer to encoder used to compress the buffer
 * \param	file_size		After the invocation, pointer to uncompressed length
 * \return	Pointer to parsing
 */
byte *extract_header(byte *parsing, char **encoder_name, std::uint32_t *file_size);

/**
 * Creates an header (to be appended in front of a raw buffer to get a full buffer)
 * \param	encoder_name	Encoder name
 * \param	file_size 		File size
 * \return	Pointer to allocated header
 */
std::unique_ptr<byte[]> create_header(
	const char *encoder_name, std::uint32_t file_size, size_t *header_size
);

/**
 * Fix a parsing composed by "gluing" together different edges by properly fixing nextliteral values.
 * \param 	encoder 		Encoder used to obtain the parsing
 * \param 	parsing 		The parsing to be fixed
 * \param 	parsing_length	Parsing length
 * \param	output			The new, fixed parsing. Memory must be allocated in advance
 */
template <typename lits_it>
void fix_parsing(
	std::string encoder, 
	byte *parsing, size_t parsing_length, size_t uncomp_len, 
	byte *output,
	lits_it next_literals)
{
	encoders_ enc_container;
	impl::proper_nexts<lits_it> patcher(parsing, parsing_length, uncomp_len, next_literals, output);
	enc_container.call(encoder, patcher);
}

}
#endif