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

#ifndef __FACITILITIES_HPP_
#define __FACITILITIES_HPP_

#include <string>
#include <sstream>
#include <memory>
#include <c_time.hpp>
#include <time.h>


// Expands to the pair of iterators of v.
#define ITERS(v) v.begin(), v.end()

/** Return the number of bits of the minimal binary rep. of u */
size_t bits(unsigned int u);

template <typename ...T>
struct joiner;

template <typename T, typename ...U>
struct joiner<T, U...> {
	static std::string join(T t, U... u)
	{
		std::stringstream ss;
		ss << t;
		return ss.str() + joiner<U...>::join(u...);
	}
};

template <>
struct joiner<> {
	static std::string join()
	{
		return "";
	}
};

template <typename ...T>
std::string join_s(T... t)
{
	return joiner<T...>::join(t...);
}

template <typename T>
std::shared_ptr<T> make_shared(std::unique_ptr<T[]> &&p)
{
	return std::shared_ptr<T>(p.release(), std::default_delete<T[]>());
}

template <typename T>
std::shared_ptr<T> make_shared(std::unique_ptr<T> &&p)
{
	return std::shared_ptr<T>(p.release());
}

#endif
