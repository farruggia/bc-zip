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

#ifndef GENERATORS_HPP
#define GENERATORS_HPP
#include <map>
#include <string>
#include <tuple>
#include <memory>

#include <utilities.hpp>
#include <fsg.hpp>
#include <fast_fsg.hpp>
#include <rightmost_fsg.hpp>

/***** Generator mismatch exception ********/
class gen_mismatch : public std::logic_error {
private:
	const std::string suggested_gen;

	std::string error_name(
			distance_kind expected, distance_kind required
	) {
		return join_s("ERROR while instantiating generator: expected ", expected, ", got ", required);
	}


public:
	gen_mismatch(
			distance_kind expected, distance_kind actual,
			std::string suggested
	)
		: std::logic_error(error_name(expected, actual)),
		  suggested_gen(suggested)
	{

	}

	std::string suggest_gen()
	{
		return suggested_gen;
	}

	virtual ~gen_mismatch() throw ()
	{
	}
};

/***** GENERATOR TYPES *******/
typedef fast_fsg_gen<std::int32_t, generic_rsa_getter> ffsg_gen;
typedef fast_fsg_gen<std::int32_t, generalized_rsa> gen_ffsg_gen;
typedef fast_fsg_gen<std::int32_t, same_rsa_getter> same_fsg_gen;
typedef fsg_gen generic_fsg_gen;

/**** FACTORIES *****/
template <typename gen_t, template <class T> class protocol_t>
class base_fsg_fact {
protected:
	sa_getter &sa;
	text_info ti;

public:
	base_fsg_fact(text_info ti, sa_getter &sa) : sa(sa), ti(ti)
	{

	}

	virtual gen_t get_gen(const cost_model &cm) = 0;

	typedef protocol_t<gen_t> fsg_t;
	typedef gen_t generator_t;

	std::shared_ptr<protocol_t<gen_t>> instantiate(const cost_model &cm)
	{
		auto dst_win = cm.get_dst();
		distance_kind dst_kind = get_kind(dst_win), gen_kind = gen_t::get_kind();
		if (!compatible(dst_kind, gen_kind)) {
			throw gen_mismatch(gen_kind, dst_kind, suggest_gen(dst_kind));
		}
		auto len_win = cm.get_len();
		return std::make_shared<protocol_t<gen_t>>(get_gen(cm), ti.len, dst_win, len_win);
	}

	virtual ~base_fsg_fact()
	{

	}
};

template <
	template <class T> class protocol_t,
	typename stats_t = stats_getter,
	template <class T> class rsa_getter_t = generic_rsa_getter,
	typename max_match_t = smart_find
>
class fast_fsg_fact : public base_fsg_fact<fast_fsg_gen<std::int32_t, rsa_getter_t, max_match_t>, protocol_t> {

public:

	//using base_fsg_fact::base_fsg_fact;
	typedef fast_fsg_gen<std::int32_t, rsa_getter_t, max_match_t> gen_t;

	fast_fsg_fact(text_info ti, sa_getter &sa) : base_fsg_fact<gen_t, protocol_t>(ti, sa)
	{

	}

	gen_t get_gen(const cost_model &cm)
	{
		auto &ti = base_fsg_fact<gen_t, protocol_t>::ti;
		auto suf = base_fsg_fact<gen_t, protocol_t>::sa.get(ti.text.get(), ti.text.get() + ti.len);
		auto dst_win = cm.get_dst();
		auto len_win = cm.get_len();
		stats_t stats(dst_win, len_win, ti.len);
		rsa_getter_t<std::int32_t> getter(stats, suf, ti.len);
		return gen_t(ti.text, ti.len, std::move(getter), stats);
	}

	virtual ~fast_fsg_fact() throw()
	{

	}
};

class ffsg_fact : public fast_fsg_fact<fsg_protocol> {
public:

	ffsg_fact(text_info ti, sa_getter &sa) : fast_fsg_fact<fsg_protocol>(ti, sa)
	{

	}

	static const char *name()
	{
		return "fast_fsg";
	}
};

class gen_ffsg_fact : public fast_fsg_fact<fsg_protocol, gen_stats_getter, generalized_rsa, split_max_match> {
public:
	gen_ffsg_fact(text_info ti, sa_getter &sa) : fast_fsg_fact<fsg_protocol, gen_stats_getter, generalized_rsa, split_max_match>(ti, sa)
	{

	}

	static const char *name()
	{
		return "gen_fast_fsg";
	}
};

class rm_fsg_fact : public fast_fsg_fact<rm_protocol> {
public:

	rm_fsg_fact(text_info ti, sa_getter &sa) : fast_fsg_fact<rm_protocol>(ti, sa)
	{

	}

	static const char *name()
	{
		return "fixed_fsg";
	}
};

class same_fsg_fact : public base_fsg_fact<same_fsg_gen, fsg_protocol> {
public:

	//using base_fsg_fact::base_fsg_fact;

	same_fsg_fact(text_info ti, sa_getter &sa) : base_fsg_fact(ti, sa)
	{

	}

	same_fsg_gen get_gen(const cost_model &cm)
	{
		auto suf = sa.get(ti.text.get(), ti.text.get() + ti.len);
		auto dst_win = cm.get_dst();
		auto len_win = cm.get_len();
		same_rsa_getter<std::int32_t> getter(dst_win.front(), suf, ti.len);
		return same_fsg_gen(ti.text, ti.len, std::move(getter), stats_getter(dst_win, len_win, ti.len));
	}

	static const char *name()
	{
		return "same_fsg";
	}
};

class fsg_fact : public base_fsg_fact<fsg_gen, fsg_protocol> {
private:
	std::shared_ptr<std::vector<std::int32_t>> isa;
public:

	typedef fsg_protocol<fsg_gen> fsg_t;

	fsg_fact(text_info ti, sa_getter &sa)
		: base_fsg_fact(ti, sa), isa(get_isa(sa.get(ti.text.get(), ti.text.get() + ti.len).get()))
	{

	}

	fsg_gen get_gen(const cost_model &cm)
	{
		auto dst_win = cm.get_dst();
		auto len_win = cm.get_len();
		return fsg_gen(ti.text, ti.len, sa.get(ti.text.get(), ti.text.get() + ti.len), isa, dst_win, len_win);
	}

	static const char *name()
	{
		return "fsg";
	}
};

/**
 * Defines a container of generators.
 * The list of included generators is encoded in the template parameters.
 * The class supports two operations:
 * - Returns the list of names associated to contained generators;
 * - Given a name, a template and a base class B, returns an instantiation of
 *   the template with the generator corresponding to the given name, in the
 *   form of a unique_ptr<B>.
 */
template <typename ...T>
struct generators;

template <typename T, typename... U>
struct generators<T, U...>
{

	template <typename X, typename Functor>
	void call_first(std::string name, Functor &&func) {
		if (name == T::name()) {
			func.template run<X, T>();
		} else {
			generators<U...>().template call_first<X, Functor>(name, std::forward<Functor>(func));
		}
	}

	template <typename X, typename Functor>
	void call_second(std::string name, Functor &&func) {
		if (name == T::name()) {
			func.template run<T, X>();
		} else {
			generators<U...>().template call_second<X, Functor>(name, std::forward<Functor>(func));
		}
	}

	// Get the names
	void get_names(std::vector<std::string> &names)
	{
		names.push_back(T::name());
		generators<U...>().get_names(names);
	}

	// Istantiate something with T
	template <typename Functor>
	void call(std::string name, Functor &&func)
	{
		if (name == T::name()) {
			func.template run<T>();
		} else {
			return generators<U...>().template call<Functor>(name, std::forward<Functor>(func));
		}
	}

	template <typename Functor>
	void call(std::string name_1, std::string name_2, Functor &&func)
	{
		if (name_1 == T::name()) {
			generators<U...>().template call_first<T, Functor>(name_2, std::forward<Functor>(func));
		} else if (name_2 == T::name()) {
			generators<U...>().template call_second<T, Functor>(name_1, std::forward<Functor>(func));
		} else {
			generators<U...>().template call<Functor>(name_1, name_2, std::forward<Functor>(func));
		}
	}
};

// Didn't find it
template <>
struct generators<>
{
	template <typename X, typename Functor>
	void call_first(std::string name, Functor &&func) {
		throw std::logic_error("No generator named " + name);
	}

	template <typename X, typename Functor>
	void call_second(std::string name, Functor &&func) {
		throw std::logic_error("No generator named " + name);
	}

	void get_names(std::vector<std::string> &) {
	}

	template <typename Functor>
	void call(std::string name, Functor &&)
	{
		throw std::logic_error("No generator named " + name);
	}

	template <typename Functor>
	void call(std::string name_1, std::string name_2, Functor &&func)
	{
		throw std::logic_error("No generators " + name_1 + " and " + name_2);
	}

};

typedef generators<fsg_fact, ffsg_fact, gen_ffsg_fact, same_fsg_fact, rm_fsg_fact> generators_;

std::string pick_gen(distance_kind kind);

#endif // GENERATORS_HPP
