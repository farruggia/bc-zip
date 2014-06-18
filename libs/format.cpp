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

#include <cstring>

#include <format.hpp>
#include <string>

uncomp_size_t extract_size(byte *comp, byte **new_ptr)
{
	uncomp_size_t to_ret = *reinterpret_cast<uncomp_size_t*>(comp);
	if (new_ptr != nullptr) {
		*new_ptr = comp += sizeof(to_ret);
	}
	return to_ret;
}

pack_info pack(std::string enc_name, size_t orig_len, size_t comp_len)
{
	size_t data_len = comp_len + sizeof(uncomp_size_t) + enc_name.length() + 1;
	std::unique_ptr<byte[]> data_holder(new byte[data_len + 8]);
	byte *data = data_holder.get();
	std::fill(data, data + data_len + 8, 0U);

	data = std::copy(enc_name.begin(), enc_name.end(), data);
	*data++ = '\0';

	data = inject_size(data, orig_len);
	return {std::move(data_holder), data_len};
}

std::tuple<std::string, size_t, byte*> unpack(byte *data)
{
	const size_t max_enc_name = 20;
	char enc_name[20];
	std::strncpy(enc_name, const_cast<char*>(reinterpret_cast<char*>(data)), max_enc_name);
	enc_name[max_enc_name - 1] = '\0';
	data += strnlen(enc_name, max_enc_name) + 1;
	size_t orig_size = extract_size(data, &data);
	return std::make_tuple(std::string(enc_name), orig_size, data);
}

std::tuple<char*, uncomp_size_t*, byte*> ptr_unpack(byte *data)
{
	std::string enc_name;
	size_t size;
	byte *start;
	std::tie(enc_name, size, start) = unpack(data);
	char *name_start = reinterpret_cast<char*>(data);
	uncomp_size_t *len_ptr = reinterpret_cast<uncomp_size_t*>(name_start + enc_name.length() + 1);
	return std::make_tuple(name_start, len_ptr, start);
}