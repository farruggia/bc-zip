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

#include <sys/time.h>
#include <assert.h>
#include <algorithm>
#include <generators.hpp>

#include <utilities.hpp>
#include <generators.hpp>
#include <c_time.hpp>

//unsigned int convert_costs(phrase *solution, size_t text_len, CostModel *cm)
//{
//    phrase *end = solution + text_len;
//    unsigned int total_cost = 0;
//    unsigned int sym_cost = static_cast<unsigned int>(cm->symbol_cost(0.0));
//    while (solution < end) {
//        unsigned int l = solution->len, d = solution->d;
//        if (d > 0) {
//            // FIXME: virtual function + vector copying in a tight loop. Mmh.
//            total_cost += static_cast<unsigned int>(cm->get_cost(d, l, 0.0));
//            solution->cost = total_cost;
//            solution += l;
//        } else {
//            total_cost += sym_cost;
//            solution->cost = total_cost;
//            solution++;
//        }
//    }
//    // Convert the last cost
//    phrase *end_pred = end - end->len;
//    end->cost = end_pred->cost;
//    return total_cost;
//}

std::shared_ptr<std::vector<std::int32_t>> get_sa(byte *s, size_t len)
{
    uint8_t *c_string = reinterpret_cast<uint8_t*>(s);
    auto sa = std::make_shared<std::vector<std::int32_t>>(len);
    auto ret = divsufsort(c_string, sa->data(), len);
    if (ret != 0) {
        throw std::logic_error("SA construction failed");
    }
    return sa;
}

std::shared_ptr<std::vector<std::int32_t>> get_isa(const std::vector<std::int32_t> *sa)
{
    auto &rsa = *sa;
    auto isa = std::make_shared<std::vector<std::int32_t>>(rsa.size() + 1);
    auto length = rsa.size();
    auto &risa = *(isa.get());
    for (unsigned int i = 0; i < length; i++) {
        risa[rsa[i]] = i;     
    }
    return isa;
}

std::vector<unsigned int> get_cost_classes(std::vector<unsigned int> dst, size_t t_len)
{
    assert(!dst.empty());
	dst.back() = std::min(dst.back(), static_cast<unsigned int>(t_len));
    auto to_ret = dst;
    for (auto i = to_ret.size() - 1; i > 0; i--) {
        to_ret[i] -= to_ret[i - 1];
    }
    return std::move(to_ret);
}

std::tuple<std::shared_ptr<std::uint8_t>, size_t> get_file(const char *file_name)
{
    size_t file_length;
    auto file = get_file<std::uint8_t>(file_name, &file_length);
    auto s_file = std::shared_ptr<std::uint8_t>(file.release(), std::default_delete<std::uint8_t[]>());
    return std::make_tuple(s_file, file_length);
}

sa_cacher::sa_cacher()
{
}

std::shared_ptr<std::vector<std::int32_t>> sa_cacher::get(byte *begin, byte *end)
{
	if (cache.find(std::make_tuple(begin, end)) == cache.end()) {
		cache[std::make_tuple(begin, end)] = get_sa(begin, std::distance(begin, end));
	}
	return cache[std::make_tuple(begin, end)];
}

sa_instantiate::sa_instantiate()
{

}

std::shared_ptr<std::vector<std::int32_t>> sa_instantiate::get(byte *begin, byte *end)
{
	return get_sa(begin, std::distance(begin, end));
}

std::vector<unsigned int> normalize_dst(const std::vector<unsigned int> &dst, size_t t_len)
{
	std::vector<unsigned int> to_ret;
	for (auto i : dst) {
		to_ret.push_back(i);
		if (i >= t_len) {
			to_ret.back() = t_len;
			break;
		}
	}
	return to_ret;
}



correctness_report check_correctness(const std::vector<edge_t> &sol, byte *start)
{
	byte *ptr = start;
	auto end = std::prev(sol.end(), 1);
	for (auto it = sol.begin(); it != end; ptr += it->ell, std::advance(it, it->ell)) {
		edge_t edge = *it;
		if (	ptr - edge.d < start ||
				ptr + edge.ell >= start + sol.size() ||
				!std::equal(ptr, ptr + edge.ell, ptr - edge.d)
		)
		{
			return correctness_report(false, ptr - start, edge.d, edge.ell);
		}
	}
	return correctness_report(true, 0, 0, 0);
}

distance_kind get_kind(std::vector<unsigned int> dst)
{
	if (dst.size() == 0) {
		throw std::logic_error("Zero distance-vector passed to get_kind");
	} else if (dst.size() == 1) {
		return ALL_SAME;
	}

	if (dst.back() == std::numeric_limits<unsigned int>::max()) {
		dst.pop_back();
	}

	std::vector<unsigned int> sizes(dst);

	if (std::find(dst.begin(), dst.end(), 0U) != dst.end()) {
		throw std::logic_error("get_kind: zero distance found");
	}
	for (auto i = 1u; i < dst.size(); i++) {
		if (dst[i] < dst[i - 1]) {
			throw std::logic_error("get_kind: non-monotonous distances passed");
		}
		sizes[i] = dst[i] - dst[i - 1];
	}

	bool multiple = true;
	bool first_time = true;
	for (auto i = 0U; i < sizes.size() - 1; i++) {
		if ((sizes[i + 1] < sizes[i]) || (sizes[i + 1] % sizes[i]) != 0) {
			return GENERIC;
		}
		if (sizes[i + 1] / sizes[i] == 1 && !first_time) {
			multiple = false;
			break;
		}
		first_time = false;
	}
	if (multiple == true) {
		return MULTIPLE;
	}
	for (auto i = 0U; i < sizes.size() - 1; i++) {
		if (sizes[i + 1] != sizes[i]) {
			return GENERIC;
		}
	}
	return ALL_SAME;
}

std::string kind_name(distance_kind kind)
{
	switch (kind) {
	case REGULAR:
		return "REGULAR";
	case ALL_SAME:
		return "ALL_SAME";
	default:
		return "MULTIPLE";
	}
}

bool compatible(distance_kind cm_kind, distance_kind gen_kind)
{
	if (gen_kind == GENERIC) {
		return true;
	}
	return cm_kind == gen_kind;
}

std::string suggest_gen(distance_kind kind)
{
	switch (kind) {
	case REGULAR:
		return fsg_fact::name();
	case ALL_SAME:
		return same_fsg_fact::name();
	default:
		assert(kind == MULTIPLE);
		return ffsg_fact::name();
	}
}

std::string get_cm_rep(const cost_model &cm)
{
	std::stringstream ss;
	ss << "Distances:\n";
	for (auto i = 0U; i < cm.get_dst().size(); i++) {
		ss << cm.get_dst()[i] << "\n";
	}

	ss << "Lengths:\n";
	for (auto i = 0U; i < cm.get_len().size(); i++) {
		ss << cm.get_len()[i] << "\n";
	}

	ss << "Costs:\n";
	for (auto i = 0U; i < cm.get_dst().size(); i++) {
		ss << "D = " << cm.get_dst()[i] << "\n";
		for (auto j = 0U; j < cm.get_len().size(); j++) {
			ss << "L = " << cm.get_len()[j] << " : " << cm.get_cost(i, j) << "\t";
		}
		ss << "\n";
	}

	ss << "Base lit_cost: " << cm.lit_cost(0) << "\n";
	ss << "Char lit cost: " << cm.lit_cost(1) - cm.lit_cost(0) << "\n";

	return ss.str();
}
