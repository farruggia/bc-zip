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

#include <space_opt_subseq.hpp>

#include <algorithm>
#include <tuple>
#include <map>
#include <limits>
#include <cstdint>

#ifndef NDEBUG
#include <iostream>
#endif

namespace optimal_subseq {

template <typename T = std::uint64_t>
T edge_cost(std::vector<unsigned int> &dsts, unsigned int i, unsigned int j)
{
	// RSA cost
	T rsa_cost = 0;
	if (i != 0) {
		rsa_cost = dsts[j] - dsts[i];
	}
	T buf_cost = (j - i) * static_cast<T>((dsts[j] - dsts[i]));
	return rsa_cost + buf_cost;
}

#ifndef NDEBUG
template <typename opt_t, typename dst_t>
void print_opt(opt_t opt, dst_t dsts)
{
	unsigned int dst_idx = 0;
	for (auto i : opt) {
		if (i.empty()) {
			return;
		}
		std::cout << dsts[dst_idx++] << " = ";
		for (auto j : i) {
			// auto cc = j.first;
			std::uint64_t best, pred, pred_pred;
			std::tie(best, pred, pred_pred) = j.second;
			std::cout << "(" << best << "," << dsts[pred] << "," << dsts[pred_pred] << ")\t";
		}
		std::cout << std::endl;
	}
}
#endif

std::vector<unsigned int> get_seq(std::vector<unsigned int> dsts, std::uint64_t *cost)
{
	dsts.insert(dsts.begin(), 0);
	// Initialize optimal costs
	// IDX -> (cost_class -> (best, pred, pred-pred))
	std::vector<std::map<unsigned int, std::tuple<std::uint64_t, unsigned int, unsigned int>>> opt(dsts.size());
	opt.front()[1] = std::make_tuple(0, 0, 0);

	// Compute optimal table
	for (auto i = 1U; i < dsts.size(); i++) {
		for (auto j = 0U; j < i; j++) {
			auto cost_class = dsts[i] - dsts[j];
			auto jump_cost	= edge_cost(dsts, j, i);
			auto inv_best	= std::numeric_limits<std::uint64_t>::max();
			auto best 		= inv_best;
			unsigned int pred;
			for (auto k : opt[j]) {
				std::uint64_t p_cost; 
				unsigned int p_class, p_pred, p_p_pred;
				p_class = k.first;
				std::tie(p_cost, p_pred, p_p_pred) = k.second;
				auto cost = jump_cost + p_cost;
				bool is_last = i == dsts.size() - 1;
				bool is_double = (dsts[i] == dsts[j] * 2) && p_pred == 0;
				bool is_cc_mul = (cost_class % p_class) == 0 && (cost_class / p_class > 1);
				bool does_impr = cost < best;
				if 	((is_last || is_double || is_cc_mul) && does_impr)
				{
					best = cost;
					pred = p_pred;
				}
			}
			if (best != inv_best) {
				opt[i][cost_class] = std::make_tuple(best, j, pred);
			}
		}
#ifndef NDEBUG
		print_opt(opt, dsts);
		std::cout << "============================================" << std::endl;
#endif
	}

	// Look for optimal solution and return it
	auto opt_cost = std::numeric_limits<std::uint64_t>::max();
	unsigned int pred = 0, p_pred = 0;
	for (auto i : opt.back()) {
		std::uint64_t p_cost;
		unsigned int t_pred, t_p_pred;
		std::tie(p_cost, t_pred, t_p_pred)	= i.second;
		if (p_cost < opt_cost) {
			opt_cost = p_cost;
			pred 	 = t_pred;
			p_pred 	 = t_p_pred;
		}
	}
	std::vector<unsigned int> sol;
	unsigned int pos = dsts.size() - 1;
	while (pos > 0) {
		sol.push_back(pos);
		pos 	= pred;
		pred 	= p_pred;
		for (auto i : opt[pos]) {
			if (std::get<1>(i.second) == pred) {
				p_pred = std::get<2>(i.second);
				break;
			}
		}
	}
	std::reverse(sol.begin(), sol.end());
	for (auto &i : sol) {
		i = dsts[i];
	}
	if (cost != nullptr) {
		*cost = opt_cost;
	}
	return sol;
}

std::vector<unsigned int> get_father(std::vector<unsigned int> dst, std::vector<unsigned int> sol)
{
	std::vector<unsigned int> to_ret;
	// sol.insert(sol.begin(), 0);
	for (auto i : dst) {
		auto found 	= std::lower_bound(sol.begin(), sol.end(), i);
		auto father = std::find(dst.begin(), dst.end(), *found);
		to_ret.push_back(std::distance(dst.begin(), father));
	}
	return to_ret;	
}

std::vector<unsigned int> get_thresholds(
	std::vector<unsigned int> dst, std::vector<unsigned int> sol, 
	std::vector<unsigned int> opt_father
)
{
	std::vector<unsigned int> to_ret;
	sol.insert(sol.begin(), 0);
	for (auto i = 0U; i < dst.size(); i++) {
		to_ret.push_back(sol[opt_father[i]]);
	}
	return to_ret;
}

std::vector<unsigned int> get_costs(
	std::vector<unsigned int> dst, std::vector<unsigned int> sol, 
	std::vector<unsigned int> opt_father
)
{
	std::vector<unsigned int> to_ret;
	for (auto i = sol.size() - 1; i > 0; i--) {
		sol[i] -= sol[i - 1];
	}
	for (auto i = 0U; i < dst.size(); i++) {
		to_ret.push_back(sol[opt_father[i]]);
	}
	return to_ret;
}

std::vector<unsigned int> get_opt_father(
	std::vector<unsigned int> dst, std::vector<unsigned int> sol
)
{
	std::vector<unsigned int> to_ret;
	auto father = get_father(dst, sol);
	auto sel_now = 0;
	for (auto i = 0U; i < dst.size(); i++) {
		to_ret.push_back(sel_now);
		if (i == father[i]) {
			sel_now++;
		}
	}
	return to_ret;
}

}
