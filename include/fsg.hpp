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

#ifndef FSG_HPP
#define FSG_HPP

#include <memory>
#include <vector>
#include <array>
#include <assert.h>
#include <utilities.hpp>
#include <common.hpp>
#include <AVLtree.h>
#include <cost_model.hpp>
#include <base_fsg.hpp>

/*
	Compute the LCP between maximal positions and the current offset.
	T: text offset kind
	U: text symbol kind
 */
template <typename T = std::uint32_t, typename U = byte>
class lcp {
private:
	std::vector<T> llmatch;
	std::vector<T> rlmatch;
	std::shared_ptr<U> text;
	T t_len;
	T offset;
	typedef typename std::make_signed<T>::type Ts;

	unsigned int large_copy_pos = 0U;

	T update_match(Ts pos, T matchlen)
	{
		if (pos < 0)
			return 0;
		U *head = text.get() + offset + matchlen;
		U *back = text.get() + pos + matchlen;
		while (head < text.get() + t_len && *head++ == *back++) {
			 matchlen++;
		}
		return matchlen;
	}
public:
	lcp(unsigned int levels, std::shared_ptr<U> text, T t_len)
		: llmatch(levels, 0), rlmatch(levels, 0), text(text), t_len(t_len), offset(0) { }

	void update_match(unsigned int level, Ts left, Ts right, T *match, T* matchlen)
	{
		auto l_len = llmatch[level];
		auto r_len = rlmatch[level];

		// if (get_offset() >= 131059697 && level == 8) {
		// 	std::cerr << "Offset = " << get_offset() << ", l_len = " << l_len << ", r_len = " << r_len << std::endl;
		// }

		llmatch[level] = update_match(left, l_len);
		rlmatch[level] = update_match(right, r_len);

		// if (get_offset() >= 131059697 && level == 8) {
		// 	std::cerr << "Updated lmatch = " << llmatch[level] << ", rmatch = " << rlmatch[level] << std::endl;
		// }

		if (llmatch[level] >= rlmatch[level]) {
			*matchlen = llmatch[level];
			*match = left;
		} else {
			*matchlen = rlmatch[level];
			*match = right;
		}

		// if (*matchlen > 1542533 && *match != ++large_copy_pos) {
		// 	std::cerr << "Detected large match: position " << get_offset();
		// 	std::cerr << ", maximal << " << *match << ", length ";
		// 	std::cerr << *matchlen << ", level = " << level << std::endl;
		// 	large_copy_pos = *match;
		// }
	}

	void next_char()
	{
		offset++;
		for (auto &i : llmatch) {
			i = std::max<Ts>(0, static_cast<Ts>(i) - 1);
		}
		for (auto &i : rlmatch) {
			i = std::max<Ts>(0, static_cast<Ts>(i) - 1);
		}
	}

	T get_offset() { return offset; }
};

class fsg_gen {
private:
	std::shared_ptr<byte> text;
	size_t t_len; // Text length
	std::shared_ptr<std::vector<std::int32_t>> sa, isa;
	lcp<> lcp_calc; // LCP calculator. Also holds the current offset in the text.
	std::vector<unsigned int> dst, len;
	std::vector<edge_t> maxedges; // maximal edges generated during the process
	mesh_cost mc; // The mesh cost calculator

	std::vector<AVLTree<std::int32_t>> t; // array of balanced trees of integer
	std::vector<PSStruct> ps; // array of VEB-like pred./succ. data structures

	// Balanced trees management
	typedef AVLNode<std::int32_t> node_t; // The node type
	std::vector<node_t> pool; // The pool
	node_t *released; // The last deallocated node
	std::uint32_t treenum; // number of windows handled with balanced trees

public:

	fsg_gen(
		std::shared_ptr<byte> text, size_t length,
		std::shared_ptr<std::vector<std::int32_t>> sa, std::shared_ptr<std::vector<std::int32_t>> isa,
		std::vector<unsigned int> dst, std::vector<unsigned int> len
	)
		:
		text(text), t_len(length), sa(sa), isa(isa),
		lcp_calc(dst.size(), text, length),
		dst(dst), len(len),
		maxedges(dst.size() + len.size() + 2, edge_t()), mc(dst, len),
		released(0)
	{
		// Allocates the balanced trees
		// In qualche modo, il loop seguente alloca un certo numero di nodi AVL
		// da utilizzare per istanziazioni future (il "pool"). Il numero di
		// nodi è scelto così: se t_len è piccolo abbastanza, allora ne alloca
		// t_len, altrimenti alloca dst[x], dove x è il più grande intero per
		// cui dst[x] * C < t_len, dove C è una costante pari a 1 << 17.
		// Speed and size inversely proportional.
		const unsigned int C = 1 << 17;
		for (treenum = 0; dst[treenum] < t_len / C; treenum++);

		if (treenum) {
			unsigned int poolsize = (t_len > dst[treenum - 1]) ? dst[treenum - 1] + 1 : t_len;
			pool.insert(pool.end(), poolsize, AVLNode<int>());
			// Questo andrebbe capito: in che modo "t" è usato? Quanti elementi
			// sono utilizzati?
			t.insert(t.end(), treenum, AVLTree<int>());
		}

		//Initialize required structures
		BMutil::FillInTables(); // What about that?
		if (treenum < dst.size())
			ps.insert(ps.end(), dst.size() - treenum, PSStruct());
		for (auto it = ps.begin(); it != ps.end(); it++)
			it->SetRange(t_len + 1);
	}

	std::vector<edge_t> &get_edges() { return maxedges; }

	/** Return the text length */
	size_t get_tlen() { return t_len; }

private:

	std::vector<std::int32_t> &sa_ref()
	{
		return *(sa.get());
	}

	std::vector<std::int32_t> &isa_ref()
	{
		return *(isa.get());
	}


	void update_btrees()
	{
		auto off = lcp_calc.get_offset();
		if (!off) return;

		int r = off - 1;

		if (treenum) {

			assert(released != 0 || r < static_cast<int>(pool.size()));
			node_t *n = (released == 0) ? &pool[r] : released;
			assert(n != 0);
			assert(r < static_cast<int>(isa->size()));
			n->key = isa_ref()[r];
			n->info = r;
			n->left = n->right = 0;
			n->height = 1;

			unsigned int i = 0;
			do {
				t[i].Insert(n);
				r = off - dst[i] - 1;
				if (r >= 0) n = t[i].Delete(isa_ref()[r]);
				else return;
			} while (++i < treenum);

			released = n;
		}

		//std::cout << "Update @ " << off << std::endl;
		for (unsigned int i = treenum; i < dst.size(); i++) {
			assert(i - treenum < ps.size());
			ps[i - treenum].Set(isa_ref()[r]);
			r = off - dst[i] - 1;
			if (r >= 0) ps[i - treenum].Remove(isa_ref()[r]);
			else return;
		}
	}

	inline void find_match(unsigned int level, int rank, int &pred, int &succ)
	{
		if (level < treenum) {
			node_t *p, *s;
			assert(level < t.size());
			t[level].Search((ktype)rank, &p, &s);
			pred = (p == 0) ? -1 : p->info;
			succ = (s == 0) ? -1: s->info;
		} else {
			int prank, srank;
			assert(level - treenum < ps.size());
			ps[level-treenum].Search(rank, prank, srank);
			pred = (prank >= 0) ? sa_ref()[prank]: -1;
			succ = (srank >= 0)? sa_ref()[srank]: -1;
		}
	}

public:

	std::tuple<unsigned int, unsigned int> max_match(unsigned int level)
	{
		int pred, succ;
		auto offset = lcp_calc.get_offset();
		find_match(level, isa_ref()[offset], pred, succ);
		if (pred == -1 && succ == -1) {
			return std::make_tuple(0U, 0U);
		}
		std::uint32_t maximal, len;
		lcp_calc.update_match(level, pred, succ, &maximal, &len);
		unsigned int d = offset - maximal;
		return std::make_tuple(d, len);
	}

	size_t levels()
	{
		// TODO: accertarsi che in "dst" ci vada a finire un valore non inferiore a t_len
		if (text_pos() > dst.back()) {
			return dst.size();
		}
		return 1 + std::distance(dst.begin(), std::lower_bound(dst.begin(), dst.end(), lcp_calc.get_offset()));
	}

	void pre_gen()
	{
		update_btrees();
	}

	void post_gen()
	{
		lcp_calc.next_char();
	}

	unsigned int text_pos()
	{
		return lcp_calc.get_offset();
	}

	static distance_kind get_kind()
	{
		return GENERIC;
	}
};

#endif // FSG_HPP
