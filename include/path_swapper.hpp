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

#ifndef __PATH_SWAPPER_HPP
#define __PATH_SWAPPER_HPP

#include <parsing_manage.hpp>
#include <cost_model.hpp>
#include <phrase_reader.hpp>
#include <common.hpp>

#include <cppformat/format.h>

#include <array>
#include <limits>
#include <vector>
#include <tuple>
#include <stdexcept>

class swapper {
protected:
	// Parsings to be swapped
	std::array<parsing, 2> parsings;
	std::array<double, 2> costs;
	std::array<double, 2> weights;

	// Get cost/weight
	cost_model cost_cm;
	cost_model weight_cm;
public:
	swapper(
		parsing p_1, double cost_1, double weight_1,
		parsing p_2, double cost_2, double weight_2,
		cost_model cost_cm, cost_model weight_cm
	)
		: 	parsings({{p_1, p_2}}), costs({{cost_1, cost_2}}), weights({{weight_1, weight_2}}),
			cost_cm(cost_cm), weight_cm(weight_cm)
	{

	}

	virtual std::vector<edge_t> swap(double W, double *cost = nullptr) = 0;

	virtual ~swapper()
	{

	}

};

template <typename enc_t>
class path_swapper : public swapper {
private:
	typedef typename enc_t::decoder decoder_t;

	/**
	 * Returns the (cost,weight) tuple of an edge (d, ell)
	 */
	std::tuple<double, double> edge_costweight(unsigned int d, unsigned int ell)
	{
		if (ell == 0) {
			return std::make_tuple(0.0, 0.0);
		}
		auto edge = cost_cm.get_edge(d, ell);
		return std::make_tuple(cost_cm.edge_cost(edge), weight_cm.edge_cost(edge));
	}

	std::tuple<unsigned int, unsigned int> swap_points(double W, double *cost = nullptr)
	{
		using std::array;
		using std::tuple;
		array<unsigned int, 2> head_costs {{0, 0}}, heads {{0, 0}}, to_process;
		array<double, 2> head_weights {{0, 0}};
		unsigned int swap_sol, swap_point, best_cost;
		swap_sol = swap_point = best_cost = std::numeric_limits<unsigned int>::max();
		array<lzopt::phrase_reader<enc_t>, 2> readers = {{
			lzopt::phrase_reader<enc_t>(parsings[0].begin, parsings[0].orig_len),
			lzopt::phrase_reader<enc_t>(parsings[1].begin, parsings[1].orig_len)
		}};

		// // DEBUG: CHECK IF BASE_PARSINGS ARE DIFFERENT
		// {
		// 	auto p_1 = parsings[0], p_2 = parsings[1];

		// 	auto enc_name = enc_t::name();

		// 	auto r_1 = lzopt::get_phrase_reader(enc_name, p_1.begin, p_1.orig_len);
		// 	auto r_2 = lzopt::get_phrase_reader(enc_name, p_2.begin, p_2.orig_len);

		// 	unsigned int pos = 0U;
		// 	while (!r_1->end() && !r_2->end()) {
		// 		unsigned int d_1, d_2, ell_1, ell_2;
		// 		r_1->next(d_1, ell_1);
		// 		r_2->next(d_2, ell_2);
		// 		if (d_1 != d_2 || ell_1 != ell_2) {
		// 			fmt::print(std::cerr, "OK: pos = {}, len_1 = {}, len_2 = {}\n", pos, ell_1, ell_2);
		// 			break;
		// 		}
		// 		pos += ell_1;
		// 	}
		// }
		// // END DEBUG

		array<edge_t, 2> incoming_edge = {{
			edge_t(0), 
			edge_t(0)
		}};

		array<double, 2> tail_costs = costs;
		array<double, 2> tail_weights = weights;
		
		while ((!readers[0].end()) && (!readers[1].end())) {
			auto swaps = 0u;
			if (heads[0] <= heads[1]) {
				to_process[swaps++] = 0;
			}

			if (heads[1] <= heads[0]) {
				to_process[swaps++] = 1;
			}

			// Perform the actual swap
			for (auto it = to_process.begin(); it != to_process.begin() + swaps; it++) {
				auto s = *it, s_other = (*it + 1) & 0x1;
				auto d_bridge = incoming_edge[s_other].d;
				auto ell_bridge = heads[s_other] - heads[s];
				
				double bridge_cost, bridge_weight;
				std::tie(bridge_cost, bridge_weight) = edge_costweight(d_bridge, ell_bridge);
				
				auto s_w = head_weights[s] + bridge_weight + tail_weights[s_other];
				auto s_c = head_costs[s] + bridge_cost + tail_costs[s_other];

				if (s_w <= W && s_c < best_cost) {
					best_cost = s_c;
					swap_point = heads[s];
					swap_sol = s;
				}
			}

			// Put heads and update costs and old edges
			array<unsigned int, 2> old_heads = heads;
			for (auto it = to_process.begin(); it != to_process.begin() + swaps; it++) {
				auto s = *it, s_other = (*it + 1) & 0x1;
				auto other_incoming_kind = incoming_edge[s_other].kind();
				do {
					std::uint32_t d, ell;
					readers[s].next(d, ell);
					heads[s] += ell;
					double edge_cost, edge_weight;
					std::tie(edge_cost, edge_weight) = edge_costweight(d, ell);
					head_costs[s] += edge_cost; 
					tail_costs[s] -= edge_cost;
					head_weights[s] += edge_weight; 
					tail_weights[s] -= edge_weight;
					incoming_edge[s] = cost_cm.get_edge(d, ell);
				} while (heads[s] <= old_heads[s_other] && other_incoming_kind == REGULAR);
			}
		}
		// Debug
		// fmt::print(std::cerr, "Head 1 = {}, Head 2 = {}\n", heads[0], heads[1]);

		if (swap_sol == std::numeric_limits<unsigned int>::max()) {
			throw std::logic_error("No swap-point found. This is supposed to never happen...");
		}
		if (cost != nullptr) {
			*cost = best_cost;
		}
		return std::make_tuple(swap_sol, swap_point);
	}

	std::vector<edge_t> generate(unsigned int first_idx, unsigned int swap_point)
	{
		auto L = parsings[0].orig_len;
		std::vector<edge_t> to_ret(L + 1);

		std::array<lzopt::phrase_reader<enc_t>, 2> readers = {{
			lzopt::phrase_reader<enc_t>(parsings[0].begin, parsings[0].comp_len),
			lzopt::phrase_reader<enc_t>(parsings[1].begin, parsings[1].comp_len)
		}};

		unsigned int pos = 0, cur_sol = first_idx;
		std::uint32_t d = 0, len = 0;

		while (pos < swap_point) {
			readers[cur_sol].next(d, len);
			assert((d > 0 && len > 0) || (d == 0 && len > 0));
			to_ret[pos] = cost_cm.get_edge(d, len);
			pos += len;
		}

		pos = 0;
		cur_sol = (cur_sol + 1) & 0x1;
		while (pos < swap_point) {
			readers[cur_sol].next(d, len);
			pos += len;
		}

		to_ret[swap_point] = cost_cm.get_edge(d, pos - swap_point);
		
		while (pos < L) {
			readers[cur_sol].next(d, len);
			assert((d > 0 && len > 0) || (d == 0 && len > 0));
			to_ret[pos] = cost_cm.get_edge(d, len);
			pos += len;
		}
		to_ret[L] = cost_cm.get_edge(d, len);
		return to_ret;
	}

public:

	path_swapper(
		parsing p_1, double cost_1, double weight_1,
		parsing p_2, double cost_2, double weight_2,
		cost_model cost_cm, cost_model weight_cm
	)
		: swapper(p_1, cost_1, weight_1, p_2, cost_2, weight_2, cost_cm, weight_cm)
	{

	}


	std::vector<edge_t> swap(double W, double *cost = nullptr)
	{
		// First: get swapping point 
		unsigned int first_idx, swap_position;
		std::tie(first_idx, swap_position) = swap_points(W, cost);

#ifndef NDEBUG
		std::cerr << "[SWAPPER] Index = " << first_idx << ", pos = " << swap_position << std::endl;
#endif

		// Second: generate that solution
		return generate(first_idx, swap_position);
	}
};

std::vector<edge_t> path_swap(
		std::string encoder_name,
		parsing p_1, double cost_1, double weight_1,
		parsing p_2, double cost_2, double weight_2,
		cost_model cost_cm, cost_model weight_cm, double W
);

#endif