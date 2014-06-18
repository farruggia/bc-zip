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

#ifndef __SPACE_OPT_SUBSEQ_HPP
#define __SPACE_OPT_SUBSEQ_HPP
#include <vector>
#include <cstdint>

namespace optimal_subseq {

std::vector<unsigned int> get_seq(std::vector<unsigned int> dsts, std::uint64_t *cost = nullptr);

std::vector<unsigned int> get_father(std::vector<unsigned int> dst, std::vector<unsigned int> sol);

std::vector<unsigned int> get_thresholds(
	std::vector<unsigned int> dst, std::vector<unsigned int> sol, 
	std::vector<unsigned int> father
);

std::vector<unsigned int> get_costs(
	std::vector<unsigned int> dst, std::vector<unsigned int> sol, 
	std::vector<unsigned int> father
);

std::vector<unsigned int> get_opt_father(std::vector<unsigned int> dst, std::vector<unsigned int> sol);

}

#endif