# frystl
# What is this?
This repository contains three C++ template classes having (almost) the same interface as STL classes 
but with better performance in some sense in some uses. They were originally minimal implementations
for use in my *KSolve* Klondike Solitaire solver, but I later filled them out to have all the 
functionality of their model STL classes, with some exceptions that stem from their data structures 
or my programming limitations. Detailed documentation is provided in the headers.

They require at least c++ 17.
## static_vector
Like an STL vector, one of these contains a sequence of items of like type that provides efficient
random access, can be grown and shrunk, and keeps track of its own length.  Unlike an STL vector,
it does not use dynamic memory.  There is a fixed limit to its size specified at compile time. 
An STL vector uses a small, fixed amount of memory where it is created and an array to contain its 
data taken from dynamic memory. A static_vector resides entirely where it is created.
## static_deque
This is a static implementation of an STL deque.  Like an STL deque, it can be expanded efficiently
at either end (unlike a vector, which can be expanded efficiently only at the back). A static_deque
has a fixed size limit declared at compile time.  Rather than overflow, it will shift elements away from the the end of the imminent overflow
if possible (although that will hurt performance). Its random access speed is actually better
than that of an STL deque because an STL deque need two levels of index.
## mf_vector
This stands for *memory friendly vector.* It is a drop-in replacement for almost any STL vector that
uses the standard allocator (it has no support for allocators) but has different performance characteristics.
Like an STL vector, it can grow dynamically. When an STL vector overflows the space in the dynamic
memory it has reserved for its elements, it allocates a larger array (often double the original size)
and copies the members to that larger array.  During this process, it is commonly using three times the memory
needed by its members.  It must do this to (1) give maximum random access speed, and (2) allow the member
array do be accessed as a C array using the *data()* method. In contrast, an mf_vector grows by adding
fixed sized blocks, like an STL deque, so a static vector with block size *b* holding a sequence of *n* 
elements never allocates more than space for *n+b* elements. Its random access speed is slower than
that of an STL vector because it uses two levels of index. It has no *data()* member function.

An mf_vector offers another advantage. It is stable with respect to *push_back()* or *emplace_back()* 
calls.  That is, pointers
and references are still valid after such a call. 
It also means that up to a predefined size, which can easily be made arbitrarily large, read access 
is safe in a multi-threaded program as long as the only changes being made are new elements added
at the back. In that circustance, iterators (except *end()*) also remain valid.
