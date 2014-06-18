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

#ifndef __API__IMPL__
#define __API__IMPL__

#include <common.hpp>
#include <phrase_reader.hpp>

/**
 * Logic needed to implement some of this API, not meant to be used directly in client code
 */
namespace bczip {
namespace impl {

template <typename edge_it_t>
class proper_nexts {
private:
	byte *to_patch;
	size_t comp_len;
	size_t orig_len;
	edge_it_t lits;
	byte *output;
public:

	proper_nexts(
		byte *to_patch, size_t comp_len, size_t orig_len, edge_it_t lits, byte *output
	)
		: to_patch(to_patch), comp_len(comp_len), orig_len(orig_len), lits(lits), output(output)
	{

	}

	template <typename encoder_>
	void run()
	{
		typedef typename encoder_::encoder enc_t;
		// Get encoder and reader
		lzopt::phrase_reader<encoder_> reader(to_patch, orig_len);
		enc_t enc(output, comp_len);

		// Start patching
		while (!reader.end()) {
			unsigned int dst, len;
			reader.next(dst, len);
			// assert(*lits == last_lits);
			if (dst > 0) {
				assert(len > 0);
				enc.encode(dst, len);
			} else {
				reader.adjust_next(*lits);
				byte *run = reader.get_buffer();
				enc.encode(run, len, *lits++);
			}
		}
	}
};
}
}

#endif