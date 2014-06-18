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

#include <solution_integrator.hpp>
#include <parsing_manage.hpp>

#include <gtest/gtest.h>
#include <cm_factory.hpp>
#include <target_read.hpp>
#include <write_parsing.hpp>
#include <chrono>


#ifdef NDEBUG
typedef fsg_meter meter_t;
#else
typedef empty_observer meter_t;
#endif

// TESTS ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
class solution_integrator_test : public ::testing::Test {
public:
	static text_info ti;
	static cached_graph cg;
	static cost_model cm;
	static cost_model w_cm;
	static sa_cacher sc;
	static char *encoder_name;
	static size_t lit_len;

	solution_getter<meter_t> sg;

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

	static void set_encoder(char *encoder_name_)
	{
		encoder_name = encoder_name_;
		set_litlen(encoders_().get_literal_len(encoder_name_));
	}

protected:
	static void SetUpTestCase()
	{

	}

	virtual void SetUp()
	{
		// Initialize solution_getter
		sg = solution_getter<meter_t>(ti, lit_len);
	}
};

text_info solution_integrator_test::ti;
cached_graph solution_integrator_test::cg;
cost_model solution_integrator_test::cm;
cost_model solution_integrator_test::w_cm;
sa_cacher solution_integrator_test::sc;
size_t solution_integrator_test::lit_len;
char *solution_integrator_test::encoder_name;

void check_eq(std::vector<edge_t> &got, std::vector<edge_t> &expected)
{
	ASSERT_EQ(got.size(), expected.size());
	auto len = got.size() - 1;
	auto i = 0U;
	while (i < len) {
		auto &integrated = got[i];
		auto &fully_generated = expected[i];
		ASSERT_EQ(integrated.d, fully_generated.d);
		ASSERT_EQ(integrated.ell, fully_generated.ell);
		ASSERT_EQ(integrated.cost_id, fully_generated.cost_id);	
		i += integrated.ell;
	}
}

TEST_F(solution_integrator_test, check)
{
	auto fact = gen_ffsg_fact(ti, sc);
	solution_integrator<meter_t> si(std::move(fact), cm);

	// Obtain real cost-optimal solution
	std::cout << "Getting cost-optimal solution..." << std::endl;
	auto t1 = std::chrono::high_resolution_clock::now();
	auto real_cost = sg.full(cm);
	auto t2 = std::chrono::high_resolution_clock::now();
	std::cout << std::endl << "Elapsed time: " << std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count() << std::endl;
	// Obtain real weight-optimal solution
	std::cout << "Getting weight-optimal solution..." << std::endl;	
	t1 = std::chrono::high_resolution_clock::now();
	auto real_weight = sg.full(w_cm);
	t2 = std::chrono::high_resolution_clock::now();
	std::cout << std::endl << "Elapsed time: " << std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count() << std::endl;
	// Obtain cached cost-optimal solution 
	ASSERT_TRUE(sg.warm());

	std::cout << "Getting cached cost-optimal solution..." << std::endl;
	t1 = std::chrono::high_resolution_clock::now();	
	auto cache_cost = sg.fast(cm);
	t2 = std::chrono::high_resolution_clock::now();
	std::cout << std::endl << "Elapsed time: " << std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count() << std::endl;

	// Obtain cached weight-optimal solution
	ASSERT_TRUE(sg.warm());
	std::cout << "Getting cached weight-optimal solution..." << std::endl;	
	t1 = std::chrono::high_resolution_clock::now();
	auto cache_weight = sg.fast(w_cm);
	t2 = std::chrono::high_resolution_clock::now();
	std::cout << std::endl << "Elapsed time: " << std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count() << std::endl;

	// Fix cached solutions
	std::cout << "Integrating solutions... ";
	// std::flush(std::cout);
	std::vector<byte> data_holder(ti.len + 8, 0);
	byte *data_ptr = data_holder.data();
	std::vector<vector_in> in = {
		vector_in(&cache_cost, data_ptr),
		vector_in(&cache_weight, data_ptr)
	};
	std::vector<vector_out> out = {
		vector_out(&cache_cost, data_ptr, data_ptr + ti.len, cm),
		vector_out(&cache_weight, data_ptr, data_ptr + ti.len, cm),
	};
	t1 = std::chrono::high_resolution_clock::now();		
	si.integrate(in, out);
	t2 = std::chrono::high_resolution_clock::now();
	std::cout << "elapsed time: " << std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count() << std::endl;

	// Check both if they are equal
	check_eq(cache_cost, real_cost);
	check_eq(cache_weight, real_weight);
}

class compressor {
public:
	virtual compressed_file compress(const std::vector<edge_t> &solution, text_info ti) = 0;
};

template <typename enc_>
class r_compressor : public compressor{
private:
	cost_model cm;
public:

	r_compressor(cost_model cm) : cm(cm)
	{

	}

	compressed_file compress(const std::vector<edge_t> &solution, text_info ti)
	{
		auto p_length = parsing_length<size_t>(ITERS(solution), cm);
		return write_parsing<enc_>(solution, p_length, ti);
	}
};

class comp_fact {
private:
	cost_model cm;
public:

	comp_fact(cost_model cm) : cm(cm)
	{

	}

	template <typename enc_>
	std::unique_ptr<compressor> get_instance() const
	{
		return make_unique<r_compressor<enc_>>(cm);
	}
};

class integrator {
public:
	virtual void run(std::vector<parsing> in, std::vector<parsing> out, solution_integrator<meter_t> &integrate_) = 0;
};

template <typename enc_>
class r_integrator : public integrator{

public:

	r_integrator()
	{

	}

	void run(std::vector<parsing> in, std::vector<parsing> out, solution_integrator<meter_t> &integrate_)
	{
		integrate<enc_>(in, out, integrate_);
	}
};

class int_fact {
public:

	int_fact()
	{

	}

	template <typename enc_>
	std::unique_ptr<integrator> get_instance() const
	{
		return make_unique<r_integrator<enc_>>();
	}
};

TEST_F(solution_integrator_test, comp_test)
{
	auto fact = gen_ffsg_fact(ti, sc);
	solution_integrator<meter_t> si(std::move(fact), cm);

	if (!sg.warm()) {
		std::cout << "Warming the cache..." << std::endl;
		auto t1 = std::chrono::high_resolution_clock::now();
		auto real_cost = sg.full(cm);
		auto t2 = std::chrono::high_resolution_clock::now();
		std::cout << std::endl << "Elapsed time: " << std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count() << std::endl;		
	}
	std::cout << "Getting cached cost-optimal solution..." << std::endl;
	auto t1 = std::chrono::high_resolution_clock::now();	
	auto cache_cost = sg.fast(cm);
	auto t2 = std::chrono::high_resolution_clock::now();
	std::cout << std::endl << "Elapsed time: " << std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count() << std::endl;

	std::cout << "Compressing..." << std::endl;
	comp_fact c_fact(cm);
	auto compress_ptr = encoders_().instantiate<compressor, comp_fact>(encoder_name, c_fact);
	auto compressed = compress_ptr->compress(cache_cost, ti);

	std::cout << "Integrating...";
	std::flush(std::cout);
	t1 = std::chrono::high_resolution_clock::now();	
	auto old_parsing = get_parsing(compressed);
	parsing new_parsing;
	auto data_holder = dup_parsing(old_parsing, new_parsing);
	int_fact i_fact;
	auto integrate_ptr = encoders_().instantiate<integrator, int_fact>(encoder_name, i_fact);
	integrate_ptr->run({old_parsing}, {new_parsing}, si);
	t2 = std::chrono::high_resolution_clock::now();
	std::cout << std::endl << "Elapsed time: " << std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count() << std::endl;

	// Now check if solution decompresses well
	std::unique_ptr<byte[]> decompressed(new byte[ti.len + 8]);
	byte *dec_ptr = decompressed.get();
	decompress_raw(encoder_name, new_parsing.begin, dec_ptr, new_parsing.orig_len);
	byte *original = ti.text.get();
	for (auto i = 0; i < ti.len; i++) {
		ASSERT_EQ(original[i], dec_ptr[i]) << join_s("Position = ", i);
	}

}

int main(int argc, char **argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  if (argc < 4) {
  	std::cerr << "ERROR: input file, target file, encoder and lit_len needed!" << std::endl;
  	exit(1);
  }
  solution_integrator_test::set_filename(*++argv);
  char *target_file = *++argv;
  char *encoder_name = *++argv;
  cost_model cost = encoders_().get_cm(encoder_name);
  cost_model weight = get_wm(target_file, encoder_name);
  auto fact = cm_factory(cost, weight);
  solution_integrator_test::set_encoder(encoder_name);
  solution_integrator_test::set_cm(fact.cost(), fact.weight());
  return RUN_ALL_TESTS();
}
