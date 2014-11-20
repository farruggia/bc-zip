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

#include <bicriteria_compress.hpp>
#include <facilities.hpp>
#include <cmd_parse.hpp>
#include <cm_factory.hpp>
#include <target_read.hpp>
#include <encoders.hpp>
#include <write_parsing.hpp>
#include <solution_getter.hpp>
#include <io.hpp>
#include <parsing_manage.hpp>
#include <solution_integrator.hpp>
#include <path_swapper.hpp>

#include <cppformat/format.h>

#include <stdexcept>
#include <iostream>
#include <boost/program_options.hpp>
#include <cctype>
#include <sstream>
#include <ratio>
#include <list>
#include <chrono>
#include <limits>
#include <cmath>
#include <iomanip>
#include <ostream>

enum bound_type {
	TIME,
	SPACE
};

class fixed_bound;

class bound {
	bound_type bt;
public:

	bound(bound_type bt) : bt(bt)
	{

	}

	virtual fixed_bound get_fixed(double max_w, double min_w) = 0;

	bound_type type()
	{
		return bt;
	}

	virtual std::string name() = 0;

	virtual ~bound()
	{

	}
};

std::string prec_print(unsigned int num, unsigned int den, unsigned int prec)
{
	std::stringstream ss;
	if (num % den == 0) {
		ss << num / den;
	} else {
		ss.precision(prec);
		ss << std::fixed << 1.0 * num / den;
	}
	return ss.str();
}

unsigned int kilo_num = 1024;
unsigned int mega_num = 1048576;

class fixed_bound : public bound {
private:
	// nanoseconds / bytes
	double fix_bound;

	std::string space_name(unsigned int value)
	{
		if (value < kilo_num) {
			return join_s(value, "B");
		} else if (value < mega_num) {
			return join_s(prec_print(value, 8 * kilo_num, 2), "KB");
		} else {
			return join_s(prec_print(value, 8 * mega_num, 2), "MB");
		}
	}

	std::string time_name(unsigned int value)
	{
		if (value < std::giga::num) {
			return join_s(value / std::mega::num, "msec");
		} else {
			return join_s(prec_print(value, std::giga::num, 2), "sec");
		}
	}

public:
	fixed_bound(bound_type bt, double fix_bound) : bound(bt), fix_bound(fix_bound)
	{

	}

	fixed_bound get_fixed(double, double)
	{
		return *this;
	}

	double get_bound()
	{
		return fix_bound;	
	}

	std::string name()
	{
		switch (type()) {
		case SPACE:
			return space_name(fix_bound);
		default:
			return time_name(fix_bound);
		}
	}
};

class relative_bound : public bound {
private:
	double c_level;
public:
	relative_bound(bound_type bt, double c_level) : bound(bt), c_level(c_level)
	{

	}

	fixed_bound get_fixed(double max, double min)
	{
		return fixed_bound(type(), min + c_level * (max - min));
	}

	std::string name()
	{
		switch (type()) {
		case SPACE:
			return join_s(c_level, "S");
		default:
			return join_s(c_level, "T");
		}
	}
};

std::vector<std::string> &split(const std::string &s, char delim, std::vector<std::string> &elems) {
	std::stringstream ss(s);
	std::string item;
	while (bool(std::getline(ss, item, delim))) {
		if (!item.empty()){
			elems.push_back(item);
		}
	}
    return elems;
}


std::vector<std::string> split(const std::string &s, char delim) {
    std::vector<std::string> elems;
    split(s, delim, elems);
    return elems;
}

void add_bounds(std::vector<std::shared_ptr<bound>> &bounds, std::string param)
{
	auto bounds_specifiers = split(param, ',');
	for (auto i : bounds_specifiers) {
		char kind = i.back();
		auto value = std::stod(i.substr(0, i.size() - 1));
		switch(kind) {
		case 'm':
			bounds.push_back(std::make_shared<fixed_bound>(TIME, value * std::nano::den / std::milli::den));
			break;
		case 's':
			bounds.push_back(std::make_shared<fixed_bound>(TIME, value * std::nano::den));
			break;
		case 'K':
			bounds.push_back(std::make_shared<fixed_bound>(SPACE, 8 * value * kilo_num));
			break;
		case 'M':
			bounds.push_back(std::make_shared<fixed_bound>(SPACE, 8 * value * mega_num));
			break;
		default:
			throw std::logic_error("No unit specifier in bound");
		}
	}	
}

void add_level(std::vector<std::shared_ptr<bound>> &bounds, std::string param)
{
	auto level_specifiers = split(param, ',');
	for (auto i : level_specifiers) {
		char kind = std::tolower(i.back());
		auto value = std::stod(i.substr(0, i.size() - 1));
		switch(kind) {
		case 's':
			bounds.push_back(std::make_shared<relative_bound>(SPACE, value));
			break;
		case 't':
			bounds.push_back(std::make_shared<relative_bound>(TIME, value));
			break;
		default:
			throw std::logic_error("No kind specifier in bound");
		}
	}	
}

class callable {
public:
	virtual void call() = 0;
	
	virtual ~callable()
	{

	}
};

enum gen_type {
	SPACE_OPT,
	TIME_OPT,
	LAMBDA
};

typedef std::tuple<std::string, std::string> gen_info_t;

class solution_info {
private:
	double space;
	double time;
	bool dual_model;
	cost_model cm_1;
	cost_model cm_2;
public:
	solution_info()
	{

	}

	solution_info(double space, double time, cost_model cm)
		: space(space), time(time), dual_model(false), cm_1(cm)
	{

	}

	solution_info(double space, double time, cost_model cm_1, cost_model cm_2)
		: space(space), time(time), dual_model(true), cm_1(cm_1), cm_2(cm_2)
	{

	}

	double get_space() const
	{
		return space;
	}

	double get_time() const
	{
		return time;
	}

	std::tuple<double, double> get() const
	{
		return std::make_tuple(space, time);
	}

	gen_info_t get_gen_info() const
	{
		return std::make_tuple(cm_1.id(), cm_2.id());
	}

	template <typename sol_get_t>
	std::vector<edge_t> generate(sol_get_t &getter) const
	{
		if (dual_model) {
			return getter.fast(cm_1, cm_2);
		}
		return getter.fast(cm_1);
	}
};

std::ostream &operator<<(std::ostream &stream, const gen_info_t &gen)
{
	std::string s1, s2;
	std::tie(s1, s2) = gen;
	char hex_1[42], hex_2[42];
	std::fill(hex_1, hex_1 + 42, '\0');
	std::fill(hex_2, hex_2 + 42, '\0');
	const unsigned char *ptr_1 = reinterpret_cast<const unsigned char *>(s1.c_str());
	const unsigned char *ptr_2 = reinterpret_cast<const unsigned char *>(s2.c_str());

	if (!s1.empty()) {
		sha1::toHexString(ptr_1, hex_1);		
	}
	if (!s2.empty()) {
		sha1::toHexString(ptr_2, hex_2);		
	}
	stream << "(" << hex_1;
	if (!s2.empty()) {
		stream << "," << hex_2;
	}
	stream << ")";

	return stream;
}

std::ostream& operator<< (std::ostream& stream, const solution_info &si)
{

	// stream.imbue(std::locale(""));
	stream.imbue(std::locale("en_US.UTF-8")); // Leaks!
	stream.precision(2);

	auto space_kb = si.get_space() / (8 * kilo_num);
	std::chrono::duration<double, std::nano> time_nano(si.get_time());
	auto time_msecs = std::chrono::duration_cast<std::chrono::milliseconds>(time_nano).count();
	fmt::print(stream, "S = {0:.0f} ({1}KB), T = {2:.8f} ({3}ms)", si.get_space(), space_kb, si.get_time(), time_msecs);
	// stream << std::fixed << "S = " << space_kb << "KB, T = " << time_msecs << "msecs";// << ", ID = " << si.get_gen_info();
	return stream;
}

struct cost_weight {
	double cost;
	double weight;

	std::tuple<double, double> get() const
	{
		return std::make_tuple(cost, weight);
	}
};

class cw_factory {
private:
	bool space_is_cost;
public:
	cw_factory(bool space_is_cost) : space_is_cost(space_is_cost)
	{

	}

	cost_weight get(double space, double time) const
	{
		if (space_is_cost) {
			return { space, time };
		}
		return {time, space};
	}

	cost_weight get(const solution_info &si) const
	{
		double space, time;
		std::tie(space, time) = si.get();
		return get(space, time);
	}
};

struct copy_compressed_file {
	std::shared_ptr<byte> data;
	// In bits!
	size_t total_size;
	// In bits!
	size_t parsing_size;

	copy_compressed_file()
	{
		
	}

	copy_compressed_file(compressed_file &&to_copy)
		: 	data(to_copy.data.release(), std::default_delete<byte[]>()),
			total_size(to_copy.total_size), parsing_size(to_copy.parsing_size)
	{

	}
};

class compressed_cache {
private:
	std::list<std::tuple<solution_info, copy_compressed_file>> cache_list;
	size_t capacity;

	bool remove_latest(cw_factory cwf, double W, bool feasible)
	{
		for (auto it = cache_list.begin(); it != cache_list.end(); it++) {
			double weight = cwf.get(std::get<0>(*it)).weight;
			if ((weight <= W) == feasible) {
#ifndef NDEBUG
				std::cerr << "[CACHE] - " << std::get<0>(*it) << std::endl;;
#endif
				cache_list.erase(it);
				return true;
			}
		}
		return false;
	}

public:
	compressed_cache(size_t capacity) : capacity(capacity)
	{

	}

	void add(solution_info si, copy_compressed_file comp, cw_factory cwf, double W)
	{
		if (cache_list.size() >= capacity) {
			auto weight = cwf.get(si).weight;
			if (!remove_latest(cwf, W, weight <= W)) {
#ifndef NDEBUG
				std::cerr << "[CACHE] - " << std::get<0>(cache_list.back()) << std::endl;
#endif	
				cache_list.pop_front();
			}
		}
#ifndef NDEBUG
		std::cerr << "[CACHE] + " << si << std::endl;
#endif
		assert(cache_list.size() < capacity);
		cache_list.push_back(std::make_tuple(si, comp));
	}

	bool get(gen_info_t gen_info, copy_compressed_file &cc)
	{
#ifndef NDEBUG
		std::cerr << "[CACHE] R:" << gen_info << std::endl;
#endif
		for (auto i : cache_list) {
			gen_info_t this_gi = std::get<0>(i).get_gen_info();
#ifndef NDEBUG
			std::cerr << "[CACHE] Considering " << this_gi << std::endl;
#endif
			if (this_gi == gen_info) {
#ifndef NDEBUG
				std::cerr << "[CACHE] Returning it" << std::endl;
#endif
				cc = std::get<1>(i);
				return true;
			}
		}
		return false;
	}
};

class solution_dual {
private:
	double cost;
	double weight;
public:

	solution_dual()
		: cost(std::numeric_limits<double>::max()), weight(std::numeric_limits<double>::max())
	{

	}

	solution_dual(double cost, double weight, double W)
		: cost(cost), weight(weight - W)
	{

	}

	solution_dual(const solution_info &si, cw_factory cwf, double W)
	{
		cost_weight cw = cwf.get(si);
		cost = cw.cost;
		weight = cw.weight - W;
	}

	double value(double lambda)
	{
		return cost + lambda * weight;
	}

	bool does_intersect(solution_dual s_2)
	{
		return weight != s_2.weight;
	}

	std::tuple<double, double> intersect(solution_dual s_2)
	{
		if (!does_intersect(s_2)) {
			throw std::logic_error("Intersection of parallel lines requested");
		}
		double lambda = (cost - s_2.cost) / (s_2.weight - weight);
		lambda = std::max<double>(0.0, lambda);
		double value_  = value(lambda);
		return std::make_tuple(lambda, value_);
	}

	bool feasible()
	{
		return weight <= 0;
	}
};

class dual_basis {
private:
	cw_factory cwf;
	std::tuple<solution_info, solution_dual> left;
	std::tuple<solution_info, solution_dual> right;
	double W;

	void update(
		std::tuple<solution_info, solution_dual> new_left, 
		std::tuple<solution_info, solution_dual> new_right
	)
	{
		double new_lambda, new_cost;
		solution_dual s_left = std::get<1>(new_left);
		solution_dual s_right = std::get<1>(new_right);
		if (!s_left.does_intersect(s_right)) {
			std::cerr << "Ooops, this should never happen..." << std::endl;
			return;
		}
		std::tie(new_lambda, new_cost) = s_left.intersect(s_right);
		double lambda, cost;
		std::tie(lambda, cost) = current();
		if (new_cost <= cost) {
			left = new_left;
			right = new_right;
		}
	}

public:
	dual_basis(cw_factory cwf, solution_info cost_opt, solution_info weight_opt, double W) 
		: cwf(cwf), W(W)
	{
		left = std::make_tuple(cost_opt, solution_dual(cost_opt, cwf, W));
		right = std::make_tuple(weight_opt, solution_dual(weight_opt, cwf, W));
	}

	std::tuple<double, double> current()
	{
		return std::get<1>(left).intersect(std::get<1>(right));
	}

	double lower_envelope(double lambda)
	{
		return std::min(
			std::get<1>(left).value(lambda),
			std::get<1>(right).value(lambda)
		);
	}

	std::tuple<double, double> update(solution_info si)
	{
		solution_dual sd(si, cwf, W);
		auto candidate = std::make_tuple(si, sd);
		if (sd.feasible()) {
			update(left, candidate);
		} else {
			update(candidate, right);
		}
		return current();
	}

	std::tuple<solution_info, solution_info> get_basis() const
	{
		return std::make_tuple(std::get<0>(left), std::get<0>(right));
	}


};

std::ostream &operator<<(std::ostream &stream, const dual_basis &basis)
{
	solution_info left, right;
	std::tie(left, right) = basis.get_basis();
	fmt::print(stream, "Left = {}\nRight = {}", left, right);
	return stream;
}

namespace Color {
    enum Code {
        FG_RED      = 31,
        FG_GREEN    = 32,
        FG_YELLOW	= 33,
        FG_BLUE     = 34,
        FG_DEFAULT  = 39,
        BG_RED      = 41,
        BG_GREEN    = 42,
        BG_BLUE     = 44,
        BG_DEFAULT  = 49,
        BOLD		= 1,
        RESET 		= 0
    };
    class Modifier {
        Code code;
    public:
        Modifier(Code pCode) : code(pCode) {}
        friend std::ostream&
        operator<<(std::ostream& os, const Modifier& mod) {
            return os << "\033[" << mod.code << "m";
        }
    };
}

template <typename enc_t, typename sol_getter_t>
class bicriteria_compressor {
private:
	sa_cacher sc;
	text_info to_compress;
	sol_getter_t &sg;
 	cost_model space_cm;
	cost_model time_cm;
	std::map<gen_info_t, solution_info> sol_cache;
	compressed_cache comp_cache;
	solution_integrator<> si;

	compressed_file get_unique_comp(const std::vector<edge_t> &sol, double &space_length, double &time)
	{
		// Gets the parsing_length
		auto p_len = parsing_length<size_t>(ITERS(sol), space_cm);
		// Compress it
		auto comp = write_parsing<enc_t>(sol, p_len, to_compress);
		space_length = p_len;
		time = parsing_length<double>(ITERS(sol), time_cm);
		return comp;		
	}

	copy_compressed_file get_comp(const std::vector<edge_t> &sol, double &space_length, double &time)
	{
		return copy_compressed_file(get_unique_comp(sol, space_length, time));
	}

	gen_info_t get_gen_info(cost_model cm_1, cost_model cm_2 = cost_model())
	{
		return std::make_tuple(cm_1.id(), cm_2.id());
	}

	/** Those solutions automatically append to the caches */
	solution_info optimal(cost_model cm_1, cost_model cm_2, bool feasible)
	{
		// See if it's cached
		auto gen_info = get_gen_info(cm_1, cm_2);
		if (sol_cache.count(gen_info) == 0) {
			// Gets the solution
			auto sol = sg.fast(cm_1, cm_2);
			// Compress that solution and put it into the caches
			double space, time;
			auto compressed = get_comp(sol, space, time);
			solution_info si(space, time, cm_1, cm_2);
			assert(si.get_gen_info() == gen_info);
			double fake_W = feasible ? std::numeric_limits<double>::max() : 0.0;
			comp_cache.add(si, compressed, cw_factory(false), fake_W);
			sol_cache[gen_info] = si;
		}
		auto to_ret = sol_cache[gen_info];
#ifndef NDEBUG
		std::cerr 	<< "Requested dual-optimal solution; " << to_ret << std::endl;
#endif
		return to_ret;
	}

	solution_info optimal(cost_model cm, cw_factory cwf, double W)
	{
		auto gen_info = get_gen_info(cm);
		if (sol_cache.count(gen_info) == 0) {
			// Gets the solution
			auto sol = sg.fast(cm);
			// Compress the solution and put it into the caches
			double space, time;
			auto compressed = get_comp(sol, space, time);
			solution_info si(space, time, cm);
			assert(si.get_gen_info() == gen_info);
			comp_cache.add(si, compressed, cwf, W);
			sol_cache[gen_info] = si;
		}
		auto to_ret = sol_cache[gen_info];
#ifndef NDEBUG
		std::cerr 	<< "Requested dual-optimal solution; " << to_ret << std::endl;
#endif
		return to_ret;
	}

	dual_basis get_basis(cw_factory cwf, solution_info cost_opt, solution_info weight_opt, double W)
	{
		dual_basis to_ret(cwf, cost_opt, weight_opt, W);
		for (auto i : sol_cache) {
			auto si = i.second;
			to_ret.update(si);
		}
		return to_ret;
	}

	compressed_file writable_solution(cost_model cm, size_t *space_ptr = nullptr, double *time_ptr = nullptr)
	{
		// Get a full-fledged solution
		auto sol = sg.full(cm);
		// Compress it 
		double space, time;
		auto to_ret = get_unique_comp(sol, space, time);
		if (space_ptr != nullptr) {
			*space_ptr = parsing_length<size_t>(ITERS(sol), space_cm);
		}
		if (time_ptr != nullptr) {
			*time_ptr = parsing_length<double>(ITERS(sol), time_cm);
		}

		return to_ret;
	}

	copy_compressed_file writable_parsing(solution_info si)
	{
		auto sol = si.generate(sg);
		double space, time;
		return get_comp(sol, space, time);
	}

	std::vector<shared_parsing> writable_parsings(solution_info s1, solution_info s2)
	{
		copy_compressed_file cc_1, cc_2;
		auto found_1 = comp_cache.get(s1.get_gen_info(), cc_1);
		auto found_2 = comp_cache.get(s2.get_gen_info(), cc_2);
		if (!found_1) {
			std::cerr << "WARNING: left not cached!" << std::endl;
			cc_1 = writable_parsing(s1);
		}
		if (!found_2) {
			std::cerr << "WARNING: right not cached!" << std::endl;
			cc_2 = writable_parsing(s2);
		}

		// We must integrate those parsings
		auto p_1 = get_parsing(cc_1.data.get(), cc_1.total_size);
		auto p_2 = get_parsing(cc_2.data.get(), cc_2.total_size);
		parsing n_1, n_2;
		auto d_1 = dup_parsing(p_1, n_1);
		auto d_2 = dup_parsing(p_2, n_2);
		std::vector<parsing> in = {p_1, p_2};
		std::vector<parsing> out = {n_1, n_2};
		integrate<enc_t>(in, out, si);
		return {
			shared_parsing(d_1, n_1.comp_len, n_1.orig_len), 
			shared_parsing(d_2, n_2.comp_len, n_2.orig_len)
		};
	}

	copy_compressed_file cached_solution(solution_info si)
	{
		copy_compressed_file cc;
		auto found = comp_cache.get(si.get_gen_info(), cc);
		if (!found) {
			cc = si.generate(sg);
		}
		return cc;
	}

	solution_integrator<> get_si()
	{
		return solution_integrator<>(gen_ffsg_fact(to_compress, sc), space_cm);
	}

	cost_model fuse_cm(cost_model to_fuse, cost_model fused_with)
	{
		cm_factory cmf(to_fuse, fused_with);
		return cmf.cost();
	}

	std::tuple<double, double> max_cost_weight(cw_factory cwf)
	{
		auto max_dst = space_cm.get_dst().back();
		auto max_len = space_cm.get_len().back();
		auto max_id = space_cm.get_id(max_dst, max_len);
		edge_t heaviest_edge(max_dst, max_len, max_id);
		auto max_space = space_cm.edge_cost(heaviest_edge);
		auto max_time = time_cm.edge_cost(heaviest_edge);
		auto max_cw = cwf.get(max_space, max_time);
		return max_cw.get();
	}

	std::vector<edge_t> path_swap(
		shared_parsing left, solution_info left_si, 
		shared_parsing right, solution_info right_si,
		double W, cw_factory cwf, cm_factory cmf, double *cost = nullptr
	)
	{
		double max_cost, max_weight;
		std::tie(max_cost, max_weight) = max_cost_weight(cwf);
		W += 2 * max_weight;

		double space_1, time_1, space_2, time_2;
		std::tie(space_1, time_1) = left_si.get();
		std::tie(space_2, time_2) = right_si.get();
		double cost_1, weight_1, cost_2, weight_2;
		std::tie(cost_1, weight_1) = cwf.get(space_1, time_1).get();
		std::tie(cost_2, weight_2) = cwf.get(space_2, time_2).get();

		path_swapper<enc_t> swapper(
			left.get_parsing(), cost_1, weight_1,
			right.get_parsing(), cost_2, weight_2,
			cmf.cost(), cmf.weight()
		);

		return swapper.swap(W, cost);
	}

	shared_parsing shared_parsing_from_pack(pack_info &&pi)
	{
		std::shared_ptr<byte> shared_parsing_ptr(pi.parsing.release(), std::default_delete<byte[]>());
		byte *ptr = shared_parsing_ptr.get();
		std::string enc_name;
		size_t orig_length;
		byte *start;
		std::tie(enc_name, orig_length, start) = unpack(ptr);
		assert(enc_name == enc_t::name());
		auto comp_len = pi.data_len - (start - ptr);
		return shared_parsing(shared_parsing_ptr, ptr, comp_len, orig_length);
	}

public:
	bicriteria_compressor(
		text_info to_compress, sol_getter_t &sg,
		cost_model space_cm, cost_model time_cm, size_t cache_size = 3
	) : to_compress(to_compress), sg(sg), space_cm(fuse_cm(space_cm, time_cm)), 
		time_cm(fuse_cm(time_cm, space_cm)), comp_cache(cache_size), si(get_si())
	{

	}

	compressed_file run(std::shared_ptr<bound> bound_cmp, bool correct_check, size_t *space, double *time = nullptr)
	{
		// Instantiate the factories
		cw_factory cwf(bound_cmp->type() == TIME);
		cm_factory cmf;
		if (bound_cmp->type() == TIME) {
			// Bound on time: time is weight, space is cost
			cmf = cm_factory(space_cm, time_cm);
		} else {
			// Bound on space: space is weight, time is cost
			cmf = cm_factory(time_cm, space_cm);
		}

		Color::Modifier red(Color::FG_RED);
		Color::Modifier green(Color::FG_GREEN);
		Color::Modifier yellow(Color::FG_YELLOW);
		Color::Modifier def(Color::RESET);
		Color::Modifier bold(Color::BOLD);


		// Obtain cost-optimal solution
		std::cout << "Getting cost-optimal solution" << std::endl;
		// auto t_1 = std::chrono::high_resolution_clock::now();
		// auto sol_info_cost = optimal(cmf.cost(), cmf.weight(), false);
		// auto t_2 = std::chrono::high_resolution_clock::now();

		solution_info sol_info_cost, sol_info_weight;
		std::chrono::seconds::rep measured_time;

		std::tie(measured_time, sol_info_cost) = measure<std::chrono::seconds>::execution([&]{
			return optimal(cmf.cost(), cmf.weight(), false);
		});
		fmt::print("Elapsed time = {}{}{} secs{}\n", bold, yellow, measured_time, def);
		fmt::print("Cost-optimal = {}\n", sol_info_cost);

		// std::cout << "Elapsed time = " 
		//   << bold << yellow << std::chrono::duration_cast<std::chrono::seconds>(t_2 - t_1).count() << " secs" 
		//   << def << std::endl;


		std::cout << "Getting weight-optimal solution" << std::endl;
		// t_1 = std::chrono::high_resolution_clock::now();
		// auto sol_info_weight = optimal(cmf.weight(), cmf.cost(), true);
		// t_2 = std::chrono::high_resolution_clock::now();

		std::tie(measured_time, sol_info_weight) = measure<std::chrono::seconds>::execution([&]{
			return optimal(cmf.weight(), cmf.cost(), true);
		});
		fmt::print("Elapsed time = {}{}{} secs{}\n", bold, yellow, measured_time, def);
		fmt::print("Weight-optimal = {}\n", sol_info_weight);

		// std::cout << "Elapsed time = " 
		//   << bold << yellow << std::chrono::duration_cast<std::chrono::seconds>(t_2 - t_1).count() << " secs" 
		//   << def << std::endl;

		double min_weight = cwf.get(sol_info_weight).weight;
		double max_weight = cwf.get(sol_info_cost).weight;
		auto fix_bound = bound_cmp->get_fixed(max_weight, min_weight);
		double W = fix_bound.get_bound();

		std::cout.imbue(std::locale("en_US.UTF-8")); // Leaks!
		fmt::print("Setting W = {0}{1}{2:.2f}{3} ({4})\n", bold, green, W, def, fix_bound.name());

		// std::cout.precision(2);
		// std::cout << "Setting W = " << std::fixed << bold << green << W << def << " ("  << fix_bound.name() << ")" << std::endl;

		// Early returns
		if (W >= max_weight) {
			return writable_solution(cmf.cost(), space, time);
		} else if (W == min_weight) {
			return writable_solution(cmf.weight(), space, time);
		} else if (W < min_weight){
			throw std::logic_error("Bound less than weight-optimal solution, problem is unfeasible");
		}

		// Initialize the basis
		dual_basis basis = get_basis(cwf, sol_info_cost, sol_info_weight, W);

		std::cout << basis << std::endl;

		// Find the optimal, dual basis
		const double eps = 1e-6;
		double phi_b, phi_bp, delta;

		do {
			double lambda;
			// (1) Find optimal (λ, φ)
			std::tie(lambda, phi_b) = basis.current();

			// (2) Solve for λ
			solution_info si;
			std::tie(measured_time, si) = measure<std::chrono::seconds>::execution([&]{
				return optimal(cmf.lambda(lambda), cwf, W);
			});

			// (3) Update basis
			basis.update(si);

			// (4) Evaluate basis at current λ
			phi_bp = basis.lower_envelope(lambda);
			delta = std::abs(phi_b - phi_bp) / phi_bp;


			fmt::print("λ = {0}, φ = {1}{2}{3:.12f}{4}, φ' = {5}{6}{7:.12f}{8}, Δ = {9:.9f}\n",lambda, bold, green, phi_b, def, bold, green, phi_bp, def, delta);
			std::cout << basis << std::endl;
			fmt::print("Iteration time time = {}\n", measured_time, si);
		} while (delta > eps);

		// Integrate the basis
		std::cout << "Integrating base" << std::endl;
		solution_info left, right;
		std::tie(left, right) = basis.get_basis();

		// t_1 = std::chrono::high_resolution_clock::now();
		// auto base_parsings = writable_parsings(left, right);
		// t_2 = std::chrono::high_resolution_clock::now();

		std::vector<shared_parsing> base_parsings;
		std::tie(measured_time, base_parsings) = measure<std::chrono::seconds>::execution([&]{
			return writable_parsings(left, right);
		});

		fmt::print("Elapsed time = {}{}{} secs{}\n", bold, yellow, measured_time, def);

		// Path-swap
		std::cout << "Swapping the base" << std::endl;
		auto t_1 = std::chrono::high_resolution_clock::now();
		auto swapped_sol = path_swap(base_parsings[0], left, base_parsings[1], right, W, cwf, cmf);
		auto t_2 = std::chrono::high_resolution_clock::now();
		measured_time = std::chrono::duration_cast<std::chrono::seconds>(t_2 - t_1).count();

		fmt::print("Elapsed time = {}{}{} secs{}\n", bold, yellow, measured_time, def);

		if (correct_check) {
			correctness_report report = check_correctness(swapped_sol, to_compress.text.get());
			if (!report.correct) {
				throw std::logic_error(join_s(
					"Incorrect parsing: position ", report.error_position,
					", distance ", report.error_d,
					", length ", report.error_ell
				));
			}
		}

		if (space != nullptr) {
			*space = parsing_length<size_t>(ITERS(swapped_sol), space_cm);
		}
		if (time != nullptr) {
			*time = parsing_length<double>(ITERS(swapped_sol), time_cm);
		}
		if (space != nullptr && time != nullptr) {
			solution_info dsi(*space, *time, cost_model());
			auto final_cost = cwf.get(dsi).cost;
			fmt::print("Optimal Δ = {0:.9f} abs, {1:.9f} rel\n", final_cost - phi_bp, (final_cost - phi_bp) / phi_bp);
			// Get maximum space cost
			auto max_dst 	= space_cm.get_dst().back();
			auto max_len 	= space_cm.get_len().back();
			auto max_cost 	= space_cm.edge_cost(space_cm.get_edge(max_dst, max_len));
			fmt::print("Ratio on theoretical maximum error = {0:.2f}\n", (final_cost - phi_bp) / max_cost);
			fmt::print("Ratio on ε = {0:.2f}\n", (final_cost - phi_bp) / eps);
		}

		// Compress and return it
		return  write_parsing(swapped_sol, to_compress, enc_t::name(), space_cm);
	}

};

template <typename enc_t>
class bicriteria_call : public callable {
private:
	std::string infile;
	std::string target;
	std::vector<std::shared_ptr<bound>> bounds;
	bool check_correct;
	bool progress_bar;

	template <typename bicriteria_compressor_t>
	void run(bicriteria_compressor_t &compressor)
	{
		for (auto i : bounds) {
			// Obtain the compressed file
			std::cout << "Compressing " << infile << " with " << i->name() << std::endl;
			size_t space;
			double time;
			auto t1 = std::chrono::high_resolution_clock::now();
			auto comp = compressor.run(i, check_correct, &space, &time);
			auto t2 = std::chrono::high_resolution_clock::now();
			// Write it on disk
			std::string file_name = infile + "#" + enc_t::name() + "#" + i->name() + ".lzo";
			std::ofstream out_file;
			open_file(out_file, file_name.c_str());
			write_file<byte>(out_file, comp.data.get(), static_cast<std::streamsize>(comp.total_size));

			std::cout << "Length = " << space << " bits" << std::endl;
			std::chrono::duration<double, std::nano> time_chrono(time);
			std::cout << "Time = " << std::chrono::duration_cast<std::chrono::milliseconds>(time_chrono).count() << " msec" << std::endl;
			std::cout << "Raw Time = " << std::fixed << time << std::endl;
			std::cout << "Total compression time = " << std::chrono::duration_cast<std::chrono::seconds>(t2 - t1).count() << " secs" << std::endl;
			std::cout << "Compressed size = " << comp.parsing_size << " bytes" << std::endl;
		}		
	}

public:
	bicriteria_call(
		std::string infile, std::string target, std::vector<std::shared_ptr<bound>> bounds, 
		bool check_correct, bool progress_bar
	) : infile(infile), target(target), bounds(bounds), 
		check_correct(check_correct), progress_bar(progress_bar)
	{

	}

	void call()
	{
		// TODO: build arguments for bicriteria_compress

		// Obtain cost_models factory
		auto enc_name = enc_t::name();
		auto space_cm = encoders_().get_cm(enc_name);
		auto time_cm = get_wm(target.c_str(), enc_name.c_str());

		// Obtain the text_info
		size_t size;
		auto file = read_file<byte>(infile.c_str(), &size);
		text_info ti(file.release(), size);

		// Obtain the solution_getter parameters
		auto lit_win = enc_t::encoder::get_literal_len();
		// solution_getter<observer_t>(text_info ti, size_t literal_window)

		if (progress_bar) {
			solution_getter<fsg_meter> sg(ti, lit_win);
			bicriteria_compressor<enc_t, solution_getter<fsg_meter>> compressor(ti, sg, space_cm, time_cm);
			run(compressor);
		} else {
			solution_getter<> sg(ti, lit_win);
			bicriteria_compressor<enc_t, solution_getter<>> compressor(ti, sg, space_cm, time_cm);
			run(compressor);
		}
	}
};

class caller_factory {
private:
	std::string infile;
	std::string target;
	std::vector<std::shared_ptr<bound>> bounds;
	bool check_correct;
	bool progress_bar;
public:
	caller_factory(
		std::string infile, std::string target, std::vector<std::shared_ptr<bound>> bounds, 
		bool check_correct, bool progress_bar
	) : infile(infile), target(target), bounds(bounds), 
		check_correct(check_correct), progress_bar(progress_bar)
	{

	}

	template <typename enc_t>
	std::unique_ptr<callable> get_instance() const
	{
		return make_unique<bicriteria_call<enc_t>>(infile, target, bounds, check_correct, progress_bar);
	}

};

void bicriteria_compress(char *tool_name, int argc, char **argv)
{
	using std::string;
	namespace po = boost::program_options;

	po::options_description desc;
	po::variables_map vm;

	assert(argc > 1);
	++argv;
	--argc;

	auto caption = join_s("Usage: ", tool_name, " [options]");
	try {
		desc.add_options()
				("input-file,i", po::value<string>()->required(),
				 "File to be compressed")
				("encoder,e", po::value<string>()->required(),
				 "Picks an encoder. Invoke the encoders command to list available cost models.")
				("target,t", po::value<string>()->required(),
				 "Specify the target machine model.")
				("bound,b", po::value<string>(),
				 "Specify a bound (append \"m\" for milliseconds, \"k\" for kilobytes")
				("level,l", po::value<string>(),
				 "Specify a compression level (float in range [0,1])")
				("check,c", "Checks if the parsing is correct.")
				// ("print-sol,p", "Prints the solution on stdout.")
				("progress-bar,z", "Prints the progress bar");
		po::positional_options_description pd;
		pd.add("input-file", 1);

		try {
			po::store(po::command_line_parser(argc, argv).options(desc).positional(pd).run(), vm);
			po::notify(vm);
		} catch (boost::program_options::error &e) {
			throw std::runtime_error(e.what());
		}

		string infile   = vm["input-file"].as<string>();

		// bool print_sol	 = vm.count("print-sol") > 0;
		bool use_meter	 = vm.count("progress-bar") > 0;
		bool correct_check = vm.count("check") > 0;
		string enc_name = vm["encoder"].as<string>();
		string target	= vm["target"].as<string>();

		bool bound_specified = false;
		// Vector of bounds
		std::vector<std::shared_ptr<bound>> bounds;
		if (vm.count("bound") > 0) {
			add_bounds(bounds, vm["bound"].as<std::string>());
			bound_specified = true;
		}
		if (vm.count("level") > 0) {
			add_level(bounds, vm["level"].as<std::string>());
			bound_specified = true;
		}
		if (!bound_specified) {
			throw std::runtime_error("No bounds specified, exiting");
		}

		// Call the function
		caller_factory cf(infile, target, bounds, correct_check, use_meter);
		encoders_().instantiate<callable, caller_factory>(enc_name, cf)->call();

	} catch (std::runtime_error e) {
		std::stringstream ss;
		ss << e.what() << std::endl;
		ss << caption << std::endl;
		ss << desc << std::endl;
		throw cmd_error(ss.str());
	}
}
