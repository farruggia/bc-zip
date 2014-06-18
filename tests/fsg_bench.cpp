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

#include <memory>

#include "fast_fsg.hpp"
#include "fsg.hpp"
#include "io.hpp"
#include "utilities.hpp"

template <typename T>
std::tuple<std::unique_ptr<T[]>, size_t> get_file(char *name)
{
	std::ifstream file;
	open_file(file, name);
	std::streamoff len = file_length(file);
	std::unique_ptr<T[]> to_ret(new T[len]);
	read_file(file, to_ret.get(), static_cast<std::streamsize>(len));
	return std::make_tuple(std::move(to_ret), len);
}

std::vector<unsigned int> get_stuff(size_t file_len, unsigned int start, unsigned int mul)
{
	std::vector<unsigned int> to_ret;
	auto first = start / mul;
	while (first < file_len) {
		first *= mul;
		to_ret.push_back(first);
	}
	return to_ret;
}

std::vector<unsigned int> get_dst(size_t file_len)
{
	return get_stuff(file_len, 8, 2);
}

std::vector<unsigned int> get_len(size_t file_len)
{
	return get_stuff(file_len, 16, 3);
}

int main(int argc, char **argv)
{
	// Get the file
	std::unique_ptr<byte[]> file;
	size_t file_length;
	std::tie(file, file_length) = get_file<byte>(argc, argv);
	// Get the suffix array
	std::shared_ptr<byte> s_file(file.release(), std::default_delete<byte[]>());
	std::cout << "Deriving the SA...";
	std::flush(std::cout);
	auto sa 	= get_sa(s_file.get(), file_length);
	std::cout << " done.\nDeriving the ISA...";
	std::flush(std::cout);
	auto isa 	= get_isa(sa.get());
	std::cout << " done." << std::endl;
	// Get cost classes
	auto dst = get_dst(file_length);
	auto len = get_len(file_length);
//    std::vector<unsigned int> dst = {64, 16384, 4194304, 1073741824};
//    std::vector<unsigned int> len = dst;

	// NEW FSG
	std::cout << "Edges generation by fast_fsg." << std::endl;
	auto lap_time = get_epoch();
	{
		// Instantiate the New FSG
		fast_fsg<std::int32_t>     new_fsg(s_file, file_length, sa, dst, len);
		// Let us now generate maximal edges, and see if the two methods find the same maximal edges.
		auto &m1 = new_fsg.get_edges();
		for (auto i = 0U; i <= file_length; i++) {
			std::uint32_t g1;
			auto b1 = new_fsg.gen_next(&g1);
		}
	}

	std::cout << "Elapsed time: " << ((1.0 * (get_epoch() - lap_time)) / 1000000) << " secs." << std::endl;

	// OLD FSG
	std::cout << "Edges generation by fsg." << std::endl;
	lap_time = get_epoch();
	{
		fsg                     old_fsg(s_file, file_length, sa, isa, dst, len);
		auto &m1 = old_fsg.get_edges();
		for (auto i = 0U; i <= file_length; i++) {
			std::uint32_t g1;
			auto b1 = old_fsg.gen_next(&g1);
		}
	}
	std::cout << "Elapsed time: " << ((1.0 * (get_epoch() - lap_time)) / 1000000) << " secs." << std::endl;
}
