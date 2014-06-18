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

#ifndef __INNER_PARSER_
#define __INNER_PARSER_

#include <memory>
#include <stdint.h>
#include <utility>
#include <vector>
#include <memory>
#include <boost/circular_buffer.hpp>
#include "cost_model.hpp"
#include "utilities.hpp"
#include "encoders.hpp"
#include "edges.hpp"

/******************* LITERAL INSERTERS ***********************/
// Minimum of a sliding window
template <typename T>
class s_window {
private:
	int t_pos;
	const int capacity;
	boost::circular_buffer<std::tuple<int, T>> increasing_seq;
public:
	s_window(size_t size) :
		t_pos(0u),
		capacity(size),
		increasing_seq(size)
	{

	}

	T update(T value)
	{
		using std::get;
		using std::tie;
		// Step one: put it on the increasing seq.
		while (!increasing_seq.empty() && value <= get<1>(increasing_seq.back())) {
			increasing_seq.pop_back();
		}
		increasing_seq.push_back(tie(t_pos, value));
		// Step two: remove last element if too old
		if (t_pos++ - capacity >= get<0>(increasing_seq.front())) {
			increasing_seq.pop_front();
		}
		// Step three: return the minimum
		return std::get<1>(increasing_seq.front());
	}
};

// A couple (position, cost) with ordering imposed by the cost
template <typename T>
struct pos_cost {
	unsigned int position;
	T cost;

	pos_cost() { }

	pos_cost(unsigned int pos, T cost) : position(pos), cost(cost)
	{}

	bool operator<(const pos_cost<T> &other)
	{
		return cost < other.cost;
	}

	bool operator==(const pos_cost<T> &other)
	{
		return cost == other.cost;
	}

	bool operator<=(const pos_cost<T> &other)
	{
		return cost < other.cost || cost == other.cost;
	}
};

// The literal inserter class
template <typename value_t>
class literal_inserter {
private:
	typedef typename value_t::factory_type v_factory;
	/** The edge to be inserted (get it via get_edge()) */
	edge_t edge;
	/** The moving window minimum */
	s_window<pos_cost<value_t>> get_minimum;
	unsigned int position;
	value_t penalty;
	value_t delta;

	value_t get_penalty(size_t text_len, const v_factory &value_fact)
	{
		return value_fact.get(edge_t(text_len));
	}

	value_t get_delta(const v_factory &value_fact)
	{
		return value_fact.get(edge_t(1)) - value_fact.get(edge_t(0));
	}

public:

	literal_inserter(size_t text_len, size_t win_size, v_factory value_fact)
		: edge(0), get_minimum(win_size), position(0),
		  penalty(get_penalty(text_len, value_fact)), delta(get_delta(value_fact))
	{
		assert(win_size > 0);
	}

	edge_t *get_edge()
	{
		return &edge;
	}

	void gen_next(value_t edge_cost)
	{
		// Add in the queue
		edge_cost += penalty;
		penalty -= delta;
		// Get the one with minimum value and extract its position
		auto source		= get_minimum.update(pos_cost<value_t>(position++, edge_cost)).position;
		// Return the distance between the NEXT position and the extracted one
		edge.ell		= position - source;
	}
};

/********************** THE OPTIMAL PARSER ***********************/
/**
 * NOTE: this design allows only for single-level literal edges.
 *		 To support multi-level literal edges, there must be a
 *		 set of plain_t (a vector), with each plain_t remembering
 *		 the minimum and maximum length. For the future, maybe.
 */
template <typename fsg_t, typename value_t, typename observer_t>
class optimal_parser {
private:
	typedef typename value_t::factory_type v_factory;
	fsg_t fsg;
	v_factory value_fact;
	literal_inserter<value_t> plain_gen;
	edge_t *back_edge;

	std::vector<edge_t> &max_edges;
    text_info text;

	observer_t observer;

//	/** Tells if a position has been visited */
//	bool unreached(const edge &, const value_t &cost)
//	{
//		return value_t() == cost;
//	}

	/**
	 * @brief
	 *      Reverses the direction of the edges in the optimal path
	 * @param sol
	 *      The parsing
	 */
	void flip(std::vector<edge_t> &sol)
	{
		// TODO: da controllare. Assumiamo che il path ottimale arrivi fino
		// all'ultimo edge.
		edge_t copy;
		std::int64_t cur_idx = sol.size() - 1;
		while (cur_idx >= 0) {
			auto &cur       = sol[cur_idx];
			cur_idx -= cur.ell;
			std::swap(cur, copy);
		}
	}

	void edge_relax(edge_t *sol_edge, value_t *sol_cost, edge_t &gen_edge)
    {
		auto new_cost	= *sol_cost + value_fact.get(gen_edge);
		auto next_cost	= std::next(sol_cost, gen_edge.ell);
		auto next_edge	= std::next(sol_edge, gen_edge.ell);

		if (new_cost < *next_cost) {
			*next_edge = gen_edge;
			*next_cost = new_cost;
//			std::cout << "Setting target cost to " << next_cost->get_value() << std::endl;
        }
	}

	void plain_relax(edge_t *sol_edge, value_t *sol_cost, edge_t &gen_edge)
	{
		size_t plain_len	= gen_edge.ell;
		assert(plain_len > 0U);
		edge_t *source_edge = std::prev(sol_edge, plain_len - 1U);
		value_t *source_cst	= std::prev(sol_cost, plain_len - 1U);
		edge_relax(source_edge, source_cst, gen_edge);
	}

public:

    template <typename T>
    optimal_parser(
            T &&fsg,
			size_t plain_range,
			v_factory value_fact,
			text_info text,
			observer_t observer
	)
		: fsg(std::forward<T>(fsg)), value_fact(value_fact), plain_gen(text.len, plain_range, value_fact),
		  max_edges(this->fsg.get_edges()), text(text), observer(observer)
    {
		assert(max_edges.size() > 0);
		back_edge = plain_gen.get_edge();
		assert(back_edge != nullptr);
    }

	std::vector<edge_t> parse(double *cost)
    {
		// Allocates the solution and the costs vector
		const size_t sol_len = text.len + 1;
		std::vector<edge_t> parsing(sol_len);
		std::vector<value_t> p_cost(sol_len);
		p_cost.front().zero();

		unsigned int generated	= 0;
//		auto next_pos			= parsing.begin();
//		auto next_cost			= p_cost.begin();

		for (unsigned int i = 0; i < text.len; i++) {
			auto &cur		= parsing[i];
			auto &cur_cst	= p_cost[i];

//			std::cout << "==== Processing vertex " << i << ", current cost: " << cur_cst.get_value() << std::endl;

            // By protocol, the following two functions must be invoked for each position.
            fsg.gen_next(&generated);
			plain_gen.gen_next(cur_cst); // TODO: take the one provided by back_relaxer as plain_edge
//			if (unreached(cur, cur_cst)) {
//				continue;
//			}
			if (generated > 0) {
				auto edge_ptr = max_edges.begin();
				for (unsigned int j = 0; j < generated; j++) {
//					std::cout << "Current edge to be evaluated: " << edge_ptr-> d << "\t" << edge_ptr->ell;
//					std::cout << ", cost: " << value_fact.get(*edge_ptr).get_value() << std::endl;
					edge_relax(&cur, &cur_cst, *edge_ptr++);
				}
			}
//			std::cout << "Plain edge to be evaluated: " << back_edge->d << "\t" << back_edge->ell;
//			std::cout << ", cost: " << value_fact.get(*back_edge).get_value() << std::endl;
			plain_relax(&cur, &cur_cst, *back_edge);

			observer.new_character();
        }
		flip(parsing);
//		for (auto i : parsing) {
//			std::cout << i.d << "\t" << i.ell << std::endl;
//		}
		*cost = p_cost.back().get_value();
		return std::move(parsing);
    }
};

#ifndef NDEBUG
struct print_cm {
void operator()(cost_model cm, size_t literal_window)
{
	using namespace std;
	cout << "==== Print CM info" << endl;
	cout << "== DST info:" << endl << "= ";
	auto win = cm.get_dst();
	for (auto i : win) {
		cout << i << "\t";
	}
	cout << endl << "= ";
//	for (auto i : len) {
//		cout << i << "\t";
//	}
//	cout << endl;

	cout << "== LEN info:" << endl << "= ";
	win = cm.get_len();
//	len = cm.get_len().costs;
	for (auto i : win) {
		cout << i << "\t";
	}
	cout << endl << "= ";
//	for (auto i : len) {
//		cout << i << "\t";
//	}
//	cout << endl;

	cout << "== Literal win: " << literal_window << endl;
	cout << "== Literal cost one literal: " << cm.lit_cost(1U) << endl;
	cout << "== Literal cost two literals: " << cm.lit_cost(2U) << endl;
}
};
#endif

/********************* MAIN FUNCTIONS **********************************/
template <typename fsg_t, typename observer_t>
std::vector<edge_t> parse(text_info text, fsg_t &&fsg, size_t literal_window,
						  cost_model cm, double *cost, observer_t observer)
{
#ifndef NDEBUG
	print_cm()(cm, literal_window);
#endif
	// (a) Obtain the value factory
	ec_factory value_factory(cm);
	// (b) instantiate the optimal parser
	optimal_parser<fsg_t, edge_cost, observer_t> parser(std::forward<fsg_t>(fsg), literal_window, value_factory, text, observer);
	// (c) return the parsing
	return parser.parse(cost);
}

template <typename fsg_t, typename observer_t>
std::vector<edge_t> bi_optimal_parse(text_info text, fsg_t &&fsg, size_t literal_window,
									   cost_model cost_cm, cost_model weight_cm, double *cost, observer_t observer)
{
	// (a) Obtain the value factory
	bi_factory value_factory(cost_cm, weight_cm);
	// (b) instantiate the optimal parser
	optimal_parser<fsg_t, bi_edge_cost, observer_t> parser(std::forward<fsg_t>(fsg), literal_window, value_factory, text, observer);
	// (c) return the parsing
	return parser.parse(cost);
}

template <typename fsg_t, typename observer_t>
std::vector<edge_t> cost_optimal_parse(text_info text, fsg_t &&fsg, size_t literal_window,
									   cost_model cost_cm, cost_model weight_cm, double *cost, observer_t observer)
{
	return bi_optimal_parse(text, std::forward<fsg_t>(fsg), literal_window, cost_cm, weight_cm, cost, observer);
}

template <typename fsg_t, typename observer_t>
std::vector<edge_t> weight_optimal_parse(text_info text, fsg_t &&fsg, size_t literal_window,
										 cost_model cost_cm, cost_model weight_cm, double *cost, observer_t observer)
{
	return bi_optimal_parse(text, std::forward<fsg_t>(fsg), literal_window, weight_cm, cost_cm, cost, observer);
}
#endif
