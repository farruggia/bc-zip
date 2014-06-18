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

#ifndef __CC_STATS_HPP
#define __CC_STATS_HPP

#include <utilities.hpp>
#include <space_opt_subseq.hpp>

#include <vector>

/**
 * Stats utili per la FSG gen (distanze, classi di costo, inizio generazione, etc.)
 */
class stats_getter {
private:
	std::vector<unsigned int> dst;
	std::vector<unsigned int> len;
	size_t t_len;
public:

	stats_getter(std::vector<unsigned int> dst, std::vector<unsigned int> len, size_t t_len)
		: dst(dst), len(len), t_len(t_len)
	{

	}

	std::vector<unsigned int> get_dst()
	{
		return normalize_dst(dst, t_len);
	}

	std::vector<unsigned int> get_len()
	{
		return len;
	}

	std::vector<unsigned int> get_cost_class()
	{
		return get_cost_classes(get_dst(), t_len);
	}

	std::vector<unsigned int> get_threshold()
	{
		auto to_ret = get_dst();
		to_ret.insert(to_ret.begin(), 0);
		return to_ret;
	}
};

/**
 * Stats utili per la FSG gen (distanze, classi di costo, inizio generazione, etc.) nel caso
 * generalizzato.
 */
class gen_stats_getter {
private:
	size_t t_len;
	std::vector<unsigned int> dst;
	std::vector<unsigned int> len;
	std::vector<unsigned int> opt;
	std::vector<unsigned int> opt_father;
public:

	gen_stats_getter(std::vector<unsigned int> dst_, std::vector<unsigned int> len, size_t t_len)
		: 	t_len(t_len), dst(normalize_dst(dst_, t_len)), len(len), 
			opt(optimal_subseq::get_seq(dst)), opt_father(optimal_subseq::get_opt_father(dst, opt))
	{

	}

	std::vector<unsigned int> get_dst()
	{
#ifndef NDEBUG
		// DEBUG, TOGLIERE!!!
		std::cerr << "Distances requested, printing'em" << std::endl;
		for (auto i : dst) {
			std::cerr << i << "\t";
		}
		std::cerr << std::endl;
#endif

		return dst;
	}

	std::vector<unsigned int> get_len()
	{
		return len;
	}

	std::vector<unsigned int> get_cost_class()
	{
		return optimal_subseq::get_costs(dst, opt, opt_father);
	}

	std::vector<unsigned int> get_threshold()
	{
		return optimal_subseq::get_thresholds(dst, opt, opt_father);
	}

	std::vector<unsigned int> get_opt_dst()
	{
#ifndef NDEBUG
		// DEBUG, TOGLIERE!!!
		std::cerr << "Optimal subseq requested, printing solution" << std::endl;
		for (auto i : opt) {
			std::cerr << i << "\t";
		}
		std::cerr << std::endl;
#endif
		return opt;
	}

	unsigned int map_opt(unsigned int dst_idx)
	{
		return opt_father[dst_idx];
	}

};

#endif
