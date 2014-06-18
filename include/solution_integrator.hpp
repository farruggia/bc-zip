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

#ifndef __SOLUTION_INTEGRATOR_HPP
#define __SOLUTION_INTEGRATOR_HPP

#include <solution_getter.hpp>
#include <generators.hpp>
#include <write_parsing.hpp>
#include <copy_routines.hpp>
#include <phrase_reader.hpp>
#include <parsing_manage.hpp>
#include <tuple>

class vector_in {
private:
	std::vector<edge_t> *sol;
	byte *text;
	unsigned int pos;
public:
	vector_in(std::vector<edge_t> *v, byte *text)
		: sol(v), text(text), pos(0)
	{

	}

	std::tuple<unsigned int, unsigned int, byte*> next()
	{
		auto &edge = (*sol)[pos];
		auto data = text;
		pos += edge.ell;
		text += edge.ell;
		return std::make_tuple(edge.d, edge.ell, data);
	}

	unsigned int nextliteral()
	{
		for (auto pos_ = pos + 1; pos_ != sol->size(); ++pos_)
		{
			if ((*sol)[pos_].kind() == PLAIN) {
				return pos_ - pos - 1;
			}
		}
		return sol->size() - pos;
	}
};

class vector_out {
private:
	std::vector<edge_t> *sol;
	byte *text;
	byte *start;
	byte *end;
	unsigned int pos;
	cost_model cm;
public:
	vector_out(std::vector<edge_t> *v, byte *text, byte *end, cost_model cm)
		: sol(v), text(text), start(text), end(end), pos(0), cm(cm)
	{

	}

	void push_lit(unsigned int len, byte *data, std::uint32_t)
	{
		(*sol)[pos] = edge_t(len);
		pos += len;
		text = std::copy(data, data + len, text);
		assert(text <= end);
	}

	void push(unsigned int dst, unsigned int len)
	{
		(*sol)[pos] = edge_t(dst, len, cm.get_id(dst, len));
		pos += len;
		assert(text - dst >= start);
		assert(text + len <= end);
		copy_fast(text, text - dst, len);
		text += len;
	}
};

template <typename enc_>
class compress_in {
private:
	lzopt::phrase_reader<enc_> pr;
public:
	/**
	 * @brief Instantiate the compressed adaptor
	 * @param data
	 *		Pointer to the compressed rep.
	 * @param length
	 *		Length of uncompressed data
	 */
	compress_in(byte *data, size_t text_length)
		: pr(data, text_length)
	{

	}

	std::tuple<unsigned int, unsigned int, byte*> next()
	{
		unsigned int dst, len;
		pr.next(dst, len);
		return std::make_tuple(dst, len, pr.get_buffer());
	}

	std::uint32_t nextliteral()
	{
		return pr.get_next();
	}

};

template <typename enc_t>
class compress_out {
private:
	typename enc_t::encoder	encoder;
public:
	compress_out(byte *data, size_t compress_size)
		: encoder(data, compress_size)
	{

	}

	void push_lit(size_t len, byte *data, std::uint32_t next)
	{
		encoder.encode(data, len, next);
	}

	void push(unsigned int dst, unsigned int len)
	{
		encoder.encode(dst, len);
	}
};

template <typename observer_t = empty_observer, typename inner_gen_fact_t = gen_ffsg_fact>
class solution_integrator {
private:
	inner_gen_fact_t gen_fact;
	cost_model cm;

	template <typename in_t, typename out_t>
	unsigned int fix(in_t &in, out_t &out, std::vector<edge_t> &edges)
	{
		unsigned int dst, len;
		byte *data;
		std::tie(dst, len, data) = in.next();
		// std::cout << "Input = " << dst << ", " << len << std::endl;
		if (dst == 0) {	
			// std::cout << "Output = 0, " << len << std::endl;
			out.push_lit(len, data, in.nextliteral());
			return len;
		} else {
			for (auto &i : edges) {
				if (i.ell == len) {
					// std::cout << "Output = " << i.d << ", " << len << std::endl;
					out.push(i.d, len);
					return len;
				}
			}
			throw std::logic_error("solution_integrator: a fixable edge found no match");
		}
	}

public:
	template <typename fact_t>
	solution_integrator(fact_t &&fact, cost_model cm)
		: gen_fact(std::forward<fact_t>(fact)), cm(cm)
	{

	}

	template <typename in_t, typename out_t>
	void integrate(std::vector<in_t> &in, std::vector<out_t> &out)
	{
		// Check if in/out cardinality is OK
		if (in.size() != out.size()) {
			throw std::logic_error("Solutions to be fixed and outputs does not match");
		}

		// Generate the fsg_protocol
		auto protocol_ptr = gen_fact.instantiate(cm);
		auto &protocol    = *(protocol_ptr.get());

		// Next positions to fix
		std::vector<unsigned int> next(in.size(), 0);

		// Generated edges
		auto &gen_edges = protocol.get_edges();

		// Fix'em
		const size_t t_len = protocol.get_tlen();
		std::uint32_t generated;
		// observer_t meter(t_len);

		unsigned int i = 0U, lowest_pos = 0U;
		while (lowest_pos < t_len) {
			auto to_skip = lowest_pos - i;
			for (auto j = 0; j < to_skip; j++) {
				protocol.gen_next(&generated);
				// meter.new_character();
			}
			i += to_skip;
			protocol.gen_next(&generated);
			// meter.new_character();

			auto round_min = std::numeric_limits<unsigned int>::max();
			for (auto j = 0U; j < next.size(); j++) {
				// Do we have to fix the current pos?
				if (next[j] == i) {
					// std::cout << "Position " << i << ", solution " << j << std::endl;
					auto len = fix(in[j], out[j], gen_edges);
					next[j] += len;
				}
				round_min = std::min<unsigned int>(round_min, next[j]);
			}
			lowest_pos = round_min;
			++i;

			// std::cerr << "[INTEGRATE] @ " << i << " / " << t_len << std::endl;
		}
		// std::cerr << "[INTEGRATE] Finished" << std::endl;
	}
};

template <typename enc_, typename integrator_t>
void integrate(std::vector<parsing> in, std::vector<parsing> out, integrator_t &integrate_)
{
	if (in.size() != out.size()) {
		throw std::logic_error("Requested integrating and integrated solution mismatch in cardinality");
	}
	std::vector<compress_in<enc_>> ins;
	std::vector<compress_out<enc_>> outs;
	for (auto i = 0U; i < in.size(); i++) {
		ins.push_back(compress_in<enc_>(in[i].begin, in[i].comp_len));
		outs.push_back(compress_out<enc_>(out[i].begin, out[i].comp_len));
	}
	integrate_.integrate(ins, outs);
}

#endif