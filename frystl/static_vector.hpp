// Template class static_vector
//
// One of these has nearly all of the API of a std::vector,
// but has a fixed capacity.  It cannot be extended past that.
// It is safe to use only where the problem limits the size needed.
//
// The implementation uses no dynamic storage; the entire vector
// resides where it is declared.
//
// The functions reserve() and shrink_to_fit() do nothing; the
// function get_allocator() is not implemented.
/*
MIT License

Copyright (c) 2020-2023 by Jonathan Fry

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
#ifndef FRYSTL_STATIC_VECTOR
#define FRYSTL_STATIC_VECTOR
#include <cstdint> // for uint32_t
#include <iterator>  // std::reverse_iterator
#include <algorithm> // for std::move...(), equal(), lexicographical_compare(), rotate()
#include <initializer_list>
#include <stdexcept> // for std::out_of_range
#include "frystl-defines.hpp"

namespace frystl
{
    template <class T, uint32_t Capacity>
    class static_vector
    {
    public:
        using this_type = static_vector<T, Capacity>;
        using value_type = T;
        using size_type = uint32_t;
        using difference_type = std::ptrdiff_t;
        using reference = value_type &;
        using const_reference = const value_type &;
        using pointer = value_type *;
        using const_pointer = const value_type *;
        using iterator = pointer;
        using const_iterator = const_pointer;
        using reverse_iterator = std::reverse_iterator<iterator>;
        using const_reverse_iterator = std::reverse_iterator<const_iterator>;
        //
        //******* Public member functions:
        //
        static_vector() noexcept : _size(0) {} 
        ~static_vector() noexcept { clear(); } 
        // copy constructors
        static_vector(const this_type &donor) : _size(0)
        {
            for (auto &m : donor)
                push_back(m);
        }
        template <unsigned C1>
        static_vector(const static_vector<T, C1> &donor) : _size(0)
        {
            FRYSTL_ASSERT2(donor.size() <= Capacity,
                    "static_vector: construction from a too-large object");
            for (auto &m : donor)
                push_back(m);
        }
        // move constructors
        // Constructs the new static_vector by moving all the elements of
        // the existing static_vector.  It leaves the moved-from object
        // empty.
        template <unsigned C1>
        static_vector(static_vector<T, C1> &&donor) noexcept : _size(0)
        {
            FRYSTL_ASSERT2(donor.size() <= Capacity,
                    "static_vector: overflow on move construction");
            for (auto &m : donor)
                Construct(data() + _size++, std::move(m));
            donor.clear();
        }
        // fill constructors
        static_vector(size_type n, const_reference value) : _size(0)
        {
            for (size_type i = 0; i < n; ++i)
                push_back(value);
        }
        explicit static_vector(size_type n) : _size(0)
        {
            for (size_type i = 0; i < n; ++i)
                emplace_back();
        }

        // range constructor
        template <class InputIterator,
                  typename = RequireInputIter<InputIterator>>
        static_vector(InputIterator begin, InputIterator end) : _size(0)
        {
            for (InputIterator k = begin; k != end; ++k)
                emplace_back(*k);
        }
        // initializer list constructor
        static_vector(std::initializer_list<value_type> il) : _size(il.size())
        {
            FRYSTL_ASSERT2(il.size() <= Capacity,
                    "static_vector: construction from a too-large list");
            pointer p = data();
            for (auto &value : il)
                Construct(p++, value);
        }
        //
        //  Assignment functions
        void assign(size_type n, const_reference val)
        {
            clear();
            while (size() < n)
                push_back(val);
        }
        void assign(std::initializer_list<value_type> x)
        {
            FRYSTL_ASSERT2(x.size() <= Capacity,
                    "static_vector: assign() from a too-large list");
            clear();
            for (auto &a : x)
                push_back(a);
        }
        template <class InputIterator,
                  typename = RequireInputIter<InputIterator>> 
        void assign(InputIterator begin, InputIterator end)
        {
            clear();
            for (InputIterator k = begin; k != end; ++k)
                push_back(*k);
        }
        // Copy operator=.
        template <unsigned C2>
        this_type &operator=(static_vector<T,C2> &other) 
        {
            if (data() != other.data()) {
                assign(other.begin(), other.end());
            }
            return *this;
        }
        // Move operator=.  Except for self-assignments, source
        // will be left empty.
        template <unsigned C2>
        this_type &operator=(static_vector<T,C2> &&other) noexcept
        {
            if (data() != other.data())
            {
                clear();
                iterator p = begin();
                for (auto &o : other)
                    Construct(p++, std::move(o));
                _size = other.size();
                other.clear();
            }
            return *this;
        }
        this_type &operator=(std::initializer_list<value_type> il)
        {
            assign(il);
            return *this;
        }
        //
        //  Element access functions
        //
        pointer data() noexcept { return reinterpret_cast<pointer>(_elem); }
        const_pointer data() const noexcept { return reinterpret_cast<const_pointer>(_elem); }
        reference at(size_type i)
        {
            Verify(i < _size);
            return data()[i];
        }
        reference operator[](size_type i) noexcept
        {
            FRYSTL_ASSERT2(i < _size,"static_vector: index out of range");
            return data()[i];
        }
        const_reference at(size_type i) const
        {
            Verify(i < _size);
            return data()[i];
        }
        const_reference operator[](size_type i) const noexcept
        {
            FRYSTL_ASSERT2(i < _size,"static_vector: index out of range");
            return data()[i];
        }
        reference back() noexcept 
        { 
            FRYSTL_ASSERT2(_size,"back() called on empty static_vector");
            return data()[_size - 1]; 
        }
        const_reference back() const noexcept 
        { 
            FRYSTL_ASSERT2(_size,"back() called on empty static_vector");
            return data()[_size - 1]; 
        }
        reference front() noexcept
        {
            FRYSTL_ASSERT2(_size,"front() called on empty static_vector");
            return data()[0];
        }
        const_reference front() const noexcept
        {
            FRYSTL_ASSERT2(_size,"front() called on empty static_vector");
            return data()[0];
        }
        //
        // Iterators
        //
        iterator begin() noexcept { return data(); }
        const_iterator begin() const noexcept { return data(); }
        const_iterator cbegin() const noexcept { return data(); }
        iterator end() noexcept { return data() + _size; }
        const_iterator end() const noexcept { return data() + _size; }
        const_iterator cend() const noexcept { return data() + _size; }
        reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }
        const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }
        const_reverse_iterator crbegin() const noexcept { return const_reverse_iterator(cend()); }
        reverse_iterator rend() noexcept { return reverse_iterator(begin()); }
        const_reverse_iterator rend() const noexcept { return const_reverse_iterator(begin()); }
        const_reverse_iterator crend() const noexcept { return const_reverse_iterator(cbegin()); }
        //
        // Capacity and size
        //
        constexpr std::size_t capacity() const noexcept { return Capacity; }
        constexpr std::size_t max_size() const noexcept { return Capacity; }
        size_type size() const noexcept { return _size; }
        bool empty() const noexcept { return _size == 0; }
        void reserve(size_type n) noexcept { 
            FRYSTL_ASSERT2(n <= capacity(), "static_vector::reserve() argument too large"); 
        }
        void shrink_to_fit() noexcept {}
        //
        //  Modifiers
        //
        void pop_back() noexcept
        {
            FRYSTL_ASSERT2(_size, "static_vector::pop_back() on empty vector");
            _size -= 1;
            Destroy(end());
        }
        void push_back(const T &cd) { emplace_back(cd); }
        void push_back(T &&cd) noexcept { emplace_back(std::move(cd)); }
        void clear() noexcept
        {
            while (_size)
                pop_back();
        }
        iterator erase(const_iterator position) noexcept
        {
            FRYSTL_ASSERT2(GoodIter(position + 1), 
                "static_vector::erase(pos): pos out of range");
            iterator x = const_cast<iterator>(position);
            Destroy(x);
            std::move(x + 1, end(), x);
            _size -= 1;
            return x;
        }
        iterator erase(const_iterator first, const_iterator last)
        {
            iterator f = const_cast<iterator>(first);
            iterator l = const_cast<iterator>(last);
            if (first != last)
            {
                FRYSTL_ASSERT2(GoodIter(first + 1),
                    "static_vector::erase(first,last): bad first");
                FRYSTL_ASSERT2(GoodIter(last),
                    "static_vector::erase(first,last): bad last");
                FRYSTL_ASSERT2(first < last,
                    "static_vector::erase(first,last): last < first");
                for (iterator it = f; it < l; ++it)
                    Destroy(it);
                std::move(l, end(), f);
                _size -= last - first;
            }
            return f;
        }
        template <class... Args>
        iterator emplace(const_iterator position, Args &&...args)
        {
            FRYSTL_ASSERT2(_size < Capacity,"static_vector::emplace() overflow");
            FRYSTL_ASSERT2(begin() <= position && position <= end(),
                "static_vector::emplace(): bad position");
            iterator p = const_cast<iterator>(position);
            MakeRoom(p, 1);
            if (p < end())
                (*p) = value_type(std::forward<Args>(args)...);
            else 
                Construct(p, std::forward<Args>(args)...);
            ++_size;
            return p;
        }
        template <class... Args>
        [[maybe_unused]] reference emplace_back(Args && ... args)
        {
            FRYSTL_ASSERT2(_size < Capacity,"static_vector::emplace_back() overflow");
            Construct(end(), std::forward<Args>(args)...);
            ++_size;
            return back();
        }
        // single element insert()
        iterator insert(const_iterator position, const value_type &val)
        {
            FRYSTL_ASSERT2(_size < Capacity, "static_vector::insert: overflow");
            FRYSTL_ASSERT2(GoodIter(position),"static_vector::insert(): bad position");
            iterator p = const_cast<iterator>(position);
            MakeRoom(p,1);
            FillCell(p, val);
            ++_size;
            return p;
        }
        // move insert()
        iterator insert(const_iterator position, value_type &&val) noexcept
        {
            FRYSTL_ASSERT2(_size < Capacity, "static_vector::insert: overflow");
            FRYSTL_ASSERT2(GoodIter(position),"static_vector::insert(): bad position");
            return emplace(position, std::move(val));
        }
        // fill insert
        iterator insert(const_iterator position, size_type n, const value_type &val)
        {
            FRYSTL_ASSERT2(_size + n <= Capacity, "static_vector::insert: overflow");
            FRYSTL_ASSERT2(begin() <= position && position <= end(),
                "static_vector::insert(): bad position");
            iterator p = const_cast<iterator>(position);
            MakeRoom(p, n);
            // copy val n times into newly available cells
            for (iterator i = p; i < p + n; ++i)
            {
                FillCell(i, val);
            }
            _size += n;
            return p;
        }
        // range insert()
        private:
            // implementation for iterators lacking operator-()
            template <class InpIter>
            iterator insert(
                const_iterator position, 
                InpIter first, 
                InpIter last,
                std::input_iterator_tag)
            {
                iterator result = const_cast<iterator>(position);
                size_type oldSize = size();
                while (first != last) {
                    push_back(*first++);
                }
                std::rotate(result, begin()+oldSize, end());
                return result;
            }
            // Implementation for iterators with operator-()
            template <class DAIter>
            iterator insert(
                const_iterator position, 
                DAIter first, 
                DAIter last,
                std::random_access_iterator_tag)
            {
                FRYSTL_ASSERT2(first <= last,"static_vector::insert(): last < first");
                iterator p = const_cast<iterator>(position);
                int n = last-first;
                FRYSTL_ASSERT2(_size + n <= Capacity, "static_vector::insert: overflow");
                MakeRoom(p,n);
                iterator result = p;
                while (first != last) {
                    Construct(p++, *first++);
                }
                _size += n;
                return result;
            }
        public:
        template <class Iter>
        iterator insert(const_iterator position, Iter first, Iter last)
        {
            return insert(position,first,last,
                typename std::iterator_traits<Iter>::iterator_category());
        }
        // initializer list insert()
        iterator insert(const_iterator position, std::initializer_list<value_type> il)
        {
            size_type n = il.size();
            FRYSTL_ASSERT2(_size + n <= Capacity, "static_vector::insert: overflow");
            FRYSTL_ASSERT2(begin() <= position && position <= end(),
                "static_vector::insert(): bad position");
            iterator p = const_cast<iterator>(position);
            MakeRoom(p, n);
            // copy il into newly available cells
            auto j = il.begin();
            for (iterator i = p; i < p + n; ++i, ++j)
            {
                FillCell(i, *j);
            }
            _size += n;
            return p;
        }
        void resize(size_type n, const value_type &val)
        {
            FRYSTL_ASSERT2(n <= Capacity, "static_vector::resize: overflow");
            while (n < size())
                pop_back();
            while (size() < n)
                push_back(val);
        }
        void resize(size_type n)
        {
            FRYSTL_ASSERT2(n <= Capacity, "static_vector::resize: overflow");
            while (n < size())
                pop_back();
            while (size() < n)
                emplace_back();
        }
        void swap(this_type &x) noexcept
        {
            std::swap(*this, x);
        }
        //
        //******* Private member functions:
        //
    private:
        using storage_type =
            std::aligned_storage_t<sizeof(value_type), alignof(value_type)>;
        size_type _size;
        storage_type _elem[Capacity];

        static void Verify(bool cond)
        {
            if (!cond)
                throw std::out_of_range("static_vector range error");
        }
        // Move cells at and to the right of p to the right by n spaces.
        void MakeRoom(iterator p, size_type n) noexcept
        {
            size_type nu = std::min(size_type(end() - p), n);
            // fill the uninitialized target cells by move construction
            for (iterator src = end()-nu; src < end(); src++)
                Construct(src + n , std::move(*src));
            // shift elements to previously occupied cells by move assignment
            std::move_backward(p, end() - nu, end());
        }
        // returns true iff it-1 can be dereferenced.
        bool GoodIter(const const_iterator &it) noexcept
        {
            return begin() < it && it <= end();
        }
        template <class... Args>
        void FillCell(iterator pos, Args && ... args)
        {
            if (pos < end())
                // fill previously occupied cell using assignment
                (*pos)  = value_type(std::forward<Args>(args)...);
            else 
                // fill unoccupied cell in place by constructon
                Construct(pos, std::forward<Args>(args)...);
        }
        //
    };
    //
    //*******  Non-member overloads
    //
    template <class T, unsigned C0, unsigned C1>
    bool operator==(const static_vector<T, C0> &lhs, const static_vector<T, C1> &rhs) noexcept
    {
        if (lhs.size() != rhs.size())
            return false;
        return std::equal(lhs.begin(), lhs.end(), rhs.begin());
    }
    template <class T, unsigned C0, unsigned C1>
    bool operator!=(const static_vector<T, C0> &lhs, const static_vector<T, C1> &rhs) noexcept
    {
        return !(rhs == lhs);
    }
    template <class T, unsigned C0, unsigned C1>
    bool operator<(const static_vector<T, C0> &lhs, const static_vector<T, C1> &rhs) noexcept
    {
        return std::lexicographical_compare(lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
    }
    template <class T, unsigned C0, unsigned C1>
    bool operator<=(const static_vector<T, C0> &lhs, const static_vector<T, C1> &rhs) noexcept
    {
        return !(rhs < lhs);
    }
    template <class T, unsigned C0, unsigned C1>
    bool operator>(const static_vector<T, C0> &lhs, const static_vector<T, C1> &rhs) noexcept
    {
        return rhs < lhs;
    }
    template <class T, unsigned C0, unsigned C1>
    bool operator>=(const static_vector<T, C0> &lhs, const static_vector<T, C1> &rhs) noexcept
    {
        return !(lhs < rhs);
    }

    template <class T, unsigned C>
    void swap(static_vector<T, C> &a, static_vector<T, C> &b) noexcept
    {
        a.swap(b);
    }
};     // namespace frystl
#endif // ndef FRYSTL_STATIC_VECTOR
