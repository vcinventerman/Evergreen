#pragma once

#include "evergreen/common/types.h"
//#include "evergreen/common/alloc.h"
#include "evergreen/string/StringView.h"

// Immutable string
// Stores strings in a map of hash values to null-terminated (until bootstrap for c compat) strings syncronized by a shared mutex
// Strings are reference counted and remove themselves from map when atomic reference count reaches zero unless they are put in at compile time and unremovable (change this?)

// buffers in the standard library are stored as a pointer to the first and one past the last element 
// end comes first in storage (not in argument) because of possibility for buffer to exist at end of class
// end is logically less useful and technically not required (just like size) so it comes second

//todo: ImStringElem created in same allocation as string in manner similar to std::shared_pointer::make_shared (using virtual function or flag of owning mem)
//todo: Omit static map if requested
//todo: sync(shared) for map that integrates shared mutex
//todo: pair with named elements
//todo: noerr keyword controlling whether error pointer is contained in return
//todo: if returning class such as error and value, allow editing individual values during function: "ret.err = "Malloc"
//todo: allow use of ImString directly by getting rid of extra <>

namespace evg
{
	class ImString
	{
	public:
		using This = StringViewBase<CChar>;
		using Iterator = RandomContigIterator<CChar>;
		using CIterator = RandomContigIterator<CChar>;
		using RevIterator = RevRandomContigIterator<CChar>;
		using CRevIterator = RevRandomContigIterator<CChar>;

		constexpr static Size npos = std::numeric_limits<Size>::max();

		// Values that are kept in map, point to a string
		//todo: custom map implementation that lets Elem contain a hash using offset pointers
		//todo: allow calling constructor manually
		class Elem
		{
		public:
			std::atomic<Size> refs;
			bool owns; // Indicates whether the attached string will be deleted with this element

			StringViewHash string;

			Elem(const StringViewHash _string, const bool copy = true) : owns(copy)
			{
				if (copy)
				{
					Char* mem = new Char[_string.size() + 1];
					std::copy(_string.cbegin(), _string.cend(), mem);
					mem[_string.size()] = '\0';
					string = StringViewHash(mem, _string.size(), _string.hash());
				}
				else
				{
					string = StringViewHash(_string);
				}
			}

			void addRef()
			{
				++refs;
			}
			void subRef()
			{
				--refs;
			}

			~Elem()
			{
				if (owns)
					delete string.data().begin_raw;
			}
		};

		class Manager
		{
		public:
			std::map<Hash, Elem> strings;
			SharedMutex m_strings;

			Elem* nullString;
			
			Manager()
			{
				nullString = &strings.emplace(std::piecewise_construct, std::forward_as_tuple(hashes::djb2<Hash>("")), std::forward_as_tuple(StringViewHash(""))).first->second;
			}

			bool has(const Hash hash)
			{
				SharedLock<SharedMutex> lock(m_strings);
				return strings.find(hash) != strings.end();
			}

			auto find (const Hash hash)
			{
				SharedLock<SharedMutex> lock(m_strings);
				return strings.find(hash);
			}

			Elem* insert(const StringViewHash str, const bool copy = true)
			{
				m_strings.lock_shared();
				auto&& exists = find(str.hash());
				m_strings.unlock_shared();

				if (exists == strings.cend())
				{
					std::lock_guard<std::shared_mutex> lock(m_strings);
					return &strings.emplace(std::piecewise_construct, std::forward_as_tuple(str.hash()), 
						std::forward_as_tuple(str, copy)).first->second;
				}
				else
				{
					exists->second.addRef();
					return &exists->second;
				}

			}

			Elem* insert(CChar* _begin, CChar* _end, const bool copy = true)
			{
				return insert(StringViewHash(_begin, _end), copy);
			}
		};

		static Manager defaultManager;


		Elem* source;

		ImString() = default;
		ImString(Elem* const _source) : source(_source) {}
		ImString(const StringViewHash str, bool copy = true)
		{
			source = defaultManager.insert(str, copy);
		}
		ImString(CChar* const _begin, CChar* const _end, bool copy = true)
		{
			source = defaultManager.insert(StringViewHash(_begin, _end), copy);
		}
		ImString(CChar* const str, const Bool copy = true)
		{
			source = defaultManager.insert(str, copy);
		}
		ImString(const std::string& str)
		{
			source = defaultManager.insert(str.data(), true);
		}
		




		CChar& operator[] (Size index)
		{
			return data()[index];
		}

		Size size() const
		{
			return source->string.size();
		}

		CChar& front() const
		{
			return data()[0];
		}
		CChar& back() const
		{
			return *(source->string.end() - 1);
		}

		ContiguousBufPtrEnd<CChar> data() const { return source->string.data(); }
		Iterator begin() const { return data().begin(); }
		Iterator end() const { return data().end(); }
		CIterator cbegin() const { return data().cbegin(); }
		CIterator cend() const { return data().cend(); }
		RevIterator rbegin() const { return data().rbegin(); }
		RevIterator rend() const { return data().rend(); }
		CRevIterator crbegin() const { return data().crbegin(); }
		CRevIterator crend() const { return data().crend(); }

		//todo: template variant by reference or integer return value
		Size find(CChar letter) const
		{
			return source->string.find(letter);
		}

		Size rfind(CChar letter) const
		{
			return source->string.rfind(letter);
		}

		ImString operator+= (const ImString& rhs)
		{
			Char* mem = new Char[this->size() + rhs.size()];
			std::copy(this->cbegin(), this->cend(), mem);
			std::copy(rhs.cbegin(), rhs.cend(), mem + this->size());
			mem[this->size() + rhs.size() + 1] = '\0';

			source = new Elem(StringViewHash(mem, this->size() + rhs.size()), false);
		}

		template<typename T>
		T operator_conv() const
		{
			return source->string.data();
		}

		EVG_CXX_CAST(CChar*)

		const bool operator< (const ImString& rhs) const { return source->string.hash() < rhs.source->string.hash(); }
	};

	ImString::Manager ImString::defaultManager = {};
}