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

#ifndef __COST_MODEL_
#define __COST_MODEL_

#include <vector>
#include <algorithm>
#include <stdexcept>
#include <assert.h>
#include <boost/numeric/ublas/matrix.hpp>

#include <common.hpp>
#include <facilities.hpp>
#include <sha1/sha1.h>

/**
 * Serializes a couple (dst idx, len idx) into an unique ID and the other way round
 */
class id_map {
private:
	unsigned int len_bits = std::numeric_limits<unsigned int>::max();
	unsigned int len_mask = std::numeric_limits<unsigned int>::max();
public:

	typedef std::uint32_t id_t;

	id_map()
	{

	}

	id_map(unsigned int lens) : len_bits(bits(lens - 1)), len_mask((1 << len_bits) - 1)
	{

	}

	id_t wrap(unsigned int len_idx, unsigned int d_idx) const
	{
		return (d_idx << len_bits) | len_idx;
	}

	/**
	 * @brief Unwraps an ID
	 * @param id
	 *      The ID
	 * @return
	 *      A couple (dst_idx, len_idx)
	 */
	std::tuple<unsigned int, unsigned int> unwrap(id_t id)
	{
		return std::make_tuple<unsigned int, unsigned int>(id >> len_bits, id & len_mask);
	}

	/**
	 * @brief Augments the len_idx of an ID, without unwrapping it.
	 * @param id
	 *      The ID
	 * @return
	 *      The augmented ID
	 */
	id_t left(id_t id)
	{
		assert((id & len_mask) < (1U << len_bits));
		return id += 1;
	}

	/**
	 * @brief Augments the dst_idx of an ID, without unwrapping it.
	 * @param id
	 *      The ID
	 * @return
	 *      The augmented ID
	 */
	id_t up(id_t id)
	{
		return id += (1 << len_bits);
	}
};

struct class_info {
	std::vector<unsigned int> win;
	std::vector<double> costs;
	size_t length;

	class_info() { }

	template <typename T, typename U>
	class_info(T win_first, T win_last, U costs_first, U costs_last) {
		win.insert(win.end(), win_first, win_last);
		costs.insert(costs.end(), costs_first, costs_last);
		length = std::distance(win_first, win_last);
	}

	size_t extent() const {
		return win[length - 1];
	}

	double get_cost(unsigned int i)
	{
		assert(i <= extent());
		auto distance = std::distance(win.begin(), std::lower_bound(win.begin(), win.end(), i));
		return costs[distance];
	}
};

class cost_matrix {
private:
	boost::numeric::ublas::matrix<double> m;
public:

	cost_matrix()
	{

	}

	cost_matrix(size_t dsts, size_t lens) : m(dsts, lens)
	{

	}

	cost_matrix(class_info dsts, class_info lens) : m(dsts.length, lens.length)
	{
		for (unsigned int dst_idx = 0; dst_idx < dsts.length; dst_idx++) {
			for (unsigned int len_idx = 0; len_idx < lens.length; len_idx++) {
				(*this)(dst_idx, len_idx) = dsts.costs[dst_idx] + lens.costs[len_idx];
			}
		}
	}

	double &operator()(unsigned int dst_idx, unsigned int len_idx)
	{
		return m(dst_idx, len_idx);
	}

	double operator()(unsigned int dst_idx, unsigned int len_idx) const
	{
		return m(dst_idx, len_idx);
	}

	size_t dsts() const
	{
		return m.size1();
	}

	size_t lens() const
	{
		return m.size2();
	}

	void resize(size_t dsts, size_t lens)
	{
		m.resize(dsts, lens);
	}

	boost::numeric::ublas::matrix<double> data()
	{
		return m;
	}

	void set(const boost::numeric::ublas::matrix<double> &cm)
	{
		m = cm;
	}

};

class cost_model {
private:
	/** Original dst */
	std::vector<unsigned int> dsts;
	/** Original len */
	std::vector<unsigned int> lens;
	/** Literal cost model: fixed cost and variable cost (bits) */
	double lit_fixed_cost;
	double lit_var_cost;
    /** id <-> idx map */
    id_map map;
    /** cost map */
    std::vector<double> cost_map;
    // Fixed cost per character
	double cost_per_char_;
	// Vectors used to find out the ID
	std::vector<unsigned int> search_dst;
	std::vector<unsigned int> search_len;

	unsigned int search(unsigned int x, const std::vector<unsigned int> &vect) const
	{
		auto ptr = std::lower_bound(vect.begin(), vect.end(), x);
		return std::distance(vect.begin(), ptr);
	}

	std::vector<double> build_cost_map(cost_matrix m)
	{
		auto max_id = map.wrap(m.lens() - 1, m.dsts() - 1);
		std::vector<double> to_ret(max_id + 1);
		for (unsigned int len_idx = 0; len_idx < m.lens(); len_idx++) {
			for (unsigned int dst_idx = 0; dst_idx < m.dsts(); dst_idx++) {
				auto cost	= m(dst_idx, len_idx);
				auto id		= map.wrap(len_idx, dst_idx);
				assert(id < to_ret.size());
				to_ret[id]	= cost;
			}
		}
		return to_ret;
	}

public:
	typedef id_map::id_t id_t;

	cost_model()
	{

	}

	cost_model(class_info dst, class_info len, double lit_fixed_cost, double lit_var_cost
		, double cost_per_char = 0.0)
		: cost_model(dst.win, len.win, cost_matrix(dst, len), lit_fixed_cost, lit_var_cost, cost_per_char)
	{
	}

	cost_model(
		std::vector<unsigned int> dsts, std::vector<unsigned int> lens, cost_matrix costs,
		double lit_fixed_cost, double lit_var_cost, double cost_per_char = 0.0
	) : dsts(dsts), lens(lens), lit_fixed_cost(lit_fixed_cost), lit_var_cost(lit_var_cost), 
		map(lens.size()), cost_map(build_cost_map(costs)), cost_per_char_(cost_per_char)
	{
		search_dst = dsts;
		search_len = lens;
		search_dst.insert(search_dst.begin(), 0);
		search_len.insert(search_len.begin(), 0);
	}

	std::vector<unsigned int> get_dst() const { return dsts; }

	std::vector<unsigned int> get_len() const { return lens; }

	/**
	 * @brief
	 *		Computes the cost (in bits) of a literal run.
	 * @param len
	 *		The substring length (in characters)
	 * @return
	 *		The cost (in bits) of a literal run.
	 */
	double lit_cost(size_t len) const
	{
		return lit_fixed_cost + len * lit_var_cost;
	}

	id_map get_map() const { return map; }

	std::tuple<unsigned int, unsigned int> get_idx(unsigned int dst, unsigned int len) const noexcept
	{
		assert(dst <= dsts.back());
		assert(len <= lens.back());
		return std::make_tuple(search(dst, dsts), search(len, lens));
	}

	id_t get_id(unsigned int dst, unsigned int len) const noexcept
	{
		unsigned int dst_idx, len_idx;
		std::tie(dst_idx, len_idx) = get_idx(dst, len);
		return map.wrap(len_idx, dst_idx);
	}

	double get_cost(unsigned int dst_idx, unsigned int len_idx) const noexcept
	{
		return cost_map[map.wrap(len_idx, dst_idx)];
	}

	double get_cost(id_t cost_id) const noexcept
	{
		assert(cost_id < cost_map.size());
		return cost_map[cost_id];
	}

	double edge_cost(const edge_t &edge) const noexcept
	{
		return (edge.kind() == REGULAR) ? get_cost(edge.cost_id) : lit_cost(edge.ell);
	}

	cost_matrix get_cm() const
	{
		cost_matrix to_ret(dsts.size(), lens.size());
		for (auto i = 0U; i < to_ret.dsts(); i++) {
			for (auto j = 0U; j < to_ret.lens(); j++) {
				to_ret(i, j) = get_cost(i, j);
			}
		}
		return to_ret;
	}

	double cost_per_char() const noexcept
	{
		return cost_per_char_;
	}

	std::string id() const noexcept
	{
		if (cost_map.empty()) {
			return "";
		}
		// Get sha1 of cost_map
		auto start_ptr = cost_map.data();
		auto length = cost_map.size() * sizeof(cost_map[0]);
		const size_t hash_size = 20;
		unsigned char hash[hash_size];
		sha1::calc(start_ptr, length, hash);
		const size_t hex_hash_size = 41;
		char hex_hash[hex_hash_size];
		sha1::toHexString(hash, hex_hash);
		return std::string(hex_hash, hex_hash + hex_hash_size);
	}

	edge_t get_edge(unsigned int dst, unsigned int len)
	{
		if (dst == 0) {
			return edge_t(len);
		}
		auto id = get_id(dst, len);
		return edge_t(dst, len, id);
	}
};

#endif
