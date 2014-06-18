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

#ifndef BASE_FSG_HPP
#define BASE_FSG_HPP

#include <edges.hpp>
#include <utilities.hpp>

class mesh_cost {
private:
	std::vector<std::uint32_t> dst, len;
	std::uint32_t cur_len, prev_len;
	unsigned int dst_idx, len_idx;
	const size_t max_len, len_size;
	std::uint32_t cost_id;
public:
	mesh_cost(const std::vector<std::uint32_t> &dst, const std::vector<std::uint32_t> &len)
		:   dst(dst), len(len), max_len(len.back()), len_size(bits(len.size() - 1))
	{
		reset();
	}

	inline void set_len(size_t len)
	{
		cur_len = std::min(len, max_len);
	}

	inline bool up(unsigned int *len, std::uint32_t *cost_id)
	{
		if (prev_len >= cur_len)
			return false;
		*cost_id = this->cost_id;
		if (cur_len >= this->len[len_idx]) {
			*len = this->len[len_idx++];
			this->cost_id++;
		} else {
			*len = cur_len;
		}
		prev_len = *len;
		return true;
	}

	inline bool right()
	{
		if (++dst_idx >= dst.size()) {
			return false;
		}
		if (len_idx < len.size() && cur_len == len[len_idx]) {
			++len_idx;
		}
		cost_id = (dst_idx << len_size) | len_idx;
		return true;
	}

	std::uint32_t max_cost_id()
	{
		return bits(len.size()) + bits(dst.size()) - 1;
	}

	void reset()
	{
		dst_idx = len_idx = cur_len = prev_len = cost_id = 0;
	}
};

template <typename inner_gen>
class fsg_protocol {
private:
	inner_gen g;
	size_t t_len; // Text length
	std::vector<edge_t> maxedges; // maximal edges generated during the process
	mesh_cost mc; // The mesh cost calculator
	std::vector<unsigned int> len;
	std::vector<unsigned int> dst;


#ifndef NDEBUG
	std::string check_data_rep(unsigned int ell, unsigned int d, unsigned int dst_idx) {
		return join_s("text_pos = ", g.text_pos(), " d = ", d, ", ell = ", ell, ", t_len = ", t_len, ", dst_idx = ", dst_idx);
	}

	void check_data(unsigned int ell, unsigned int d, unsigned int dst_idx)
	{
		if (g.text_pos() + ell > t_len) {
			std::string err = join_s(
				"Condition text_pos + ell <= t_len failed\n",
				check_data_rep(ell, d, dst_idx)
			);
			throw std::logic_error(err);
		}
		if (ell > 0 && d > g.text_pos()) {
			std::string err = join_s(
				"Condition ell < 0 || d > g.text_pos() failed\n",
				check_data_rep(ell, d, dst_idx)
			);
			throw std::logic_error(err);
		}
		if (ell > 0 && d > dst.back()) {
			std::string err = join_s(
				"Condition ell <= 0 && d < dst.back() failed\n",
				check_data_rep(ell, d, dst_idx)
			);
			throw std::logic_error(err);
		}
//		assert(g.text_pos() + ell <= t_len);
//		assert(ell == 0 || d <= g.text_pos());
//		assert(ell == 0 || d <= dst.back());
	}
#endif

	size_t generate_edges()
	{
		int maxlen = 0;
		size_t p = 0;

		mc.reset();
		auto levels = g.levels();
		for (unsigned int i = 0; i < levels; i++) {
			unsigned int d, ell;
			std::tie(d, ell) = g.max_match(i);
#ifndef NDEBUG
			check_data(ell, d, i);
#endif

			if (ell > maxlen) {
				maxlen = ell;
				mc.set_len(ell);
				unsigned int len;
				std::uint32_t cost_id;
				while (mc.up(&len, &cost_id)) {
					assert(p < maxedges.size());
					maxedges[p++].set(d, len, cost_id);
				}
			}
			if (ell >= this->len.back()) {
				break;
			}
			mc.right();
		}
		return p;
	}

	size_t max_levels(const std::vector<unsigned int> &dst, const std::vector<unsigned int> &len)
	{
		return dst.size() + len.size() + 2;
	}

public:

	template <typename T>
	fsg_protocol(
		T &&gen, size_t t_len, const std::vector<unsigned int> &dst,
		const std::vector<unsigned int> &len
	)
		: g(std::forward<T>(gen)), t_len(t_len),
		  maxedges(max_levels(dst, len), edge_t()), mc(dst, len), len(len), dst(dst)
	{

	}

	inline bool gen_next(std::uint32_t *generated)
	{
		if (g.text_pos() >= t_len) {
			return false;
		}
		g.pre_gen();
		*generated = generate_edges();
		g.post_gen();
		return true;
	}

	std::vector<edge_t> &get_edges()
	{
		return maxedges;
	}

	/** Return the text length */
	size_t get_tlen()
	{
		return t_len;
	}
};

#endif // BASE_FSG_HPP
