// static_deque.hpp - defines a fixed-capacity deque-like template class
//
// This file defines static_deque<value_type, Capacity>, where value_type
// is the type of the elements and Capacity specifies its capacity,
// using a fixed-size array.
//
// The first elements added to it are placed in the middle, and it can
// expand in either direction. 
//
// Whenever a static_deque runs out of space on one end, it slides all
// of the data to the other end. This can be costly and
// invalidate iterators, pointers, and references.
//
// The template implements the semantics of std::deque with the following
// exceptions:
//      shrink_to_fit() does nothing.
//      get_allocator() is not implemented.
//      capacity() returns the maximum size.
//      The function data() is added.  Like std::vector::data(), it
//          returns a pointer to the front element.  The pointer it
//          returns can be used like the iterator returned by begin().
//      
// Note that this container can work for implementing small queues, 
// since the push_... and emplace_... functions slide the data 
// rather than overflowing, but it may be slower than std::deque 
// or std::list in that role.
//
// Iterators, references, and pointers to elements remain valid 
// through all operations except erase() and insert() unless the
// operation slides data to avoid overflow.
//
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
#ifndef FRYSTL_STATIC_DEQUE
#define FRYSTL_STATIC_DEQUE
#include <iterator>  // std::reverse_iterator, iterator_traits, input_iterator_tag, random_access_iterator_tag
#include <algorithm> // std::move...(), equal(), lexicographical_compare()
#include <initializer_list>
#include <stdexcept> // for std::out_of_range
#include <cstdint>   // uint32_t etc.
#include "frystl-defines.hpp"

namespace frystl
{
    template <typename value_type, unsigned Capacity>
    class static_deque
    {
    public:
        using this_type = static_deque<value_type,Capacity>;
        using reference = value_type &;
        using const_reference = const value_type &;
        using size_type = uint32_t;
        using difference_type = std::ptrdiff_t;
        using pointer = value_type *;
        using const_pointer = const value_type *;
        using iterator = pointer;
        using const_iterator = const_pointer;
        using reverse_iterator = std::reverse_iterator<iterator>;
        using const_reverse_iterator = std::reverse_iterator<const_iterator>;

        // default c'tor
        static_deque() noexcept
            : _begin(Centered(1)), _end(_begin)
        {
        }
        // fill c'tor with explicit value
        static_deque(size_type count, const_reference value)
            : _begin(Centered(count)), _end(_begin + count)
        {
            FRYSTL_ASSERT2(count <= capacity(),"Overflow in static_deque");
            for (pointer p = _begin; p < _end; ++p)
                new (p) value_type(value);
        }
        // fill c'tor with default value
        explicit static_deque(size_type count)
            : _begin(Centered(count)), _end(_begin + count)
        {
            FRYSTL_ASSERT2(count <= capacity(),"Overflow in static_deque");
            for (pointer p = _begin; p < _end; ++p)
                new (p) value_type();
        }
        // range c'tor
        template <class Iter,
                  typename = RequireInputIter<Iter> > 
        static_deque(Iter begin, Iter end)
        {
            Center(begin,end,
                typename std::iterator_traits<Iter>::iterator_category());
            for (Iter k = begin; k != end; ++k) {
                emplace_back(*k);
            }
        }
        // copy constructors

        static_deque(const this_type &donor)
            : _begin(Centered(donor.size()))
            , _end(_begin)
        {
            for (auto &m : donor)
                emplace_back(m);
        }
        template <unsigned C1>
        static_deque(const static_deque<value_type, C1> &donor)
            : _begin(Centered(donor.size()))
            , _end(_begin)
        {
            FRYSTL_ASSERT2(donor.size() <= capacity(), "Too big");
            for (auto &m : donor)
                emplace_back(m);
        }
        // move constructors
        // Constructs the new static_deque by moving all the elements of
        // the existing static_deque.  It leaves the moved-from object
        // empty.
        static_deque(this_type &&donor) noexcept
            : _begin(Centered(donor.size()))
            , _end(_begin)
        {
            for (auto &m : donor)
                emplace_back(std::move(m));
            donor.clear();
        }
        template <unsigned C1>
        static_deque(static_deque<value_type, C1> &&donor) noexcept
            : _begin(Centered(donor.size()))
            , _end(_begin)
        {
            FRYSTL_ASSERT2(donor.size() <= capacity(),"Overflow");
            for (auto &m : donor)
                emplace_back(std::move(m));
            donor.clear();
        }
        // initializer list constructor
        static_deque(std::initializer_list<value_type> il)
            : _begin(Centered(il.size()))
            , _end(_begin)
        {
            FRYSTL_ASSERT2(il.size() <= capacity(),"Overflow");
            for (auto &value : il)
                emplace_back(value);
        }
        ~static_deque() noexcept
        {
            DestroyAll();
        }
        void clear() noexcept
        {
            DestroyAll();
            _begin = _end = Centered(1);
        }
        size_type size() const noexcept
        {
            return _end - _begin;
        }
        bool empty() const noexcept
        {
            return _begin == _end;
        }
        size_type capacity() const noexcept
        {
            return Capacity;
        }
        template <class... Args>
        [[maybe_unused]] reference emplace_front(Args&&... args)
        {
            FRYSTL_ASSERT2(size() < capacity(),"static_deque overflow");
            if (_begin == FirstSpace()) SlideAllToBack();
            Construct(_begin-1, std::forward<Args>(args)...);
            --_begin;
            return *_begin;
        }
        void push_front(const_reference t)
        {
            FRYSTL_ASSERT2(size() < capacity(),"static_deque overflow");
            if (_begin == FirstSpace()) SlideAllToBack();
            Construct(_begin-1, t);
            --_begin;
        }
        void push_front(value_type&& t) noexcept
        {
            FRYSTL_ASSERT2(size() < capacity(),"static_deque overflow");
            if (_begin == FirstSpace()) SlideAllToBack();
            Construct(_begin-1, std::move(t));
            --_begin;
        }
        void pop_front() noexcept
        {
            FRYSTL_ASSERT2(_begin < _end, "pop_front called on empty static_deque");
            ++_begin;
            Destroy(_begin-1);
        }
        template <class... Args>
        [[maybe_unused]] reference emplace_back(Args&&... args)
        {
            FRYSTL_ASSERT2(size() < capacity(),"static_deque overflow");
            if (_end == PastLastSpace()) SlideAllToFront();
            Construct(_end,std::forward<Args>(args)...);
            ++_end;
            return *(_end-1);
        }
        void push_back(const_reference t) 
        {
            FRYSTL_ASSERT2(size() < capacity(),"static_deque overflow");
            if (_end == PastLastSpace()) SlideAllToFront();
            Construct(_end, t);
            ++_end;
        }
        void push_back(value_type && t) noexcept
        {
            FRYSTL_ASSERT2(size() < capacity(),"static_deque overflow");
            if (_end == PastLastSpace()) SlideAllToFront();
            Construct(_end, std::move(t));
            ++_end;
        }
        void pop_back() noexcept
        {
            FRYSTL_ASSERT2(_begin < _end,"pop_back() called on empty static_deque");
            --_end;
            Destroy(_end);
        }

        reference operator[](size_type index) noexcept
        {
            FRYSTL_ASSERT2(_begin + index < _end,"Index out of range");
            return *(_begin + index);
        }
        const_reference operator[](size_type index) const noexcept
        {
            FRYSTL_ASSERT2(_begin + index < _end,"Index out of range");
            return *(_begin + index);
        }

        pointer data() noexcept 
        { 
            return _begin; 
        }

        const_pointer data() const noexcept
        { 
            return _begin; 
        }

        reference at(size_type index)
        {
            Verify(_begin + index < _end);
            return *(_begin + index);
        }
        const_reference at(size_type index) const
        {
            Verify(_begin + index < _end);
            return *(_begin + index);
        }
        reference front() noexcept
        {
            FRYSTL_ASSERT2(_begin < _end,"front() called on empty static_deque");
            return *_begin;
        }
        const_reference front() const noexcept
        {
            FRYSTL_ASSERT2(_begin < _end,"front() called on empty static_deque");
            return *_begin;
        }
        reference back() noexcept
        {
            FRYSTL_ASSERT2(_begin < _end,"back() called on empty static_deque");
            return *(_end-1);
        }
        const_reference back() const noexcept
        {
            FRYSTL_ASSERT2(_begin < _end,"back() called on empty static_deque");
            return *(_end-1);
        }
        template <class... Args>
        iterator emplace(const_iterator pos, Args && ... args)
        {
            FRYSTL_ASSERT2(cbegin() <= pos && pos <= cend(),
                "Invalid position in static_deque::emplace()");
            unsigned offset = pos - cbegin();
            if (pos == _begin) emplace_front(std::forward<Args>(args)...);
            else if (pos == _end) emplace_back(std::forward<Args>(args)...);
            else {
                iterator p = MakeRoom(pos,1);
                Construct(p,std::forward<Args>(args)...);
            }
            return begin()+offset;
        }
        //
        //  Assignment functions
        void assign(size_type n, const_reference val)
        {
            FRYSTL_ASSERT2(n <= capacity(),"Overflow in static_deque::assign()");
            DestroyAll(); 
            _begin = _end = Centered(n);
            while (size() < n)
                push_back(val);
        }
        void assign(std::initializer_list<value_type> x)
        {
            FRYSTL_ASSERT2(x.size() <= capacity(),"Overflow in static_deque::assign()");
            DestroyAll();
            _begin = _end = Centered(x.size());
            for (auto &a : x)
                emplace_back(a);
        }
        template <class Iter,
                  typename = RequireInputIter<Iter>>
        void assign(Iter begin, Iter end)
        {
            DestroyAll();
            Center(begin,end,
                typename std::iterator_traits<Iter>::iterator_category());
            for (Iter k = begin; k != end; ++k) {
                push_back(*k);
            }
        }
        this_type &operator=(const this_type &other) 
        {
            if (this != &other)
                assign(other.begin(), other.end());
            return *this;
        }
        this_type &operator=(this_type &&other) noexcept
        {
            if (this != &other)
            {
                clear();
                for (auto &o : other)
                    push_back(std::move(o));
                other.clear();
            }
            return *this;
        }
        this_type &operator=(std::initializer_list<value_type> il)
        {
            assign(il);
            return *this;
        }
        // single element insert()
        iterator insert(const_iterator position, const value_type &val)
        {
            return insert(position, 1, val);
        }
        // move insert()
        iterator insert(const_iterator position, value_type &&val) noexcept
        {
            FRYSTL_ASSERT2(begin() <= position && position <= end(),
                "Bad position argument in static_deque::insert()");
            iterator b = begin();
            iterator e = end();
            iterator t = MakeRoom(position, 1);
            if (b <= t && t < e)
                (*t) = std::move(val);
            else 
                Construct(t, std::move(val));
            return t;
        }
        // fill insert
        iterator insert(const_iterator position, size_type n, const_reference val)
        {
            FRYSTL_ASSERT2(begin() <= position && position <= end(),
                "Bad position argument in static_deque::insert()");
            iterator b = begin();
            iterator e = end();
            iterator t = MakeRoom(position, n);
            // copy val n times into newly available cells
            for (iterator i = t; i < t + n; ++i)
            {
                FillCell(b, e, i, val);
            }
            return t;
        }
        // range insert()
        private:
            // implementation for iterators with no operator-()
            template <class InpIter>
            iterator insert(
                const_iterator position, 
                InpIter first, 
                InpIter last,
                std::input_iterator_tag)
            {
                size_type posIndex = position-begin();
                size_type oldSize = size();
                while (first != last) {
                    emplace_back(*first++);
                }
                std::rotate(begin()+posIndex, begin()+oldSize, end());
                return begin()+posIndex;
            }
            // Implementation for iterators having operator-()
            template <class DAIter>
            iterator insert(
                const_iterator position, 
                DAIter first, 
                DAIter last,
                std::random_access_iterator_tag)
            {
                iterator b = begin();
                iterator e = end();
                int n = last-first;
                iterator t = MakeRoom(position,n);
                iterator result = t;
                while (first != last) {
                    FillCell(b, e, t++, *first++);
                }
                return result;
            }
        public:
        template <class Iter,typename = RequireInputIter<Iter>>
        iterator insert(const_iterator position, Iter first, Iter last)
        {
            return insert(position,first,last,
                typename std::iterator_traits<Iter>::iterator_category());
        }
        // initializer list insert()
        iterator insert(const_iterator position, std::initializer_list<value_type> il)
        {
            size_type n = il.size();
            FRYSTL_ASSERT2(begin() <= position && position <= end(),
                "Bad position argument in static_deque::insert()");
            iterator b = begin();
            iterator e = end();
            iterator t = MakeRoom(position, n);
            // copy il into newly available cells
            auto j = il.begin();
            for (iterator i = t; i < t + n; ++i, ++j)
            {
                FillCell(b, e, i, *j);
            }
            return t;
        }
        void resize(size_type n, const value_type &val)
        {
            FRYSTL_ASSERT2(n <= capacity(),"n too large in static_deque::resize(n,value)");
            while (n < size())
                pop_back();
            while (size() < n)
                push_back(val);
        }
        void resize(size_type n)
        {
            FRYSTL_ASSERT2(n <= capacity(),"n too large in static_deque::resize(n)");
            while (n < size())
                pop_back();
            while (size() < n)
                emplace_back();
        }
        void swap(this_type &x) noexcept
        {
            std::swap(*this, x);
        }
        void shrink_to_fit()
        {}                  // does nothing
        iterator begin() noexcept
        {
            return _begin;
        }
        iterator end()
        {
            return _end;
        }
        const_iterator begin() const noexcept
        {
            return _begin;
        }
        const_iterator end() const noexcept
        {
            return _end;
        }
        const_iterator cbegin() noexcept
        {
            return _begin;
        }
        const_iterator cend() noexcept
        {
            return _end;
        }
        reverse_iterator rbegin() noexcept
        {
            return reverse_iterator(_end);
        }
        reverse_iterator rend() noexcept
        {
            return reverse_iterator(_begin);
        }
        const_reverse_iterator crbegin() const noexcept
        {
            return const_reverse_iterator(_end);
        }
        const_reverse_iterator crend() const noexcept
        {
            return const_reverse_iterator(_begin);
        }
        iterator erase(const_iterator first, const_iterator last) noexcept
        {
            iterator result = const_cast<iterator>(last);
            if (first != last)
            {
                FRYSTL_ASSERT2(first < last,"Bad arguments to static_deque::erase()");
                FRYSTL_ASSERT2(Dereferencable(first),"Bad arguments to static_deque::erase()");
                FRYSTL_ASSERT2(Dereferencable(last-1),"Bad arguments to static_deque::erase()");
                const iterator f = const_cast<iterator>(first);
                const iterator l = const_cast<iterator>(last);
                unsigned nToErase = last-first;
                for (iterator it = f; it < l; ++it)
                    Destroy(it);
                if (first-_begin < _end-last) {
                    // Move the elements before first
                    std::move_backward(_begin,f,l);
                    _begin += nToErase;
                    result = l;
                } else {
                    // Move the elements at and after last
                    std::move(l, end(), f);
                    _end -= nToErase;
                    result = f;
                }
            }
            return result;
        }
        iterator erase(const_iterator position) noexcept
        {
            return erase(position, position+1);
        }

    private:
        using storage_type =
            std::aligned_storage_t<sizeof(value_type), alignof(value_type)>;
        pointer _begin;
        pointer _end;
        storage_type _elem[Capacity];

        pointer FirstSpace() noexcept
        {
            return reinterpret_cast<pointer>(_elem);
        }
        const_pointer FirstSpace() const noexcept
        {
            return reinterpret_cast<const_pointer>(_elem);
        }
        pointer PastLastSpace() noexcept
        {
            return reinterpret_cast<pointer>(_elem+capacity());
        }
        const_pointer PastLastSpace() const noexcept
        {
            return reinterpret_cast<const_pointer>(_elem+capacity());
        }
        const_pointer Data() const noexcept
        {
            return reinterpret_cast<const_pointer>(_elem);
        }
        static void Verify(bool cond)
        {
            if (!cond)
                throw std::out_of_range("static_deque range error");
        }
        // returns true iff iter can be dereferenced.
        bool Dereferencable(const const_iterator &iter) const noexcept
        {
            return begin() <= iter && iter < end();
        }
        // Slide cells at and behind p to the back by n spaces.
        // Return an iterator pointing to the first cleared cell (p).
        // Update _end.
        iterator MakeRoomAfter(iterator p, size_type n) noexcept
        {
            SlideToBack(p,_end+n);
            _end += n;
            return p;
        }
        // Slide cells before p to the front by n spaces.
        // Return an iterator pointing to the first cleared cell (p-n).
        // Update _begin.
        iterator MakeRoomBefore(iterator p, size_type n) noexcept
        {
            SlideToFront(p, _begin-n);
            _begin -= n;
            return p-n;
        }
        // Slide cells such that there are n empty cells between 
        // constp and constp+n.  The order of all other cells
        // is preserved.  Update _begin or _end or both. 
        // Return an iterator pointing to the first cleared space, which
        // may be different from constp.
        iterator MakeRoom(const_iterator constp, size_type n) noexcept
        {
            FRYSTL_ASSERT2(size()+n <= capacity(), "static_deque overflow");
            iterator p = const_cast<iterator>(constp);
            if (end()-p < p-begin() && end()+n <= PastLastSpace())
                return MakeRoomAfter(p, n);
            else if (FirstSpace() + n <= _begin)
                return MakeRoomBefore(p, n);
            else {
                // Neither side has enough extra space
                p -= _begin - FirstSpace();
                SlideAllToFront();
                return MakeRoomAfter(p, n);
            }
        }
        // Return a pointer to the front end of a range of n cells centered
        // in the space.
        constexpr pointer Centered(unsigned n) noexcept
        {
            return FirstSpace()+(Capacity-n)/2;
        }
        template <class RAIter>
        void Center(RAIter begin, RAIter end, std::random_access_iterator_tag) noexcept
        {
            FRYSTL_ASSERT2(end-begin <= capacity(), "Overflow");
            _begin = _end = Centered(end-begin);
        }
        template <class InpIter>
        void Center(InpIter begin, InpIter end, std::input_iterator_tag) noexcept
        {
            _begin = _end = FirstSpace();
        }
        template <class... Args>
        void FillCell(const_iterator b, const_iterator e, iterator pos, Args... args)
        {
            if (b <= pos && pos < e)
                // fill previously occupied cell using assignment
                (*pos)  = value_type(args...);
            else 
                // fill unoccupied cell in place by constructon
                new (pos) value_type(args...);
        }
        void DestroyAll() noexcept
        {
            for (reference elem : *this) Destroy(&elem);
        }
        void SlideAllToFront() noexcept
        {
            auto sz = size();
            SlideToFront(_end, FirstSpace());
            _begin = FirstSpace();
            _end = _begin + sz;
        }
        void SlideToFront(pointer last, pointer tgt) noexcept
        {
            pointer src = _begin;
            while (src != last && tgt < _begin) {
                    new(tgt++) value_type(std::move(*src++));                
            }
            std::move(src, last, tgt);
        }
        void SlideAllToBack() noexcept
        {
            auto sz = size();
            SlideToBack(_begin, PastLastSpace());
            _end = PastLastSpace();
            _begin = _end-sz;
        }
        void SlideToBack(pointer first, pointer end) noexcept
        {
            pointer tgt = end;
            pointer src = _end;
            while (first < src && _end < tgt) {
                new(--tgt) value_type(std::move(*--src));
            }
            std::move_backward(first, src, tgt);
        }
    };
    //
    //*******  Non-member overloads
    //
    template <class T, unsigned C0, unsigned C1>
    bool operator==(const static_deque<T, C0> &lhs, const static_deque<T, C1> &rhs) noexcept
    {
        if (lhs.size() != rhs.size())
            return false;
        return std::equal(lhs.begin(), lhs.end(), rhs.begin());
    }
    template <class T, unsigned C0, unsigned C1>
    bool operator!=(const static_deque<T, C0> &lhs, const static_deque<T, C1> &rhs) noexcept
    {
        return !(rhs == lhs);
    }
    template <class T, unsigned C0, unsigned C1>
    bool operator<(const static_deque<T, C0> &lhs, const static_deque<T, C1> &rhs) noexcept
    {
        return std::lexicographical_compare(lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
    }
    template <class T, unsigned C0, unsigned C1>
    bool operator<=(const static_deque<T, C0> &lhs, const static_deque<T, C1> &rhs) noexcept
    {
        return !(rhs < lhs);
    }
    template <class T, unsigned C0, unsigned C1>
    bool operator>(const static_deque<T, C0> &lhs, const static_deque<T, C1> &rhs) noexcept
    {
        return rhs < lhs;
    }
    template <class T, unsigned C0, unsigned C1>
    bool operator>=(const static_deque<T, C0> &lhs, const static_deque<T, C1> &rhs) noexcept
    {
        return !(lhs < rhs);
    }

    template <class T, unsigned C>
    void swap(static_deque<T, C> &a, static_deque<T, C> &b) noexcept
    {
        a.swap(b);
    }
}       // namespace frystl
#endif  // ndef FRYSTL_STATIC_DEQUE
