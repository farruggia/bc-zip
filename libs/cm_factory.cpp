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

#include <cm_factory.hpp>

#include <cost_model.hpp>
#include <algorithm>

std::vector<unsigned int> fuse(std::vector<unsigned int> v1, std::vector<unsigned int> v2)
{
	std::vector<unsigned int> to_ret;
	to_ret.insert(to_ret.begin(), ITERS(v1));
	to_ret.insert(to_ret.begin(), ITERS(v2));
	std::sort(ITERS(to_ret));
	auto new_end = std::unique(ITERS(to_ret));
	to_ret.erase(new_end, to_ret.end());
	return to_ret;
}

cost_model fuse(cost_model c1, cost_model c2, double lambda)
{
	// "Fuse" distances and lengths
	std::vector<unsigned int> distances, lengths;
	distances 	= fuse(c1.get_dst(), c2.get_dst());
	lengths 	= fuse(c1.get_len(), c2.get_len());

	// "Fuse" the cost matrix
	cost_matrix cm(distances.size(), lengths.size());
	unsigned int dst_idx = 0;
	for (auto dst : distances) {
		unsigned int len_idx = 0;
		for (auto len : lengths) {
			unsigned int d_1, d_2, l_1, l_2;
			std::tie(d_1, l_1) = c1.get_idx(dst, len);
			std::tie(d_2, l_2) = c2.get_idx(dst, len);
			double cost_1 = c1.get_cost(d_1, l_1);
			double cost_2 = c2.get_cost(d_2, l_2);
			cm(dst_idx, len_idx) = cost_1 + lambda * cost_2;
			++len_idx;
		}
		++dst_idx;
	}

	// "Fuse" lit costs
	double lit_fix = c1.lit_cost(0) + lambda * c2.lit_cost(0);
	double lit_var = c1.lit_cost(1) + lambda * c2.lit_cost(1) - lit_fix;

	// "Fuse" the fixed cost
	double fixed_cost = c1.cost_per_char() + lambda * c2.cost_per_char();

	// Return the cost model
	return cost_model(distances, lengths, cm, lit_fix, lit_var, fixed_cost);
}

cm_factory::cm_factory(cost_model cost, cost_model weight) 
	: cost_(fuse(cost, weight, 0.0)), weight_(fuse(weight, cost, 0.0))
{

}

cost_model cm_factory::cost()
{
	return cost_;
}

cost_model cm_factory::weight()
{
	return weight_;
}

cost_model cm_factory::lambda(double lambda)
{
	return fuse(cost_, weight_, lambda);
}