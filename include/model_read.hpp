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

#ifndef MODEL_READ_HPP
#define MODEL_READ_HPP

#include "cost_model.hpp"

/**
 * @brief Read a model from file.
 * @param file_name
 *		The file where it is stored. Format: three groups of lines, delimited by empty lines.
 *		First two groups: distances and lengths, one line per class cost (len, cost).
 *		Third group: literal window, fixed and variable cost.
 * @param lit_window
 *		Returns the literal window (output parameter)
 * @return
 *		The cost model.
 */
cost_model read_model(const char *file_name, unsigned int *lit_window);

#endif // MODEL_READ_HPP
