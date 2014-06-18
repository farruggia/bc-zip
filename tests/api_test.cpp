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

#include <iostream>
#include <string>
#include <memory>
#include <algorithm>

#include <api.hpp>
#include <phrase_reader.hpp>
#include <gtest/gtest.h>
#include <io.hpp>

class config {
public:
	static std::unique_ptr<byte[]> data;
	static size_t data_len;
	static std::string encoder;
	static void set_filename(const char *name)
	{
		// Read file into data
		auto file_read = read_file<byte>(name, &data_len);
		std::swap(data, file_read);
	}

	static void set_encoder(std::string enc)
	{
		encoder = enc;
	}
};

std::unique_ptr<byte[]> config::data;
size_t config::data_len;
std::string config::encoder;

void check_equal(byte *T1, byte *T2, size_t len)
{
	ASSERT_TRUE(std::equal(T1, T1 + len, T2));
}


void raw_compress_test()
{
	// Compress data
	auto enc_name = config::encoder;
	auto text_ptr = config::data.get();
	auto text_len = config::data_len;
	size_t comp_len;
	auto compressed_raw = bczip::compress_buffer(enc_name.c_str(), text_ptr, text_len, &comp_len);

	// Copy in a slightly enlarged buffer so we can decompress it without memory access issues.
	std::vector<byte> enlarged(bczip::safe_buffer_size(enc_name.c_str(), comp_len));
	std::fill(ITERS(enlarged), 0);
	std::copy(compressed_raw.get(), compressed_raw.get() + comp_len, enlarged.begin());

	// Decompress it
	auto dec_space = bczip::safe_buffer_size(enc_name.c_str(), text_len);
	std::vector<byte> data_storage(dec_space);
	byte *uncomp_ptr 	= data_storage.data();
	byte *comp_ptr 		= enlarged.data();
	bczip::decompress_buffer(enc_name.c_str(), comp_ptr, uncomp_ptr, text_len);

	// Check if they are equal
	check_equal(text_ptr, uncomp_ptr, text_len);
}

void header_check()
{
	// Compress a dummy string
	auto enc_name = config::encoder;
	std::string dummy = "mississippibananamississippi";
	size_t text_len = dummy.length();
	auto compressed = bczip::compress(enc_name.c_str(), reinterpret_cast<byte*>(const_cast<char*>(dummy.data())), text_len);

	// Get infos about the header
	char *enc_name_ptr;
	std::uint32_t orig_len;
	byte *start_parsing = bczip::extract_header(compressed.get(), &enc_name_ptr, &orig_len);
	ASSERT_EQ(std::string(enc_name_ptr), enc_name);
	ASSERT_EQ(orig_len, text_len);
	size_t header_size = start_parsing - compressed.get();

	// Create an header with the same infos
	size_t new_header_size;
	auto new_header = bczip::create_header(enc_name.c_str(), text_len, &new_header_size);
	ASSERT_EQ(header_size, new_header_size);
	// Get back those info
	bczip::extract_header(new_header.get(), &enc_name_ptr, &orig_len);
	ASSERT_EQ(std::string(enc_name_ptr), enc_name);
	ASSERT_EQ(orig_len, text_len);
}

void compress_test()
{
	// Compress it
	auto enc_name = config::encoder;
	auto text_ptr = config::data.get();
	auto text_len = config::data_len;
	size_t comp_len;
	auto compressed = bczip::compress(enc_name.c_str(), text_ptr, text_len, &comp_len);

	// Decompress it
	size_t dec_len;
	auto dec_ptr = bczip::decompress(compressed.get(), &dec_len);
	ASSERT_EQ(dec_len, text_len);

	// Check if they are equal
	check_equal(text_ptr, dec_ptr.get(), text_len);
}

class fuck_parsing {
private:
	std::vector<unsigned int> nexts;
	byte *old_parsing;
	byte *new_parsing;
	size_t text_len;
	size_t comp_len;
public:
	fuck_parsing(byte *old_parsing, byte *new_parsing, size_t text_len, size_t comp_len)
		: old_parsing(old_parsing), new_parsing(new_parsing), text_len(text_len), comp_len(comp_len)
	{

	}

	template <typename enc>
	void run()
	{
		typedef typename enc::encoder enc_t;

		enc_t coder(new_parsing, comp_len);
		auto reader_ptr = lzopt::get_phrase_reader(enc::name(), old_parsing, text_len);

		std::uint32_t dst, len;
		while (!reader_ptr->end()) {
			reader_ptr->next(dst, len);
			if (dst == 0) {
				byte *buffer = reader_ptr->get_buffer();
				nexts.push_back(reader_ptr->get_next());
				coder.encode(buffer, len, 42);
			} else {
				coder.encode(dst, len);
			}
		}
	}

	std::vector<unsigned int> get_nexts()
	{
		return std::move(nexts);
	}
};

void fix_parsing()
{
	// Compress something
	auto enc_name = config::encoder;
	auto text_ptr = config::data.get();
	auto text_len = config::data_len;
	size_t comp_len;
	auto compressed = bczip::compress(enc_name.c_str(), text_ptr, text_len, &comp_len);
	char *enc_name_ptr;
	std::uint32_t file_size;
	auto parsing_start = bczip::extract_header(compressed.get(), &enc_name_ptr, &file_size);
	auto parsing_length = comp_len - (parsing_start - compressed.get());

	// Fuck the nexts but keep the real nexts in an array
	std::vector<byte> old_parsing_storage(bczip::safe_buffer_size(enc_name.c_str(), parsing_length));
	std::copy(parsing_start, parsing_start + parsing_length, old_parsing_storage.begin());

	std::vector<byte> new_parsing_storage(bczip::safe_buffer_size(enc_name.c_str(), parsing_length));
	std::fill(ITERS(new_parsing_storage), 0U);
	
	fuck_parsing fucker(old_parsing_storage.data(), new_parsing_storage.data(), text_len, parsing_length);
	encoders_().call(enc_name, fucker);
	auto nexts = fucker.get_nexts();

	// Fix it
	// std::copy(new_parsing_storage.data(), new_parsing_storage.data() + parsing_length, parsing_start);
	std::fill(parsing_start, parsing_start + parsing_length, 0U);
	bczip::fix_parsing(
		enc_name, new_parsing_storage.data(), parsing_length, text_len, parsing_start, nexts.begin()
	);

	// Check if it decompress well	
	size_t dec_len;
	auto dec_ptr = bczip::decompress(compressed.get(), &dec_len);
	check_equal(text_ptr, dec_ptr.get(), text_len);

}

TEST(raw_compress, all)
{
	raw_compress_test();
}

TEST(compress, all)
{
	compress_test();
}

TEST(header_check, all)
{
	header_check();
}

TEST(fix_parsing, all)
{
	fix_parsing();
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  if (argc < 2) {
  	std::cerr << "ERROR: input file required.\nUsage: tool <input file> [encoder name]" << std::endl;
  	exit(1);
  }
  std::string encoder = argc > 2 ? argv[2] : "soda09_16";
  config::set_filename(argv[1]);
  config::set_encoder(encoder);
  return RUN_ALL_TESTS();
}