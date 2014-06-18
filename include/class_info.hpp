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

#ifndef CLASS_INFO_HPP
#define CLASS_INFO_HPP

#include <vector>
#include <algorithm>
#include <iterator>
#include <assert.h>

struct class_info {
    std::vector<unsigned int> win;
    std::vector<double> costs;
    size_t length;

    template <typename T, typename U>
    class_info(T win_first, T win_last, U costs_first, U costs_last) {
        win.insert(win.end(), win_first, win_last);
        costs.insert(costs.end(), costs_first, costs_last);
        length = std::distance(win_first, win_last);
    }

    size_t extent() const {
        return win[length - 1];
    }

    double get_cost(unsigned int i)
    {
        assert(i <= extent());
        auto distance = std::distance(win.begin(), std::lower_bound(win.begin(), win.end(), i));
        return costs[distance];
    }
};

#endif // CLASS_INFO_HPP
