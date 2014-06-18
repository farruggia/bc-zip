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

#ifndef __COMMON_
#define __COMMON_

#include <cstdint>
#include <memory>
#include <vector>
#include <limits>

typedef std::uint8_t byte;

/*--------------------------------------------------------------
 Class edge_t:
 Encapsulate a phrase in the parsing 
 ---------------------------------------------------------------*/
enum edge_kind {
	REGULAR,
	PLAIN
};

struct edge_t {
	std::uint32_t d;
	std::uint32_t ell;
	unsigned int cost_id;

	edge_t()
		: d(std::numeric_limits<std::uint32_t>::max()), ell(d), cost_id(d)
	{

	}

	/**
	 * @brief Builds a literal edge
	 * @param ell
	 *		Literal length
	 */
	edge_t(unsigned int ell)
		: d(0), ell(ell), cost_id(0)
	{

	}

	/**
	 * @brief Builds a copy edge
	 * @param d
	 *		Distance (bytes)
	 * @param ell
	 *		Length (bytes)
	 * @param cost_id
	 *		Cost id (w.r.t. the currently used cost model)
	 */
	edge_t(unsigned int d, unsigned int ell, unsigned int cost_id)
		: d(d), ell(ell), cost_id(cost_id)
	{

	}

	inline void set(std::uint32_t d, std::uint32_t len, std::uint32_t cost_id)
	{
		this->d = d;
		this->ell = len;
		this->cost_id = cost_id;
	}

	inline edge_kind kind() const noexcept
	{
		return (d > 0) ? REGULAR : PLAIN;
	}

	bool invalid() const
	{
		return d == std::numeric_limits<std::uint32_t>::max();
	}
};


template<typename T, typename ...Args>
std::unique_ptr<T> make_unique( Args&& ...args )
{
    return std::unique_ptr<T>( new T( std::forward<Args>(args)... ) );
}

struct text_info {
	std::shared_ptr<byte> text;
	size_t len;
	text_info()
	{

	}

	text_info(std::shared_ptr<byte> text, size_t len) : text(text), len(len) 
	{ 

	}
    text_info(byte *text, size_t len) 
        : text(std::shared_ptr<byte>(text, std::default_delete<byte[]>())), len(len) 
    { 

    }
};

/**
 * A deleter which does not delete anything.
 */
template <typename T>
struct null_deleter {
	null_deleter() = default;

	template <typename U>
	null_deleter(const null_deleter<U> &u)
	{
	}

	void operator()(T *)
	{
	}
};

#endif
