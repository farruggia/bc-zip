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

#ifndef __GRAPH_CACHE_HPP
#define __GRAPH_CACHE_HPP

#include <generators.hpp>
#include <unaligned_io.hpp>
#include <encoders.hpp>
#include <utilities.hpp>

#include <memory>
#include <vector>

namespace unary_gammalike {

template <typename unary_type>
struct unary_info {
	const static constexpr size_t max_unary = sizeof(unary_type) * 8U - 1U;
	const static constexpr size_t unary_bytes = sizeof(unary_type);
};

template <typename cost_class, typename unary_type>
class encoder {
private:
	unaligned_io::writer w;
	size_t bits;
	byte *start_byte;
	const static constexpr size_t max_unary   = unary_info<unary_type>::max_unary;
	const static constexpr size_t unary_bytes = unary_info<unary_type>::unary_bytes;

	void unary_write(unsigned int bits)
	{
		unary_type to_write = 1;
		to_write <<= bits;
		w.write(to_write, bits + 1);
	}

public:
	encoder(byte *storage, size_t bits) : w(storage), bits(bits), start_byte(storage)
	{

	}

	template <typename T>
	void encode(T value)
	{
		if (value <= max_unary) {
			unary_write(value);
		} else {
			unary_type u = unary_type();
			w.write(u);
			gamma_like::encode<cost_class>(value - max_unary, w);
		}
		assert(w.writing_head() - start_byte <= std::ceil(1.0 * bits / 8));
	}

	static double ub_gamma()
	{
		assert(cost_class::cost_classes[1] > 1);
		return 1 + cost_class::binary_width[0] / 9.0;
	}
};

template <typename cost_class, typename unary_type>
class decoder {
private:
	unaligned_io::reader r;
	unsigned int bits;
	const static constexpr size_t max_unary   = sizeof(unary_type) * 8U - 1U;
	const static constexpr size_t unary_bytes = sizeof(unary_type);

public:
	decoder(byte *storage, unsigned int bits) : r(storage), bits(bits)
	{

	}

	template <typename T = unsigned int>
	T decode()
	{
		unary_type read = unary_type();
		r.peek(read);
		if (read == unary_type()) {
			r.skip_bytes(unary_bytes);
			return max_unary + gamma_like::decode<cost_class>(r);
		} else {
			unsigned int length;
			unary_suffix_length(read, length);
			r.skip_bits(length + 1);
			return length;
		}
	}
};

}

class cached_graph {
private:
	std::shared_ptr<byte> data_hold;
	byte *end;
	size_t stride;
public:
	cached_graph()
	{
		data_hold.reset();
		end = data_hold.get();
		stride = 0;
	}

	byte *get_data()
	{
		return data_hold.get();
	}

	byte *get_begin(unsigned int idx)
	{
		byte *to_ret = data_hold.get() + idx * stride;
		assert(to_ret < end);
		return to_ret;
	}

	size_t levels()
	{
		return allocated_size() / stride;
	}

	/**
	 * Return the size of each level, in bytes
	 */
	size_t level_size()
	{
		return stride;
	}

	size_t allocated_size()
	{
		return end - data_hold.get();
	}

	bool empty()
	{
		return allocated_size() == 0U;
	}

	/**
	 * Allocate storage. level_size is each level's size, in BYTES, while
	 * levels is the number of levels to cache.
	 */
	void set(size_t level_size, unsigned int levels)
	{
		stride = level_size;
		auto size = levels * stride;
		data_hold.reset(new byte[size], std::default_delete<byte[]>());
		auto ptr = data_hold.get();
		std::fill(ptr, ptr + size, 0U);
		end = ptr + size;
	}
};

template <typename inner_gen_t, typename encoder_t>
class caching_fsg_gen {
private:
	inner_gen_t gen;
	cached_graph *cg;
	std::vector<encoder_t> encoders;
	std::vector<unsigned int> prev_len;
	std::vector<std::int64_t> prev_pos;
public:
	template <typename inner_gen>
	caching_fsg_gen(inner_gen &&gen, cached_graph *cg, size_t levels, size_t t_len)
		: gen(std::forward<inner_gen_t>(gen)), cg(cg), prev_len(levels, 1), prev_pos(levels, -1)
	{
		auto max_size = std::ceil(encoder_t::ub_gamma() * t_len * 2);
		cg->set(max_size, levels);
		for (unsigned int i = 0; i < levels; i++) {
			encoders.push_back(encoder_t(cg->get_begin(i), max_size * 8U));
		}
	}

	std::tuple<unsigned int, unsigned int> max_match(unsigned int dst_idx)
	{
		auto i = gen.max_match(dst_idx);
		auto ell = std::get<1>(i);
		auto &p_len = prev_len[dst_idx];
		auto &p_pos = prev_pos[dst_idx];
		p_len = p_len > 0 ? p_len - 1 : 0;
		assert(
			(p_pos < text_pos() - 1) ||
			(p_pos == text_pos() - 1 && ell >= p_len)
		);
		if (p_pos == text_pos() - 1) {
			// If no jump, delta-encoding
			auto to_encode = ell - p_len;
			encoders[dst_idx].encode(to_encode);
		} else {
			// Otherwise, regular encoding
			encoders[dst_idx].encode(ell);
		}
		p_len = ell;
		p_pos = text_pos();
		return i;
	}

	size_t levels()
	{
		return gen.levels();
	}

	void pre_gen()
	{
		gen.pre_gen();
	}

	void post_gen()
	{
		gen.post_gen();
	}

	static distance_kind get_kind()
	{
		return inner_gen_t::get_kind();
	}

	std::uint32_t text_pos()
	{
		return gen.text_pos();
	}

	caching_fsg_gen(caching_fsg_gen &&other) = default;

	caching_fsg_gen &operator=(const caching_fsg_gen &) = delete;

	caching_fsg_gen &operator=(caching_fsg_gen &&) = default;
};

template <typename decoder_t>
class cached_fsg_gen {
private:
	std::vector<decoder_t> decoders;
	std::vector<unsigned int> dsts;
	std::vector<unsigned int> to_ret;
	unsigned int t_pos;
	size_t t_len;
	unsigned int cur_dst_idx;
	std::vector<unsigned int> previous_len;
	std::vector<std::int64_t> previous_pos;
public:
	cached_fsg_gen(cached_graph cg, std::vector<unsigned int> dsts, size_t t_len) 
		: dsts(dsts), t_pos(0), t_len(t_len), cur_dst_idx(0), previous_len(cg.levels(), 1), previous_pos(cg.levels(), -1)
	{
		for (auto i = 0; i < cg.levels(); i++) {
			decoders.push_back(decoder_t(cg.get_begin(i), cg.level_size() * 8));
		}
		to_ret = dsts;
		to_ret.insert(to_ret.begin(), 0);
		for (auto &i : to_ret) {
			++i;
		}
		to_ret.erase(std::prev(to_ret.end(), 1), to_ret.end());
	}

	std::tuple<unsigned int, unsigned int> max_match(unsigned int dst_idx)
	{
		auto ell = decoders[dst_idx].decode();
		auto dst = to_ret[dst_idx];
		auto &p_len = previous_len[dst_idx];
		auto &p_pos = previous_pos[dst_idx];
		p_len = p_len > 0 ? p_len - 1 : 0;
		if (p_pos == text_pos() - 1) {
			ell += p_len;
		}
		p_len = ell;
		p_pos = text_pos();
		return std::make_tuple(dst, ell);
	}

	size_t levels()
	{
		if (cur_dst_idx < dsts.size() - 1 && dsts[cur_dst_idx] < t_pos) {
			++cur_dst_idx;
		}
		return 1 + cur_dst_idx;
	}

	void pre_gen()
	{

	}

	void post_gen()
	{
		++t_pos;
	}

	static distance_kind get_kind()
	{
		return GENERIC;
	}

	std::uint32_t text_pos()
	{
		return t_pos;
	}

	cached_fsg_gen(cached_fsg_gen &&other) = default;

	cached_fsg_gen &operator=(const cached_fsg_gen &) = delete;

	cached_fsg_gen &operator=(cached_fsg_gen &&) = default;

};

template <
	typename fsg_fact_t = gen_ffsg_fact, 
	typename encoder_t = unary_gammalike::encoder<nibble::class_desc, byte>,
	template <class T> class protocol_t = fsg_protocol
>
class caching_fsg_fact 
	: public base_fsg_fact<caching_fsg_gen<typename fsg_fact_t::generator_t, encoder_t>, protocol_t>
{
private:
	text_info ti;
	sa_getter &sa;
	cached_graph *cg;
public:
	typedef caching_fsg_gen<typename fsg_fact_t::generator_t, encoder_t> gen_t;

	/* No need to comply to standard interface: won't be registered */
	caching_fsg_fact(text_info ti, sa_getter &sa, cached_graph *cg) 
		: base_fsg_fact<gen_t, protocol_t>(ti, sa), ti(ti), sa(sa), cg(cg)
	{

	}

	gen_t get_gen(const cost_model &cm)
	{
		// Obtain the generator
		auto generator = fsg_fact_t(ti, sa).get_gen(cm);
		// Get the number of needed levels
		auto dsts = cm.get_dst();
		auto levels = 1 + std::distance(dsts.begin(), std::lower_bound(ITERS(dsts), ti.len));
		// Move the gen to cached_fsg_gen and return the new instance
		return gen_t(std::move(generator), cg, levels, ti.len);
	}

	// No name: does not need to be registered.
};

template <
	typename decoder_t = unary_gammalike::decoder<nibble::class_desc, byte>,
	template <class T> class protocol_t = fsg_protocol
>
class cached_fsg_fact : public base_fsg_fact<cached_fsg_gen<decoder_t>, protocol_t> {
private:
	cached_graph cg;
	size_t t_len;
public:
	typedef cached_fsg_gen<decoder_t> gen_t;

	cached_fsg_fact(text_info ti, sa_getter &sa, cached_graph cg)
		: base_fsg_fact<gen_t, protocol_t>(ti, sa), cg(cg), t_len(ti.len)
	{

	}

	gen_t get_gen(const cost_model &cm)
	{
		return gen_t(cg, cm.get_dst(), t_len);
	}
};

#endif