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
#include <string.h>
#include <sstream>
#include <fstream>
#include <memory>
#include <signal.h>

#include <bit_compress.hpp>
#include <bicriteria_compress.hpp>
#include <decompress.hpp>
#include <list_gens.hpp>
#include <cmd_parse.hpp>
#include <common.hpp>
#include <io.hpp>
#include <list.hpp>

namespace {

void error_message(const char *message)
{
	std::cerr << "ERROR: " << message << std::endl;
}

}

int main(int argc, char *argv[])
{
	// List of commands
	const char    *compress    = "compress",      *decompress = "decompress",
			*bit_optimal = "bit-optimal",   *encoders       = "encoders",  *fsgs = "gens";
	// Manage the command-line options
	if (argc < 2) {
		error_message("Must specify a command");
		std::cerr << "Commands:\n";
		std::cerr << compress << "\t" << decompress << "\t" << bit_optimal << "\t" << encoders << "\t" << fsgs << std::endl;
		return 0;
	}

	char *command = argv[1];
	try {
		if (strcmp(command, compress) == 0) {
			bicriteria_compress(command, argc, argv);
		} else if (strcmp(command, decompress) == 0) {
			decompress_file(command, argc, argv);
		} else if (strcmp(command, bit_optimal) == 0) {
			bit_compress(bit_optimal, argc, argv);
		} else if (strcmp(command, encoders) == 0) {
			list_encoders();
		} else if (strcmp(command, fsgs) == 0){
			list_generators();
		} else {
			error_message("Invalid command");
			std::cerr << "Commands:\n";
			std::cerr << compress << "\t" << decompress << "\t" << bit_optimal << "\t" << encoders << "\t" << fsgs << std::endl;
		}
	} catch (cmd_error &e) {
		std::stringstream what;
		what << "Tool " << command << " wrongly invoked" << std::endl;
		what << e.cmd_usage() << std::endl;
		error_message(what.str().c_str());
	} catch (std::logic_error &e) {
		std::cout << "LOGIC ERROR while processing." << std::endl;
		std::cout << "Cause: " << e.what() << std::endl;
	}
}
