#pragma once


namespace Evex
{
	/*
		Evex iterator class, constructed in lieu of the capability to use generic iterators.
		It was either this, or put triple templates on everything. Needless to say, I chose this.
	*/
	template<typename T>
	class RegexRangeIterator
	{
	public:
		RegexRangeIterator() {}

		template<typename IterType>
		RegexRangeIterator(IterType& c, IterType& b, IterType& e) : Current(&*c), Begin(&*b), End((&*(--e))) { ++End; }
		RegexRangeIterator(const T* c, const T* b, const T* e) : Current(c), Begin(b), End(e) {}
		RegexRangeIterator(const RegexRangeIterator& o) : Current(o.Current), Begin(o.Begin), End(o.End) {}

		RegexRangeIterator& operator=(const RegexRangeIterator& o) { Current = o.Current; Begin = o.Begin; End = o.End; return *this; }

		inline bool operator==(const RegexRangeIterator<T>& o) const { return Current == o.Current && Begin == o.Begin && End == o.End; }
		inline bool operator!=(const RegexRangeIterator<T>& o) const { return !(*this == o); }

		inline bool IsPreBegin() const { return Current == Begin - 1; }
		inline bool IsBegin() const { return Current == Begin; }
		inline bool IsEnd() const { return Current == End; }

		inline RegexRangeIterator CloneAtBegin() { return RegexRangeIterator(Begin, Begin, End); }
		inline RegexRangeIterator CloneAtEnd() { return RegexRangeIterator(End, Begin, End); }

		inline operator const T*() const { return Current; }

		inline RegexRangeIterator& operator++() { ++Current; return *this; }
		inline RegexRangeIterator& operator++(int) { Current++; return *this; }

		inline RegexRangeIterator& operator--() { --Current; return *this; }
		inline RegexRangeIterator& operator--(int) { Current--; return *this; }

	private:
		// And this is where I'd put my iterators... IF THEY WERE ACTUALLY CONSIDERED TYPES.
		const T* Current = nullptr, *Begin = nullptr, *End = nullptr;
	};
}
