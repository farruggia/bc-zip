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

#ifndef SAME_FSG_HPP
#define SAME_FSG_HPP

#include <stdint.h>
#include <boost/iterator/transform_iterator.hpp>
#include <boost/iterator/filter_iterator.hpp>
#include <algorithm>

#include <base_fast.hpp>

template <typename T, typename rsa_getter_t>
class same_fast_fsg {
private:
	std::shared_ptr<byte> text;
	size_t t_len; // Text length

	std::vector<maximal_length<byte>> lengths;
	std::vector<edge_t> maxedges; // maximal edges generated during the process
	std::uint32_t text_pos; // Current position on the text
	mesh_cost mc; // The mesh cost calculator

	rsa_getter_t rsa_get;

	// Buffer of (pred, succ)
	maximal_buffer mbuf;

	// RSA pre-allocated storage
	std::vector<T> rsa_storage;

	rsa<std::tuple<T, T>> empty_rsa()
	{
		return rsa<std::tuple<T,T>>();
	}

	std::vector<unsigned int> get_thresholds(const std::vector<unsigned int> &dst, size_t text_len)
	{
		auto to_ret = dst;
		assert(to_ret.back() >= text_len);
		to_ret.back() = text_len;
		to_ret.insert(to_ret.begin(), 0);
		return to_ret;
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
		if (level == dst.size() - 1) {
			auto rsa        = b_getter.get_sa();
			T *begin        = &(*(rsa.begin()));
			T *end          = &(*(rsa.end()));
			return std::make_tuple(begin, end);
		} else {
			/* Get the rsa<T, T> */
			auto    b_level = b_getter.get_level(cst[level]),
					w_level = w_getter.get_level(cst[level]);

//			std::cout << "Getting B-block, level " << b_level << ", from " << text_pos << std::endl;
			auto    b       = b_getter.get(b_level, pos);
			auto w_1 = empty_rsa(), w_2 = empty_rsa();
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
				// ATTENZIONE, BUGGATO! FILTRARE ELEMENTI SUPERIORI DI SOGLIA
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

	std::tuple<unsigned int, unsigned int> max_match(unsigned int dst_idx)
	{
		// mbuf.print_info();
		mbuf.skip(dst_idx, text_pos);
		if (mbuf.empty(dst_idx)) {
//			std::cout << "Empty buffer, refilling for level " << dst_idx << " for position " << text_pos << std::endl;
//			mbuf.print_info();

			// The current position may not be on a block boundary: find the closest one and generate from there
			double threshold_dst	= text_pos - thresholds[dst_idx];
			auto boundary			= static_cast<unsigned int>(threshold_dst / cst[dst_idx]) * cst[dst_idx] + thresholds[dst_idx];
			T *rsa_begin, *rsa_end;
			std::tie(rsa_begin, rsa_end) = get_rsa(dst_idx, boundary);
			auto &buf_line = mbuf.reset(dst_idx, boundary);
			smart_find(rsa_begin, rsa_end, boundary, dst[dst_idx], buf_line);

			// Since some blocks may not be generated, we must tell the matcher to start over with the counters in the case
			// we are generating a non-consecutive block. We tell this with the scanned_pos (match_length keeps the previous
			// position). Finally, since we might fall into a block, we must skip the appropriate number of elements.
			auto scanned_pos = boundary;
			for (auto &i : buf_line) {
				unsigned int pred, succ;
				std::tie(pred, succ) = i;
				i = lengths[dst_idx].match(pred, succ, scanned_pos++);
			}
			mbuf.skip(dst_idx, text_pos);
		}
		return mbuf.get_head(dst_idx);
	}

	size_t generate_edges(std::uint32_t)
	{
		int maxlen = 0;
		size_t p = 0;

		mc.reset();
		for (unsigned int i = 0; i < dst.size() && text_pos >= thresholds[i]; i++) {
			unsigned int d, ell;
			std::tie(d, ell) = max_match(i);
			assert(text_pos + ell <= t_len);
			assert(d <= text_pos);

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

	void update_rsa()
	{
		b_getter.notify(text_pos);
		w_getter.notify(text_pos);
	}

	std::vector<maximal_length<byte>> init_lengths(byte* text, size_t t_len, std::vector<unsigned int> dst)
	{
		dst.insert(dst.begin(), 0);
		std::vector<maximal_length<byte>> to_ret;
		for (auto i = 0u; i < dst.size() - 1; i++) {
			to_ret.push_back(maximal_length<byte>(text, text + dst[i], t_len));
		}
		return to_ret;
	}

	std::vector<unsigned int> normalize_dst(const std::vector<unsigned int> &dst)
	{
		std::vector<unsigned int> to_ret;
		for (auto i : dst) {
			to_ret.push_back(i);
			if (i >= get_tlen()) {
				break;
			}
		}
		return to_ret;
	}

	/** Return the text length */
	size_t get_tlen() { return t_len; }

public:

	/** Costruisce una istanza del FSG.
	 * text:    puntatore al testo
	 * length:  lunghezza del testo
	 * sa:      Suffix Array del testo, come ottenuto da libdivsufsort (primo indice parte da zero)
	 * dst:     Classi di costo della distanza. l'elemento i-mo rappresenta il massimo valore codificabile
	 *          con la i-ma classe di costo (compreso).
	 * len:     Classi di costo della lunghezza. Stessa interpretazione di dst.
	 */
	fast_fsg(
		std::shared_ptr<byte> text, size_t length,
		std::shared_ptr<std::vector<T>> sa,
		std::vector<unsigned int> dst_, std::vector<unsigned int> len_
	)
		:   text(text), t_len(length),
			dst(normalize_dst(dst_)), len(len_), cst(get_cost_class(dst, length)), thresholds(get_thresholds(dst, t_len)),
			lengths(init_lengths(text.get(), t_len, dst)),
			maxedges(dst.size() + len.size() + 2),
			text_pos(0), mc(dst, len),
			b_getter(rsa_getter<T>::get_b_getter(dst, sa)),
			w_getter(rsa_getter<T>::get_w_getter(dst, sa)),
			// TODO: diminuire la dimensione richiesta da RSA a 3 Q(Q - 1)!
			mbuf(dst, length), rsa_storage(length)
	{
	}

	fast_fsg(fast_fsg<T> &&other) = default;

	fast_fsg<T> &operator=(const fast_fsg &) = delete;

	fast_fsg<T> &operator=(fast_fsg &&) = default;

};

#endif // SAME_FSG_HPP
