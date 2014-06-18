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

#ifndef FACTORIES_HPP
#define FACTORIES_HPP

#include "fast_fsg.hpp"
#include "fsg.hpp"

template <typename T>
class ffsg_factory {
private:
	std::shared_ptr<byte> text;
	size_t length;
	class_info dst, len;
	std::shared_ptr<std::vector<T>> sa;
public:
	ffsg_factory(std::shared_ptr<byte> text, size_t length, class_info dst, class_info len)
		: text(text), length(length), dst(dst), len(len)
	{
		sa = get_sa(text.get(), length);
	}

	fast_fsg<T> get()
	{
		return fast_fsg<T>(text, length, sa, dst, len);
	}
};

#endif // FACTORIES_HPP
