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

#ifndef RIGHTMOST_FSG_HPP
#define RIGHTMOST_FSG_HPP

#include <base_fsg.hpp>

template <typename inner_gen>
class rm_protocol {
	fsg_protocol<inner_gen> gen;
	std::vector<edge_t> max_edge;
	std::vector<edge_t> *inner_edges;
	int next;
public:
	template <typename T>
	rm_protocol(T &&gen, size_t t_len, const std::vector<unsigned int> &dst,
				const std::vector<unsigned int> &len)
		: gen(fsg_protocol<inner_gen>(std::forward<T>(gen), t_len, dst, len)), max_edge(1), inner_edges(&this->gen.get_edges()), next(1U)
	{

	}

	inline bool gen_next(std::uint32_t *generated)
	{
		std::uint32_t gen_now;
		bool still_left = gen.gen_next(&gen_now);

		if (!still_left) {
			return false;
		}

		if (--next > 0 || gen_now == 0U) {
			next = std::max(0, next);
			*generated = 0U;
			return true;
		}

		edge_t &candidate = max_edge.front();
		candidate = edge_t(0U);
		for (auto i = 0U; i < gen_now; i++) {
			auto cur = (*inner_edges)[i];
			if (cur.ell > candidate.ell) {
				candidate = cur;
			}
		}

//		assert(candidate.ell > 0U);
		assert(!candidate.invalid());
		assert(candidate.ell <= std::numeric_limits<int>::max());
		next = candidate.ell;
		*generated = 1U;
		return true;
	}

	std::vector<edge_t> &get_edges()
	{
		return max_edge;
	}

	size_t get_tlen()
	{
		return gen.get_tlen();
	}

};

#endif // RIGHTMOST_FSG_HPP
