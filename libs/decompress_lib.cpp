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

#include <decompress.hpp>
#include <format.hpp>

dec_output decompress_full(byte *data)
{
	std::string enc_name;
	size_t orig_size;
	std::tie(enc_name, orig_size, data) = unpack(data);
	char *enc_ptr = const_cast<char*>(enc_name.data());
	std::unique_ptr<byte[]> uncompressed = std::unique_ptr<byte[]>(new byte[orig_size + 8]);
	std::uint64_t nanosecs = decompress_raw(enc_ptr, data, uncompressed.get(), orig_size);
	return {std::move(uncompressed), enc_name, orig_size, nanosecs};
}