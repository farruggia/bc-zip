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

#include <parsing_manage.hpp>

parsing get_parsing(byte *compress, size_t compress_length)
{
	std::string enc_name;
	size_t uncomp_length;
	byte *start;
	std::tie(enc_name, uncomp_length, start) = unpack(compress);
	size_t comp_length = compress + compress_length - start;
	return {start, comp_length, uncomp_length};
}

parsing get_parsing(compressed_file &cf)
{
	return get_parsing(cf.data.get(), cf.total_size);
}

std::shared_ptr<byte> dup_parsing(parsing &in, parsing &out)
{
	std::shared_ptr<byte> to_ret(new byte[in.comp_len + 8], std::default_delete<byte[]>());
	std::fill(to_ret.get(), to_ret.get() + in.comp_len + 8, 0);
	out = {to_ret.get(), in.comp_len, in.orig_len};
	return to_ret;
}