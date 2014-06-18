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

class swapper_fact {
private:
	parsing p_1;
	double cost_1;
	double weight_1;
	parsing p_2;
	double cost_2;
	double weight_2;
	cost_model cost_cm;
	cost_model weight_cm;
public:
	swapper_fact(
		parsing p_1, double cost_1, double weight_1,
		parsing p_2, double cost_2, double weight_2,
		cost_model cost_cm, cost_model weight_cm
	)
		: 	p_1(p_1), cost_1(cost_1), weight_1(weight_1), 
			p_2(p_2), cost_2(cost_2), weight_2(weight_2), 
			cost_cm(cost_cm), weight_cm(weight_cm)
	{

	}

	template <typename enc_t>
	std::unique_ptr<swapper> get_instance() const
	{
		return make_unique<path_swapper<enc_t>>(p_1, cost_1, weight_1, p_2, cost_2, weight_2, cost_cm, weight_cm);
	}

};

std::vector<edge_t> path_swap(
		std::string encoder_name,
		parsing p_1, double cost_1, double weight_1,
		parsing p_2, double cost_2, double weight_2,
		cost_model cost_cm, cost_model weight_cm, double W
)
{
	swapper_fact fact(p_1, cost_1, weight_1, p_2, cost_2, weight_2, cost_cm, weight_cm);
	auto swap = encoders_().instantiate<swapper, swapper_fact>(encoder_name, fact);
	return swap->swap(W);
}