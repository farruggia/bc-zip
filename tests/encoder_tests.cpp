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
#include <vector>
#include <array>
#include <algorithm>

#include <stdint.h>
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <common.hpp>
#include <cost_model.hpp>
#include <copy_routines.hpp>
#include <encoders.hpp>
#include <gtest/gtest.h>

template <typename dst_desc>
void soda_test()
{
	const size_t bytes = 1024U;
	byte data[bytes];
	std::fill(data, data + bytes, 0U);
	unaligned_io::reader read(data);
	unaligned_io::writer write(data);

	// Check boundaries
	for (unsigned int i = 1U; i < dst_desc::cost_classes.size(); i++) {
		auto to_encode = dst_desc::cost_classes[i];
		gamma_like::encode<dst_desc>(to_encode, write);
		auto decoded = gamma_like::decode<dst_desc>(read);
		ASSERT_EQ(to_encode, decoded);
	}

	// Check for increasing powers of two
	unsigned int power = 1U;
	while (power <= dst_desc::cost_classes.back()) {
		gamma_like::encode<dst_desc>(power, write);
		power <<= 1U;
	}
	power = 1;
	while (power <= dst_desc::cost_classes.back()) {
		auto check = gamma_like::decode<dst_desc>(read);
		ASSERT_EQ(check, power);
		power <<= 1U;
	}
}

template <typename T>
T wide_head(byte *head)
{
	return *reinterpret_cast<T>(head);
}

template <typename T>
void check_head(byte *head, T value)
{
	T v_head = *reinterpret_cast<T*>(head);
	ASSERT_EQ(v_head, value) << "Head = " << std::hex << v_head << ", value = " << value;
}

void writer_test()
{
	const size_t storage_size = 1024U;
	std::vector<byte> storage(storage_size);
	std::fill(storage.begin(), storage.end(), 0U);
	byte *head = storage.data();
	unaligned_io::writer writer(head);
	std::uint32_t word = 0xDEADBEEF;
//	std::cout << std::hex << word << std::endl;
//	std::uint64_t dword = word | (static_cast<std::uint64_t>(word) << (sizeof(word) * 8));
//	std::cout << dword << std::endl;

	// Ok, start easy: write a single word.
	writer.write(word);
	ASSERT_EQ(std::distance(head, writer.writing_head()), sizeof(word));
	check_head(head, word);

	head = writer.writing_head();
	// Now let's write in pieces
	auto to_write = word;
	std::vector<unsigned int> shifts = {5, 7, 10, 8, 2};
	for (auto i : shifts) {
		writer.write(to_write, i);
		to_write >>= i;		
	}
	ASSERT_EQ(std::distance(head, writer.writing_head()), sizeof(word));
	check_head(head, word);

	head = writer.writing_head();

	// Ok, what about this?
	writer.write(word, 12);
	word >>= 12;
//	std::cout << std::hex << writer.writing_head() << std::endl;
	ASSERT_EQ(std::distance(head, writer.writing_head()), 1);
	ASSERT_EQ(writer.offset(), 4U);
	writer.write(word, 4);
	word >>= 4;
	ASSERT_EQ(writer.offset(), 0U);
	auto to_check = *reinterpret_cast<std::uint16_t*>(head);
//	std::cout << to_check << std::endl;
	ASSERT_EQ(to_check, 0xBEEF);
//	std::cout << to_check << std::endl;
	ASSERT_EQ(writer.offset(), 0U);

	head = writer.writing_head();
	writer.write(0x8, 4);
	writer.write(0x9, 4);
	writer.write(0xA, 4);
	writer.write(0xB, 4);
	writer.write(0xC, 4);
	writer.write(0xD, 4);
	writer.write(0xE, 4);
	writer.write(0xF, 4);
	std::uint32_t exp_word = *reinterpret_cast<std::uint32_t*>(head);
	ASSERT_EQ(exp_word, 0xFEDCBA98U);
}

void reader_test()
{
	// Write something with the writer
	const size_t byte_size = 1024U;
	byte data[byte_size];
	std::fill(data, data + byte_size, 0U);
	unaligned_io::writer writer(data);
	unsigned int write = 0U;
	byte *data_ptr = data;
	for (unsigned int i = 0; i < byte_size / 4U; i++) {
		writer.write(write++, 4);
//		if ((i % 2) == 0) {
//			std::cout << std::hex << (static_cast<unsigned int>(*data_ptr) & 0xF) << " ";
//		} else {
//			std::cout << std::hex << (static_cast<unsigned int>(*data_ptr) >> 4U) << " ";
//			data_ptr++;
//		}
	}
//	std::cout << std::endl;

//	data_ptr = data;
//	for (auto i = 0U; i < byte_size / 8U; i++) {
//		std::cout << std::hex << (static_cast<unsigned int>(*data_ptr) & 0xF) << " ";
//		std::cout << std::hex << (static_cast<unsigned int>(*data_ptr) >> 4U) << " ";
//		data_ptr++;
//	}

	// Read the same of what we read, with matching operations
	write = 0U;
	unaligned_io::reader reader(data);
	for (unsigned int i = 0; i < byte_size / 4U; i++) {
		byte read = 0U;
		reader.read(read, 4);
//		std::cout << std::hex << (static_cast<unsigned int>(read) & 0xF) << " ";
//		std::cout << std::flush;
		ASSERT_EQ(read, (write++) & 0xF) << "read = " 
				<< std::hex << static_cast<unsigned int>(read) 
				<< ", expected " << ((static_cast<unsigned int>(write) - 1) & 7U);
	}
//	std::cout << std::endl;

	// Read the same of what we read, but this time reading blocks of 16-bit word with offsets.
	reader = unaligned_io::reader(data);
	byte read = 0U;
	reader.read(read, 4);
	ASSERT_EQ(read, 0U);
	for (unsigned int i = 1; i < byte_size / 4U; i += 4) {
		std::uint16_t dbyte = 0U;
		std::uint16_t	first_nibble = (i & 0xF), second_nibble = ((i + 1) & 0xF),
				third_nibble = ((i + 2) & 0xF), fourth_nibble = ((i + 3) & 0xF);
		std::uint16_t expected = first_nibble | (second_nibble << 4U) | (third_nibble << 8U) | (fourth_nibble << 12U);
		reader.read(dbyte);
		ASSERT_EQ(expected, dbyte);
	}

	// The same, but with peek().
	reader = unaligned_io::reader(data);
	read = 0U;
	reader.peek(read, 4);
	reader.skip_bits(4);
	ASSERT_EQ(read, 0U);
	for (unsigned int i = 1; i < byte_size / 4U; i += 4) {
		std::uint16_t dbyte = 0U;
		std::uint16_t	first_nibble = (i & 0xF), second_nibble = ((i + 1) & 0xF),
				third_nibble = ((i + 2) & 0xF), fourth_nibble = ((i + 3) & 0xF);
		std::uint16_t expected = first_nibble | (second_nibble << 4U) | (third_nibble << 8U) | (fourth_nibble << 12U);
		reader.peek(dbyte);
		ASSERT_EQ(expected, dbyte);
		reader.skip_bytes(2);
	}

	// Get three-bits and assemble'em
	reader = unaligned_io::reader(data);
	read = 0U;
	reader.peek(read, 4);
	reader.skip_bits(4);
	ASSERT_EQ(read, 0U);
	for (unsigned int i = 1; i < byte_size / 4U; i += 4) {
		std::uint16_t dbyte = 0U;
		std::uint16_t	first_nibble = (i & 0xF), second_nibble = ((i + 1) & 0xF),
				third_nibble = ((i + 2) & 0xF), fourth_nibble = ((i + 3) & 0xF);
		std::uint16_t expected = first_nibble | (second_nibble << 4U) | (third_nibble << 8U) | (fourth_nibble << 12U);

		std::uint16_t built = 0U;
		unsigned int step = 3U;
		for (auto j = 0U; j < 16U; j += step) {
			std::uint32_t read_now = 0U;
			auto bits_now = std::min(16U - j, step);
			reader.peek(read_now, bits_now);
			built |= (read_now << j);
			reader.skip_bits(bits_now);
		}
		ASSERT_EQ(expected, built);
	}

}

template <typename run_t, std::uint64_t start>
void write_string
(
		byte *write_buffer, const char *test_string, run_t &len,
		std::uint32_t next, unaligned_io::literal::writer<run_t, start> &lw,
		unaligned_io::writer &write
)
{
	len = strlen(test_string);
	std::copy(test_string, test_string + len, write_buffer);
	lw.write(write_buffer, len, write, next);
}

template <typename run_t, std::uint64_t start>
std::string read_string
(
	byte *read_buffer, run_t &len,
	std::uint32_t &next, unaligned_io::literal::reader<run_t, start> &lr,
	unaligned_io::reader &read
)
{
	next = len = 0U;
	std::uint32_t pass_len;
	lr.read(read_buffer, pass_len, next, read);
	len = pass_len;
	return std::string(reinterpret_cast<char*>(read_buffer), len);
}

template <typename run_t, std::uint64_t start_>
void literal_test()
{
	// Get a synchronized reader/writer
	const unsigned int data_size = 1024U;
	byte data[data_size];
	std::fill(data, data + data_size, 0U);
	unaligned_io::reader read(data);
	unaligned_io::writer write(data);
	unaligned_io::literal::reader<run_t, start_> lr;
	unaligned_io::literal::writer<run_t, start_> lw;

	/* First test: unaligned I/O */

	// Write/read something out of them
	std::uint16_t something = 0xBEEF;
	write.write(something, 10);
	read.read(something, 10);

	// Write a string
	const size_t buffer_size = 1024U;
	byte write_buffer[buffer_size];
	std::string expected = "The answer is 42";
	run_t expected_len;
	std::uint32_t next = 5U;
	write_string(write_buffer, expected.c_str(), expected_len, next, lw, write);

	byte read_buffer[buffer_size];
	std::uint32_t read_next;
	run_t read_len;
	std::string read_seq = read_string(read_buffer, read_len, read_next, lr, read);
	ASSERT_EQ(read_len, expected_len);
	ASSERT_EQ(read_next, next);
	ASSERT_EQ(read_seq, expected);

	// Again
	expected = "The answer is 1982.";
	write_string(write_buffer, expected.c_str(), expected_len, next, lw, write);

	read_seq = read_string(read_buffer, read_len, read_next, lr, read);
	ASSERT_EQ(read_len, expected_len);
	ASSERT_EQ(read_next, next);
	ASSERT_EQ(expected, read_seq);

	// Write TWO strings, aligned this time, and read'em.
	ASSERT_EQ(read.offset(), write.offset());
	read.skip_bits(8U - read.offset());
	write.skip_bits(8U - write.offset());

	std::vector<std::string> expected_seqs = {"The answer is 1982.", "Walter White"};
	std::vector<std::uint32_t> expected_nexts = {5U, 16U};
	std::vector<run_t> expected_lens(2);

	for (unsigned int i = 0U; i < expected_seqs.size(); i++) {
		write_string(write_buffer, expected_seqs[i].c_str(), expected_lens[i], expected_nexts[i], lw, write);
	}

	for (unsigned int i = 0U; i < expected_seqs.size(); i++) {
		run_t len;
		std::uint32_t next;
		auto read_seq = read_string(read_buffer, len, next, lr, read);
		ASSERT_EQ(next, expected_nexts[i]);
		ASSERT_EQ(len, expected_lens[i]);
		ASSERT_EQ(read_seq, expected_seqs[i]);
	}
}

template <typename encoder_cc, typename lit_writer, typename lit_reader>
void soda09_test()
{
	typedef typename gamma_like::encoder<encoder_cc, lit_writer> encoder;
	typedef typename gamma_like::decoder<encoder_cc, lit_reader> decoder;

	const size_t file_len = 1024U;
	std::vector<edge_t> dummy_sol(file_len + 1U);

	// Initialize the encoder and the decoder
	std::array<byte, file_len> data_array;
	std::fill(data_array.begin(), data_array.end(), 0U);
	byte *data = data_array.data();
	encoder enc(data, file_len);
	decoder dec(data, file_len);
	ASSERT_EQ(dec.get_len(), file_len);

	// Is the maximum allowed literal length OK?
	ASSERT_EQ(enc.get_literal_len(), lit_writer().max_length());

	// Is the fixed cost OK?
	{
		auto fix_cost = lit_writer().fixed_cost();
		auto var_cost = lit_writer().var_cost();
		ASSERT_EQ(enc.get_cm().lit_cost(1U), fix_cost + var_cost);
		ASSERT_EQ(enc.get_cm().lit_cost(2U), fix_cost + 2 * var_cost);
	}

	// Are the distance informations OK?
	cost_model cm = enc.get_cm();
	{
		typedef typename encoder_cc::dst dst_cc;
		auto dst_win = cm.get_dst();
		ASSERT_EQ(dst_win.size(), dst_cc::cost_classes.size() - 1);
		for (auto i = 1U; i < dst_cc::cost_classes.size(); i++) {
			ASSERT_EQ(dst_win[i - 1], dst_cc::cost_classes[i]);
		}
	}

	// Are the length informations OK?
	{
		typedef typename encoder_cc::len len_cc;
		auto len_win = cm.get_len();
		ASSERT_EQ(len_win.size(), len_cc::cost_classes.size() - 1);
		for (auto i = 1U; i < len_cc::cost_classes.size(); i++) {
			ASSERT_EQ(len_win[i - 1], len_cc::cost_classes[i]);
		}
	}

	// Are the costs OK?
	{
		typedef typename encoder_cc::dst dst_cc;
		typedef typename encoder_cc::len len_cc;

		ASSERT_EQ(cm.get_dst().size(), dst_cc::binary_width.size());
		ASSERT_EQ(cm.get_len().size(), len_cc::binary_width.size());
		for (auto i = 0U; i < cm.get_dst().size(); i++) {
			for (auto j = 0U; j < cm.get_len().size(); j++) {
				auto cost = cm.get_cost(i, j);
				ASSERT_EQ(cost, dst_cc::binary_width[i] + len_cc::binary_width[j] + i + j + 2);
			}
		}
	}

	// Encode some numbers
	{
		enc.encode(42U, 64U);
		enc.encode(64U, 128U);
		std::uint32_t dst, len;
		dec.decode(dst, len);
		ASSERT_EQ(dst, 42U);
		ASSERT_EQ(len, 64U);
		dec.decode(dst, len);
		ASSERT_EQ(dst, 64U);
		ASSERT_EQ(len, 128U);
	}

	// Mix numbers and strings
	{
		byte data[10];
		std::string test_string_1 = "test";
		std::copy(test_string_1.begin(), test_string_1.end(), data);
		enc.encode(data, test_string_1.size(), 2U);
		enc.encode(1 << 20, 1 << 10);
		enc.encode(1 << 10, 1 << 20);
		std::string test_string_2 = "pluto";
		std::copy(test_string_2.begin(), test_string_2.end(), data);
		enc.encode(data, test_string_2.size(), 2U);
		//======
		std::uint32_t dst, len, next;
		dec.decode(data, len, next);
		ASSERT_EQ(len, test_string_1.size());
		ASSERT_EQ(next, 2U);
		for (auto i = 0U; i < len; i++) {
			ASSERT_EQ(test_string_1[i], data[i]);
		}
		dec.decode(dst, len);
		ASSERT_EQ(dst, 1 << 20);
		ASSERT_EQ(len, 1 << 10);
		dec.decode(dst, len);
		ASSERT_EQ(len, 1 << 20);
		ASSERT_EQ(dst, 1 << 10);
		dec.decode(data, len, next);
		ASSERT_EQ(len, test_string_2.size());
		ASSERT_EQ(next, 2U);
		for (auto i = 0U; i < len; i++) {
			ASSERT_EQ(test_string_2[i], data[i]);
		}
	}
}

void char_writer_test() {
	const size_t buffer_size = 1024U;
	byte buffer[buffer_size];
	std::fill(buffer, buffer + buffer_size, 0U);
	unaligned_io::writer write(buffer);
	unaligned_io::reader read(buffer);

	unaligned_io::literal::single_reader sr;
	unaligned_io::literal::single_writer sw;
	byte to_write[3] = {'A', 'B', 'C'};
	sw.write(to_write, 1, write, 5U);
	write.write(10U, 4);
	sw.write(to_write + 1, 1, write, 10U);
	write.write(7U, 3);
	sw.write(to_write + 2, 1, write, 15U);

	unsigned int bits;
	std::uint32_t length, next;
	sr.read(buffer, length, next, read);
	ASSERT_EQ(*buffer, 'A');
	ASSERT_EQ(length, 1U);
	ASSERT_EQ(next, 5U);
	read.read(bits, 4);
	ASSERT_EQ(bits, 10U);
	sr.read(buffer, length, next, read);
	ASSERT_EQ(*buffer, 'B');
	ASSERT_EQ(length, 1U);
	ASSERT_EQ(next, 10U);
	read.read(bits, 3);
	ASSERT_EQ(bits, 7U);
	sr.read(buffer, length, next, read);
	ASSERT_EQ(*buffer, 'C');
	ASSERT_EQ(length, 1U);
	ASSERT_EQ(next, 15U);
}

TEST(unaligned_io, writer) {
	writer_test();
}

TEST(unaligned_io, reader) {
	reader_test();
}

TEST(gamma_like, SODA09_DST) {
	soda_test<soda09::soda09_dst>();	
}

TEST(gamma_like, SODA09_LEN) {
	soda_test<soda09::soda09_len>();	
}

TEST(gamma_like, NIBBLE4) {
	soda_test<nibble::class_desc>();	
}

TEST(gamma_like, SODA09_0U) {
	soda09_test<
		soda09::cost_classes,
		unaligned_io::literal::writer<std::uint16_t, 0U>,
		unaligned_io::literal::reader<std::uint16_t, 0U>
	>();	
}

TEST(gamma_like, SODA09_1U) {
	soda09_test<
		soda09::cost_classes,
		unaligned_io::literal::writer<std::uint16_t, 1U>,
		unaligned_io::literal::reader<std::uint16_t, 1U>
	>();
}

TEST(gamma_like, NIBBLE_0U) {
	soda09_test<
		nibble::cost_classes,
		unaligned_io::literal::writer<std::uint16_t, 0U>,
		unaligned_io::literal::reader<std::uint16_t, 0U>
	>();
}

TEST(gamma_like, NIBBLE_1U) {
	soda09_test<
		nibble::cost_classes,
		unaligned_io::literal::writer<std::uint16_t, 1U>,
		unaligned_io::literal::reader<std::uint16_t, 1U>
	>();
}

TEST(character_writer, general) {
	char_writer_test();
}

TEST(literal_writer, 0U) {
	literal_test<std::uint16_t, 0U>();	
}

TEST(literal_writer, 1U) {
	literal_test<std::uint16_t, 1U>();	
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}