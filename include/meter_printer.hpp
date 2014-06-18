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

#ifndef METER_PRINTER_HPP
#define METER_PRINTER_HPP

#include <sys/ioctl.h>
#include <iostream>
#include <ios>
#include <assert.h>

class meter {
private:
	unsigned int get_columns()
	{
		struct winsize w;
		ioctl(0, TIOCGWINSZ, &w);
		return w.ws_col;
	}

	unsigned int last_percent;
public:
	meter() : last_percent(0U)
	{

	}

	void print_meter(double percent) {
		if (static_cast<unsigned int>(percent * 100) <= last_percent) {
			return;
		}
		last_percent = percent * 100;
		auto columns = get_columns();
		columns -= 6U;
		std::cerr << '\r' << "[";
		auto i = 0U;
		for (; i + 1 < columns * percent; i++) {
			std::cerr << '=';
		}
		std::cerr << ">";
		++i;
		for (; i < columns; i++) {
			std::cerr << " ";
		}
		std::cerr << "]" << std::flush;
		std::cerr << " " << std::right << static_cast<unsigned int>(percent * 100) << std::flush;
	}
};

class empty_observer {
public:

	empty_observer(size_t = 0)
	{

	}

	void set_character(size_t position)
	{
		
	}

	void new_character()
	{

	}
};

class fsg_meter {
private:
	unsigned int current_char;
	meter m;
	size_t text_len;
public:

	fsg_meter(size_t text_len) : current_char(0U), text_len(text_len)
	{

	}

	void set_character(size_t position)
	{
		assert(position >= current_char);
		current_char = position;
		m.print_meter(current_char / (text_len * 1.0));
	}

	void new_character(size_t count = 1)
	{
		set_character(current_char + count);
	}

};

#endif // METER_PRINTER_HPP
