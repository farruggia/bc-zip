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

#include <stdexcept>
#include <sstream>
#include <fstream>
#include <iostream>
#include <boost/program_options.hpp>
#include <vector>
#include <string>
#include <algorithm>
#include <map>
#include <assert.h>
#include <cstdint>
#include <chrono>

#include <bit_compress.hpp>
#include <cmd_parse.hpp>
#include <cost_model.hpp>
#include <io.hpp>
#include <utilities.hpp>
#include <optimal_parser.hpp>
#include <write_parsing.hpp>
#include <model_read.hpp>
#include <generators.hpp>
#include <bucket_fsg.hpp>
#include <meter_printer.hpp>
#include <type_traits>

void print_solution(const std::vector<edge_t> &sol, cost_model cm)
{
	auto it = sol.begin(), end = std::prev(sol.end(), 1);
	unsigned int cost = 0U;
	std::cout << "Distance\tLength\tEnding Cost\tEnding Position" << std::endl;
	while (it != end) {
		if (it->kind() == PLAIN) {
			cost += cm.lit_cost(it->ell);
			std::cout << "L\t" << it->ell << "\t" << cost;;
		} else {
			unsigned int dst_idx, len_idx;
			std::tie(dst_idx, len_idx) = cm.get_idx(it->d, it->ell);
			cost += cm.get_cost(dst_idx, len_idx);
			std::cout << it->d << "\t" << it->ell << "\t" << cost;
		}
		std::advance(it, it->ell);
		std::cout << "\t" << std::distance(sol.begin(), it) << std::endl;
	}
}

namespace {

class call_func {
protected:
	std::string in_file;
	std::string out_file;
	std::string encoder;
	const size_t bucket;
	bool correct_check;
	bool print_sol;
	bool use_meter;

	virtual std::tuple<cost_model, size_t> get_model() = 0;

	virtual std::string encoder_name() = 0;

	virtual void write(std::vector<edge_t> &&solution, text_info t_info) = 0;

	template <typename fsg_t>
	std::vector<edge_t> get_solution(
		text_info t_info, fsg_t &&fsg, size_t lit_win, cost_model cm, double *cost, bool use_meter
	)
	{
		static_assert(std::is_rvalue_reference<decltype(fsg)>::value, "FSG must be a rvalue.");
		if (use_meter) {
			return parse(t_info, std::forward<fsg_t>(fsg), lit_win, cm, cost, fsg_meter(t_info.len));
		}
		return parse(t_info, std::forward<fsg_t>(fsg), lit_win, cm, cost, empty_observer());
	}

	template <typename fsg_fact_t>
	void generic_run()
	{
		namespace chr = std::chrono;
		cost_model cm;
		size_t lit_win;
		std::tie(cm, lit_win) = get_model();

		auto file_info = get_file(in_file.c_str());
		text_info t_info(std::get<0>(file_info), std::get<1>(file_info));

		if (t_info.len == 0U) {
			throw std::logic_error("Cowardly refusing to run on an empty file.");
		}

		double cost;
		std::cout << "Encoder: " << encoder_name() << std::endl;
		std::cout << "Generator: " << fsg_fact_t::name() << std::endl;
		auto t1 = chr::high_resolution_clock::now();
		std::vector<edge_t> solution;
		sa_instantiate sa;

		auto mb_bucket = bucket * 1024 * 1024; // Convert from MBytes in bytes
		if (mb_bucket == 0) {
			// Do not use BMI without bucketing - it's useless.
			auto fsg = fsg_fact_t(t_info, sa).instantiate(cm);
			solution = get_solution(t_info, std::move(*(fsg.get())), lit_win, cm, &cost, use_meter);
		} else {
			bucket_fsg<fsg_fact_t> fsg(t_info, sa, mb_bucket, cm);
			solution = get_solution(t_info, std::move(fsg), lit_win, cm, &cost, use_meter);
		}

		auto t2 = chr::high_resolution_clock::now();
		auto spent_time = chr::duration_cast<chr::milliseconds>(t2 - t1);
		std::cout << "Compression time: " << spent_time.count() << " msecs" << std::endl;

		if (correct_check) {
			correctness_report report = check_correctness(solution, t_info.text.get());
			if (!report.correct) {
				throw std::logic_error(join_s(
					"Incorrect parsing: position ", report.error_position,
					", distance ", report.error_d,
					", length ", report.error_ell
				));
			}
		}

		std::cout << "Cost: " << static_cast<std::uint64_t>(cost) << std::endl;
		if (print_sol) {
			print_solution(solution, cm);
		}
		write(std::move(solution), t_info);
	}

public:
	call_func(std::string in_file, std::string out_file, std::string encoder, size_t bucket, bool correct_check, bool print_sol, bool use_meter)
		: in_file(in_file), out_file(out_file), encoder(encoder), bucket(bucket), correct_check(correct_check), print_sol(print_sol), use_meter(use_meter)
	{

	}

	virtual ~call_func()
	{

	}
};

class call_real_func : public call_func {
protected:
	std::tuple<cost_model, size_t> get_model() {
		// The encoder container
		encoders_ enc_container;

		// Check if the encoder name is OK
		std::vector<std::string> e_names;
		enc_container.get_names(e_names);
		auto found      = std::find(e_names.begin(), e_names.end(), encoder);
		if (found == e_names.end()) {
			throw std::runtime_error("Illegal encoder: " + encoder);
		}

		// Get cost model and literal window
		cost_model cm	= enc_container.get_cm(encoder);
		size_t lit_win	= enc_container.get_literal_len(encoder);
		return std::make_tuple(cm, lit_win);
	}

	std::string encoder_name()
	{
		return encoder;
	}

	void write(std::vector<edge_t> &&solution, text_info t_info) {
		write_parsing(solution, t_info, out_file, encoder);
	}

public:

	call_real_func(std::string in_file, std::string out_file, std::string encoder, size_t bucket, bool correct_check, bool print_sol, bool use_meter)
		: call_func(in_file, out_file, encoder, bucket, correct_check, print_sol, use_meter)
	{

	}

	template <typename fsg_fact_t>
	void run()
	{
		call_func::generic_run<fsg_fact_t>();
	}
};

class call_bogus_func : public call_func {
protected:
	std::tuple<cost_model, size_t> get_model() {
		unsigned int lit_win;
		cost_model cm = read_model(encoder.c_str(), &lit_win);
		return std::make_tuple(cm, lit_win);
	}

	std::string encoder_name()
	{
		return join_s("emulated, ", encoder);
	}

	void write(std::vector<edge_t> &&solution, text_info)
	{

	}

public:
	call_bogus_func(std::string infile, const char *model_name, size_t bucket, bool correct_check, bool print_sol, bool use_meter)
		: call_func(infile, "", model_name, bucket, correct_check, print_sol, use_meter)
	{

	}

	template <typename fsg_fact_t>
	void run()
	{
		call_func::generic_run<fsg_fact_t>();
	}
};

/**
 * @brief Bit-optimal parser, compressing as well
 * @param in_file
 *      Input file
 * @param out_file
 *      Output file
 * @param encoder
 *      Encoder name
 * @param generator
 *		Generator name
 * @param bucket
 *		Bucket size. 0 = no bucketization.
 */
void call_real(std::string in_file, std::string out_file, std::string encoder, std::string generator,
			   size_t bucket, bool correct_check, bool print_sol, bool use_meter)
{
	if (generator.empty()) {
		auto dst_win = encoders_().get_cm(encoder).get_dst();
		generator = suggest_gen(get_kind(dst_win));
	}

	const unsigned int max_try = 2U;
	for (unsigned int tries = 0U; tries < max_try; tries++) {
		try {
			generators_().call(generator, call_real_func(in_file, out_file, encoder, bucket, correct_check, print_sol, use_meter));
			return;
		} catch (gen_mismatch &e) {
			std::cerr << e.what() << std::endl;
			generator = e.suggest_gen();
		}
	}
}


/**
 * @brief Bit-optimal parser, with emulation
 * @param infile
 *      Input file
 * @param model_name
 *		The name of the file containing the model
 * @param generator
 *		Generator name
 * @param bucket
 *		Bucket size. 0 = no bucketization.
 */
void call_bogus(std::string infile, const char *model_name, std::string generator,
				size_t bucket, bool correct_check, bool print_sol, bool use_meter)
{
	if (generator.empty()) {
		unsigned int lit_win;
		auto dst_win = read_model(model_name, &lit_win).get_dst();
		generator = suggest_gen(get_kind(dst_win));
	}
	const unsigned int max_try = 2U;
	for (unsigned int tries = 0U; tries < max_try; tries++) {
		try {
			generators_().call(generator, call_bogus_func(infile, model_name, bucket, correct_check, print_sol, use_meter));
			return;
		} catch (gen_mismatch &e) {
			std::cerr << e.what() << std::endl;
			generator = e.suggest_gen();
		}
	}
}

}

/**
 * @brief Parses the command line and executes the command
 * @param argc - command-line argc
 * @param argv - command-line argv
 */
void bit_compress(const char *tool_name, int argc, char **argv)
{
	using std::string;
	using std::cout;
	using std::cerr;
	using std::endl;
	namespace po = boost::program_options;
	po::options_description desc;
	po::variables_map vm;
	namespace chr = std::chrono;

	assert(argc > 1);
	++argv;
	--argc;

	auto caption = join_s("Usage: ", tool_name, " [options]");
	try {
		desc.add_options()
				("input-file,i", po::value<string>()->required(),
				 "File to be compressed")
				("out-file,o", po::value<string>()->required(),
				 "Output file")
				("encoder,e", po::value<string>(),
				 "Picks an encoder. Invoke the encoders command to list available cost models.")
				("emulate,m", po::value<string>(),
				 "Emulate compression. Argument: a file containing the model.")
				("generator,g", po::value<string>(),
				 "Picks the generator. Invoke the gens command to list available generators. Selects the best one by default.")
				("bucket,b", po::value<unsigned int>(),
				 "Buckets the input. Argument in megabytes.")
				("check,c", "Checks if the parsing is correct.")
				("print-sol,p", "Prints the solution on stdout.")
				("progress-bar,z", "Prints the progress bar");
		po::positional_options_description pd;
		pd.add("input-file", 1).add("out-file", 1).add("encoder", 1);

		try {
			po::store(po::command_line_parser(argc, argv).options(desc).positional(pd).run(), vm);
			po::notify(vm);
		} catch (boost::program_options::error &e) {
			throw std::runtime_error(e.what());
		}

		string infile   = vm["input-file"].as<string>();
		string outfile  = infile + ".lzo";
		string gen		= (vm.count("generator") == 1) ? vm["generator"].as<string>() : "";

		if (vm.count("out-file") == 1) {
			outfile     = vm["out-file"].as<string>();
		}

		bool use_encoder = vm.count("encoder") > 0;
		bool use_model   = vm.count("emulate") > 0;
		bool print_sol	 = vm.count("print-sol") > 0;
		bool use_meter	 = vm.count("progress-bar") > 0;
		if (!use_encoder && !use_model) {
			throw std::runtime_error("Need exactly one of encoder and emulated encoder");
		}

		unsigned int bucket_size = 0U;
		if (vm.count("bucket") > 0) {
			bucket_size = vm["bucket"].as<unsigned int>();
		}

		bool correct_check = vm.count("check") > 0;

		auto t_start = chr::high_resolution_clock::now();
		if (use_encoder) {
			string enc_name = vm["encoder"].as<string>();
			call_real(infile, outfile, enc_name, gen, bucket_size, correct_check, print_sol, use_meter);
		} else {
			string model_name = vm["emulate"].as<string>();
			assert(use_model);
			call_bogus(infile, model_name.c_str(), gen, bucket_size, correct_check, print_sol, use_meter);
		}
		auto t_end = chr::high_resolution_clock::now();
		std::cout 	<< "Total running time = " 
					<< chr::duration_cast<chr::seconds>(t_end - t_start).count() 
					<< " seconds." << std::endl;
	} catch (std::runtime_error e) {
		std::stringstream ss;
		ss << e.what() << std::endl;
		ss << caption << std::endl;
		ss << desc << std::endl;
		throw cmd_error(ss.str());
	}
}
