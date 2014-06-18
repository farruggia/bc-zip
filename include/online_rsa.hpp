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

#ifndef _RSA_GEN_ONLINE_
#define _RSA_GEN_ONLINE_

#include <vector>
#include <tuple>
#include <iterator>
#include <boost/circular_buffer.hpp>
#include <stdexcept>
#include <cmath>
#include <unordered_map>
#include <assert.h>

#include "utilities.hpp"

template <typename T, typename Iter = typename std::vector<T>::iterator>
class rsa {
private:
	Iter begin_;
	Iter end_;
	unsigned int level_;
	unsigned int start_;
	unsigned int term_;
public:

	typedef Iter iterator;

	// Returns an empty RSA (which should not be resized!)
	rsa() : start_(0) { resize(0); }

	rsa (Iter begin_, unsigned int start_, unsigned int level_, size_t size_) 
		: begin_(begin_), level_(level_), start_(start_)
	{
		resize(size_);
	}

	Iter begin()
	{
		return begin_;
	}

	Iter end()
	{
		return end_;
	}

	size_t size()
	{
		return std::distance(begin(), end());
	}

	unsigned int level() const { return level_; }

	unsigned int start() { return start_; }

	unsigned int term() { return term_; }

	void set_start(unsigned int start_)
	{
		auto size = term_ - this->start_;
		this->start_ = start_;
		term_ = start_ + size;
	}

	void resize(size_t size_)
	{
		end_ = std::next(begin_, size_);
		term_ = start_ + size_;
	}

	T& operator[](int a)
	{
		assert(std::next(begin_, a) < end_);
		return *std::next(begin_, a);
	}

	T& operator*()
	{
		return *begin_;
	}
};

/** RSA splitter implementation */
class rsa_div_eq {
private:
	unsigned int step;
	unsigned int n_blocks;
public:
	rsa_div_eq() { }
	rsa_div_eq(unsigned int step, unsigned int blocks) : step(step), n_blocks(blocks) {}
	template <typename T>
	unsigned int get_block(const std::tuple<T,T> &el, unsigned int offset) const
	{
		return (std::get<0>(el) - offset) / step;
	}

	template <typename T>
	void set_entry(std::tuple<T,T> &entry, const std::tuple<T,T> &el)
	{
		entry = el;
	}

	unsigned int blocks() { return n_blocks; }
};

class rsa_div_w {
private:
	unsigned int step;
	unsigned int n_blocks;
	unsigned int parent_idx;

public:
	rsa_div_w() { }
	rsa_div_w(unsigned int step, unsigned int blocks) : step(step), n_blocks(blocks), parent_idx(0U)
	{ }

	template <typename T>
	unsigned int get_block(T el, unsigned int offset)
	{
		assert(offset == 0);
		return el / step;
	}

	template <typename T>
	void set_entry(std::tuple<T,T> &entry, T pos)
	{
		entry = std::make_tuple(pos, parent_idx++);
	}

	unsigned int blocks()
	{
		return n_blocks;
	}
};


class rsa_div_b {
private:
	std::vector<unsigned int> thresholds;
	size_t last_block_size;
	size_t classes;
	unsigned int parent_idx;

	unsigned int text_len()
	{
		return thresholds.back();
	}

	unsigned int max_distance()
	{
		return *std::prev(thresholds.end(), 2);
	}

public:
	rsa_div_b() { }
	rsa_div_b(std::vector<unsigned int> thresholds, size_t last_block_size) 
		: thresholds(thresholds), last_block_size(last_block_size), classes(thresholds.size() - 2), parent_idx(0U)
	{ }

	template <typename T>
	unsigned int get_block(T el, unsigned int)
	{
		auto pos = std::prev(std::upper_bound(thresholds.begin(), thresholds.end(), el), 1);
		auto idx = std::distance(thresholds.begin(), pos);
		if (idx == classes) {
			idx += (el - max_distance()) / last_block_size;
		}
		return idx;
	}

	template <typename T>
	void set_entry(std::tuple<T,T> &entry, T pos)
	{
		entry = std::make_tuple(pos, parent_idx++);
	}

	unsigned int blocks() 
	{
		const auto normal_blocks 	= thresholds.size() - 2;
		const auto div_blocks		= std::ceil((1.0 * text_len() - max_distance()) / last_block_size);
		return normal_blocks + div_blocks;
	}
};

template <typename T, typename RsaDiv>
class rsa_splitter {
private:
	typename std::vector<T> counter;
	RsaDiv div;
public:

	rsa_splitter() { }

	rsa_splitter(RsaDiv div) : counter(div.blocks()), div(div) {}

	/* Rsa must be, and RsaIter must be an iterator to, a sequence of objects satisfying the RSA concept.
	  RsaIter must be a RandomAccessElement iterator.
	*/
	template <typename Rsa, typename RsaIter>
	void distribute(Rsa parent, RsaIter begin, RsaIter end)
	{
		std::fill(counter.begin(), counter.end(), 0);
		const auto off  = begin->start();

		// std::cout << "Distribute: " << parent.start() << " " << parent.term() << std::endl;
		// std::cout << "Offset of children: " << off << std::endl;
		const auto blocks = std::distance(begin, end);

		// Distribute
		for (auto el : parent) {
			const auto block  = div.get_block(el, off);
			assert(block < counter.size());
			assert(block < blocks);
			div.set_entry(begin[block][counter[block]++], el);
		}
	}
};

/** RSA queue */
template <typename T>
class rsa_queue {
private:
	typedef typename boost::circular_buffer<rsa<std::tuple<T,T>>> rsa_ring;
	// The storage
	std::vector<std::tuple<T,T>> storage;
	// The Suffix Array
	std::shared_ptr<std::vector<T>> sa;
	// The queue ring
	std::vector<rsa_ring> queue;
	// A map size -> level
	std::unordered_map<size_t, unsigned int> size_map;
	// Level -> (end index, end_pos)
	std::vector<std::tuple<unsigned int, unsigned int>> idx_pos;
	// Text length
	size_t text_len;

	std::vector<T> &sa_ref()
	{
		return *(sa.get());
	}

	std::tuple<bool, unsigned int> get_block(unsigned int level, unsigned int start)
	{
		// std::cout << "get_block(): level " << level << ", start = " << start << ", first position = " << queue[level].front().start() << std::endl;
		unsigned int end_pos, end_idx;
		assert(level < idx_pos.size());
		std::tie(end_idx, end_pos) = idx_pos[level];
		if (level >= queue.size() || end_pos == 0)
			return std::make_tuple(false, 0u);
		const auto first_pos       = queue[level].front().start();
		const auto level_size      = queue[level].front().size();
		if (first_pos > start || start >= end_pos || (start -  first_pos) % (level_size) != 0)
			return std::make_tuple(false, 0u);
		const auto blocks_forward = (start - first_pos)/(level_size);
		if (blocks_forward >= queue[level].size())
			return std::make_tuple(false, 0u);
		return std::make_tuple(true, blocks_forward);
	}

	// DEBUG
	void print_level_rsas(unsigned int level)
	{
		for (auto j : queue[level]) {
			std::cout << "(" << j.start() << ", " << j.term() << ")\t";
		}
		std::cout << std::endl;
	}

public:
	typedef decltype(queue[0].begin()) iterator;       // Iterator to a sequence of rsa
	typedef rsa<std::tuple<T,T>> rsa_t;

	rsa_queue() {}

	/**
	* Builds a queue out of a descriptor.
	* A descriptor is a sequence of couple (Q, l), where:
	* - Q is the size of the cost class;
	* - l is the number of cost classes to keep.
	*/
	rsa_queue(
		std::vector<std::tuple<unsigned int, unsigned int>> descriptor, 
		std::shared_ptr<std::vector<T>> sa
	) : sa(sa), idx_pos(descriptor.size(), std::make_tuple(0u,0u)), text_len(sa_ref().size())
	{
		// Determine how much storage we need and allocate it
		auto storage_needed = 0u;
		for (auto i : descriptor) {
			unsigned int csize, blocks;
			std::tie(csize, blocks) = i;
			storage_needed += csize * blocks;
		}
		storage.resize(storage_needed);
		// Initialize the RSA ring
		auto it = storage.begin();
		auto level = 0u;
		for (auto i : descriptor) {
			unsigned int csize, blocks;
			std::tie(csize, blocks) = i;
			queue.push_back(rsa_ring(blocks));
			for (auto j = 0u; j < blocks; j++) {
				auto &ring = queue.back();
				rsa_t to_push(it, 0, level, csize);
				ring.push_back(to_push);
				std::advance(it, csize);
			}
			size_map[csize] = level;
			// std::cout << "Size of level " << level << ": " << csize << std::endl;
			level++;
		}
	}

	// DEBUG
	void print_state()
	{
		for (auto lev = 0u; lev < queue.size(); lev++) {
			std::cout << "Level " << lev << ": ";
			print_level_rsas(lev);
			unsigned int idx, pos;
			std::tie(idx, pos) = idx_pos[lev];
			std::cout << "IDX: " << idx << ", POS: " << pos << std::endl;
		}
	}

	rsa<T> get_sa()
	{
		return rsa<T>(sa->begin(), 0, levels() + 1, text_len);
	}

	/* Get the RSA of level ``level'' ranging from "start" (included) */
	rsa_t get(unsigned int level, unsigned int start)
	{
		bool present;
		unsigned int blocks_forward;
		std::tie(present, blocks_forward) = get_block(level, start);
		// print_level_rsas(level);
#ifndef NDEBUG
		// TODO: debug, togliere
		if (!present) {
			std::cout << "Request for non-existent block, starting from " << start << " at level " << level << std::endl;
			print_state();
			assert(present);
		}
#endif
		return queue[level][blocks_forward];
	}

	/* Given a level, returns the couple (size, start of next block to be generated, number of blocks) */
	std::tuple<size_t, unsigned int, unsigned int> level_info(unsigned int level)
	{
		auto &ring 	= queue[level];
		auto &rsa 	= ring.front();
		return std::make_tuple(rsa.size(), std::get<1>(idx_pos[level]), ring.size());
	}

	/* Given a size, gives its level (-1 if there isn't one) */
	int get_level(size_t size)
	{
		auto it = size_map.find(size);
		if (it == size_map.end()) {
			return -1;
		}
		return it->second;
	}

	size_t levels()
	{
		return queue.size();
	}

	/* Tells if the block of level "level" ranging from "start" is available */
	bool present(unsigned int level, unsigned int start)
	{
		return std::get<0>(get_block(level, start));
	}

	/** Get the k-oldest blocks of level "level" so they can be filled */
	iterator set(unsigned int level, unsigned int k)
	{
		assert(level < queue.size());
		assert(k > 0);
		auto &ring = queue[level];
		assert(ring.size() >= k);

		auto &end_pos	= std::get<0>(idx_pos[level]);
		auto &new_min 	= std::get<1>(idx_pos[level]);
		const auto size = queue[level].front().size();

		// std::cout << "set(" << level << "," << k << "): before\t";
		// print_level_rsas(level);

		// Rotate the ring
		unsigned int rotate = std::max<int>(0, end_pos + k - ring.size());
		end_pos = std::min<unsigned int>(ring.size(), end_pos + k);
		if (rotate != ring.size()) {
			ring.rotate(std::next(ring.begin(), rotate));
		}
		// Get the iterator to the first block to be returned and set it
		auto to_ret = std::next(ring.begin(), end_pos - k);

		auto end = std::next(to_ret, k);
		for (auto it = to_ret; it != end; it++) {
			it->set_start(new_min);
			auto b_size = std::min(size, text_len - new_min);
			it->resize(b_size);
			new_min = it->term();
		}

		// std::cout << "set(" << level << "," << k << "): after\t";
		// print_level_rsas(level);

		return to_ret;
	}

	void slide(unsigned int level, unsigned int quantity)
	{
		assert(queue[level].size() > 0);
		for (auto &i : queue[level]) {
			i.set_start(i.start() + quantity);
		}
		std::get<1>(idx_pos[level]) += quantity;
	}

	// Use the (defaulted) move constructors
	rsa_queue(const rsa_queue<T> &r) = delete;
	rsa_queue(rsa_queue<T> &&r) = default;
	// Use move assignment operator
	rsa_queue<T> &operator=(const rsa_queue<T> &r) = delete;
	rsa_queue<T> &operator=(rsa_queue<T> &&r) = default;
};

/* RSA getter (main class) */
template <typename T>
class rsa_getter {
public:
	typedef typename rsa_queue<T>::rsa_t rsa_t;
private:
	// Holds the RSA computed so far and not deallocated
	rsa_queue<T> queue;
	// Holds the RSA splitter
	std::vector<rsa_splitter<T, rsa_div_eq>> splitters;
	// The text length
	size_t t_len;

	// compute the first RSAs
	void b_compute_rsa(std::vector<unsigned int> d_cost_class)
	{
		assert(d_cost_class.size() >= 2);
		d_cost_class.insert(d_cost_class.begin(), 0);
		// The "last block size" is the size of the penultimate cost class
		const auto last_block_size = d_cost_class[d_cost_class.size() - 2] - d_cost_class[d_cost_class.size() - 3];
		// First, compute the threshold values
		d_cost_class.back() = t_len;
		auto &thresholds = d_cost_class;

		// Get the RSAs
		std::vector<rsa_t> rsas;
		auto c_level = -1;
		bool last_level = false;
		for (auto i = 0u; !last_level; i++) {
			assert(i + 1 < thresholds.size() - 1);
			const auto c_size 	= thresholds[i + 1] - thresholds[i];
			const auto level 	= queue.get_level(c_size);
			if (level != c_level) {
				assert(level == c_level + 1);
				c_level++;
				queue.slide(c_level, thresholds[i]);
			}
			assert(queue.levels() >= 1);
			last_level 		= (level == queue.levels() - 1);
			auto blocks 	= last_level ? std::get<2>(queue.level_info(level)) : 1;
			auto to_push 	= queue.set(c_level, blocks);
			rsas.insert(rsas.end(), to_push, std::next(to_push, blocks));
		}

		// Distribute
		assert(thresholds.size() >= 2);
		auto whole_sa 			= queue.get_sa();
		rsa_div_b uneven_split(thresholds, last_block_size);
		rsa_splitter<T, rsa_div_b> splitter(uneven_split);
		splitter.distribute(whole_sa, rsas.begin(), rsas.end());
	}

	void w_compute_rsa()
	{
		// Get every block of the last level
		const auto level = queue.levels() - 1;
		unsigned int size, start , blocks;
		std::tie(size, start, blocks) = level_info(level);
		// Generate everyone of these by partitioning the SA
		assert(blocks > 0);
		assert(start == 0);
		auto whole_sa 	= queue.get_sa();
		auto rsas		= queue.set(level, blocks);

		rsa_div_w even_split(rsas->size(), blocks);
		rsa_splitter<T, rsa_div_w> splitter(even_split);
		splitter.distribute(whole_sa, rsas, std::next(rsas, blocks));
	}

	void build_queue(
		std::vector<unsigned int> cost_length,
		std::vector<unsigned int> d_cost_class,
		std::shared_ptr<std::vector<T>> sa,
		bool is_b
	)
	{
		// Builds the queue descriptor (length, number of cost classes to keep, initial starting position)
		const auto n_cost_class = cost_length.size();
		assert(n_cost_class > 0);
		assert(cost_length.back() < t_len);

		std::vector<std::tuple<unsigned int, unsigned int>> queue_descriptor;
		// The number of block needed changes if we are generating B-blocks or W-blocks.
		if (is_b) {
			// For each level k except for the last, get Q(k + 1)/Q(k) blocks
			for (auto i = 0u; i < n_cost_class - 1; i++) {
				const auto number_cost_classes = cost_length[i + 1] / cost_length[i];
				assert(number_cost_classes >= 2);
				queue_descriptor.push_back(std::make_tuple(cost_length[i], number_cost_classes));
			}
			// For the last, get 1 + the number of blocks needed to "cover" the remainder of the SA.
			{
				const auto base_cost_classes 	= (d_cost_class.size() == 1 && d_cost_class.back() > cost_length.back()) ? 2 : 1;
				const auto number_cost_classes 	= base_cost_classes + std::ceil((1.0 * t_len - d_cost_class.back()) / cost_length.back());
				queue_descriptor.push_back(std::make_tuple(cost_length.back(), number_cost_classes));
			}
		} else {
			// For each level k except for the last, get Q(k + 1)/Q(k) blocks plus (floor of) D(k) / Q(k).
			for (auto i = 0u; i < n_cost_class - 1; i++) {
				auto number_cost_classes = cost_length[i + 1] / cost_length[i];
				assert(number_cost_classes >= 2);
				number_cost_classes 	+= d_cost_class[i] / cost_length[i];
				queue_descriptor.push_back(std::make_tuple(cost_length[i], number_cost_classes));
			}
			// For the last, get the whole length and divide by the last cost class
			{
				const auto number_cost_classes = std::ceil((1.0 * t_len) / cost_length.back());
				queue_descriptor.push_back(std::make_tuple(cost_length.back(), number_cost_classes));
			}			
		}
		// Finally, get the queue.
		queue = rsa_queue<T>(queue_descriptor, sa);
	}

	void initialize_splitters(std::vector<unsigned int> cost_length, bool is_b)
	{
		auto n_cost_class = cost_length.size();
		assert(cost_length.back() < t_len);

		if (is_b) {
			// No splitter for the last level
			--n_cost_class;
		} else {
			// Needed to compute the number of RSA of the last level to be computed en ensemble
			cost_length.push_back(t_len);
		}

		for (auto i = 0u; i < n_cost_class; i++) {
			auto blocks = std::ceil((1.0 * cost_length[i + 1]) / cost_length[i]);
			auto step = cost_length[i];
			splitters.push_back(rsa_splitter<T, rsa_div_eq>(rsa_div_eq(step, blocks)));
		}
	}

	// Private constructor
	rsa_getter(std::shared_ptr<std::vector<T>> sa, std::vector<unsigned int> d_cost_class, bool is_b) : t_len(sa->size())
	{
		/****
		 * This way we can treat bounded window as unbounded ones.
		 * This is because we are gonna erase the last cost class anyway.
		 ****/
		if (d_cost_class.back() < t_len) {
			d_cost_class.push_back(t_len);
		}

		// Refuse to run if d_cost_class or sa is empty
		assert(!sa->empty() && !d_cost_class.empty());

		// If we have just one cost class, then we just have to return the SA.
		if (d_cost_class.size() == 1) {
			assert(d_cost_class[0] >= t_len);
			queue = rsa_queue<T>(std::vector<std::tuple<unsigned int, unsigned int>>(), sa);
			return;
		}
		assert(d_cost_class[d_cost_class.size() - 2] < t_len);
		assert(d_cost_class.back() >= t_len);

		auto original_d_cost_class = d_cost_class;
		// Compute cost classes and remove the last cost class
		auto cost_length = get_cost_classes(d_cost_class, t_len);
		d_cost_class.pop_back();
		cost_length.pop_back();

		// Erase the second cost class if it has the same size as the first.
		// Useful for cleanly instantiate the queue (only distinct cost classes matters
		// and distance is needed to compute how many excess block to allocate, so no need
		// to keep the smallest distance among the twos) and for initializing the splitters
		// (same as above, but no need for the distance)
		if (cost_length.size() >= 2 && cost_length[0] == cost_length[1]) {
			cost_length.erase(cost_length.begin());
			d_cost_class.erase(d_cost_class.begin());
		}

		build_queue(cost_length, d_cost_class, sa, is_b);
		initialize_splitters(cost_length, is_b);

		// Compute the first RSAs, if B
		if (is_b) {
			// Here we need the ORIGINAL distances, because they define the boundaris of the first RSAs to be generated
			b_compute_rsa(original_d_cost_class);
		} else {
			w_compute_rsa();
		}
	}

public:

	rsa_t get(unsigned int level, unsigned int begin)
	{
		// queue.print_state();
		return queue.get(level, begin);
	}

	rsa<T> get_sa()
	{
		return queue.get_sa();
	}

	std::tuple<size_t, unsigned int, unsigned int> level_info(unsigned int level)
	{
		return queue.level_info(level);
	}

	// DEBUG
	rsa_queue<T> &get_queue()
	{
		return queue;
	}

	// See rsa_queue::get_level(size_t).
	int get_level(size_t size)
	{
		return queue.get_level(size);
	}

    size_t levels()
    {
        return queue.levels();
    }

	unsigned int notify(unsigned int offset)
	{
		// See what is the highest RSA to be generated
		if (queue.levels() == 0U) {
			return 0U;
		}
		int level;
		for (level = 0; static_cast<unsigned int>(level) < queue.levels() - 1; level++) {
			auto info = queue.level_info(level);
			if (std::get<1>(info) != offset) {
				break;
			}
		}

		const auto to_ret = level;
		// Generate all the RSA
		for (level = level - 1; level >= 0; level--) {
			auto rsa_parent 			= queue.get(level + 1, offset);
			auto positions_to_generate 	= std::min<int>(t_len - offset, rsa_parent.size());
			auto child_size 			= std::get<0>(queue.level_info(level));
			unsigned int children 		= std::ceil((1.0 * positions_to_generate) / child_size);
			auto rsas_level 			= queue.set(level, children);
			// TODO: controllare che funzioni anche con ultimo livello
			assert(static_cast<unsigned int>(level) < splitters.size());
//			std::cout << "Distributing from level " << level + 1 << "into "
//					  << (1.0 * positions_to_generate) / child_size << "children, startirting from " << rsa_parent.start() << std::endl;
			splitters[level].distribute(rsa_parent, rsas_level, std::next(rsas_level, children));
		}
		return to_ret;
	}

	// We don't want to perform pricey copies
	rsa_getter(const rsa_getter<T> &t) = delete;

	rsa_getter(rsa_getter<T> &&t) = default;

	/************
		Gets a getter for W-blocks.
		* d_cost_class 	sequence of maximum distances for each cost class. No need to adjust the last cost class to be = text length.
		* sa 			the Suffix Array.
	 *********** */
	static rsa_getter get_w_getter(std::vector<unsigned int> d_cost_class, std::shared_ptr<std::vector<T>> sa)
	{
		return rsa_getter(sa, d_cost_class, false);
	}

	/************
		Gets a getter for B-blocks.
		* d_cost_class 	sequence of maximum distances for each cost class. No need to adjust the last cost class to be = text length.
		* sa 			the Suffix Array.
	 *********** */
	static rsa_getter get_b_getter(std::vector<unsigned int> d_cost_class, std::shared_ptr<std::vector<T>> sa)
	{
		return rsa_getter(sa, d_cost_class, true);
	}
};

#endif
