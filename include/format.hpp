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

#ifndef FORMAT_HPP
#define FORMAT_HPP

#include <tuple>
#include <memory>

#include <common.hpp>

typedef std::uint32_t uncomp_size_t;
// INJECT/EJECT SIZE ////////////////////////////////////////////////////////////////////////
template <typename T>
byte *inject_size(byte *comp, T size)
{
	*reinterpret_cast<T*>(comp) = size;
	return comp += sizeof(size);
}

uncomp_size_t extract_size(byte *comp, byte **new_ptr);

// PACK FUNCTIONS ///////////////////////////////////////////////////////////////////////////
/**
 * @brief Unpacks parameters
 * @param data
 *		Compressed data
 * @return
 *		Gets the encoder name, the uncompressed length, the start of the encoded run and the encoded run length
 */
std::tuple<std::string, size_t, byte*> unpack(byte *data);

std::tuple<char*, uncomp_size_t*, byte*> ptr_unpack(byte *data);

struct pack_info {
	std::unique_ptr<byte[]> parsing;
	size_t data_len;
};

/**
 * @brief Packs the parameters and reserve enough space for the comp. representation
 * @param enc_name
 *		The encoder name
 * @param orig_len
 *		The decompressed file lenght
 * @param comp_len
 *		The compressed length
 * @return
 *		A pointer to the start of the comp. rep. and the length of the compressed rep.
 */
pack_info pack(std::string enc_name, size_t orig_len, size_t comp_len);
/////////////////////////////////////////////////////////////////////////////////////////////

#endif // FORMAT_HPP
