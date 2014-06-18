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

#include <iostream>
#include <sstream>

#include <stdint.h>
#include <limits>

#include <io.hpp>
#include <cost_model.hpp>
#include <utilities.hpp>

class_info read_ci(std::ifstream &input)
{
	std::string line;
	std::vector<unsigned int> Q_vec, cst_vec;

	while(bool(std::getline(input, line))) {
		if (line == "") {
			break;
		}
		std::uint64_t Q, cost;
		std::stringstream(line) >> Q >> cost;
		if (Q >= std::numeric_limits<unsigned int>::max()) {
			Q_vec.push_back(std::numeric_limits<unsigned int>::max());
		} else {
			Q_vec.push_back(Q);
		}
		cst_vec.push_back(cost);
	}
	auto new_end = std::unique(Q_vec.begin(), Q_vec.end());
	Q_vec.erase(new_end, Q_vec.end());
	cst_vec.erase(std::next(cst_vec.begin(), Q_vec.size()), cst_vec.end());
	return class_info(ITERS(Q_vec), ITERS(cst_vec));
}

cost_model read_model(const char *file_name, unsigned int *lit_window)
{
	std::string line;
	std::ifstream input(file_name);

	class_info dst = read_ci(input), len = read_ci(input);

	/* Read literal model */
	unsigned int lit_fix, lit_var;
	bool read = bool(std::getline(input, line));
	std::stringstream(line) >> *lit_window;
	read = read && bool(std::getline(input, line));
	std::stringstream(line) >> lit_fix;
	read = read && bool(std::getline(input, line));
	std::stringstream(line) >> lit_var;

	if (dst.length == 0 || len.length == 0 || !read) {
		throw std::logic_error("Invalid model file.");
	}

	return cost_model(dst, len, lit_fix, lit_var);
}

int main(int argc, char **argv)
{
	if (argc < 2) {
		std::cerr << "No input file passed" << std::endl;
	}
	char *file_name = *++argv;
	unsigned int lit_window;
	cost_model cm = read_model(file_name, &lit_window);

	std::cout << get_cm_rep(cm) << std::endl;
}
