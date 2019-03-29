#pragma once

#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <string>

namespace Evex
{
	enum class RegexInstructionType : uint8_t
	{
		MakeCapture, // std::string manual, std::string[] names (if empty then will just be reffed by index)
		MakeCaptureCollection, // std::string manual, std::string[] names (if empty then will just be reffed by index)
	
		MakeCharClassSymbol, // std::string minChar, std::string maxChar
		MakeCharClassLigatureSymbol, // std::string[] charsInvolved
	
		MakeLiteralCharClass, // std::string negated, std::string caseinsensitive, std::string[] inds
	
		MakeUnitedCharClass, // std::string lhsInd, std::string rhsInd
		MakeSubtractedCharClass, // std::string lhsInd, std::string rhsInd
		MakeIntersectedCharClass, // std::string lhsInd, std::string rhsInd
	
		Literal, // std::string[] characterclassindices
	
		StartCheck, // std::string IsExclusive, std::string IsLastMatchEnd, std::string LineCharsInd
		EndCheck, // std::string IsExclusive, std::string BeforeLastNewline, std::string LineCharsInd
	
		WordBoundary, // std::string negated, std::string WordCharsInd
	
		Backref_Numbered, // std::string capturenumber
		Backref_Named, // std::string capturename
	
		Subroutine_Numbered, // std::string capturenumber, std::string maxdepth
		Subroutine_Named, // std::string capturename, std::string maxdepth
	
		Recursion, // std::string maxdepth, std::string Lazy
	
		CaptureGroup_Numbered, // std::string index, std::string Lazy. Pop one off stack, make group, put back in.
		CaptureGroup_Named, // std::string name, std::string Lazy. Pop one off stack, make group, put back in.
	
		NonCaptureGroup, // std::string Lazy. Pop one off stack, make group, put back in.
	
		LookAhead, // std::string negated, std::string lazy. pop one off stack, make lookahead, put back in.
		LookBehind, // std::string negated, std::string lazy. pop one off stack, make lookbehind, put back in.
	
		DefineAsSubroutine, // std::string name. pops one off the stack, and that's it. i.e. makes a chunk only accessible via subroutine call.
	
		CodeHook, // std::string funcname
	
		Conditional, // std::string NumConds, std::string Lazy. pop NumConds + 1, i.e. 2 or 3, off the top, create a conditional with them, place back in
	
		NOnce, // pop one off stack, make none-or-once, put back in
		NOnceLazy,
		NPlus, // pop one off stack, make none-plus, put back in
		NPlusLazy,
		OPlus, // pop one off stack, make one-plus, put back in
		OPlusLazy,
		Repeat, // std::string type, std::string mintimes, std::string maxtimes. pop one off stack, make the respective repeat, put back in
		RepeatLazy,
	
		Concat, // pop two off the top, concat them together, place back in.
		Alternate, // pop two off the top, alt them together, place back in.
	
	
		MAX
	};
	
	template<typename T>
	struct RegexInstruction
	{
		RegexInstructionType InstructionType;
		std::vector<std::basic_string<T>> InstructionData;
	};
	
	
	/*
		Translates a given infix regex into a postfix set of instructions.
	
		Note: Class is useless on its own! Needs template specializations to function properly.
	*/
	template<typename T>
	class RegexTranslator
	{
	public:
		template<typename ContainerType, typename IterType>
		static std::vector<RegexInstruction<T>> Translate(ContainerType& Infix, IterType& iter, std::string& error)
		{
			static_assert(false, "RegexTranslator<T> requires a specialization for the current type.");
			return {};
		}
	};
	
	template<>
	class RegexTranslator<char>
	{
		using ContainerType = std::basic_string<char>;
		using IterType = std::basic_string<char>::iterator;
		using ConstIterType = std::basic_string<char>::const_iterator;
		using InstructionSet = std::vector<RegexInstruction<char>>;
	
		struct IndexTracker
		{
			int NextIndex = 0;
			std::unordered_map<ContainerType, int> Indices;
		};
	
		struct Modifiers
		{
			bool CaseInsensitive : 1, // (?i), (?c)
				DotAll : 1, // (?a)
				SingleLine : 1, // (?s), (?m)
				NoAutoCap : 1, // (?n)
				UnixLines : 1, // (?d)
				LazyGroups : 1; // (?l)
	
			Modifiers()
			{
				CaseInsensitive =
					DotAll =
					SingleLine =
					NoAutoCap =
					UnixLines =
					LazyGroups = false;
			}
		};
	
		// Handles creation of Character Class Symbols.
		static ContainerType MunchCharClassSymbol(IterType& iter,
			ConstIterType& endIter,
			std::string& error,
			IndexTracker& CCSymbsToInds,
			InstructionSet& OutInstructions)
		{
			ContainerType Munched;
	
			if (*iter == '.') // potential ligature
			{
				IterType EndDot = iter;
				Munched += *iter;
				while (*(++EndDot) != '.' && EndDot != endIter)
					Munched += *EndDot;
				Munched += *EndDot;
	
				auto found = CCSymbsToInds.Indices.find(Munched);
				if (found != CCSymbsToInds.Indices.end())
				{
					iter = EndDot;
					return std::to_string(found->second);
				}
	
				if (--EndDot != iter)
				{
					ContainerType SnippedMunched = Munched;
					SnippedMunched.erase(SnippedMunched.begin());
					SnippedMunched.pop_back();
	
					std::vector<ContainerType> Pieces;
					for (char& currPiece : SnippedMunched)
						Pieces.push_back(ContainerType(1, currPiece));
	
					iter = ++EndDot;
	
					OutInstructions.push_back({ RegexInstructionType::MakeCharClassLigatureSymbol, Pieces });
	
					CCSymbsToInds.Indices[Munched] = CCSymbsToInds.NextIndex;
					return std::to_string(CCSymbsToInds.NextIndex++);
				}
			}
			Munched.clear();
	
			IterType ahead = iter;
			bool IsRange = (++ahead != endIter && *ahead == '-') && (++ahead != endIter && *ahead != '[' && *ahead != '\\');
			if (IsRange) // range ("a-b")
			{
				ContainerType min(1, *iter), max(1, *ahead);
				Munched = min + '-' + max;
	
				auto found = CCSymbsToInds.Indices.find(Munched);
				if (found != CCSymbsToInds.Indices.end())
				{
					iter = ahead;
					return std::to_string(found->second);
				}
	
				if (min[0] < max[0])
					OutInstructions.push_back({ RegexInstructionType::MakeCharClassSymbol, { min, max } });
				else
				{
					error = "Regex Compile Error: Character range invalid. "
						"(#" + std::to_string((int)min[0]) + "-#" + std::to_string((int)max[0]) + ")";
					return "-1";
				}
				iter = ahead;
			}
			else // single ("a")
			{
				Munched += *iter;
	
				auto found = CCSymbsToInds.Indices.find(Munched);
				if (found != CCSymbsToInds.Indices.end())
					return std::to_string(found->second);
	
				OutInstructions.push_back({ RegexInstructionType::MakeCharClassSymbol, { Munched, Munched } });
			}
	
			CCSymbsToInds.Indices[Munched] = CCSymbsToInds.NextIndex;
			return std::to_string(CCSymbsToInds.NextIndex++);
		}
	
		// Handles creation of direct literal nodes, i.e. nodes that are just "a" rather than a Character Class like "[a-z]".
		static void MunchLiteral(IterType& iter,
			ConstIterType& endIter,
			std::string& error,
			IndexTracker& CCSymbsToInds,
			IndexTracker& CCsToInds,
			InstructionSet& OutInstructions,
			Modifiers& Modifs)
		{
			ContainerType CCName(1, *iter);
	
			auto found = CCsToInds.Indices.find(CCName);
			if (found != CCsToInds.Indices.end())
			{
				OutInstructions.push_back({ RegexInstructionType::Literal, { std::to_string(found->second) } });
				return;
			}
	
			ContainerType Ind;
			{
				ContainerType CCSymbName(1, *iter);
	
				auto found = CCSymbsToInds.Indices.find(CCSymbName);
				if (found != CCSymbsToInds.Indices.end())
					Ind = std::to_string(found->second);
	
				OutInstructions.push_back({ RegexInstructionType::MakeCharClassSymbol,{ CCSymbName, CCSymbName } });
	
				CCSymbsToInds.Indices[CCSymbName] = CCSymbsToInds.NextIndex;
				Ind = std::to_string(CCSymbsToInds.NextIndex++);
			}
	
			OutInstructions.push_back({ RegexInstructionType::MakeLiteralCharClass, { "f", (Modifs.CaseInsensitive ? "t" : "f"), Ind } });
	
			CCsToInds.Indices[CCName] = CCsToInds.NextIndex;
	
			OutInstructions.push_back({ RegexInstructionType::Literal, { std::to_string(CCsToInds.NextIndex++) } });
		}
	
		// Does not handle Shorthand Character Classes on its own; rather, it returns the real class behind the shorthand for construction elsewhere.
		static ContainerType ExpandShorthand(IterType& iter)
		{
			ContainerType Pre = "";
			if (isupper(*iter))
				Pre += '^';
	
			char TrueChecked = tolower(*iter);
	
			switch (TrueChecked)
			{
			case 'l': // lowercase ("[a-z]")
				return Pre + "a-z";
	
			case 'u': // uppercase ("[A-Z]")
				return Pre + "A-Z";
	
			case 'd': // digit ("[0-9]")
				return Pre + "0-9";
	
			case 'w': // word character ("[A-Za-z0-9_]")
				return Pre + "A-Za-z0-9_";
	
			case 's': // whitespace ("[ \t\r\n\f]", "[ \t\r\n\f\v]")
				return Pre + " \t\r\n\v\f";
	
			case 'h': // horizontal whitespace ("[ \t]")
				return Pre + " \t";
	
			case 'v': // vertical whitespace ("[\r\n\v\f]")
				return Pre + "\r\n\v\f";
	
			case 'n': // not newline ("[^\n]")
				return Pre + "\n";
	
			case 'r': // \R, i.e. not digraph newline ("[[.\r\n.]\\v]")
				return Pre + ".\r\n.\r\n\v\f";
	
			default:
				return "";
			}
		}

		// Helper function for finding the proper end brace of a group/character class/name/etc.
		static void FindEnd(IterType& iter, ConstIterType& endIter, char startBracket, char endBracket, ContainerType* Output)
		{
			int Depth = 0;
			while (++iter != endIter)
			{
				if (*iter == startBracket)
				{
					if (*(--iter) != '\\')
						++Depth;
					++iter;
				}
				else if (*iter == endBracket)
				{
					if (*(--iter) != '\\')
					{
						++iter;
						if (Depth == 0)
							break;
						else
							--Depth;
					}
					else
						++iter;
				}
	
				if (Output)
					*Output += *iter;
			}
		}
	
		// Handles construction of Character Classes.
		static ContainerType MunchCharClass(IterType& iter,
			ConstIterType& endIter,
			std::string& error,
			IndexTracker& CCSymbsToInds,
			IndexTracker& CCsToInds,
			InstructionSet& OutInstructions,
			Modifiers& Modifs,
			int MaxDepth)
		{
			if (MaxDepth <= 0)
			{
				error = "Regex Compile Error: Char class nesting surpasses the Max Nesting Depth limit.";
				return "";
			}
	
			ContainerType FullCCName;
			if (iter + 1 != endIter)
				FullCCName.insert(FullCCName.end(), iter, endIter);
			else
				FullCCName += *iter;
	
			{
				auto found = CCsToInds.Indices.find(FullCCName);
				if (found != CCsToInds.Indices.end())
					return std::to_string(found->second);
			}
	
			std::vector<RegexInstructionType> Types;
			std::vector<ContainerType> Chunks;
			ContainerType Stragglers;
			while (iter != endIter)
			{
				if (*iter == '\\') // many-escape or shorthand
				{
					if (*(++iter) == 'Q') // many-escape
					{
						while (++iter != endIter)
						{
							if (*iter == '\\')
							{
								if (*(++iter) == 'E') // end many-escape
									break;
								--iter;
							}
							else if (*iter == '[' || *iter == ']')
								Stragglers += '\\';
							Stragglers += *iter;
						}
	
						if (iter == endIter)
							break;
					}
					else // shorthand
					{
						ContainerType Short = ExpandShorthand(iter);
	
						if (Short.empty())
						{
							Stragglers += *(--iter);
							Stragglers += *(++iter);
						}
						else
						{
							if (Stragglers.empty() && Chunks.empty())
								Stragglers = Short;
							else
							{
								if (!Stragglers.empty())
								{
									Chunks.push_back(Stragglers);
									Stragglers.clear();
	
									if (Chunks.size() > 1)
										Types.push_back(RegexInstructionType::MakeUnitedCharClass);
								}
	
								Chunks.push_back(Short);
	
								Types.push_back(RegexInstructionType::MakeUnitedCharClass);
							}
						}
					}
				}
				else if (*iter == '&') // potential intersect
				{
					if (*(++iter) == '&') // even more likely it's an intersect
					{
						if (*(++iter) == '[') // character class intersection
						{
							if (Stragglers.empty() && Chunks.empty())
							{
								error = "Regex Compile Error: Malformed character class intersect.";
								return "";
							}
	
							if (!Stragglers.empty())
							{
								Chunks.push_back(Stragglers);
								Stragglers.clear();
	
								if (Chunks.size() > 1)
									Types.push_back(RegexInstructionType::MakeUnitedCharClass);
							}
	
							Chunks.push_back("");
							FindEnd(iter, endIter, '[', ']', &Chunks.back());
	
							Types.push_back(RegexInstructionType::MakeIntersectedCharClass);
						}
						else
						{
							Stragglers += *(--(--iter));
							Stragglers += *(++iter);
						}
					}
					else
						Stragglers += *(--iter);
				}
				else if (*iter == '-') // potential subtraction
				{
					if (*(++iter) == '[') // character class subtraction
					{
						if (Stragglers.empty() && Chunks.empty())
						{
							error = "Regex Compile Error: Malformed character class subtract.";
							return "";
						}
	
						if (!Stragglers.empty())
						{
							Chunks.push_back(Stragglers);
							Stragglers.clear();
	
							if (Chunks.size() > 1)
								Types.push_back(RegexInstructionType::MakeUnitedCharClass);
						}
	
						Chunks.push_back("");
						FindEnd(iter, endIter, '[', ']', &Chunks.back());
	
						Types.push_back(RegexInstructionType::MakeSubtractedCharClass);
					}
					else
						Stragglers += *(--iter);
				}
				else if (*iter == '[') // character class union
				{
					if (Stragglers.empty() && Chunks.empty())
					{
						Chunks.push_back("");
						Chunks.push_back("");
						FindEnd(iter, endIter, '[', ']', &Chunks.back());
						Types.push_back(RegexInstructionType::MakeLiteralCharClass);
					}
					else
					{
						if (!Stragglers.empty())
						{
							Chunks.push_back(Stragglers);
							Stragglers.clear();
	
							if (Chunks.size() > 1)
								Types.push_back(RegexInstructionType::MakeUnitedCharClass);
						}
	
						Chunks.push_back("");
						FindEnd(iter, endIter, '[', ']', &Chunks.back());
	
						Types.push_back(RegexInstructionType::MakeUnitedCharClass);
					}
				}
				else
					Stragglers += *iter;
	
				++iter;
			}
	
			if (!Types.empty())
			{
				ContainerType rhs, Ind, RollingName;
				if (Chunks[0].empty())
				{
					Ind =
						MunchCharClass(Chunks[1].begin(), Chunks[1].end(), error, CCSymbsToInds, CCsToInds, OutInstructions, Modifs, MaxDepth - 1);
	
					RollingName = Chunks[1];
				}
				else
				{
					ContainerType lhs =
						MunchCharClass(Chunks[0].begin(), Chunks[0].end(), error, CCSymbsToInds, CCsToInds, OutInstructions, Modifs, MaxDepth - 1);
					ContainerType rhs =
						MunchCharClass(Chunks[1].begin(), Chunks[1].end(), error, CCSymbsToInds, CCsToInds, OutInstructions, Modifs, MaxDepth - 1);
	
					OutInstructions.push_back({ Types[0],{ lhs, rhs } });
	
					RollingName = Chunks[0] + Chunks[1];
	
					CCsToInds.Indices[RollingName] = CCsToInds.NextIndex;
					Ind = std::to_string(CCsToInds.NextIndex++);
				}
	
				for (unsigned int i = 2; i < Chunks.size(); ++i)
				{
					rhs =
						MunchCharClass(Chunks[i].begin(), Chunks[i].end(), error, CCSymbsToInds, CCsToInds, OutInstructions, Modifs, MaxDepth - 1);
	
					OutInstructions.push_back({ Types[i - 1],{ Ind, rhs } });
	
					RollingName += Chunks[i];
	
					CCsToInds.Indices[RollingName] = CCsToInds.NextIndex;
					Ind = std::to_string(CCsToInds.NextIndex++);
				}
	
				return Ind;
			}
			else
			{
				if (!Stragglers.empty())
				{
					std::vector<ContainerType> RollingSymbolInds;
	
					IterType TrueIter = Stragglers.begin();
	
					if (Stragglers[0] == '^')
					{
						RollingSymbolInds.push_back("t");
						++TrueIter;
					}
					else
						RollingSymbolInds.push_back("f");
	
					RollingSymbolInds.push_back((Modifs.CaseInsensitive ? "t" : "f"));
	
					for (; TrueIter != Stragglers.end(); ++TrueIter)
					{
						if (*TrueIter == '\\')
							++TrueIter;
						RollingSymbolInds.push_back(MunchCharClassSymbol(TrueIter, Stragglers.end(), error, CCSymbsToInds, OutInstructions));
					}
	
					OutInstructions.push_back({ RegexInstructionType::MakeLiteralCharClass, RollingSymbolInds });
	
					CCsToInds.Indices[FullCCName] = CCsToInds.NextIndex;
					return std::to_string(CCsToInds.NextIndex++);
				}
				else
				{
					error = "Regex Compile Error: Empty Character Class.";
					return "";
				}
			}
		}

		// Handles numbered, named, relative, and forward references.
		static void MunchBackref(IterType& iter, ConstIterType& endIter, std::string& error, InstructionSet& OutInstructions, int& NextCapGroup)
		{
			bool(*Pred)(IterType&) = [](IterType& iter) { return isdigit(*iter) != 0; };
			int sign = 0; // -1 relative, +1 forward, 0 numbered/named
			if (!isdigit(*iter)) // if it isn't immediately a digit it's a relative, forward, or named reference.
			{
				if (*iter == '-') // relative
				{
					++iter;
					sign = -1;
				}
				else if (*iter == '+') // forward
				{
					++iter;
					sign = 1;
				}
				else if (isalpha(*iter)) // expecting name
					Pred = [](IterType& iter) { return true; };
				else
				{
					error = "Regex Compile Error: Malformed backreference.";
					return;
				}
	
				if (sign && !isdigit(*iter))
				{
					error = "Regex Compile Error: Malformed backreference.";
					return;
				}
			}
	
			ContainerType Munch;
			while (iter != endIter && Pred(iter))
			{
				Munch += *iter;
				++iter;
			}
	
			if (isdigit(Munch[0])) // relative, forward, or numbered
			{
				if (sign) // relative or forward
				{
					if (sign > 0) // forward
					{
						ContainerType TrueCap = std::to_string(std::stoi(Munch) + NextCapGroup);
						OutInstructions.push_back({ RegexInstructionType::Backref_Numbered, { TrueCap } });
					}
					else // relative
					{
						int TrueCap = NextCapGroup + 1 + (-std::stoi(Munch));
	
						if (TrueCap > 0)
							OutInstructions.push_back({ RegexInstructionType::Backref_Numbered, { std::to_string(TrueCap) } });
						else
						{
							error = "Regex Compile Error: Capture Group \'" + std::to_string(TrueCap) + "\' does not and cannot exist. "
								"The relative backreference given is malformed.";
							return;
						}
					}
				}
				else // numbered
				{
					if (std::stoi(Munch) > 0)
						OutInstructions.push_back({ RegexInstructionType::Backref_Numbered, { Munch } });
					else
					{
						error = "Regex Compile Error: Capture Group \'" + Munch + "\' does not and cannot exist. "
							"The relative backreference given is malformed.";
						return;
					}
				}
			}
			else // named
				OutInstructions.push_back({ RegexInstructionType::Backref_Named, { Munch } });
		}

		// Handles numbered, named, relative, and forward subroutines, as well as recursion
		static void MunchSubroutine(IterType& iter,
			ConstIterType& endIter,
			std::string& error,
			InstructionSet& OutInstructions,
			int& NextCapGroup,
			int MaxDepth,
			Modifiers& Modifs)
		{
			if (*iter == 'R' || *iter == '0') // recursion
			{
				OutInstructions.push_back({ RegexInstructionType::Recursion, { std::to_string(MaxDepth), (Modifs.LazyGroups ? "t" : "f") } });
				++iter;
				return;
			}
	
			bool(*Pred)(IterType&) = [](IterType& iter) { return isdigit(*iter) != 0; };
			int sign = 0; // -1 relative, +1 forward, 0 numbered/named
			if (!isdigit(*iter)) // if it isn't immediately a digit then it's a relative, forward, or named subroutine
			{
				if (*iter == '-') // relative
				{
					++iter;
					sign = -1;
				}
				else if (*iter == '+') // forward
				{
					++iter;
					sign = 1;
				}
				else if (isalpha(*iter)) // expecting name
					Pred = [](IterType& iter) { return true; };
				else
				{
					error = "Regex Compile Error: Malformed subroutine.";
					return;
				}
	
				if (sign && !isdigit(*iter))
				{
					error = "Regex Compile Error: Malformed subroutine.";
					return;
				}
			}
	
			ContainerType Munch;
			while (Pred(iter) && iter != endIter)
			{
				Munch += *iter;
				++iter;
			}
	
			if (isdigit(Munch[0])) // relative, forward, or numbered
			{
				if (sign) // relative or forward
				{
					if (sign > 0) // forward
					{
						ContainerType TrueCap = std::to_string(std::stoi(Munch) + NextCapGroup);
						OutInstructions.push_back({ RegexInstructionType::Subroutine_Numbered,{ Munch, TrueCap } });
					}
					else // relative
					{
						int TrueCap = NextCapGroup + 1 + (-std::stoi(Munch));
	
						if (TrueCap > 0)
							OutInstructions.push_back({ RegexInstructionType::Subroutine_Numbered,{ Munch, std::to_string(TrueCap) } });
						else
						{
							error = "Regex Compile Error: Capture Group \'" + std::to_string(TrueCap) + "\' does not and cannot exist. "
								"The relative subroutine given is malformed.";
							return;
						}
					}
				}
				else // numbered
				{
					if (std::stoi(Munch) > 0)
						OutInstructions.push_back({ RegexInstructionType::Subroutine_Numbered,{ Munch, std::to_string(MaxDepth) } });
					else
					{
						error = "Regex Compile Error: Capture Group \'" + Munch + "\' does not and cannot exist. "
							"The relative subroutine given is malformed.";
						return;
					}
				}
			}
			else // named
				OutInstructions.push_back({ RegexInstructionType::Subroutine_Named,{ Munch, std::to_string(MaxDepth) } });
		}

		// Handles special characters/escapes
		static void MunchEscaped(IterType& iter,
			ConstIterType& endIter,
			std::string& error,
			IndexTracker& CCSymbsToInds,
			IndexTracker& CCsToInds,
			InstructionSet& OutInstructions,
			int& NextCapGroup,
			Modifiers& Modifs,
			int MaxDepth)
		{
			switch (*(++iter))
			{
			case 'A': // Exclusive start of string
				{
					ContainerType LineChars = "\r\n\v\f]";
					ContainerType Ind =
						MunchCharClass(LineChars.begin(), LineChars.end() - 1, error, CCSymbsToInds, CCsToInds, OutInstructions, Modifs, MaxDepth - 1);
	
					OutInstructions.push_back({ RegexInstructionType::StartCheck, { "t", "f", Ind } });
				}
				break;
	
			case 'z': // Exclusive end of string
				{
					ContainerType LineChars = "\r\n\v\f";
					ContainerType Ind =
						MunchCharClass(LineChars.begin(), LineChars.end(), error, CCSymbsToInds, CCsToInds, OutInstructions, Modifs, MaxDepth - 1);
	
					OutInstructions.push_back({ RegexInstructionType::EndCheck,{ "t", "f", Ind } });
				}
				break;
	
			case 'Z': // end of string before final line break
				{
					ContainerType LineChars = "\r\n\v\f";
					ContainerType Ind =
						MunchCharClass(LineChars.begin(), LineChars.end(), error, CCSymbsToInds, CCsToInds, OutInstructions, Modifs, MaxDepth - 1);
	
					OutInstructions.push_back({ RegexInstructionType::EndCheck,{ "t", "t", Ind } });
				}
				break;
	
			case 'b': // word boundary
				{
					ContainerType WordChars = "A-Za-z0-9_";
					ContainerType Ind =
						MunchCharClass(WordChars.begin(), WordChars.end(), error, CCSymbsToInds, CCsToInds, OutInstructions, Modifs, MaxDepth - 1);
	
					OutInstructions.push_back({ RegexInstructionType::WordBoundary,{ "f", Ind } });
				}
				break;
	
			case 'B': // not-word boundary
				{
					ContainerType WordChars = "A-Za-z0-9_";
					ContainerType Ind =
						MunchCharClass(WordChars.begin(), WordChars.end(), error, CCSymbsToInds, CCsToInds, OutInstructions, Modifs, MaxDepth - 1);
	
					OutInstructions.push_back({ RegexInstructionType::WordBoundary,{ "t", Ind } });
				}
				break;
	
			case 'G': // start of string or end of last match
				{
					ContainerType LineChars = "\r\n\v\f";
					ContainerType Ind =
						MunchCharClass(LineChars.begin(), LineChars.end(), error, CCSymbsToInds, CCsToInds, OutInstructions, Modifs, MaxDepth - 1);
	
					OutInstructions.push_back({ RegexInstructionType::StartCheck,{ "t", "t", Ind } });
				}
				break;
	
			case 'Q': // begin many-escape
				{
					int Lits = 0;
					while (++iter != endIter)
					{
						if (*iter == '\\')
						{
							if (*(++iter) == 'E') // end many-escape
								break;
							--iter;
						}
						MunchLiteral(iter, endIter, error, CCSymbsToInds, CCsToInds, OutInstructions, Modifs);
						if (++Lits > 1)
							OutInstructions.push_back({ RegexInstructionType::Concat, {} });
					}
	
					if (iter == endIter)
					{
						--iter;
						return;
					}
				}
				break;
	
			case 'k': // named or numbered backref
				{
					if (*(++iter) == '<' || *iter == '\'' || *iter == '{')
					{
						char EndGlyph = (*iter == '<' ? '>' : (*iter == '\'' ? '\'' : '}'));
						IterType Copy = iter;
						
						while (++Copy != endIter && *Copy != EndGlyph);
	
						if (Copy != endIter)
							MunchBackref(++iter, Copy, error, OutInstructions, NextCapGroup);
						else
						{
							error = "Regex Compile Error: Couldn't find closing bracket to backreference. (Was expecting: ";
							error = error + EndGlyph + " )";
							return;
						}
					}
					else
						MunchLiteral(--(--iter), endIter, error, CCSymbsToInds, CCsToInds, OutInstructions, Modifs);
				}
				break;
	
			case 'g': // named or numbered subroutine
				{
					if (*(++iter) == '<' || *iter == '\'' || *iter == '{')
					{
						char EndGlyph = (*iter == '<' ? '>' : (*iter == '\'' ? '\'' : '}'));
						IterType Copy = iter;
	
						while (++Copy != endIter && *Copy != EndGlyph);
	
						if (Copy != endIter)
							MunchSubroutine(++iter, Copy, error, OutInstructions, NextCapGroup, MaxDepth, Modifs);
						else
						{
							error = "Regex Compile Error: Couldn't find closing bracket to subroutine. (Was expecting: ";
							error = error + EndGlyph + " )";
							return;
						}
					}
					else
						MunchLiteral(--(--iter), endIter, error, CCSymbsToInds, CCsToInds, OutInstructions, Modifs);
				}
				break;
	
			default: // most likely a literal, shorthand, or numbered backref
				{
					if (isdigit(*iter)) // backref ("\###...")
					{
						MunchBackref(iter, endIter, error, OutInstructions, NextCapGroup);
						--iter;
					}
					else // shorthand or literal
					{
						ContainerType Short = ExpandShorthand(iter);
	
						if (Short.empty())
							MunchLiteral(iter, endIter, error, CCSymbsToInds, CCsToInds, OutInstructions, Modifs);
						else
						{
							ContainerType Ind;
							auto found = CCsToInds.Indices.find(Short);
							if (found != CCsToInds.Indices.end())
								Ind = std::to_string(found->second);
							else
								Ind = MunchCharClass(Short.begin(), Short.end(), error, CCSymbsToInds, CCsToInds, OutInstructions, Modifs, MaxDepth);
	
							OutInstructions.push_back({ RegexInstructionType::Literal,{ Ind } });
						}
					}
				}
				break;
			}
		}

		// Helper function for reversing a group. Specifically used during construction of lookbehinds.
		static ContainerType ReverseGroup(IterType& iter, ConstIterType& endIter)
		{
			ContainerType Munch;
			while (++iter != endIter)
			{
				if (*iter == '\\')
				{
					Munch.insert(Munch.begin(), *iter);
					Munch.insert(Munch.begin() + 1, *(++iter));
					continue;
				}
				if (*iter == '[')
				{
					if (*(--iter) != '\\')
					{
						ContainerType SubChunk;
						IterType Copy = ++iter;
						SubChunk += *Copy;
						while (++Copy != endIter)
						{
							if (*Copy == ']')
							{
								if (*(--Copy) != '\\')
									break;
								++Copy;
							}
							SubChunk += *Copy;
						}
						SubChunk += *(++Copy);
						Munch.insert(Munch.begin(), SubChunk.begin(), SubChunk.end());
						iter = Copy;
						continue;
					}
					++iter;
				}
				if (*iter == '(')
				{
					if (*(--iter) != '\\')
					{
						ContainerType SubChunk;
						IterType Copy = ++iter;
						SubChunk += *Copy;
						while (++Copy != endIter)
						{
							if (*Copy == ')')
							{
								if (*(--Copy) != '\\')
									break;
								++Copy;
							}
							SubChunk += *Copy;
						}
						SubChunk += *(++Copy);
						Munch.insert(Munch.begin(), SubChunk.begin(), SubChunk.end());
						iter = Copy;
						continue;
					}
					++iter;
				}
				Munch.insert(Munch.begin(), *iter);
			}
	
			return Munch;
		}

		// Handles special and regular groups.
		static void MunchGroup(IterType& iter,
			ConstIterType endIter,
			std::string& error,
			IndexTracker& CCSymbsToInds,
			IndexTracker& CCsToInds,
			InstructionSet& OutInstructions,
			int& NextCapGroup,
			Modifiers& Modifs,
			int MaxDepth)
		{
			if (MaxDepth <= 0)
			{
				error = "Regex Compile Error: Group nesting surpasses the Max Nesting Depth limit.";
				return;
			}
	
			if (*iter == '?') // special group
			{
				switch (*(++iter))
				{
				case '|': // branch reset group ("(?|(a)|(b)|(c))")
					{
						int AltCount = 0;
						std::vector<ContainerType> Branches;
						Branches.push_back("");
						while (++iter != endIter)
						{
							if (*iter == '|')
							{
								++AltCount;
								Branches.push_back("");
							}
							else
								Branches[Branches.size() - 1] += *iter;
						}
	
						std::vector<std::pair<unsigned int, unsigned int>> Caps;
						for (ContainerType& currBranch : Branches)
						{
							unsigned int CurrCap = 0;
							InstructionSet Chunks =
								TranslateInternal(currBranch, currBranch.begin(), error, CCSymbsToInds, CCsToInds, Modifs, MaxDepth - 1);
	
							if (Chunks.empty())
								--AltCount;
							else
							{
								for (unsigned int i = 0; i < Chunks.size(); ++i)
								{
									if (Chunks[i].InstructionType == RegexInstructionType::MakeCapture)
									{
										if (Caps.empty() || CurrCap >= Caps.size())
										{
											OutInstructions.push_back({ RegexInstructionType::MakeCapture, { "f" } });
											Caps.push_back({ ++NextCapGroup, OutInstructions.size() - 1 });
										}
										else
										{
											RegexInstruction<char> LastInst = OutInstructions[Caps[CurrCap].second];
											std::vector<ContainerType>& NamesToCopy = Chunks[i].InstructionData;
	
											LastInst.InstructionData.insert(LastInst.InstructionData.end(), NamesToCopy.begin() + 1, NamesToCopy.end());
										}
										Chunks.erase(Chunks.begin() + i);
										--i;
										++CurrCap;
									}
									else if (Chunks[i].InstructionType == RegexInstructionType::MakeCaptureCollection)
									{
										if (Caps.empty() || CurrCap >= Caps.size())
										{
											OutInstructions.push_back({ RegexInstructionType::MakeCaptureCollection, { "f" } });
											Caps.push_back({ ++NextCapGroup, OutInstructions.size() - 1 });
										}
										else
										{
											RegexInstruction<char> LastInst = OutInstructions[Caps[CurrCap].second];
											std::vector<ContainerType>& NamesToCopy = Chunks[i].InstructionData;
	
											LastInst.InstructionType = RegexInstructionType::MakeCaptureCollection;
											LastInst.InstructionData.insert(LastInst.InstructionData.end(), NamesToCopy.begin() + 1, NamesToCopy.end());
										}
										Chunks.erase(Chunks.begin() + i);
										--i;
										++CurrCap;
									}
								}
								OutInstructions.insert(OutInstructions.end(), Chunks.begin(), Chunks.end());
							}
						}
	
						while (AltCount-- > 0)
							OutInstructions.push_back({ RegexInstructionType::Alternate, {} });
	
						OutInstructions.push_back({ RegexInstructionType::NonCaptureGroup, { (Modifs.LazyGroups ? "t" : "f") } });
					}
					break;
	
				case '=': // positive lookahead ("(?=regex)")
					{
						ContainerType Munch;
						while (++iter != endIter)
							Munch += *iter;
	
						InstructionSet NewInsts =
							TranslateInternal(Munch, Munch.begin(), error, CCSymbsToInds, CCsToInds, Modifs, MaxDepth - 1);
						OutInstructions.insert(OutInstructions.end(), NewInsts.begin(), NewInsts.end());
						OutInstructions.push_back({ RegexInstructionType::LookAhead, { "f", (Modifs.LazyGroups ? "t" : "f") } });
					}
					break;
	
				case '<': // potential lookbehind
					switch (*(++iter))
					{
					case '=': // positive lookbehind ("(?<=regex)")
						{
							ContainerType Munch = ReverseGroup(iter, endIter);
	
							InstructionSet NewInsts =
								TranslateInternal(Munch, Munch.begin(), error, CCSymbsToInds, CCsToInds, Modifs, MaxDepth - 1);
							OutInstructions.insert(OutInstructions.end(), NewInsts.begin(), NewInsts.end());
							OutInstructions.push_back({ RegexInstructionType::LookBehind,{ "f", (Modifs.LazyGroups ? "t" : "f") } });
						}
						break;
	
					case '!': // negative lookbehind ("(?<!regex)")
						{
							ContainerType Munch = ReverseGroup(iter, endIter);
	
							InstructionSet NewInsts =
								TranslateInternal(Munch, Munch.begin(), error, CCSymbsToInds, CCsToInds, Modifs, MaxDepth - 1);
							OutInstructions.insert(OutInstructions.end(), NewInsts.begin(), NewInsts.end());
							OutInstructions.push_back({ RegexInstructionType::LookBehind,{ "t", (Modifs.LazyGroups ? "t" : "f") } });
						}
						break;
	
					default: // named capture group ("(?<name>regex)") or subroutine call ("(?<name>)")
						{
							ContainerType Name;
							while (iter != endIter)
							{
								if (*iter == '>')
									break;
								Name += *iter;
								++iter;
							}
	
							if (iter == endIter)
							{
								error = "Regex Compile Error: Couldn't find closing bracket for Named Capture Group or Subroutine.";
								return;
							}
	
							if (iter + 1 == endIter) // named subroutine
							{
								OutInstructions.push_back({ RegexInstructionType::Subroutine_Named, { Name, std::to_string(MaxDepth) } });
								++iter;
							}
							else // named capture group
							{
								++NextCapGroup;
								OutInstructions.push_back({ RegexInstructionType::MakeCapture, { "f", Name } });
	
								ContainerType Regex;
								while (++iter != endIter)
									Regex += *iter;
	
								InstructionSet NewInsts =
									TranslateInternal(Regex, Regex.begin(), error, CCSymbsToInds, CCsToInds, Modifs, MaxDepth - 1);
								OutInstructions.insert(OutInstructions.end(), NewInsts.begin(), NewInsts.end());
	
								OutInstructions.push_back({ RegexInstructionType::CaptureGroup_Named, { Name, (Modifs.LazyGroups ? "t" : "f") } });
							}
						}
						break;
					}
					break;
	
				case ':': // non-capturing group ("(?:regex)")
					{
						ContainerType Regex;
						while (++iter != endIter)
							Regex += *iter;
	
						InstructionSet NewInsts =
							TranslateInternal(Regex, Regex.begin(), error, CCSymbsToInds, CCsToInds, Modifs, MaxDepth - 1);
						OutInstructions.insert(OutInstructions.end(), NewInsts.begin(), NewInsts.end());
	
						OutInstructions.push_back({ RegexInstructionType::NonCaptureGroup, { (Modifs.LazyGroups ? "t" : "f") } });
					}
					break;
	
				case '(': // potential conditional
					switch (*(++iter))
					{
					case '<':
					case '\'':
					case '{': // named backref conditional ("(?(<name>)(a)|(b))", "(?('name')(a)|(b))", "(?({name})(a)|(b))")
						{
							char endBracket = (*iter == '<' ? '>' : (*iter == '\'' ? '\'' : '}'));
							ContainerType Name;
							while (++iter != endIter)
							{
								if (*iter == endBracket)
									break;
								Name += *iter;
							}
	
							if (iter == endIter)
							{
								error = "Regex Compile Error: Couldn't find closing bracket for Named Capture Collection Group.";
								return;
							}
	
							if (*(++iter) != ')')
							{
								error = "Regex Compile Error: Malformed Conditional; Missing closing parenthesis in conditional statement.";
								return;
							}
	
							MunchBackref(Name.begin(), Name.end(), error, OutInstructions, NextCapGroup);
	
							ContainerType prevPiece;
							ContainerType currPiece;
							int Depth = 0;
							while (++iter != endIter)
							{
								if (*iter == '|' && Depth == 0)
								{
									if (!prevPiece.empty())
									{
										error = "Regex Compile Error: too many branches in conditional.";
										return;
									}
	
									prevPiece = currPiece;
									currPiece.clear();
								}
								else if (*iter == '(')
									++Depth;
								else if (*iter == ')')
									--Depth;
								else
									currPiece += *iter;
							}
	
							if (prevPiece.empty())
							{
								prevPiece = currPiece;
								currPiece.clear();
							}
	
							InstructionSet ThenSet =
								TranslateInternal(prevPiece, prevPiece.begin(), error, CCSymbsToInds, CCsToInds, Modifs, MaxDepth - 1);
							OutInstructions.insert(OutInstructions.end(), ThenSet.begin(), ThenSet.end());
							OutInstructions.push_back({ RegexInstructionType::NonCaptureGroup, { (Modifs.LazyGroups ? "t" : "f") } });
	
							if (!currPiece.empty())
							{
								InstructionSet ElseSet =
									TranslateInternal(currPiece, currPiece.begin(), error, CCSymbsToInds, CCsToInds, Modifs, MaxDepth - 1);
	
								OutInstructions.insert(OutInstructions.end(), ElseSet.begin(), ElseSet.end());
								OutInstructions.push_back({ RegexInstructionType::NonCaptureGroup, { (Modifs.LazyGroups ? "t" : "f") } });
								OutInstructions.push_back({ RegexInstructionType::Conditional, { "2", (Modifs.LazyGroups ? "t" : "f") } });
							}
							else
								OutInstructions.push_back({ RegexInstructionType::Conditional, { "1", (Modifs.LazyGroups ? "t" : "f") } });
						}
						break;
	
					case '-': // relative backref conditional ("(?(-N)(a)|(b))")
					case '+': // forward ref conditional ("(?(+N)(a)|(b))")
						{
							IterType CondEdge = iter;
							while (++CondEdge != endIter && *CondEdge != ')');
	
							if (CondEdge == endIter)
							{
								error = "Regex Compile Error: Malformed Conditional; Missing closing parenthesis in conditional statement.";
								return;
							}
	
							MunchBackref(iter, CondEdge, error, OutInstructions, NextCapGroup);
	
							ContainerType prevPiece;
							ContainerType currPiece;
							int Depth = 0;
							while (++iter != endIter)
							{
								if (*iter == '|' && Depth == 0)
								{
									if (!prevPiece.empty())
									{
										error = "Regex Compile Error: too many branches in conditional.";
										return;
									}
	
									prevPiece = currPiece;
									currPiece.clear();
								}
								else if (*iter == '(')
									++Depth;
								else if (*iter == ')')
									--Depth;
								else
									currPiece += *iter;
							}
	
							if (prevPiece.empty())
							{
								prevPiece = currPiece;
								currPiece.clear();
							}
	
							InstructionSet ThenSet =
								TranslateInternal(prevPiece, prevPiece.begin(), error, CCSymbsToInds, CCsToInds, Modifs, MaxDepth - 1);
							OutInstructions.insert(OutInstructions.end(), ThenSet.begin(), ThenSet.end());
							OutInstructions.push_back({ RegexInstructionType::NonCaptureGroup, { (Modifs.LazyGroups ? "t" : "f") } });
	
							if (!currPiece.empty())
							{
								InstructionSet ElseSet =
									TranslateInternal(currPiece, currPiece.begin(), error, CCSymbsToInds, CCsToInds, Modifs, MaxDepth - 1);
	
								OutInstructions.insert(OutInstructions.end(), ElseSet.begin(), ElseSet.end());
								OutInstructions.push_back({ RegexInstructionType::NonCaptureGroup, { (Modifs.LazyGroups ? "t" : "f") } });
								OutInstructions.push_back({ RegexInstructionType::Conditional,{ "2", (Modifs.LazyGroups ? "t" : "f") } });
							}
							else
								OutInstructions.push_back({ RegexInstructionType::Conditional,{ "1", (Modifs.LazyGroups ? "t" : "f") } });
						}
						break;
	
					default: // either a normal conditional, subroutine definition, or numbered backref conditional
	
						if (isdigit(*iter)) // numbered backref conditional ("(?(N)(a)|(b))")
						{
							IterType CondEdge = iter;
							while (++CondEdge != endIter && *CondEdge != ')');
	
							if (CondEdge == endIter)
							{
								error = "Regex Compile Error: Malformed Conditional; Missing closing parenthesis in conditional statement.";
								return;
							}
	
							MunchBackref(iter, CondEdge, error, OutInstructions, NextCapGroup);
	
							ContainerType prevPiece;
							ContainerType currPiece;
							int Depth = 0;
							while (++iter != endIter)
							{
								if (*iter == '|' && Depth == 0)
								{
									if (!prevPiece.empty())
									{
										error = "Regex Compile Error: too many branches in conditional.";
										return;
									}
	
									prevPiece = currPiece;
									currPiece.clear();
								}
								else if (*iter == '(')
									++Depth;
								else if (*iter == ')')
									--Depth;
								else
									currPiece += *iter;
							}
	
							if (prevPiece.empty())
							{
								prevPiece = currPiece;
								currPiece.clear();
							}
	
							InstructionSet ThenSet =
								TranslateInternal(prevPiece, prevPiece.begin(), error, CCSymbsToInds, CCsToInds, Modifs, MaxDepth - 1);
							OutInstructions.insert(OutInstructions.end(), ThenSet.begin(), ThenSet.end());
							OutInstructions.push_back({ RegexInstructionType::NonCaptureGroup, { (Modifs.LazyGroups ? "t" : "f") } });
	
							if (!currPiece.empty())
							{
								InstructionSet ElseSet =
									TranslateInternal(currPiece, currPiece.begin(), error, CCSymbsToInds, CCsToInds, Modifs, MaxDepth - 1);
	
								OutInstructions.insert(OutInstructions.end(), ElseSet.begin(), ElseSet.end());
								OutInstructions.push_back({ RegexInstructionType::NonCaptureGroup, { (Modifs.LazyGroups ? "t" : "f") } });
								OutInstructions.push_back({ RegexInstructionType::Conditional,{ "2", (Modifs.LazyGroups ? "t" : "f") } });
							}
							else
								OutInstructions.push_back({ RegexInstructionType::Conditional,{ "1", (Modifs.LazyGroups ? "t" : "f") } });
						}
						else // Subroutine Definition or Regular Conditional
						{
							ContainerType Regex;
							Regex += *iter;
							FindEnd(iter, endIter, '(', ')', &Regex);
	
							if (Regex == "DEFINE") // Subroutine Definition ("(?(DEFINE)regex)")
							{
								if (*(++iter) == '(' && *(++iter) == '?' && (*(++iter) == '<' || *iter == '\'' || *iter == '{'))
								{
									char endBracket = (*iter == '<' ? '>' : (*iter == '\'' ? '\'' : '}'));
									ContainerType Name;
									while (++iter != endIter)
									{
										if (*iter == endBracket)
											break;
										Name += *iter;
									}
	
									if (iter == endIter)
									{
										error = "Regex Compile Error: Failed to find closing bracket for subroutine definition name.";
										return;
									}
	
									Regex.clear();
									FindEnd(iter, endIter, '(', ')', &Regex);
	
									if (iter == endIter)
									{
										error = "Regex Compile Error: Failed to find closing bracket for subroutine definition internal regex.";
										return;
									}
	
									++iter;
	
									InstructionSet NewInsts =
										TranslateInternal(Regex, Regex.begin(), error, CCSymbsToInds, CCsToInds, Modifs, MaxDepth - 1);
									OutInstructions.insert(OutInstructions.end(), NewInsts.begin(), NewInsts.end());
	
									OutInstructions.push_back({ RegexInstructionType::NonCaptureGroup, { (Modifs.LazyGroups ? "t" : "f") } });
									OutInstructions.push_back({ RegexInstructionType::DefineAsSubroutine, { Name } });
								}
								else
								{
									error = "Regex Compile Error: Malformed subroutine definition.";
									return;
								}
							}
							else // Regular Conditional ("(?(regex)(a)|(b))")
							{
								if (!(Regex[0] == '(' || Regex[0] == '?'))
									Regex = "?:" + Regex;
	
								MunchGroup(Regex.begin(), Regex.end(), error, CCSymbsToInds, CCsToInds, OutInstructions, NextCapGroup, Modifs, MaxDepth - 1);
	
								ContainerType prevPiece;
								ContainerType currPiece;
								int Depth = 0;
								while (++iter != endIter)
								{
									if (*iter == '|' && Depth == 0)
									{
										if (!prevPiece.empty())
										{
											error = "Regex Compile Error: too many branches in conditional.";
											return;
										}
	
										prevPiece = currPiece;
										currPiece.clear();
									}
									else if (*iter == '(')
										++Depth;
									else if (*iter == ')')
										--Depth;
									else
										currPiece += *iter;
								}
	
								if (prevPiece.empty())
								{
									prevPiece = currPiece;
									currPiece.clear();
								}
	
								InstructionSet ThenSet =
									TranslateInternal(prevPiece, prevPiece.begin(), error, CCSymbsToInds, CCsToInds, Modifs, MaxDepth - 1);
								OutInstructions.insert(OutInstructions.end(), ThenSet.begin(), ThenSet.end());
								OutInstructions.push_back({ RegexInstructionType::NonCaptureGroup, { (Modifs.LazyGroups ? "t" : "f") } });
	
								if (!currPiece.empty())
								{
									InstructionSet ElseSet =
										TranslateInternal(currPiece, currPiece.begin(), error, CCSymbsToInds, CCsToInds, Modifs, MaxDepth - 1);
	
									OutInstructions.insert(OutInstructions.end(), ElseSet.begin(), ElseSet.end());
									OutInstructions.push_back({ RegexInstructionType::NonCaptureGroup, { (Modifs.LazyGroups ? "t" : "f") } });
									OutInstructions.push_back({ RegexInstructionType::Conditional,{ "2", (Modifs.LazyGroups ? "t" : "f") } });
								}
								else
									OutInstructions.push_back({ RegexInstructionType::Conditional,{ "1", (Modifs.LazyGroups ? "t" : "f") } });
							}
						}
						break;
					}
					break;
	
				case '\'': // named capture group ("(?'name'regex)") or subroutine call ("(?'name')")
					{
						++iter;
						ContainerType Name;
						while (iter != endIter)
						{
							if (*iter == '\'')
								break;
							Name += *iter;
							++iter;
						}
	
						if (iter == endIter)
						{
							error = "Regex Compile Error: Couldn't find closing bracket for Named Capture Group or Subroutine.";
							return;
						}
	
						if (iter + 1 == endIter) // named subroutine
						{
							OutInstructions.push_back({ RegexInstructionType::Subroutine_Named, { Name, std::to_string(MaxDepth) } });
							++iter;
						}
						else // named capture group
						{
							++NextCapGroup;
							OutInstructions.push_back({ RegexInstructionType::MakeCapture, { "f", Name } });
	
							ContainerType Regex;
							while (++iter != endIter)
								Regex += *iter;
	
							InstructionSet NewInsts =
								TranslateInternal(Regex, Regex.begin(), error, CCSymbsToInds, CCsToInds, Modifs, MaxDepth - 1);
							OutInstructions.insert(OutInstructions.end(), NewInsts.begin(), NewInsts.end());
	
							OutInstructions.push_back({ RegexInstructionType::CaptureGroup_Named,{ Name, (Modifs.LazyGroups ? "t" : "f") } });
						}
					}
					break;
	
				case '!': // negative lookahead ("(?!regex)")
					{
						ContainerType Munch;
						while (++iter != endIter)
							Munch += *iter;
	
						InstructionSet NewInsts =
							TranslateInternal(Munch, Munch.begin(), error, CCSymbsToInds, CCsToInds, Modifs, MaxDepth - 1);
						OutInstructions.insert(OutInstructions.end(), NewInsts.begin(), NewInsts.end());
						OutInstructions.push_back({ RegexInstructionType::LookAhead,{ "t", (Modifs.LazyGroups ? "t" : "f") } });
					}
					break;
	
				case '@': // Capture collection ("(?@regex)", "(?@<name>regex)", "(?@'name'regex")
					if (*(++iter) == '<' || *iter == '\'') // Named
					{
						char endBracket = (*iter == '<' ? '>' : '\'');
						ContainerType Name;
						while (++iter != endIter)
						{
							if (*iter == endBracket)
								break;
							Name += *iter;
						}
	
						if (iter == endIter)
						{
							error = "Regex Compile Error: Couldn't find closing bracket for Named Capture Collection Group.";
							return;
						}
	
	
						++NextCapGroup;
						OutInstructions.push_back({ RegexInstructionType::MakeCaptureCollection, { "f", Name } });
	
						ContainerType Regex;
						while (++iter != endIter)
							Regex += *iter;
	
						InstructionSet NewInsts =
							TranslateInternal(Regex, Regex.begin(), error, CCSymbsToInds, CCsToInds, Modifs, MaxDepth - 1);
						OutInstructions.insert(OutInstructions.end(), NewInsts.begin(), NewInsts.end());
	
						OutInstructions.push_back({ RegexInstructionType::CaptureGroup_Named,{ Name, (Modifs.LazyGroups ? "t" : "f") } });
					}
					else // numbered
					{
						if (!Modifs.NoAutoCap)
							OutInstructions.push_back({ RegexInstructionType::MakeCaptureCollection, { "f" } });
	
						ContainerType Regex;
						while (iter != endIter)
						{
							Regex += *iter;
							++iter;
						}
	
						InstructionSet NewInsts =
							TranslateInternal(Regex, Regex.begin(), error, CCSymbsToInds, CCsToInds, Modifs, MaxDepth - 1);
						OutInstructions.insert(OutInstructions.end(), NewInsts.begin(), NewInsts.end());
	
						if (!Modifs.NoAutoCap)
							OutInstructions.push_back({ RegexInstructionType::CaptureGroup_Numbered, { std::to_string(++NextCapGroup), (Modifs.LazyGroups ? "t" : "f") } });
						else
							OutInstructions.push_back({ RegexInstructionType::NonCaptureGroup, { (Modifs.LazyGroups ? "t" : "f") } });
					}
					break;
	
				case '{': // Code Hook ("(?{funcname})")
					{
						ContainerType FuncName;
						while (++iter != endIter)
						{
							if (*iter == '}')
								break;
							FuncName += *iter;
						}
	
						if (iter == endIter)
						{
							error = "Regex Compile Error: Couldn't find closing bracket for Code Hook.";
							return;
						}
	
						OutInstructions.push_back({ RegexInstructionType::CodeHook, { FuncName } });
						++iter;
					}
					break;
	
				case '$': // Manual Capture ("(?$)", "(?$<name>)", "(?$'name')"
					{
						RegexInstructionType CapType;
						if (*(++iter) == '@')
							CapType = RegexInstructionType::MakeCaptureCollection;
						else
						{
							--iter;
							CapType = RegexInstructionType::MakeCapture;
						}
	
						if (*(++iter) == '<' || *iter == '\'') // Named
						{
							char endBracket = (*iter == '<' ? '>' : '\'');
							ContainerType Name;
							while (++iter != endIter)
							{
								if (*iter == endBracket)
									break;
								Name += *iter;
							}
	
							if (iter == endIter)
							{
								error = "Regex Compile Error: Couldn't find closing bracket for Named Manual Capture.";
								return;
							}
	
	
							++NextCapGroup;
							OutInstructions.push_back({ CapType, { "t", Name } });
							++iter;
						}
						else // numbered
						{
							++NextCapGroup;
							OutInstructions.push_back({ CapType, { "t" } });
						}
					}
					break;
	
				default: // expecting a (regular/relative/forward) subroutine call or recursion, or inline modifiers
					if ((isalpha(*iter) && *iter != 'R') || *iter == '^') // individual or multiple modifiers, or a modifier non-cap
					{
						bool Out = false;
						Modifiers Old = Modifs;
						IterType Further = iter;
						while (!Out && Further != endIter)
						{
							switch (*Further)
							{
							case 'i': // case-insensitive
								Modifs.CaseInsensitive = true;
								break;
							case 'c': // case sensitive
								Modifs.CaseInsensitive = false;
								break;
	
							case 's': // single-line
								Modifs.SingleLine = true;
								break;
							case 'm': // multiline
								Modifs.SingleLine = false;
								break;
	
							case 'n': // no auto-cap
								Modifs.NoAutoCap = true;
								break;
	
							case 'd': // unix lines
								Modifs.UnixLines = true;
								break;
	
							case 'l': // lazy groups
								Modifs.LazyGroups = true;
								break;
	
							case 'a': // dot-all
								Modifs.DotAll = true;
								break;
	
							case '^': // remove all modifs
								Modifs.CaseInsensitive
									= Modifs.SingleLine
									= Modifs.NoAutoCap
									= Modifs.UnixLines
									= Modifs.LazyGroups
									= Modifs.DotAll = false;
								break;
	
							default:
								Out = true;
								break;
							}
	
							if (!Out)
								++Further;
						}
	
						if (Out && Further != iter && *Further == ':') // modifier noncap
						{
							iter = Further;
							ContainerType Regex;
							while (++iter != endIter)
								Regex += *iter;
	
							InstructionSet NewInsts =
								TranslateInternal(Regex, Regex.begin(), error, CCSymbsToInds, CCsToInds, Modifs, MaxDepth - 1);
							OutInstructions.insert(OutInstructions.end(), NewInsts.begin(), NewInsts.end());
	
							OutInstructions.push_back({ RegexInstructionType::NonCaptureGroup,{ (Modifs.LazyGroups ? "t" : "f") } });
	
							Modifs = Old;
						}
						else if (Out) // error
						{
							error = "Regex Compile Error: \'";
							error += *iter;
							error += "\' is not a valid modifier.";
							return;
						}
						else
							iter = Further;
					}
					else if (*iter == '-') // individual or multiple removed modifiers, a modifier noncap, or a relative subroutine
					{
						if (isdigit(*(++iter))) // relative subroutine
							MunchSubroutine(iter, endIter, error, OutInstructions, NextCapGroup, MaxDepth, Modifs);
						else // indiv or mult modifiers, or modifier noncap
						{
							bool Out = false;
							IterType Further = iter;
							Modifiers Old = Modifs;
							while (!Out && Further != endIter)
							{
								switch (*Further)
								{
								case 'i': // case-insensitive
									Modifs.CaseInsensitive = false;
									break;
								case 'c': // case sensitive
									Modifs.CaseInsensitive = true;
									break;
	
								case 's': // single-line
									Modifs.SingleLine = false;
									break;
								case 'm': // multiline
									Modifs.SingleLine = true;
									break;
	
								case 'n': // no auto-cap
									Modifs.NoAutoCap = false;
									break;
	
								case 'd': // unix lines
									Modifs.UnixLines = false;
									break;
	
								case 'l': // lazy groups
									Modifs.LazyGroups = false;
									break;
	
								case 'a': // dot-all
									Modifs.DotAll = false;
									break;
	
								case '^': // remove all modifs
									Modifs.CaseInsensitive
										= Modifs.SingleLine
										= Modifs.NoAutoCap
										= Modifs.UnixLines
										= Modifs.LazyGroups
										= Modifs.DotAll = true;
									break;
	
								default:
									Out = true;
									break;
								}
	
								if (!Out)
									++Further;
							}
	
							if (Out && Further != iter && *Further == ':') // modifier noncap
							{
								iter = Further;
								ContainerType Regex;
								while (++iter != endIter)
									Regex += *iter;
	
								InstructionSet NewInsts =
									TranslateInternal(Regex, Regex.begin(), error, CCSymbsToInds, CCsToInds, Modifs, MaxDepth - 1);
								OutInstructions.insert(OutInstructions.end(), NewInsts.begin(), NewInsts.end());
	
								OutInstructions.push_back({ RegexInstructionType::NonCaptureGroup, { (Modifs.LazyGroups ? "t" : "f") } });
	
								Modifs = Old;
							}
							else if (Out) // error
							{
								error = "Regex Compile Error: \'";
								error += *iter;
								error += "\' is not a valid modifier.";
								return;
							}
							else
								iter = Further;
						}
					}
					else // forward subroutine, regular subroutine, or recursion
						MunchSubroutine(iter, endIter, error, OutInstructions, NextCapGroup, MaxDepth, Modifs);
					break;
				}
			}
			else // regular group
			{
				if (!Modifs.NoAutoCap)
					OutInstructions.push_back({ RegexInstructionType::MakeCapture, { "f" } });
	
				ContainerType Regex;
				while (iter != endIter)
				{
					Regex += *iter;
					++iter;
				}
	
				InstructionSet NewInsts =
					TranslateInternal(Regex, Regex.begin(), error, CCSymbsToInds, CCsToInds, Modifs, MaxDepth - 1);
				OutInstructions.insert(OutInstructions.end(), NewInsts.begin(), NewInsts.end());
	
				if (!Modifs.NoAutoCap)
					OutInstructions.push_back({ RegexInstructionType::CaptureGroup_Numbered,{ std::to_string(++NextCapGroup), (Modifs.LazyGroups ? "t" : "f") } });
				else
					OutInstructions.push_back({ RegexInstructionType::NonCaptureGroup, { (Modifs.LazyGroups ? "t" : "f") } });
			}
		}

		// Handles operators and their lazy variants.
		static void MunchOp(IterType& iter,
			ConstIterType& endIter,
			std::string& error,
			IndexTracker& CCSymbsToInds,
			IndexTracker& CCsToInds,
			InstructionSet& OutInstructions,
			Modifiers& Modifs,
			char Op)
		{
			bool Lazy = false;
			if (Op != '{')
			{
				if (++iter != endIter && *iter == '?')
					Lazy = true;
				else
					--iter;
			}
	
			switch (Op)
			{
			case '?': // none or once
				OutInstructions.push_back({ (Lazy ? RegexInstructionType::NOnceLazy : RegexInstructionType::NOnce), {} });
				break;
	
			case '*': // none or more
				OutInstructions.push_back({ (Lazy ? RegexInstructionType::NPlusLazy : RegexInstructionType::NPlus),{} });
				break;
	
			case '+': // one or more
				OutInstructions.push_back({ (Lazy ? RegexInstructionType::OPlusLazy : RegexInstructionType::OPlus),{} });
				break;
	
			case '{': // potential repetition
				{
					if (++iter != endIter && isdigit(*iter)) // most likely a repetition ("{N}", "{N,}", "{N,M}")
					{
						ContainerType Min(1, *iter);
	
						if (++iter != endIter && *iter == ',')
						{
							if (++iter != endIter && isdigit(*iter))
							{
								ContainerType Max(1, *iter);
	
								if (++iter != endIter && *iter == '}')
								{
									if (++iter != endIter && *iter == '?')
										Lazy = true;
									else
										--iter;
	
									OutInstructions.push_back({ (Lazy ? RegexInstructionType::RepeatLazy : RegexInstructionType::Repeat), { "MinMax", Min, Max } });
								}
								else
									error = "Regex Compile Error: Min-Max Repeat quantifier is missing closing bracket.";
							}
							else if (iter != endIter && *iter == '}')
							{
								if (++iter != endIter && *iter == '?')
									Lazy = true;
								else
									--iter;
	
								OutInstructions.push_back({ (Lazy ? RegexInstructionType::RepeatLazy : RegexInstructionType::Repeat), { "Min", Min, Min } });
							}
							else
								error = "Regex Compile Error: Min Repeat quantifier is missing closing bracket.";
						}
						else if (*iter == '}')
						{
							if (++iter != endIter && *iter == '?')
								Lazy = true;
							else
								--iter;
	
							OutInstructions.push_back({ (Lazy ? RegexInstructionType::RepeatLazy : RegexInstructionType::Repeat), { "Exact", Min, Min } });
						}
						else
							error = "Regex Compile Error: Exact repeat quantifier is missing closing bracket.";
					}
					else // probably a literal
						MunchLiteral(--iter, endIter, error, CCSymbsToInds, CCsToInds, OutInstructions, Modifs);
				}
				break;
	
			default:
				error = "Regex Compile Error: Invalid quantifier.";
				break;
			}
		}

		// Inlet of the recursive descent parser; branches out into all of the functions above.
		static InstructionSet TranslateInternal(const ContainerType& Infix,
			IterType& iter,
			std::string& error,
			IndexTracker& CCSymbolsToInds,
			IndexTracker& CCsToInds,
			Modifiers Modifs,
			int MaxDepth)
		{
			InstructionSet Out;
	
			int NumNodes = 0;
	
			// used to bind relative/forward references. Starts at 0 in case a forward ref is at the very front of the regex.
			int LastGroup = 0;
	
			int PrevNumNodes = NumNodes;
			while (iter != Infix.end())
			{
				switch (*iter)
				{
				case '[': // begin character class
					{
						IterType EndBracket = iter;
						FindEnd(EndBracket, Infix.end(), '[', ']', nullptr);
	
						if (EndBracket != Infix.end())
						{
							ContainerType Ind = MunchCharClass(++iter, EndBracket, error, CCSymbolsToInds, CCsToInds, Out, Modifs, MaxDepth);
							Out.push_back({ RegexInstructionType::Literal, { Ind } });
						}
						else
							error = "Regex Compile Error: Couldn't find closing bracket for character class.";
	
						++NumNodes;
					}
					break;
	
				case '\\': // begin escaped metacharacter or special character
					MunchEscaped(iter, Infix.end(), error, CCSymbolsToInds, CCsToInds, Out, LastGroup, Modifs, MaxDepth);
					++NumNodes;
					break;
	
				case '^': // start of string/line (only valid at beginning of regex)
					if (!Out.empty() && iter != Infix.begin())
						MunchLiteral(iter, Infix.end(), error, CCSymbolsToInds, CCsToInds, Out, Modifs);
					else
					{
						ContainerType LineChars;
						if (Modifs.UnixLines)
							LineChars = "[\n]";
						else
							LineChars = "[\r\n\v\f]";
						ContainerType Ind =
							MunchCharClass(LineChars.begin() + 1, LineChars.end() - 1, error, CCSymbolsToInds, CCsToInds, Out, Modifs, MaxDepth);
	
						if (Modifs.SingleLine)
							Out.push_back({ RegexInstructionType::StartCheck, { "t", "f", Ind } });
						else
							Out.push_back({ RegexInstructionType::StartCheck, { "f", "f", Ind } });
					}
					++NumNodes;
					break;
	
				case '$': // end of string/line (only valid at end of regex)
					if (iter + 1 != Infix.end())
						MunchLiteral(iter, Infix.end(), error, CCSymbolsToInds, CCsToInds, Out, Modifs);
					else
					{
						ContainerType LineChars;
						if (Modifs.UnixLines)
							LineChars = "[\n]";
						else
							LineChars = "[\r\n\v\f]";
						ContainerType Ind =
							MunchCharClass(LineChars.begin() + 1, LineChars.end() - 1, error, CCSymbolsToInds, CCsToInds, Out, Modifs, MaxDepth);
	
						if (Modifs.SingleLine)
							Out.push_back({ RegexInstructionType::EndCheck, { "t", "f", Ind } });
						else
							Out.push_back({ RegexInstructionType::EndCheck, { "f", "f", Ind } });
					}
					++NumNodes;
					break;
	
				case '.': // any character
					{
						ContainerType DotClass = "[";
						if (Modifs.DotAll)
						{
							DotClass += '\0';
							DotClass += '-';
							DotClass += CHAR_MAX;
						}
						else if (Modifs.UnixLines)
							DotClass += "^\n";
						else
							DotClass += "^\r\n\v\f";
	
						DotClass += "]";
	
						ContainerType Ind =
							MunchCharClass(DotClass.begin() + 1, DotClass.end() - 1, error, CCSymbolsToInds, CCsToInds, Out, Modifs, MaxDepth);
	
						Out.push_back({ RegexInstructionType::Literal, { Ind } });
	
						++NumNodes;
					}
					break;
	
				case '|': // choice
					{
						InstructionSet SecondPart = TranslateInternal(Infix, ++iter, error, CCSymbolsToInds, CCsToInds, Modifs, MaxDepth);
						if (!Out.empty() && !SecondPart.empty())
						{
							Out.insert(Out.end(), SecondPart.begin(), SecondPart.end());
							Out.push_back({ RegexInstructionType::Alternate,{} });
						}
						else
							Out.insert(Out.end(), SecondPart.begin(), SecondPart.end());
	
						return Out;
					}
					break;
	
				case '?': // none or once
				case '*': // none or more
				case '+': // one or more
				case '{': // Repetition ("{N}", "{N,}", "{N,M}")
					MunchOp(iter, Infix.end(), error, CCSymbolsToInds, CCsToInds, Out, Modifs, *iter);
					break;
	
				case '(': // group
					{
						size_t PriorSize = Out.size();
						{
							IterType EndBracket = iter;
							FindEnd(EndBracket, Infix.end(), '(', ')', nullptr);
	
							if (EndBracket != Infix.end())
								MunchGroup(++iter, EndBracket, error, CCSymbolsToInds, CCsToInds, Out, LastGroup, Modifs, MaxDepth);
							else
								error = "Regex Compile Error: Couldn't find closing bracket for group.";
						}
	
						if (Out.size() > PriorSize &&
							Out.back().InstructionType != RegexInstructionType::MakeCapture &&
							Out.back().InstructionType != RegexInstructionType::MakeCaptureCollection &&
							Out.back().InstructionType != RegexInstructionType::DefineAsSubroutine)
							++NumNodes;
					}
					break;
	
				default: // literals
					MunchLiteral(iter, Infix.end(), error, CCSymbolsToInds, CCsToInds, Out, Modifs);
					++NumNodes;
					break;
				}
	
				bool ShouldConcat = true;
				{
					IterType Copy = iter;
					ShouldConcat = ++Copy == Infix.end();
					if (!ShouldConcat)
					{
						ShouldConcat =
							*Copy != '?' &&
							*Copy != '*' &&
							*Copy != '+' &&
							!(*Copy == '{' && (++Copy != Infix.end() && isdigit(*Copy)));
					}
				}
	
				if (!error.empty())
					return {};
	
				if (NumNodes > 1 && NumNodes > PrevNumNodes && ShouldConcat)
				{
					Out.push_back({ RegexInstructionType::Concat, {} });
					PrevNumNodes = NumNodes;
				}
	
				++iter;
			}
	
			return Out;
		}
	
	public:

		// Translates a given string into instructions for the regex assembler to follow.
		static InstructionSet Translate(ContainerType& Infix, std::string& error, int MaxDepth)
		{
			IterType Iter = Infix.begin();
			Modifiers Modifs;
			IndexTracker CCSymbolsToInds;
			IndexTracker CCsToInds;
			return TranslateInternal(Infix, Iter, error, CCSymbolsToInds, CCsToInds, Modifs, MaxDepth);
		}
	};
}
