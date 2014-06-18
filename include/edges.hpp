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

#ifndef EDGES_HPP
#define EDGES_HPP

#include <limits>
#include <memory>
#include <vector>
#include <common.hpp>
#include <cost_model.hpp>

/**********     EDGES   **************/
struct solution {
	std::vector<edge_t> edges;
	/** Empty if dummy */
	std::string	encoder_name;

	template <typename T>
	solution(T &&edges, std::string encoder_name)
		: edges(std::forward<std::vector<edge_t>>(edges)), encoder_name(encoder_name)
	{

	}

	template <typename T>
	solution(T &&edges)
		: solution(std::forward<std::vector<edge_t>>(edges), "")
	{

	}

	solution(solution &&sol)
		: edges(std::move(sol.edges)), encoder_name(sol.encoder_name)
	{
	}

	/* Forbid copying around expensive solutions */
	solution(const solution &other) = delete;
};

/********** FORWARD DECLARATIONS     ***********/
class edge_cost;
class ec_factory;
class bi_factory;

/********** EDGE COSTS: WRAPPERS    ***********/
class edge_cost {
private:
	double cost;

	edge_cost(double cost) : cost(cost) { }

public:
	/** Il tipo della factory */
	typedef ec_factory factory_type;

	edge_cost() : cost(std::numeric_limits<double>::max()) { }

	edge_cost(const edge_t &edge, const cost_model &cm)
		: cost(cm.edge_cost(edge))
	{}

	bool operator<(const edge_cost &other) const { return cost < other.cost; }

	bool operator==(const edge_cost &other) const { return cost == other.cost; }

	edge_cost operator+(const edge_cost &other) const
	{
		edge_cost to_ret(cost);
		to_ret += other;
		return to_ret;
	}

	edge_cost &operator+=(const edge_cost &other)
	{
		cost += other.cost;
		return *this;
	}

	edge_cost operator-(const edge_cost &other) const
	{
		edge_cost to_ret(cost);
		to_ret -= other;
		return to_ret;
	}

	edge_cost &operator-=(const edge_cost &other)
	{
		cost -= other.cost;
		return *this;
	}

	double get_value() const
	{
		return cost;
	}

	void zero()
	{
		cost = 0.0;
	}

//	edge_cost &add_cost(double amount)
//	{
//		cost += amount;
//		return *this;
//	}
};

class bi_edge_cost {
protected:
	double cost;
	double weight;

	bi_edge_cost(double cost, double weight) : cost(cost), weight(weight) { }

public:

	typedef bi_factory factory_type;

	bi_edge_cost() : bi_edge_cost(std::numeric_limits<double>::max(), std::numeric_limits<double>::max())
	{

	}

	bi_edge_cost(const edge_t &edge, const cost_model &cost_cm, const cost_model &weight_cm)
		: bi_edge_cost(cost_cm.edge_cost(edge), weight_cm.edge_cost(edge))
	{

	}

	bool operator<(const bi_edge_cost &other) const
	{
		if (cost != other.cost) {
			return cost < other.cost;
		} else {
			return weight < other.weight;
		}
	}

	bool operator==(const bi_edge_cost &other) const
	{
		return cost == other.cost && weight == other.weight;
	}

	bi_edge_cost operator+(const bi_edge_cost &other) const
	{
		bi_edge_cost to_ret(cost, weight);
		to_ret += other;
		return to_ret;
	}

	bi_edge_cost &operator+=(const bi_edge_cost &other)
	{
		cost += other.cost;
		weight += other.weight;
		return *this;
	}

	bi_edge_cost operator-(const bi_edge_cost &other) const
	{
		bi_edge_cost to_ret(cost, weight);
		to_ret -= other;
		return to_ret;
	}

	bi_edge_cost &operator-=(const bi_edge_cost &other)
	{
		cost -= other.cost;
		weight -= other.weight;
		return *this;
	}

	void zero() { cost = weight = 0.0; }

	double get_value() const { return cost; }
};

/********** EDGE COSTS: FACTORIES   ***********/
class ec_factory {
private:
    cost_model cm;
public:

    ec_factory(cost_model cm) : cm(cm) { }

	edge_cost get(const edge_t &edge) const
    {
        return edge_cost(edge, cm);
    }
};

class bi_factory {
private:
    cost_model cost_cm;
    cost_model weight_cm;
public:
    bi_factory(cost_model cost_cm, cost_model weight_cm)
        : cost_cm(cost_cm), weight_cm(weight_cm)
    {}

	bi_edge_cost get(const edge_t &edge) const
    {
		return bi_edge_cost(edge, cost_cm, weight_cm);
    }
};
#endif // EDGES_HPP
