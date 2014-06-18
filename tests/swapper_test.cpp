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

#include <path_swapper.hpp>
#include <solution_getter.hpp>
#include <encoders.hpp>
#include <write_parsing.hpp>
#include <decompress.hpp>
#include <target_read.hpp>
#include <cm_factory.hpp>

#include <gtest/gtest.h>

#include <iostream>
#include <chrono>

#ifdef NDEBUG
	typedef fsg_meter meter_t;
#else
	typedef empty_observer meter_t;
#endif

class swapper_test : public ::testing::Test {
public:
	static text_info ti;
	static cached_graph cg;
	static cost_model space_cm;
	static cost_model time_cm;
	static sa_cacher sc;
	static char *encoder_name;

	solution_getter<meter_t> sg;

	static void set_filename(char *file_name)
	{
		size_t file_length;
		auto file = get_file<byte>(file_name, &file_length);
		ti = text_info(file.release(), file_length);
	}

	static void set_cm(cost_model cm_, cost_model time_cm_)
	{
		space_cm = cm_;
		time_cm = time_cm_;
	}

	static void set_encoder(char *encoder_name_)
	{
		encoder_name = encoder_name_;
	}

protected:
	static void SetUpTestCase()
	{

	}

	virtual void SetUp()
	{
		// Initialize solution_getter
		sg = solution_getter<meter_t>(ti, encoders_().get_literal_len(encoder_name));
	}
};

text_info swapper_test::ti;
cached_graph swapper_test::cg;
cost_model swapper_test::space_cm;
cost_model swapper_test::time_cm;
sa_cacher swapper_test::sc;
char *swapper_test::encoder_name;

bool operator==(const edge_t &e1, const edge_t &e2)
{
	if (e1.d != e2.d) {
		return false;
	}
	if (e1.ell != e2.ell) {
		return false;
	}
	return e1.cost_id == e2.cost_id;
}

::std::ostream& operator<<(::std::ostream& os, const edge_t& edge) {
  return os << "{ D = " << edge.d << ", LEN = " << edge.ell << "}";
}

void check_eq(const std::vector<edge_t> &expected, const std::vector<edge_t> &got)
{
	ASSERT_EQ(expected.size(), got.size());
	unsigned int pos = 0;
	while (pos < expected.size() - 1) {
		ASSERT_EQ(expected[pos], got[pos]);
		pos += expected[pos].ell;
	}
}

void check_eq(byte *expected, byte *got, size_t len)
{
	for (auto i = 0; i < len; i++) {
		ASSERT_EQ(expected[i], got[i]) << "Mismatch at position " << i;
	}
}

TEST_F(swapper_test, all)
{
	// Get space-optimal solution parsing
	std::cerr << "Getting space-optimal solution" << std::endl;
	auto t1 = std::chrono::high_resolution_clock::now();
	auto space_optimal = sg.full(space_cm);
	auto space_comp = write_parsing(space_optimal, ti, encoder_name, space_cm);
	auto space_p = get_parsing(space_comp);
	auto t2 = std::chrono::high_resolution_clock::now();
	std::cerr << "Elapsed time = " << std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count() << " msecs" << std::endl;

	// Get time-optimal solution parsing
	std::cerr << "Getting time-optimal solution" << std::endl;
	t1 = std::chrono::high_resolution_clock::now();
	auto time_optimal = sg.full(time_cm);
	auto time_comp = write_parsing(time_optimal, ti, encoder_name, space_cm);
	auto time_p = get_parsing(time_comp);
	t2 = std::chrono::high_resolution_clock::now();
	std::cerr << "Elapsed time = " << std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count() << " msecs" << std::endl;

	// Get times
	std::cerr << "Getting spaces and weights solution" << std::endl;
	t1 = std::chrono::high_resolution_clock::now();
	double weight_space = parsing_length<double>(ITERS(space_optimal), time_cm);
	double cost_space = parsing_length<double>(ITERS(space_optimal), space_cm);
	double weight_time = parsing_length<double>(ITERS(time_optimal), time_cm);
	double cost_time = parsing_length<double>(ITERS(time_optimal), space_cm);
	t2 = std::chrono::high_resolution_clock::now();
	std::cerr << "Space-optimal: space = " << cost_space << ", time = " << weight_space << std::endl;
	std::cerr << "Time-optimal: space = " << cost_time << ", time = " << weight_time << std::endl;
	std::cerr << "Elapsed time = " << std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count() << " msecs" << std::endl;

	if (weight_time == weight_space) {
		std::cerr << "Solutions can't be differentiated, returning" << std::endl;
		return;
	}

	// Weight on time: let's try for different W.
	{
		double W = weight_space;
		std::cerr << "Testing when W = W_max (" << W << ")" << std::endl;
		// Set bound W = weight_space: solution must be identical to space_optimal
		t1 = std::chrono::high_resolution_clock::now();
		auto swapped = path_swap(encoder_name,
			space_p, cost_space, weight_space, time_p, cost_time, weight_time, space_cm, time_cm, W
		);
		t2 = std::chrono::high_resolution_clock::now();
		std::cerr << "Elapsed time = " << std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count() << " msecs" << std::endl;
		check_eq(space_optimal, swapped);
	}
	{
		double W = weight_time;
		std::cerr << "Testing when W = W_min (" <<  W << ")" << std::endl;
		// Set bound W = weight_space: solution must be identical to space_optimal
		t1 = std::chrono::high_resolution_clock::now();
		auto swapped = path_swap(encoder_name,
			space_p, cost_space, weight_space, time_p, cost_time, weight_time, space_cm, time_cm, W

		);
		t2 = std::chrono::high_resolution_clock::now();
		std::cerr << "Elapsed time = " << std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count() << " msecs" << std::endl;

		check_eq(time_optimal, swapped);
	}

	// Vary W: check if (A) path is decompressible (B) bound is not violated and (C) spaces are decreasing
	const double base_step = 0.2;
	auto prev_cost = cost_time;
	for (auto level = base_step; level < 1; level += base_step) {
		double W = weight_time + level * (weight_space - weight_time);
		std::cerr << "Testing when W = (" <<  W << ")" << std::endl;
		t1 = std::chrono::high_resolution_clock::now();
		auto swapped = path_swap(encoder_name,
			space_p, cost_space, weight_space, time_p, cost_time, weight_time, space_cm, time_cm, W
		);
		// (A) path is decompressible 
		auto swap_comp = write_parsing(swapped, ti, encoder_name, space_cm);
		auto swap_p = get_parsing(swap_comp);
		t2 = std::chrono::high_resolution_clock::now();
		std::cerr << "Elapsed time = " << std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count() << " msecs" << std::endl;

		std::vector<byte> uncompressed_rep(ti.len);
		decompress_raw(encoder_name, swap_p.begin, uncompressed_rep.data(), ti.len);
		check_eq(ti.text.get(), uncompressed_rep.data(), ti.len);

		// (B) bound is not violated 
		auto weight = parsing_length<double>(ITERS(swapped), time_cm);
		ASSERT_LE(weight, W);

		// (C) costs are decreasing
		auto cost = parsing_length<double>(ITERS(swapped), space_cm);
		ASSERT_LE(cost, prev_cost);

		prev_cost = cost;
	}

}

int main(int argc, char **argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  if (argc < 4) {
  	std::cerr << "ERROR: input file, target file and encoder needed!" << std::endl;
  	exit(1);
  }
  swapper_test::set_filename(*++argv);
  char *target_file = *++argv;
  char *encoder_name = *++argv;
  cost_model cost = encoders_().get_cm(encoder_name);
  cost_model weight = get_wm(target_file, encoder_name);
  auto fact = cm_factory(cost, weight);
  swapper_test::set_encoder(encoder_name);
  swapper_test::set_cm(fact.cost(), fact.weight());
  return RUN_ALL_TESTS();
}