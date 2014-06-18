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
#include <gtest/gtest.h>
#include <fsg_check.hpp>

#include <io.hpp>
#include <wm_serializer.hpp>
#include <sstream>

template <typename T>
std::unique_ptr<T[]> get_file(char *name, size_t *file_size = nullptr)
{
	std::ifstream file;
	open_file(file, name);
	std::streamoff len = file_length(file);
	std::unique_ptr<T[]> to_ret(new T[len]);
	read_file(file, to_ret.get(), static_cast<std::streamsize>(len));
	if (file_size != nullptr){
		*file_size = len;
	}
	return to_ret;
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

char* file_name;
std::string gen_1;
std::string gen_2;
std::string model_file;

TEST(fsg_check, compare)
{
	std::unique_ptr<byte[]> file;
	size_t file_length;
	file = get_file<byte>(file_name, &file_length);
	text_info ti(file.release(), file_length);
	cost_model cm = wm_load(model_file);

	// Get functor
	compare_run cr(ti, cm);
	generators_().call(gen_1, gen_2, cr);
}



int main(int argc, char **argv)
{
	::testing::InitGoogleTest(&argc, argv);
	if (argc < 5) {
		std::cerr << "Usage: file_name generator_1 generator_2 cost_model" << std::endl;
		exit(1);
	}
	// Get text_info
	file_name = *++argv;
	gen_1 = *++argv;
	gen_2 = *++argv;
	model_file  = *++argv;
	return RUN_ALL_TESTS();
}
