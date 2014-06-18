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

#ifndef CMD_PARSE_HPP
#define CMD_PARSE_HPP
#include <stdexcept>

/** Parsing exception */
class cmd_error : public std::runtime_error {
private:
    const std::string usage;
public:
    cmd_error(std::string usage) :
        std::runtime_error("Command line parsing error"), usage(usage) { }
    std::string cmd_usage() { return usage; }
    ~cmd_error() throw() { }
};

#endif // CMD_PARSE_HPP
