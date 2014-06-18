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

#ifndef FACTORY_HPP
#define FACTORY_HPP

#include <memory>

/**
 * Wraps a constructor into a function.
 */
template <typename T>
class ctor {
public:
    template <typename ...Args>
    std::unique_ptr<T> operator()(Args ...args)
    {
        return make_unique<T>(args...);
    }
};

/**
 * Defines a generic factory.
 * - Base:      Base type
 * - T:         Instantiated type
 */
template <typename Base, typename T>
class generic_factory {
private:
	std::function<std::unique_ptr<T>()> ctor_;
public:
    template <typename ...Args>
    generic_factory(Args... args)
    {
        ctor_ = std::bind(ctor<T>(), args...);
    }

    std::unique_ptr<Base> get()
    {
        return ctor_();
    }
};

#endif // FACTORY_HPP
