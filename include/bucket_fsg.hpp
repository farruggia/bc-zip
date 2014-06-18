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

#ifndef BUCKET_FSG_HPP
#define BUCKET_FSG_HPP

#include <common.hpp>
#include <utilities.hpp>


template <typename gen_fact>
class bucket_fsg {
private:
	size_t bucket_size;
	unsigned int bucket_idx;
	text_info ti;
	sa_getter &sa_cache;
	std::shared_ptr<typename gen_fact::fsg_t> gen;
	cost_model cm;
	std::vector<edge_t> maxedges;

	bool next_gen()
	{
#ifndef NDEBUG
		std::cout << "Generating bucket " << bucket_idx << std::endl;
#endif
		if (bucket_idx * bucket_size >= ti.len) {
			return false;
		}
		byte *start = ti.text.get() + bucket_size * bucket_idx;
		size_t len = std::min(bucket_size, ti.len - bucket_size * bucket_idx);
#ifndef NDEBUG
		std::cout << "Bucket length: " << len << std::endl;
#endif
		std::shared_ptr<byte> text(start, null_deleter<byte>());
		text_info new_ti(text, len);
		auto new_gen = gen_fact(new_ti, sa_cache).instantiate(cm);
		std::swap(gen, new_gen);
		bucket_idx++;
		return true;
	}

public:
	bucket_fsg(text_info ti, sa_getter &sa_cache, size_t bucket_size, cost_model cm)
		: bucket_size(bucket_size), bucket_idx(0U), ti(ti), sa_cache(sa_cache), cm(cm)
	{
		next_gen();
		maxedges.resize(gen->get_edges().size());
	}

	std::vector<edge_t> &get_edges()
	{
		return maxedges;
	}

	inline bool gen_next(std::uint32_t *generated)
	{
		while (gen->gen_next(generated) == false) {
			if (next_gen() == false){
				*generated = 0;
				return false;
			}
		}

		auto &gen_max = gen->get_edges();
		std::copy(gen_max.begin(), std::next(gen_max.begin(), *generated), maxedges.begin());
		return true;
	}
};

#endif // BUCKET_FSG_HPP
