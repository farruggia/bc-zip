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

#ifndef TUPLE_EXECUTER_HPP
#define TUPLE_EXECUTER_HPP

#include <tuple>

template <int N>
struct tuple_exec {
    template <typename Fun_, typename ...Args>
    void execute(std::tuple<Args...> &t, Fun_ &f) {
        f(std::get<N>(t));
        tuple_exec<N-1>().execute(t, f);
    }
};

template <>
struct tuple_exec<-1> {
    template <typename Fun_, typename ...Args>
    void execute(std::tuple<Args...> &t, Fun_ &f) {
    }
};

template <typename Fun_, typename... Args>
void tuple_func(Fun_ &f, std::tuple<Args...> t)
{
    tuple_exec<std::tuple_size<std::tuple<Args...>>::value - 1>().execute(t, f);
}

#endif // TUPLE_EXECUTER_HPP
