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

#ifndef _SCAN_ALG_
#define _SCAN_ALG_

#include <tuple>
#include <vector>
#include <limits>
#include <algorithm>
#include <deque>
#include <iterator>
#include <assert.h>
#include <boost/circular_buffer.hpp>

template <typename Container_, int Idx>
class vector_adapter {
private:
	Container_ v;
	typedef decltype(std::get<Idx>(v[0])) T;
public:
	vector_adapter(Container_ v) : v(v)
	{

	}

	T operator[](int idx)
	{
		return std::get<Idx>(v[idx]);
	}

	size_t size()
	{
		return v.size();
	}
};

/**
 * Element i + shift is accessed as i.
 */
template <typename Iterator_>
class shift_view {
private:
	Iterator_ begin;
	size_t size_;
	int shift;
	typedef decltype (*begin) T;
public:
	shift_view(Iterator_ begin_, size_t size_, int shift)
		: begin(begin_ - shift), size_(size_), shift(shift)
	{

	}

	T operator[](int idx)
	{
		assert(idx >= shift);
		assert(idx < size_ + shift);
		return *std::next(begin, idx);
	}

	size_t size()
	{
		return size_;
	}
};

template <typename Iterator_>
shift_view<Iterator_> get_view(Iterator_ begin, size_t size, int shift)
{
	return shift_view<Iterator_>(begin, size, shift);
}

template <int idx, typename Container_>
vector_adapter<Container_, idx> get_container(Container_ &&v)
{
//	std::cout << "get_container, lvalue reference: " << std::boolalpha << std::is_lvalue_reference<Container_>::value << std::endl;
	return vector_adapter<Container_, idx>(std::forward<Container_>(v));
}

template <int idx, typename Iterator_>
vector_adapter<shift_view<Iterator_>, idx> get_shift_adapter(Iterator_ begin, size_t size, int shift)
{
//	std::cout << "Passing by value" << std::endl;
	return get_container<idx>(get_view(begin, size, shift));
}

template <typename T, typename output_t>
class max_queue {
private:
	T first_b;
	unsigned int max_dst;
	// We put in output[i] the element associated to i.
	output_t output;
	boost::circular_buffer<T> &queue;
public:

	max_queue(T first_b, unsigned int max_dist, output_t output, boost::circular_buffer<T> &queue)
		: first_b(first_b), max_dst(max_dist), output(output), queue(queue)
	{

	}

	void update(T p)
	{
		if (p < first_b) {
			unsigned int pu = static_cast<unsigned int>(p);
			while (!queue.empty() && pu + max_dst >= static_cast<unsigned int>(queue.front())) {
				output[queue.front()] = p;
				queue.pop_front();
			}
		} else {
			while (!queue.empty() && queue.back() > p) {
				output[queue.back()] = p;
				queue.pop_back();
			}
			queue.push_back(p);
		}
	}

	void finish()
	{
		for (auto i : queue) {
			// We can represent texts up to std::numerical_limits<T>::max();
			output[i] = std::numeric_limits<T>::max();
		}
		queue.clear();
	}
};


class smart_find {
private:
	template <int tuple_idx, typename T, typename Iter>
	void smart_scan(Iter begin, Iter end, T first_b, unsigned int max_dist, std::vector<std::tuple<T,T>> &result, boost::circular_buffer<T> &queue)
	{
		auto output_view = get_shift_adapter<tuple_idx>(result.begin(), result.size(), first_b);
		typedef typename std::remove_reference<decltype(output_view)>::type output_t;
		max_queue<T, output_t> m_q(first_b, max_dist, output_view, queue);
		for (auto it = begin; it != end; it++) {
			m_q.update(*it);
		}
		m_q.finish();
	}
public:
	template <typename T, typename Iter>
	void operator()(Iter begin, Iter end, T first_b, unsigned int max_dist, std::vector<std::tuple<T,T>> &result)
	{
		const auto b_size = result.size();
		boost::circular_buffer<T> queue(b_size);

		// Successors
		smart_scan<1>(begin, end, first_b, max_dist, result, queue);
		// Predecessors
		auto rbegin = std::reverse_iterator<Iter>(end);
		auto rend = std::reverse_iterator<Iter>(begin);
		smart_scan<0>(rbegin, rend, first_b, max_dist, result, queue);
	}
};

class split_max_match {
private:
	template <int out_tuple_idx,typename T, typename Iter>
	void fill_output(
			Iter begin, Iter end, T first_b, unsigned int max_dist,
			std::vector<std::tuple<T,T>> &result, std::vector<boost::circular_buffer<T>> &buffers
	)
	{
		auto view = get_shift_adapter<out_tuple_idx>(result.begin(), result.size(), first_b);
		typedef typename std::remove_reference<decltype(view)>::type out_t;
		std::vector<max_queue<T, out_t>> Q;
		for (auto i = 0U; i < buffers.size(); i++) {
			Q.push_back(max_queue<T, out_t>(first_b + i * max_dist, max_dist, view, buffers[i]));
		}
		auto first = (first_b < max_dist) ? 0 : first_b - max_dist;
		for (auto it = begin; it != end; it++) {
			auto el = *it;
			if (el < first) {
				continue;
			}
			auto zone = el < first_b ? 0 : 1 + (el - first_b) / max_dist;
			if (zone > 0) {
				Q[zone - 1].update(el);
			}
			if (zone < Q.size()) {
				Q[zone].update(el);
			}
		}
		for (auto &i : Q) {
			i.finish();
		}
	}

public:
	template <typename T, typename Iter>
	void operator()(Iter begin, Iter end, T first_b, unsigned int max_dist, std::vector<std::tuple<T,T>> &result)
	{
		if (result.size() <= max_dist) {
			smart_find finder;
			finder(begin, end, first_b, max_dist, result);
			return;
		}
		const size_t queues		= std::ceil(1.0 * result.size() / max_dist);
		std::vector<boost::circular_buffer<T>> buffers; //(queues, boost::circular_buffer<T>(max_dist));
		for (auto i = 0U; i < queues; i++) {
			size_t size = std::min<size_t>(max_dist, result.size() - max_dist * i);
			buffers.push_back(boost::circular_buffer<T>(size));
		}
		fill_output<1>(begin, end, first_b, max_dist, result, buffers);
		auto rbegin = std::reverse_iterator<Iter>(end);
		auto rend = std::reverse_iterator<Iter>(begin);
		fill_output<0>(rbegin, rend, first_b, max_dist, result, buffers);
	}
};

#endif
