#pragma once

#include "EvexRangeIterator.h"

#include <vector>


namespace Evex
{
	template<typename T>
	struct RegexCharacterClassSymbol
	{
		using IterType = RegexRangeIterator<T>;

		bool Ligature = false;
		std::vector<T> Characters;

		RegexCharacterClassSymbol(const T& c) : Characters({ c, c }) {}
		RegexCharacterClassSymbol(const T& m, const T& M, bool Digraph = false) : Characters({ m, M }), Ligature(Digraph) {}
		RegexCharacterClassSymbol(const std::vector<T>& Chars) : Characters(Chars), Ligature(true) {}

		bool Matches(IterType& input, bool CaseInsensitive)
		{
			if (!Ligature)
			{
				if (CaseInsensitive)
					return tolower(Characters[0]) <= tolower(*input) && tolower(*input) <= tolower(Characters[1]);
				else
					return Characters[0] <= *input && *input <= Characters[1];
			}
			else
			{
				IterType Next = input;
				unsigned int CharInd = 0;
				while (!Next.IsEnd() && CharInd < Characters.size())
				{
					if ((CaseInsensitive ? tolower(*Next) : *Next) != (CaseInsensitive ? tolower(Characters[CharInd]) : Characters[CharInd]))
						return false;

					++Next;
					++CharInd;
				}

				input = --Next;

				return CharInd >= Characters.size();
			}
		}

		std::basic_string<T> Written()
		{
			std::basic_ostringstream<T> oss;

			if (Characters[0] == Characters[1])
				oss << Characters[0];
			else if (!Ligature)
				oss << Characters[0] << "-" << Characters[1];
			else
			{
				oss << "\'";
				for (T& currChar : Characters)
					oss << currChar;
				oss << "\'";
			}

			return oss.str();
		}
	};

	template<typename T>
	struct RegexCharacterClassBase
	{
		using StringType = std::basic_string<T>;
		using IterType = RegexRangeIterator<T>;

		bool Negate = false;

		// Is the given input present within this character class?
		inline virtual bool Matches(IterType& Input) = 0;

		// Outputs the written character class form of the given character class (without brackets!)
		virtual StringType WrittenForm() = 0;
	};

	template<typename T>
	struct RegexCharacterClass : public RegexCharacterClassBase<T>
	{
		bool CaseInsensitive = false;

		std::vector<RegexCharacterClassSymbol<T>*> Symbols;

		RegexCharacterClass(const std::vector<RegexCharacterClassSymbol<T>*>& c, bool n = false, bool ci = false)
			: Symbols(c), CaseInsensitive(ci) {
			Negate = n;
		}

		inline bool Matches(IterType& Input) final
		{
			bool Success = false;
			for (RegexCharacterClassSymbol<T>* currSymb : Symbols)
			{
				Success = Success || currSymb->Matches(Input, CaseInsensitive);

				if (Success)
					break;
			}

			return (Negate ? !Success : Success);
		}

		virtual StringType WrittenForm()
		{
			StringType out = (Negate ? "^" : "");
			for (RegexCharacterClassSymbol<T>* currSymb : Symbols)
				out += currSymb->Written();
			return out;
		}
	};

	template<typename T>
	struct RegexSubtractCharacterClass : public RegexCharacterClassBase<T>
	{
		RegexCharacterClassBase<T>* lhs = nullptr, *rhs = nullptr;

		RegexSubtractCharacterClass(RegexCharacterClassBase<T>* l, RegexCharacterClassBase<T>* r) : lhs(l), rhs(r) {}
		inline bool Matches(IterType& Input) final { return lhs->Matches(Input) && !rhs->Matches(Input); }
		virtual StringType WrittenForm() { return (lhs->Negate ? "^" : "") + lhs->WrittenForm() + "-[" + rhs->WrittenForm() + "]"; }
	};

	template<typename T>
	struct RegexIntersectCharacterClass : public RegexCharacterClassBase<T>
	{
		RegexCharacterClassBase<T>* lhs = nullptr, *rhs = nullptr;

		RegexIntersectCharacterClass(RegexCharacterClassBase<T>* l, RegexCharacterClassBase<T>* r) : lhs(l), rhs(r) {}
		inline bool Matches(IterType& Input) final { return lhs->Matches(Input) && rhs->Matches(Input); }
		virtual StringType WrittenForm() { return (lhs->Negate ? "^" : "") + lhs->WrittenForm() + "&&[" + rhs->WrittenForm() + "]"; }
	};

	template<typename T>
	struct RegexUnionCharacterClass : public RegexCharacterClassBase<T>
	{
		RegexCharacterClassBase<T>* lhs = nullptr, *rhs = nullptr;

		RegexUnionCharacterClass(RegexCharacterClassBase<T>* l, RegexCharacterClassBase<T>* r) : lhs(l), rhs(r) {}
		inline bool Matches(IterType& Input) final { return lhs->Matches(Input) || rhs->Matches(Input); }
		virtual StringType WrittenForm() { return (lhs->Negate ? "^" : "") + lhs->WrittenForm() + "[" + rhs->WrittenForm() + "]"; }
	};
}
