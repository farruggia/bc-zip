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

#include <phrase_reader.hpp>
#include <format.hpp>

std::unique_ptr<lzopt::i_phrase_reader> lzopt::get_phrase_reader(byte *compressed_file)
{
	using namespace lzopt;
	std::string enc_name;
	size_t t_len;
	byte *start;
	std::tie(enc_name, t_len, start) = unpack(compressed_file);

	return lzopt::get_phrase_reader(enc_name, start, t_len);
}

std::unique_ptr<lzopt::i_phrase_reader> lzopt::get_phrase_reader(std::string enc_name, byte *start, size_t text_len)
{
	pr_factory fct(start, text_len);
	return encoders_().instantiate<i_phrase_reader, pr_factory>(enc_name, fct);	
}
