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

#ifndef __FAST_FSG
#define __FAST_FSG

#include <stdint.h>
#include <boost/iterator/transform_iterator.hpp>
#include <boost/iterator/filter_iterator.hpp>
#include <algorithm>
#include <cmath>

#include <scan.hpp>
#include <online_rsa.hpp>
#include <match_length.hpp>
#include <utilities.hpp>
#include <base_fsg.hpp>
#include <cc_stats.hpp>

class maximal_buffer {
private:
	typedef std::vector<std::tuple<unsigned int, unsigned int>> ring;
	typedef ring::iterator iterator;
	/** ring, head ring position, boundary */
	std::vector<std::tuple<ring, unsigned int, std::int_fast64_t>> buffer;
public:
	template <typename stats_get_t_>
	maximal_buffer(stats_get_t_ getter, size_t t_len)
	{
		auto cost_classes = getter.get_cost_class();
		for (auto i : cost_classes) {
			/**
			 * Each ring is initialized so that a skip(level, 0)
			 * yields an empty ring.
			 */
			assert(i > 0);
			std::int_fast64_t boundary = - static_cast<std::int64_t>(i);
			buffer.push_back(std::make_tuple(ring(i), i - 1, boundary));
		}
	}

	bool empty(unsigned int level)
	{
		assert(level < buffer.size());
		auto &el = buffer[level];
		return std::get<0>(el).size() == std::get<1>(el);
	}

	std::tuple<unsigned int, unsigned int> get_head(unsigned int level)
	{
		assert(!empty(level));
		auto &el = buffer[level];
		auto &idx = std::get<1>(el);
		return std::get<0>(el)[idx];
	}

	void skip(unsigned int level, unsigned int position)
	{
		auto &el		= buffer[level];
		auto &idx		= std::get<1>(el);
		auto &boundary	= std::get<2>(el);
		auto size		= std::get<0>(el).size();
		assert(position >= boundary);
		assert(position >= boundary + idx);
		idx = std::min<unsigned int>(size, position - boundary);
	}

	ring &reset(unsigned int level, unsigned int boundary)
	{
		assert(level < buffer.size());
		auto &el = buffer[level];
		std::get<1>(el) = 0;
		std::get<2>(el) = boundary;
		return std::get<0>(el);
	}

	// DEBUG
	void print_info()
	{
		std::cout << "Maximal_buffer infos:" << std::endl;
		for (auto i = 0u; i < buffer.size(); i++) {

			std::cout << i << ": head " << std::get<1>(buffer[i]) << ", size " << std::get<0>(buffer[i]).size() << std::endl;
		}
	}
};

/****** MERGE FACILITIES ******/
template <typename T>
class tuple_comp {
public:
	bool operator()(std::tuple<T, T> &e1, std::tuple<T, T> &e2)
	{
		return std::get<1>(e1) < std::get<1>(e2);
	}
};

template <typename T>
class get_tuple {
public:

	typedef std::tuple<T&, T> result_type;

	std::tuple<T&, T> operator()(T &val) const
	{
		return std::tuple<T&, T>(val, T());
	}
};

template <typename T, typename U>
boost::transform_iterator<T, U> get_converter(T unary_fun, U iterator)
{
	return boost::transform_iterator<T, U>(iterator, unary_fun);
}

template <typename T>
class filter {
private:
	T threshold;
public:

	filter() : threshold(T()) { }

	filter(T threshold) : threshold(threshold) { }

	bool operator()(const std::tuple<T, T> &t) const
	{
		return std::get<0>(t) < threshold;
	}

};

template <typename Iter1, typename Iter2, typename Iter3, typename Output, typename Compare>
Output merge(
		Iter1 w_1_first, Iter1 w_1_last,
		Iter2 w_2_first, Iter2 w_2_last,
		Iter3 w_3_first, Iter3 w_3_last,
		Output output,
		Compare comp
		)
{
	if ((w_1_first != w_1_last) && (w_2_first != w_2_last) && (w_3_first != w_3_last)) {
		bool c_1 = comp(*w_1_first, *w_2_first);
		auto min = c_1 ? *w_1_first : *w_2_first;
		while (true) {
			bool c_2    = comp(min, *w_3_first);
			if (!c_2) {
				*output++ = *w_3_first++;
				if (w_3_first == w_3_last) {
					break;
				}
			} else if (c_1) {
				*output++ = *w_1_first++;
				if (w_1_first == w_1_last) {
					break;
				}
				c_1 = comp(*w_1_first, *w_2_first);
				min = c_1 ? *w_1_first : *w_2_first;
			} else {
				*output++ = *w_2_first++;
				if (w_2_first == w_2_last) {
					break;
				}
				c_1 = comp(*w_1_first, *w_2_first);
				min = c_1 ? *w_1_first : *w_2_first;
			}
		}
	}

	if (w_3_first == w_3_last) {
		return std::merge(w_1_first, w_1_last, w_2_first, w_2_last, output, comp);
	} else if (w_2_first == w_2_last) {
		return std::merge(w_1_first, w_1_last, w_3_first, w_3_last, output, comp);
	} else {
		return std::merge(w_2_first, w_2_last, w_3_first, w_3_last, output, comp);
	}
}

template <typename T>
class generic_rsa_getter {
	size_t text_len;
	std::vector<unsigned int> dst;
	std::vector<unsigned int> cst;
	// Sorted blocks getter
	rsa_getter<T> b_getter;
	rsa_getter<T> w_getter;
	// RSA pre-allocated storage
	std::vector<T> rsa_storage;

	rsa<std::tuple<T, T>> empty_rsa()
	{
		return rsa<std::tuple<T,T>>();
	}

public:

	template <typename stats_get_t_>
	generic_rsa_getter(
			stats_get_t_ stats,
			std::shared_ptr<std::vector<T>> sa,
			size_t text_length
	)
		:	text_len(text_length),
			dst(stats.get_dst()), cst(stats.get_cost_class()),
			b_getter(rsa_getter<T>::get_b_getter(dst, sa)),
			w_getter(rsa_getter<T>::get_w_getter(dst, sa)),
			rsa_storage(text_length)
	{

	}

	/**
	 * @brief Gets the RSA of the block of level "level", starting from position "pos"
	 * @param level
	 *		The level
	 * @param pos
	 *		The first position of the B-block.
	 * @return
	 */
	std::tuple<T*, T*> get_rsa(unsigned int level, unsigned int pos)
	{
		// TODO: controllare questo if...
		assert(level < dst.size());
		if (dst[level] >= text_len) {
			auto rsa        = b_getter.get_sa();
			T *begin        = &(*(rsa.begin()));
			T *end          = &(*(rsa.end()));
			return std::make_tuple(begin, end);
		} else {
			/* Get the rsa<T, T> */
			auto    b_level = b_getter.get_level(cst[level]);
			auto	w_level = w_getter.get_level(cst[level]);

			//			std::cout << "Getting B-block, level " << b_level << ", from " << text_pos << std::endl;
			auto b		= b_getter.get(b_level, pos);
			auto w_1	= empty_rsa();
			auto w_2 	= empty_rsa();
			if (pos >= dst[level]) {
				//				std::cout << "Getting W-block, level " << w_level << ", from " << text_pos - dst[level] << std::endl;
				w_1 = w_getter.get(w_level, pos - dst[level]);
			}
			if (level > 0 && pos >= dst[level - 1]) {
				//				std::cout << "Getting W-block, level " << w_level << ", from " << text_pos - dst[level - 1] << std::endl;
				w_2 = w_getter.get(w_level, pos - dst[level - 1]);
			}

			/* Extracts the first element out of a std::tuple<T,T>. */
			auto converter = get_converter(get_tuple<T>(),rsa_storage.begin());
			/* If w_2 overlaps, then filter incoming data */
			if (w_2.term() > pos) {
				filter<T> flt(pos);
				auto f_w2_begin		= boost::make_filter_iterator(flt, w_2.begin(), w_2.end());
				auto f_w2_end		= boost::make_filter_iterator(flt, w_2.end(), w_2.end());
				converter = merge(ITERS(w_1), f_w2_begin, f_w2_end, ITERS(b), converter, tuple_comp<T>());
			} else {
				converter = merge(ITERS(w_1), ITERS(w_2), ITERS(b), converter, tuple_comp<T>());
			}
			size_t window_size = std::distance(rsa_storage.begin(), converter.base());
			T *begin          = &(*rsa_storage.begin());
			T  *end           = &(*std::next(rsa_storage.begin(), window_size));
			return std::make_tuple(begin, end);
		}
	}

	void notify(unsigned int text_pos)
	{
		b_getter.notify(text_pos);
		w_getter.notify(text_pos);
	}

	static distance_kind get_kind()
	{
		return MULTIPLE;
	}

	generic_rsa_getter(const generic_rsa_getter &) = delete;
	generic_rsa_getter(generic_rsa_getter &&) = default;

	generic_rsa_getter &operator=(const generic_rsa_getter &) = delete;
	generic_rsa_getter &operator=(generic_rsa_getter &&) = default;
};

template <typename T>
class same_rsa_getter {
private:
	unsigned int block_size;
	rsa_getter<T> block_getter;
	std::vector<T> output_storage;
	size_t t_len;

	std::vector<unsigned int> get_dst_v(unsigned int block_size, unsigned int text_length)
	{
		std::vector<unsigned int> to_ret = {block_size, text_length};
		return to_ret;
	}

public:
	same_rsa_getter(unsigned int block_size, std::shared_ptr<std::vector<T>> sa, unsigned int text_length)
		: block_size(block_size),
		  block_getter(rsa_getter<T>::get_w_getter(get_dst_v(block_size, text_length), sa)),
		  output_storage(block_size * 3),
		  t_len(text_length)
	{
//		rsa_splitter<T, rsa_div_eq> splitter(rsa_div_eq(block_size, blocks));
//		rsa<T, T*> suffix_array(sa.get()->data(), 0U, 1, text_length);
//		splitter.distribute(suffix_array, rsas.begin(), rsas.end());
	}

	std::tuple<T*, T*> get_rsa(unsigned int level, unsigned int pos)
	{
		assert(pos % block_size == 0);
		std::int_fast64_t b_block	= pos;
		auto w_2_block				= b_block  - level * block_size;
		auto w_1_block				= w_2_block - block_size;
		rsa<std::tuple<T, T>> w_1, w_2, b = block_getter.get(0U, b_block);
		if (w_1_block >= 0) {
			w_1 = block_getter.get(0U, w_1_block);
		}
		if (w_2_block != b_block) {
			w_2 = block_getter.get(0U, w_2_block);
		}

		auto converter = get_converter(get_tuple<T>(),output_storage.begin());
		converter = merge(ITERS(w_1), ITERS(w_2), ITERS(b), converter, tuple_comp<T>());

		size_t window_size = std::distance(output_storage.begin(), converter.base());
		T *begin          = &(*output_storage.begin());
		T  *end           = &(*std::next(output_storage.begin(), window_size));
		return std::make_tuple(begin, end);
	}

	void notify(unsigned int)
	{

	}

	static distance_kind get_kind()
	{
		return ALL_SAME;
	}
};

template <typename T>
class generalized_rsa {
private:
	gen_stats_getter stats;
	generic_rsa_getter<T> getter;
	std::tuple<T*, T*> cached_rsa;
	unsigned int cached_opt_lev = std::numeric_limits<unsigned int>::max();
	unsigned int cached_opt_pos = std::numeric_limits<unsigned int>::max();
public:
	generalized_rsa(
		gen_stats_getter stats_,
		std::shared_ptr<std::vector<T>> sa,
		size_t text_length
	) : stats(stats_),
		getter(stats_getter(stats.get_opt_dst(), stats.get_len(), text_length), sa, text_length)
	{

	}

	std::tuple<T*, T*> get_rsa(unsigned int level, unsigned int pos)
	{
		auto translated_level = stats.map_opt(level);
		if (translated_level != cached_opt_lev || pos != cached_opt_pos) {
			cached_rsa = getter.get_rsa(translated_level, pos);
			cached_opt_lev = translated_level;
			cached_opt_pos = pos;
		}
		return cached_rsa;
	}

	void notify(unsigned int text_pos)
	{
		getter.notify(text_pos);
	}

	static distance_kind get_kind()
	{
		return GENERIC;
	}

	generalized_rsa(const generalized_rsa &) = delete;
	generalized_rsa(generalized_rsa &&) = default;

	generalized_rsa &operator=(const generalized_rsa &) = delete;
	generalized_rsa &operator=(generalized_rsa &&) = default;
};

template <typename T, template<class> class rsa_getter_t, typename max_match_t_ = smart_find>
class fast_fsg_gen {
private:
	std::shared_ptr<byte> text;
	size_t t_len; // Text length
	std::vector<unsigned int> dst, len, cst, thresholds;
	std::vector<maximal_length<byte>> lengths;
	std::vector<edge_t> maxedges; // maximal edges generated during the process
	std::uint32_t t_pos; // Current position on the text
	mesh_cost mc; // The mesh cost calculator

	// RSA getter
	rsa_getter_t<T> rsa_get;

	// Buffer of (pred, succ)
	maximal_buffer mbuf;

	max_match_t_ m_match;

	std::vector<unsigned int> get_thresholds(const std::vector<unsigned int> &dst, size_t text_len)
	{
		auto to_ret = dst;
		to_ret.insert(to_ret.begin(), 0);
		return to_ret;
	}

	std::vector<maximal_length<byte> > init_lengths(byte* text, size_t t_len, std::vector<unsigned int> dst)
	{
		dst.insert(dst.begin(), 0);
		// TODO: questo è stato cambiato, verificare che sia OK!
		std::vector<maximal_length<byte>> to_ret(dst.size() - 1, maximal_length<byte>(text, t_len));
		return to_ret;
	}

	/** Return the text length */
	size_t get_tlen()
	{
		return t_len;
	}

public:

	/** Costruisce una istanza del FSG.
		 * text:    puntatore al testo
		 * length:  lunghezza del testo
		 * sa:      Suffix Array del testo, come ottenuto da libdivsufsort (primo indice parte da zero)
		 * dst:     Classi di costo della distanza. l'elemento i-mo rappresenta il massimo valore codificabile
		 *          con la i-ma classe di costo (compreso).
		 * len:     Classi di costo della lunghezza. Stessa interpretazione di dst.
		 */
	template <typename stats_gen_t>
	fast_fsg_gen(
			std::shared_ptr<byte> text, size_t length,
			rsa_getter_t<T> &&getter,
			stats_gen_t stats
		)
		:   text(text), t_len(length),
		  dst(stats.get_dst()), len(stats.get_len()), cst(stats.get_cost_class()),
		  thresholds(stats.get_threshold()),
		  lengths(init_lengths(text.get(), t_len, dst)),
		  maxedges(dst.size() + len.size() + 2),
		  t_pos(0), mc(dst, len),
		  rsa_get(std::move(getter)),
		  mbuf(stats, length)
	{

	}

	std::tuple<unsigned int, unsigned int> max_match(unsigned int dst_idx)
	{
		// mbuf.print_info();
		mbuf.skip(dst_idx, t_pos);
		if (mbuf.empty(dst_idx)) {
			//			std::cout << "Empty buffer, refilling for level " << dst_idx << " for position " << text_pos << std::endl;
			//			mbuf.print_info();

			// The current position may not be on a block boundary: find the closest one and generate from there
			double threshold_dst	= t_pos - thresholds[dst_idx];
			auto boundary			= static_cast<unsigned int>(threshold_dst / cst[dst_idx]) * cst[dst_idx] + thresholds[dst_idx];
			T *rsa_begin, *rsa_end;
			std::tie(rsa_begin, rsa_end) = rsa_get.get_rsa(dst_idx, boundary);
			auto &buf_line = mbuf.reset(dst_idx, boundary);
			m_match(rsa_begin, rsa_end, boundary, dst[dst_idx], buf_line);

			// Since some blocks may not be generated, we must tell the matcher to start over with the counters in the case
			// we are generating a non-consecutive block. We tell this with the scanned_pos (match_length keeps the previous
			// position). Finally, since we might fall into a block, we must skip the appropriate number of elements.
			auto scanned_pos = boundary;
			for (auto &i : buf_line) {
				unsigned int pred, succ;
				std::tie(pred, succ) = i;
				i = lengths[dst_idx].match(pred, succ, scanned_pos++);
				assert(std::get<0>(i) <= dst[dst_idx]);
			}
			mbuf.skip(dst_idx, t_pos);
		}
		return mbuf.get_head(dst_idx);
	}

	size_t levels()
	{
		if (t_pos > dst.back()) {
			return dst.size();
		}
		return 1 + std::distance(dst.begin(), std::lower_bound(dst.begin(), dst.end(), t_pos));
	}

	void pre_gen()
	{
		rsa_get.notify(t_pos);
	}

	void post_gen()
	{
		++t_pos;
	}

	static distance_kind get_kind()
	{
		return rsa_getter_t<T>::get_kind();
	}

	std::uint32_t text_pos()
	{
		return t_pos;
	}

	fast_fsg_gen(fast_fsg_gen &&other) = default;

	fast_fsg_gen &operator=(const fast_fsg_gen &) = delete;

	fast_fsg_gen &operator=(fast_fsg_gen &&) = default;

};

#endif
