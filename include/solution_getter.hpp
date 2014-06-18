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

#ifndef __SOLUTION_GETTER_HPP
#define __SOLUTION_GETTER_HPP

#include <graph_cache.hpp>
#include <optimal_parser.hpp>
#include <meter_printer.hpp>

class invoker {
protected:
	cost_model cm;
	text_info ti;
	size_t literal_window;
public:

	invoker(cost_model cm, text_info ti, size_t literal_window) 
		: cm(cm), ti(ti), literal_window(literal_window)
	{

	}


	cost_model get_cm()
	{
		return cm;
	}

	virtual ~invoker()
	{

	}	
};

template <typename observer_t>
class single_invoker : public invoker {
public:

	single_invoker(cost_model cm, text_info ti, size_t literal_window) 
		: invoker(cm, ti, literal_window)
	{

	}

	template <typename fsg_t>
	std::vector<edge_t> invoke(fsg_t &&fsg, double *cost)
	{
		double sol_cost;
		auto to_ret = parse(ti, std::forward<fsg_t>(fsg), literal_window, cm, &sol_cost, observer_t(ti.len));
		if (cost != nullptr) {
			*cost = sol_cost;
		}
		return to_ret;
	}

	virtual ~single_invoker()
	{

	}

};

template <typename observer_t>
class double_invoker : public invoker {
private:
	cost_model w_cm;
public:

	double_invoker(cost_model cm, cost_model w_cm, text_info ti, size_t literal_window) 
		: invoker(cm, ti, literal_window), w_cm(w_cm)
	{

	}

	template <typename fsg_t>
	std::vector<edge_t> invoke(fsg_t &&fsg, double *cost)
	{
		double sol_cost;
		auto to_ret = bi_optimal_parse(ti, std::forward<fsg_t>(fsg), literal_window, cm, w_cm, &sol_cost, observer_t(ti.len));
		if (cost != nullptr) {
			*cost = sol_cost;
		}
		return to_ret;
	}

	virtual ~double_invoker()
	{

	}

};

// Assumption: we always pass compatible cost_models
template <typename observer_t = empty_observer, typename inner_fact_t = gen_ffsg_fact>
class solution_getter {
private:
	text_info ti;
	cached_graph cg;
	sa_cacher sc;
	size_t literal_window;

	template <typename Invoker_>
	std::vector<edge_t> invoke_fast(Invoker_ invoker, double *cost)
	{
		auto cm = invoker.get_cm();
		if (cg.empty()) {
			return full(cm);
		}
		auto cache_fsg = cached_fsg_fact<>(ti, sc, cg).instantiate(cm);
		return invoker.invoke(*cache_fsg.get(), cost);
	}	

	template <typename Invoker_>
	std::vector<edge_t> invoke_full(Invoker_ invoker, double *cost)
	{
		auto cm = invoker.get_cm();
		if (cg.empty()) {
			// Use caching
			auto caching_fsg = caching_fsg_fact<>(ti, sc, &cg).instantiate(cm);
			return invoker.invoke(*caching_fsg.get(), cost);
		} else {
			// Use regular generation
			auto full_fsg = inner_fact_t(ti, sc).instantiate(cm);
			return invoker.invoke(*full_fsg.get(), cost);
		}
	}

public:

	solution_getter()
	{

	}

	solution_getter(text_info ti, size_t literal_window) : ti(ti), literal_window(literal_window)
	{

	}

	std::vector<edge_t> fast(cost_model cm, double *cost = nullptr)
	{
		return invoke_fast(single_invoker<observer_t>(cm, ti, literal_window), cost);
	}

	std::vector<edge_t> full(cost_model cm, double *cost = nullptr)
	{
		return invoke_full(single_invoker<observer_t>(cm, ti, literal_window), cost);
	}

	std::vector<edge_t> fast(cost_model cm, cost_model w_cm, double *cost = nullptr)
	{
		return invoke_fast(double_invoker<observer_t>(cm, w_cm, ti, literal_window), cost);
	}

	std::vector<edge_t> full(cost_model cm, cost_model w_cm, double *cost = nullptr)
	{
		return invoke_full(double_invoker<observer_t>(cm, w_cm, ti, literal_window), cost);
	}

	bool warm()
	{
		return !cg.empty();
	}

};

#endif