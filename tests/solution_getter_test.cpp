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

#include <graph_cache.hpp>
#include <optimal_parser.hpp>
#include <solution_getter.hpp>

#include <iostream>
#include <string>
#include <write_parsing.hpp>
#include <cm_factory.hpp>
#include <target_read.hpp>
#include <gtest/gtest.h>
#include <chrono>


#ifdef NDEBUG
typedef fsg_meter meter_t;
#else
typedef empty_observer meter_t;
#endif

// TEST ////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class solution_getter_test : public ::testing::Test {
protected:

	static void SetUpTestCase()
	{

	}

	virtual void SetUp()
	{
		// Initialize solution_getter
		sg = solution_getter<meter_t>(ti, lit_len);
		// Initialize optimal solution
		auto fsg_gen = fsg_fact(ti, sc).instantiate(cm);
		double cost;
		optimal_sol = parse(ti, *fsg_gen.get(), lit_len, cm, &cost, empty_observer());
		optimal_cost = parsing_length<double>(ITERS(optimal_sol), cm);
		optimal_weight = parsing_length<double>(ITERS(optimal_sol), w_cm);		
	}

public:
	static text_info ti;
	static cached_graph cg;
	static cost_model cm;
	static cost_model w_cm;
	static sa_cacher sc;
	static char *encoder_name;
	static std::vector<edge_t> optimal_sol;
	static double optimal_cost;
	static double optimal_weight;

	solution_getter<meter_t> sg;
	static size_t lit_len;

	static void set_filename(char *file_name)
	{
		size_t file_length;
		auto file = get_file<byte>(file_name, &file_length);
		ti = text_info(file.release(), file_length);
	}

	static void set_cm(cost_model cm_, cost_model w_cm_)
	{
		cm = cm_;
		w_cm = w_cm_;
	}

	static void set_litlen(size_t lit_len_)
	{
		lit_len = lit_len_;			
	}
};

text_info solution_getter_test::ti;
cached_graph solution_getter_test::cg;
cost_model solution_getter_test::cm;
cost_model solution_getter_test::w_cm;
sa_cacher solution_getter_test::sc;
std::vector<edge_t> solution_getter_test::optimal_sol;
size_t solution_getter_test::lit_len;
double solution_getter_test::optimal_cost;
double solution_getter_test::optimal_weight;

TEST_F(solution_getter_test, optimal_full_fsg)
{
	// Get optimal, full solution with our solution
	auto sol = sg.full(cm);

	// Get costs and certify they are indeed equal
	auto cost = parsing_length<double>(ITERS(sol), cm);
	ASSERT_EQ(cost, optimal_cost);
}

TEST_F(solution_getter_test, optimal_full_full)
{

	// Get two times a full solution 
	auto sol_1 = sg.full(cm);
	auto sol_2 = sg.full(cm);

	// Assert they are exactly the same
	ASSERT_EQ(sol_1.size(), sol_2.size());
	for (unsigned int i = 0; i < sol_1.size(); i++) {
		ASSERT_EQ(sol_1[i].d, sol_2[i].d);
		ASSERT_EQ(sol_1[i].ell, sol_2[i].ell);
		ASSERT_EQ(sol_1[i].cost_id, sol_2[i].cost_id);
	}
}

TEST_F(solution_getter_test, optimal_full_fast)
{
	// Make sure graph is cached
	if (!sg.warm()) {
		sg.full(cm);
	}
	ASSERT_TRUE(sg.warm());
	// Get cached solution
	auto sol = sg.fast(cm);

	// Make sure its cost is OK
	auto cost_opt = parsing_length<double>(ITERS(sol), cm);
	ASSERT_EQ(cost_opt, optimal_cost);
}

double get_weight_lower_bound(size_t t_len, size_t lit_len, cost_model w_cm)
{
	std::vector<edge_t> dummy_sol(t_len);
	for (auto i = 0u; i < dummy_sol.size();) {
		auto max_lit_len = std::min<unsigned int>(dummy_sol.size(), lit_len);
		dummy_sol[i] = edge_t(max_lit_len);
		i += max_lit_len;
	}
	return parsing_length<double>(ITERS(dummy_sol), w_cm);
}

TEST_F(solution_getter_test, weight_optimal)
{
	// Get a weight-optimal solution
	auto sol = sg.full(w_cm);
	auto weight = parsing_length<double>(ITERS(sol), w_cm);

	// Obtain an upper-bound on solution's weight: literal cost
	auto m_weight = get_weight_lower_bound(ti.len, lit_len, w_cm);

	// Compare that cost with the obtained one
	ASSERT_LE(weight, m_weight);
}

TEST_F(solution_getter_test, bi_optimal_full)
{
	// Get bi-optimal solution
	auto sol = sg.full(cm, w_cm);

	// Get cost and weight
	auto cost = parsing_length<double>(ITERS(sol), cm);
	auto weight = parsing_length<double>(ITERS(sol), w_cm);

	ASSERT_EQ(cost, optimal_cost);
	ASSERT_LE(weight, optimal_weight);
	EXPECT_LT(weight, optimal_weight);
#ifndef NDEBUG
	std::cout << "weight = " << weight << ", optimal_weight = " << optimal_weight << std::endl;
#endif
}

TEST_F(solution_getter_test, bi_full_fast)
{
	std::cout << "Getting cost-optimal solution..." << std::endl;
	auto t1 = std::chrono::high_resolution_clock::now();
	auto full_sol = sg.full(cm, w_cm);
	auto t2 = std::chrono::high_resolution_clock::now();
	std::cout << std::endl << "Elapsed time: " << std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count() << std::endl;
	ASSERT_TRUE(sg.warm());
	std::cout << "Getting weight-optimal solution..." << std::endl;	
	t1 = std::chrono::high_resolution_clock::now();
	auto fast_sol = sg.fast(cm, w_cm);
	t2 = std::chrono::high_resolution_clock::now();
	std::cout << std::endl << "Elapsed time: " << std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count() << std::endl;
	std::cout << "Estimating parsing costs... ";
	t1 = std::chrono::high_resolution_clock::now();
	auto full_cost = parsing_length<double>(ITERS(full_sol), cm);
	auto full_weight = parsing_length<double>(ITERS(full_sol), w_cm);
	ASSERT_TRUE(sg.warm());
	auto fast_cost = parsing_length<double>(ITERS(fast_sol), cm);
	ASSERT_TRUE(sg.warm());
	auto fast_weight = parsing_length<double>(ITERS(fast_sol), w_cm);
	t2 = std::chrono::high_resolution_clock::now();
	std::cout << "time: " << std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count() << std::endl;
	ASSERT_EQ(fast_cost, full_cost);
	ASSERT_EQ(fast_weight, full_weight);
}

int main(int argc, char **argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  if (argc < 4) {
  	std::cerr << "ERROR: input file, target file and encoder needed!" << std::endl;
  	exit(1);
  }
  solution_getter_test::set_filename(*++argv);
  char *target_file = *++argv;
  char *encoder_name = *++argv;
  cost_model cost = encoders_().get_cm(encoder_name);
  cost_model weight = get_wm(target_file, encoder_name);
  auto fact = cm_factory(cost, weight);
  solution_getter_test::set_cm(fact.cost(), fact.weight());
  solution_getter_test::set_litlen(encoders_().get_literal_len(encoder_name));
  return RUN_ALL_TESTS();
}
