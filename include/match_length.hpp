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

#ifndef __MAXIMAL_LENGTH
#define __MAXIMAL_LENGTH

#include <limits>
#include <tuple>
#include <assert.h>

template <typename T>
class maximal_length {
private:
	/** Pointer to beginning of text */
	T *text;
	/** Pointer to end-of-text */
	T *end;
	/** Previously queried element */
	std::int_fast64_t previous_pos;
	unsigned int pmatch;
	unsigned int smatch;

	void update_match(unsigned int position, unsigned int current, unsigned int &match)
	{
		T *match_p 	= text + position + match;
		T *cur_p	= text + current + match;
		assert(match_p < cur_p);
		while (cur_p < end && *match_p == *cur_p) {
			match_p++;
			cur_p++;
		}
		match = cur_p - (text + current);
	}

public:
	maximal_length(T * text, size_t len) : text(text), end(text + len), previous_pos(-1), pmatch(0), smatch(0) { }

	/**
	 * @brief Find the longest match between T[pos] and T[pred] and T[succ].
	 * @param pred
	 *		The predecessor of T[pos]
	 * @param succ
	 *		The successor of T[pos]
	 * @param pos
	 *		The current position
	 * @return
	 *
	 */
	std::tuple<unsigned int, unsigned int> match(unsigned int pred, unsigned int succ, unsigned int pos)
	{
		using std::make_tuple;
		auto distance = pos - previous_pos;
		previous_pos = pos;
		pmatch = pmatch > distance ? pmatch - distance : 0U;
		smatch = smatch > distance ? smatch - distance : 0U;
		auto no_match = std::numeric_limits<unsigned int>::max();
		if (pred != no_match) {
			update_match(pred, pos, pmatch);
		}
		if (succ != no_match) {
			update_match(succ, pos, smatch);
		}

		if (pmatch == 0 && smatch == 0) {
			return make_tuple(0, 0);
		}
		return pmatch > smatch ? make_tuple(pos - pred, pmatch) : make_tuple(pos - succ, smatch);
	}
};

#endif
