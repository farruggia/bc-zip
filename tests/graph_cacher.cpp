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
#include <random>
#include <memory>
#include <vector>

#include <graph_cache.hpp>
#include <gtest/gtest.h>
#include <wm_serializer.hpp>
#include <fsg_check.hpp>

template <typename cost_class_t, typename unary_type_t>
void test_encoder(size_t v_size, double max_expansion)
{
	// Max expansion is OK
	auto exp = unary_gammalike::encoder<cost_class_t, unary_type_t>::ub_gamma();
	ASSERT_EQ(exp, max_expansion);
	// Get the highest value encodable
	auto highest = cost_class_t::cost_classes.back();
	
	// Generate v_size random integers in range [0, highest]
    std::random_device rd;
    std::uniform_int_distribution<int> dist(0, highest);

	std::vector<unsigned int> to_check(v_size);
	for (auto &i : to_check) {
		i = dist(rd);
	}
	to_check[0] = 4;
	to_check[1] = 7;
	to_check[2] = 8;

	// Compute maximum possible storage space
	auto max_storage = v_size * sizeof(unary_type_t);
	auto max_bits = cost_class_t::binary_width.back() + cost_class_t::binary_width.size() + 1;
	max_storage = std::floor(1.0 * (max_bits + 8) * v_size / 8);
	std::vector<byte> storage(max_storage, 0U);

	// Instantiate encoder and decoder
	unary_gammalike::encoder<cost_class_t, unary_type_t> enc(storage.data(), max_storage * 8);
	unary_gammalike::decoder<cost_class_t, unary_type_t> dec(storage.data(), max_storage * 8);
	for (auto i : to_check) {
		enc.encode(i);
		auto to_verify = dec.decode();
		ASSERT_EQ(to_verify, i);
	}
}

TEST(Encoder, soda09_dst)
{
	test_encoder<soda09::soda09_dst, byte>(1000000, 23.0 / 9);
}

TEST(Encoder, soda09_len)
{
	test_encoder<soda09::soda09_len, byte>(1000000, 12.0 / 9);
}

TEST(Encoder, nibble)
{
	test_encoder<nibble::class_desc, byte>(1000000, 12.0 / 9);
}

/**
 * Start "cached_graph" implementation. Safe to move and copy around.
 */
class graph_cache_test : public ::testing::Test {
protected:

	static void SetUpTestCase()
	{

	}

public:
	static text_info ti;
	static cached_graph cg;
	static cost_model cm;
	static sa_cacher sc;

	static void set_filename(char *file_name)
	{
		size_t file_length;
		auto file = get_file<byte>(file_name, &file_length);
		ti = text_info(file.release(), file_length);
	}

	static void set_cm(cost_model cm_)
	{
		cm = cm_;
	}
};

text_info graph_cache_test::ti;
cached_graph graph_cache_test::cg;
cost_model graph_cache_test::cm;
sa_cacher graph_cache_test::sc;


template <typename encoder_t>
void test_caching(text_info ti, cost_model cm, cached_graph *cg, sa_cacher &sc)
{
	// Grab the two factories
	caching_fsg_fact<fsg_fact, encoder_t> cache_fact(ti, sc, cg);
	fsg_fact fsg(ti, sc);

	// Run the test
	compare_run(ti, cm).run(std::move(fsg), std::move(cache_fact));
}

template <typename decoder_t>
void test_cached(text_info ti, cost_model cm, cached_graph cg, sa_cacher &sc)
{
	// Grab the two factories
	cached_fsg_fact<decoder_t> cache_fact(ti, sc, cg);
	fsg_fact fsg(ti, sc);

	// Run the test
	compare_run(ti, cm).run(std::move(fsg), std::move(cache_fact));
}

TEST_F(graph_cache_test, Caching)
{
	test_caching<unary_gammalike::encoder<nibble::class_desc, byte>>(graph_cache_test::ti, graph_cache_test::cm, &graph_cache_test::cg, graph_cache_test::sc);
}

TEST_F(graph_cache_test, Cached)
{
	test_cached<unary_gammalike::decoder<nibble::class_desc, byte>>(graph_cache_test::ti, graph_cache_test::cm, graph_cache_test::cg, graph_cache_test::sc);
}

int main(int argc, char **argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  if (argc < 3) {
  	std::cerr << "ERROR: input file and cost_model needed!" << std::endl;
  	exit(1);
  }
  graph_cache_test::set_filename(*++argv);
  graph_cache_test::set_cm(wm_load(*++argv));
  return RUN_ALL_TESTS();
}
