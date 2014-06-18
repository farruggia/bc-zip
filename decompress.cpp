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

std::uint64_t decompress_file(const char *tool_name, int argc, char **argv, std::ostream &out)
{
	argc -= 2;
	argv += 2;
	if (argc < 2) {
		throw cmd_error(join_s(tool_name, " input output"));
	}
	char *input		= *argv++;
	char *output	= *argv++;

	size_t file_size;
	auto data_holder = read_file<byte>(input, &file_size, 8);
	byte *data = data_holder.get();

	dec_output dec_res = decompress_full(data);

	out << "Encoder: " << dec_res.enc_name << std::endl;
	out << "Decompression time: " << dec_res.dec_time / (std::nano::den / std::milli::den) << " msecs" << std::endl;

	// Now we should write "uncompressed".
	std::ofstream out_file;
	open_file(out_file, output);
	write_file(out_file, dec_res.rep.get(), dec_res.uncompressed_size);

	out << "Original size: " << dec_res.uncompressed_size << std::endl;
	return dec_res.dec_time;
}