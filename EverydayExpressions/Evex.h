#pragma once

#include "EvexTranslator.h"
#include "EvexGroupNode.h"


namespace Evex
{
	/*
		Intermediate structure which keeps track of the 'loose ends' of the current regex graph.
		Makes joining of regex chunks much simpler.
	*/
	template<typename T>
	struct RegexChunkLooseEnds
	{
		std::unordered_set<RegexChunk<T>*> ChunksInvolved;
		std::unordered_set<RegexNodeGhostIn<T>*> Ins;
		std::unordered_set<RegexNodeGhostOut<T>*> Outs;
	
		std::unordered_set<RegexChunk<T>*> GetStartChunks()
		{
			std::unordered_set<RegexChunk<T>*> Out;
			for (RegexNodeGhostIn<T>* currIn : Ins)
			{
				for (RegexChunk<T>* currChunk : ChunksInvolved)
				{
					if (currChunk->ContainsNode(currIn))
					{
						Out.insert(currChunk);
						break;
					}
				}
			}
			return Out;
		}
	
		std::unordered_set<RegexChunk<T>*> GetEndChunks()
		{
			std::unordered_set<RegexChunk<T>*> Out;
			for (RegexNodeGhostOut<T>* currOut : Outs)
			{
				for (RegexChunk<T>* currChunk : ChunksInvolved)
				{
					if (currChunk->ContainsNode(currOut))
					{
						Out.insert(currChunk);
						break;
					}
				}
			}
			return Out;
		}
	};
	
	template<typename T> class RegexAssembler;
	
	class RegexCompileException : public std::exception
	{
	private:
		std::string msg;
	public:
		explicit RegexCompileException(const std::string& inMsg) : msg(inMsg) {}
		virtual const char* what() const throw() { return msg.c_str(); }
	};
	
	/*
		Automaton and matching class for Evex regexes.
	*/
	template<typename T>
	class Regex
	{
	private:
	
		std::vector<RegexCaptureBase<T>*> Captures;
		std::vector<RegexCaptureBase<T>*> DefinedSubroutines;
		std::unordered_map<std::basic_string<T>, RegexCaptureBase<T>*> NamesToCaptures;
	
		std::vector<RegexCharacterClassSymbol<T>*> CharClassSymbols;
		std::vector<RegexCharacterClassBase<T>*> CharacterClasses;
	
		std::vector<RegexTicker<T>> Tickers;
	
		std::unordered_set<RegexChunk<T>*> Chunks;
		std::unordered_set<RegexNodeGhostIn<T>*> StartNodes;
		std::unordered_set<RegexNodeGhostOut<T>*> EndNodes;
	
		std::string CompileError = "";
		std::vector<std::string> RuntimeErrors;
	
		bool StartsWithLineCheck = false; // starts with word bound check or line check
		bool EndsWithLineCheck = false; // ends with word bound check or line check
	
		// End of the last match, for exclusive use by "\\G"
		RegexRangeIterator<T> LastMatchEnd;
	
		void ResetPreMatch()
		{
			for (RegexTicker<T>& currTicker : Tickers)
				currTicker.Reset();
	
			for (RegexCaptureBase<T>* currCap : Captures)
			{
				if (!currCap->Manual)
					currCap->Reset();
			}
	
			for (RegexCaptureBase<T>* currSub : DefinedSubroutines)
				currSub->Reset();
		}
	
	public:
	
		using HookFuncType = std::function<void(RegexRangeIterator<T>&)>;
		using FuncMapType = std::unordered_map<std::basic_string<T>, HookFuncType>;
	
		template<typename TranslatorType = RegexTranslator<T>, typename AssemblerType = RegexAssembler<T>>
		Regex(std::basic_string<T> c, FuncMapType* Funcs = nullptr, std::vector<RegexInstruction<T>>* OutInstructions = nullptr, int MaxNestingDepth = 100, RegexRangeIterator<T>* PresetLastMatchEnd = nullptr);

		Regex(std::vector<RegexInstruction<T>>& Instructions, FuncMapType* Funcs = nullptr);
	
		~Regex()
		{
			for (RegexChunk<T>* CurrChunk : Chunks)
				delete CurrChunk;
	
			for (RegexCaptureBase<T>* currCap : Captures)
				delete currCap;
	
			for (RegexCaptureBase<T>* currSub : DefinedSubroutines)
				delete currSub;
	
			for (RegexCharacterClassBase<T>* currCharClass : CharacterClasses)
				delete currCharClass;
	
			for (RegexCharacterClassSymbol<T>* currSymb : CharClassSymbols)
				delete currSymb;
		}
	
		bool IsValidForMatching() const { return CompileError.empty(); }
		std::string GetCompileError() const { return CompileError; }
		const std::vector<std::string>& GetRuntimeErrors() const { return RuntimeErrors; }
	
		RegexRangeIterator<T>& GetLastMatchEnd() const { return LastMatchEnd; }
		RegexRangeIterator<T>* GetLastMatchEndPtr() const { return &LastMatchEnd; }
		void SetLastMatchEnd(RegexRangeIterator<T>& NewLastMatch) { LastMatchEnd = NewLastMatch; }
		void SetLastMatchEnd(RegexRangeIterator<T>* NewLastMatch) { LastMatchEnd = *NewLastMatch; }
	
	private:
	
		// Returns true if matches the given input string, from the beginning.
		bool MatchInternal(std::basic_string<T>& String)
		{
			if (!CompileError.empty())
				throw RegexCompileException(CompileError);
	
			RuntimeErrors.clear();
	
			ResetPreMatch();
	
			RegexRangeIterator<T> Iter(String.begin(), String.begin(), String.end());
	
			std::vector<RegexNode<T>*> CurrentNexts;
	
			for (RegexNodeGhostIn<T>* currIn : StartNodes)
			{
				std::vector<RegexNode<T>*> NextReals = currIn->GetNexts();
				CurrentNexts.insert(CurrentNexts.end(), NextReals.begin(), NextReals.end());
			}
	
			bool FirstTime = true;
			bool LastTime = EndsWithLineCheck;
			RegexNode<T>* CurrNode = nullptr;
			while (CurrentNexts.size() > 0 && (!Iter.IsEnd() || LastTime))
			{
				bool NewNodeFound = false;
				for (RegexNode<T>* currNext : CurrentNexts)
				{
					try
					{
						if (currNext->CanEnter(Iter))
						{
							CurrNode = currNext;
							NewNodeFound = true;
							break;
						}
					}
					catch (RegexRuntimeException e)
					{
						RuntimeErrors.push_back(e.what());
						return false;
					}
				}
	
				if (!NewNodeFound)
					break;
	
				CurrentNexts = CurrNode->GetNexts();
	
				if (!(StartsWithLineCheck && FirstTime))
				{
					if (!Iter.IsEnd())
						++Iter;
					else if (LastTime)
						LastTime = false;
				}
				else
					FirstTime = false;
			}
	
			CurrentNexts.clear();
	
			if (CurrNode)
			{
				for (RegexNodeGhostOut<T>* CurrOut : CurrNode->GhostNexts)
				{
					if (EndNodes.find(CurrOut) != EndNodes.end())
						return true;
				}
			}
	
			return false;
		}
	
		// Returns true if matches the given input string, from the given offset position onward
		bool MatchFromInternal(std::basic_string<T>& String, int Offset, std::basic_string<T>& OutSubstring)
		{
			if (!CompileError.empty())
				throw RegexCompileException(CompileError);
	
			RuntimeErrors.clear();
	
			ResetPreMatch();
	
			RegexRangeIterator<T> Iter(String.begin() + Offset, String.begin(), String.end());
	
			std::vector<RegexNode<T>*> CurrentNexts;
	
			for (RegexNodeGhostIn<T>* currIn : StartNodes)
			{
				std::vector<RegexNode<T>*> NextReals = currIn->GetNexts();
				CurrentNexts.insert(CurrentNexts.end(), NextReals.begin(), NextReals.end());
			}
	
			bool FirstTime = true;
			bool LastTime = EndsWithLineCheck;
			RegexNode<T>* CurrNode = nullptr;
			while (CurrentNexts.size() > 0 && (!Iter.IsEnd() || LastTime))
			{
				bool NewNodeFound = false;
				for (RegexNode<T>* currNext : CurrentNexts)
				{
					try
					{
						if (currNext->CanEnter(Iter))
						{
							CurrNode = currNext;
							NewNodeFound = true;
							break;
						}
					}
					catch (RegexRuntimeException e)
					{
						RuntimeErrors.push_back(e.what());
						return false;
					}
				}
	
				if (!NewNodeFound)
					break;
	
				CurrentNexts = CurrNode->GetNexts();
	
				if (!(StartsWithLineCheck && FirstTime))
				{
					if (!Iter.IsEnd())
						++Iter;
					else if (LastTime)
						LastTime = false;
				}
				else
					FirstTime = false;
			}
	
			CurrentNexts.clear();
	
			if (CurrNode)
			{
				for (RegexNodeGhostOut<T>* CurrOut : CurrNode->GhostNexts)
				{
					if (EndNodes.find(CurrOut) != EndNodes.end())
					{
						RegexRangeIterator<T> Copy(String.begin() + Offset, String.begin(), String.end());
						for (; Copy != Iter; ++Copy)
							OutSubstring += *Copy;
	
						return true;
					}
				}
			}
	
			return false;
		}
	
		// Returns true if any matching substrings were found in the given text, from any position.
		bool MatchAllInternal(std::basic_string<T>& String, std::vector<std::basic_string<T>>& OutSubstrings)
		{
			if (!CompileError.empty())
				throw RegexCompileException(CompileError);
	
			OutSubstrings.clear();
	
			for (unsigned int i = 0; i < String.length(); ++i)
			{
				std::basic_string<T> NextSub;
				if (MatchFromInternal(String, i, NextSub))
				{
					OutSubstrings.push_back(NextSub);
					i += NextSub.length() - 1;
					LastMatchEnd = RegexRangeIterator<T>(String.begin() + i, String.begin(), String.end());
				}
			}
	
			return !OutSubstrings.empty() && RuntimeErrors.empty();
		}
	
	public:

		// Returns true if matches the given input string, from the beginning.
		inline bool Match(const T* String) { return MatchInternal(std::basic_string<T>(String)); }

		// Returns true if matches the given input string, from the beginning.
		inline bool Match(std::basic_string<T>& String) { return MatchInternal(String); }


		// Returns true if matches the given input string, from the given offset position onward
		inline bool MatchFrom(const T* String, int Offset, std::basic_string<T>& OutSubstring) { return MatchFromInternal(std::basic_string<T>(String), Offset, OutSubstring); }

		// Returns true if matches the given input string, from the given offset position onward
		inline bool MatchFrom(std::basic_string<T>& String, int Offset, std::basic_string<T>& OutSubstring) { return MatchFromInternal(String, Offset, OutSubstring); }


		// Returns true if any matching substrings were found in the given text, from any position.
		inline bool MatchAll(const T* String, std::vector<std::basic_string<T>>& OutSubstrings) { return MatchAllInternal(std::basic_string<T>(String), OutSubstrings); }

		// Returns true if any matching substrings were found in the given text, from any position.
		inline bool MatchAll(std::basic_string<T>& String, std::vector<std::basic_string<T>>& OutSubstrings) { return MatchAllInternal(String, OutSubstrings); }
	
	private:
	
		friend class RegexAssembler<T>;
		template<typename T> friend bool DrawRegex(Regex<T>& RegexGraph, const std::basic_string<T>& Filepath);

		/*
			Keeps track of referential nodes in need of post-construction initialization.
			Primarily used to make sure that references within groups are also initialized.
		*/
		struct CollapsePacket
		{
			std::unordered_map<RegexBackreferenceNode<T>*, int>& Backs_Numbered;
			std::unordered_map<RegexBackreferenceNode<T>*, std::basic_string<T>>& Backs_Named;
	
			std::unordered_map<RegexSubroutineNode<T>*, int>& Subs_Numbered;
			std::unordered_map<RegexSubroutineNode<T>*, std::basic_string<T>>& Subs_Named;
	
			std::unordered_set<RegexRecursionNode<T>*>& Recursions;
	
			std::unordered_map<RegexCaptureNode<T>*, int>& Caps_Numbered;
			std::unordered_map<RegexCaptureNode<T>*, std::basic_string<T>>& Caps_Named;
		};

		// Creates a Loose-Ends structure from the specified regex chunk.
		inline RegexChunkLooseEnds<T> GetLooseEnds(RegexChunk<T>* chunk)
		{
			RegexChunkLooseEnds<T> Out;
			Out.ChunksInvolved.insert(chunk);
	
			for (RegexNodeGhostIn<T>& currIn : chunk->Ins)
				Out.Ins.insert(&currIn);
	
			for (RegexNodeGhostOut<T>& currOut : chunk->Outs)
				Out.Outs.insert(&currOut);
	
			return Out;
		}

		// Handles setup of group nodes.
		inline RegexChunkLooseEnds<T> AssembleGroup(RegexChunkLooseEnds<T>& chunk, RegexGroupNode<T>* FocusNode, CollapsePacket& CloneMaps)
		{
			RegexChunkLooseEnds<T> CollapsedChunk = Collapse(chunk, CloneMaps);
			PruneIntermediaryGhosts(CollapsedChunk);
	
			FocusNode->Chunks.insert(FocusNode->Chunks.end(), CollapsedChunk.ChunksInvolved.begin(), CollapsedChunk.ChunksInvolved.end());
			FocusNode->Ins = CollapsedChunk.Ins;
			FocusNode->Outs = CollapsedChunk.Outs;
	
			RegexChunk<T>* NewChunk = RegexChunk<T>::Wrap(FocusNode);
			Chunks.insert(NewChunk);
	
			return GetLooseEnds(NewChunk);
		}
	
		// Handles setup of multiple group nodes. Collapse is only called once here, making this perfect for many groups referencing the same chunk.
		inline std::vector<RegexChunkLooseEnds<T>> AssembleGroups(RegexChunkLooseEnds<T>& chunk, std::vector<RegexGroupNode<T>*> FocusNodes,
			CollapsePacket& CloneMaps)
		{
			RegexChunkLooseEnds<T> CollapsedChunk = Collapse(chunk, CloneMaps);
			PruneIntermediaryGhosts(CollapsedChunk);
			
			std::vector<RegexChunkLooseEnds<T>> Out;
			for (RegexGroupNode<T>* currFocus : FocusNodes)
			{
				currFocus->Chunks.insert(currFocus->Chunks.end(), CollapsedChunk.ChunksInvolved.begin(), CollapsedChunk.ChunksInvolved.end());
				currFocus->Ins = CollapsedChunk.Ins;
				currFocus->Outs = CollapsedChunk.Outs;
	
				RegexChunk<T>* NewChunk = RegexChunk<T>::Wrap(currFocus);
				Chunks.insert(NewChunk);
	
				Out.push_back(GetLooseEnds(NewChunk));
			}
	
			return Out;
		}
	
		// Constructs the operation "ab"
		inline RegexChunkLooseEnds<T> Concat(RegexChunkLooseEnds<T>& lhs, RegexChunkLooseEnds<T>& rhs)
		{
			RegexChunkLooseEnds<T> Out;
			Out.ChunksInvolved = lhs.ChunksInvolved;
			Out.ChunksInvolved.insert(rhs.ChunksInvolved.begin(), rhs.ChunksInvolved.end());
	
			std::unordered_set<RegexChunk<T>*> LeftOuts = lhs.GetEndChunks();
			std::unordered_set<RegexChunk<T>*> RightIns = rhs.GetStartChunks();
	
			for (RegexChunk<T>* currLeftOut : LeftOuts)
				currLeftOut->ConnectedTos.insert(RightIns.begin(), RightIns.end());
	
			for (RegexNodeGhostOut<T>* currLeftOut : lhs.Outs)
				currLeftOut->GhostNexts.insert(rhs.Ins.begin(), rhs.Ins.end());
	
			Out.Ins = lhs.Ins;
			Out.Outs = rhs.Outs;
	
			return Out;
		}
	
		// Cosntructs the operation "a?"
		inline RegexChunkLooseEnds<T> OccurNoneOrOnce(RegexChunkLooseEnds<T>& chunk, CollapsePacket& CloneMaps, bool Lazy = false)
		{
			RegexNoneOrMoreNode<T>* NewNoneOrOnce = new RegexNoneOrMoreNode<T>(true, Lazy);
	
			return AssembleGroup(chunk, NewNoneOrOnce, CloneMaps);
		}
	
		// Constructs the operation "a*"
		inline RegexChunkLooseEnds<T> OccurNonePlus(RegexChunkLooseEnds<T>& chunk, CollapsePacket& CloneMaps, bool Lazy = false)
		{
			RegexNoneOrMoreNode<T>* NewNonePlus = new RegexNoneOrMoreNode<T>(false, Lazy);
	
			return AssembleGroup(chunk, NewNonePlus, CloneMaps);
		}
	
		// Constructs the operation "a+"
		inline RegexChunkLooseEnds<T> OccurOncePlus(RegexChunkLooseEnds<T>& chunk, CollapsePacket& CloneMaps, bool Lazy = false)
		{
			RegexLoopNode<T>* NewLoop = new RegexLoopNode<T>(nullptr, Lazy);
	
			return AssembleGroup(chunk, NewLoop, CloneMaps);
		}
	
		// Constructs an alternation ("a|b") in NFA(!) format. Must be collapsed later on during group assembly or DFA finalization.
		inline RegexChunkLooseEnds<T> Alternate(RegexChunkLooseEnds<T>& lhs, RegexChunkLooseEnds<T>& rhs)
		{
			RegexChunkLooseEnds<T> Out;
			Out.ChunksInvolved = lhs.ChunksInvolved;
			Out.ChunksInvolved.insert(rhs.ChunksInvolved.begin(), rhs.ChunksInvolved.end());
	
			Out.Ins = lhs.Ins;
			Out.Ins.insert(rhs.Ins.begin(), rhs.Ins.end());
	
			Out.Outs = lhs.Outs;
			Out.Outs.insert(rhs.Outs.begin(), rhs.Outs.end());
	
			return Out;
		}
	
		// Handles remapping of referential nodes to their proper references during a collapse.
		void TryFitCloneMap(RegexNode<T>* Node, RegexNode<T>* PriorNode, CollapsePacket& CloneMaps)
		{
			if (RegexBackreferenceNode<T>* AsBack = dynamic_cast<RegexBackreferenceNode<T>*>(Node))
			{
				RegexBackreferenceNode<T>* PriorAsBack = dynamic_cast<RegexBackreferenceNode<T>*>(PriorNode);
	
				auto foundNumbered = CloneMaps.Backs_Numbered.find(PriorAsBack);
	
				if (foundNumbered != CloneMaps.Backs_Numbered.end())
				{
					CloneMaps.Backs_Numbered[AsBack] = foundNumbered->second;
					CloneMaps.Backs_Numbered.erase(PriorAsBack);
				}
				else
				{
					auto foundNamed = CloneMaps.Backs_Named.find(PriorAsBack);
	
					if (foundNamed != CloneMaps.Backs_Named.end())
					{
						CloneMaps.Backs_Named[AsBack] = foundNamed->second;
						CloneMaps.Backs_Named.erase(PriorAsBack);
					}
				}
			}
			else if (RegexSubroutineNode<T>* AsSub = dynamic_cast<RegexSubroutineNode<T>*>(Node))
			{
				RegexSubroutineNode<T>* PriorAsSub = dynamic_cast<RegexSubroutineNode<T>*>(PriorNode);
	
				auto foundNumbered = CloneMaps.Subs_Numbered.find(PriorAsSub);
	
				if (foundNumbered != CloneMaps.Subs_Numbered.end())
				{
					CloneMaps.Subs_Numbered[AsSub] = foundNumbered->second;
					CloneMaps.Subs_Numbered.erase(PriorAsSub);
				}
				else
				{
					auto foundNamed = CloneMaps.Subs_Named.find(PriorAsSub);
	
					if (foundNamed != CloneMaps.Subs_Named.end())
					{
						CloneMaps.Subs_Named[AsSub] = foundNamed->second;
						CloneMaps.Subs_Named.erase(PriorAsSub);
					}
				}
			}
			else if (RegexRecursionNode<T>* AsRec = dynamic_cast<RegexRecursionNode<T>*>(Node))
			{
				CloneMaps.Recursions.insert(AsRec);
				CloneMaps.Recursions.erase(dynamic_cast<RegexRecursionNode<T>*>(PriorNode));
			}
			else if (RegexCaptureNode<T>* AsCap = dynamic_cast<RegexCaptureNode<T>*>(Node))
			{
				RegexCaptureNode<T>* PriorAsCap = dynamic_cast<RegexCaptureNode<T>*>(PriorNode);
	
				auto foundNumbered = CloneMaps.Caps_Numbered.find(PriorAsCap);
	
				if (foundNumbered != CloneMaps.Caps_Numbered.end())
				{
					CloneMaps.Caps_Numbered[AsCap] = foundNumbered->second;
					CloneMaps.Caps_Numbered.erase(PriorAsCap);
				}
				else
				{
					auto foundNamed = CloneMaps.Caps_Named.find(PriorAsCap);
	
					if (foundNamed != CloneMaps.Caps_Named.end())
					{
						CloneMaps.Caps_Named[AsCap] = foundNamed->second;
						CloneMaps.Caps_Named.erase(PriorAsCap);
					}
				}
			}
		}
	
		// Collapses an alternation ("a|b") from NFA format into DFA format, i.e. collapses duplicate Nexts on nodes.
		inline RegexChunkLooseEnds<T> Collapse(RegexChunkLooseEnds<T>& chunk, CollapsePacket& CloneMaps)
		{
			RegexChunkLooseEnds<T> Out;
	
			RegexChunk<T>* NewChunk = new RegexChunk<T>();
			Chunks.insert(NewChunk);
			Out.ChunksInvolved.insert(NewChunk);
	
			NewChunk->Ins.push_back(RegexNodeGhostIn<T>());
	
			for (RegexNodeGhostIn<T>* currIn : chunk.Ins)
				NewChunk->Ins.back().Incorporate(currIn);
	
			int EndInd = -1;
			std::unordered_set<int> CurrIns = { int(NewChunk->Ins.size() - 1) };
			std::unordered_map<int, std::unordered_set<int>> InsToInds;
			auto CollapseIns =
				[&NewChunk, &InsToInds, this, &CloneMaps](std::unordered_set<int>& CurrentIns, std::unordered_set<int>& NextNodes)
				{
					NextNodes.clear();
					std::unordered_set<int> SeggedNexts;
					for (const int& currInd : CurrentIns)
					{
						RegexNodeGhostIn<T>* curr = &NewChunk->Ins[currInd];
						for (RegexNode<T>* currNext : curr->Nexts)
						{
							int FoundInd = -1;
							for (const int& currCollapseInd : SeggedNexts)
							{
								if (currNext->SimilarTo(NewChunk->Nodes[currCollapseInd]))
								{
									FoundInd = currCollapseInd;
									break;
								}
							}
	
							if (FoundInd < 0)
							{
								NewChunk->Nodes.push_back(currNext->Clone());
								TryFitCloneMap(NewChunk->Nodes.back(), currNext, CloneMaps);
								FoundInd = NewChunk->Nodes.back()->Index = NewChunk->Nodes.size() - 1;
							}
	
							NewChunk->Nodes[FoundInd]->Incorporate(currNext);

							InsToInds[currInd].insert(FoundInd);
							NextNodes.insert(FoundInd);
							SeggedNexts.insert(FoundInd);
						}
						curr->Nexts.clear();
						SeggedNexts.clear();
					}
				};
	
			std::unordered_set<int> CurrNodes;
			std::unordered_map<int, std::unordered_set<int>> NodesToInds;
			auto CollapseNodes =
				[&NewChunk, &chunk, &EndInd, &NodesToInds](std::unordered_set<int>& CurrentNodes, std::unordered_set<int>& NextOuts)
				{
					int ConstructedInd = -1;
					NextOuts.clear();
					for (const int& currInd : CurrentNodes)
					{
						RegexNode<T>* curr = NewChunk->Nodes[currInd];
						for (RegexNodeGhostOut<T>* currNext : curr->GhostNexts)
						{
							int FoundInd = -1;
							if (chunk.Outs.find(currNext) != chunk.Outs.end())
							{
								if (EndInd < 0)
								{
									NewChunk->Outs.push_back(RegexNodeGhostOut<T>());
									EndInd = NewChunk->Outs.back().Index = NewChunk->Outs.size() - 1;
								}
	
								FoundInd = EndInd;
							}
							else
							{
								if (ConstructedInd < 0)
								{
									NewChunk->Outs.push_back(RegexNodeGhostOut<T>());
									ConstructedInd = NewChunk->Outs.back().Index = NewChunk->Outs.size() - 1;
								}
	
								FoundInd = ConstructedInd;
							}
	
							NewChunk->Outs[FoundInd].Incorporate(currNext);

							NodesToInds[currInd].insert(FoundInd);
							NextOuts.insert(FoundInd);
						}
						curr->Nexts.clear();
						curr->GhostNexts.clear();
						ConstructedInd = -1;
					}
				};
	
			std::unordered_set<int> CurrOuts;
			std::unordered_map<int, std::unordered_set<int>> OutsToInds;
			auto CollapseOuts =
				[&NewChunk, &OutsToInds](std::unordered_set<int>& CurrentOuts, std::unordered_set<int>& NextIns)
				{
					int ConstructedInd = -1;
					NextIns.clear();
					for (const int& currInd : CurrentOuts)
					{
						RegexNodeGhostOut<T>* curr = &NewChunk->Outs[currInd];
						for (RegexNodeGhostIn<T>* currNext : curr->GhostNexts)
						{
							int FoundInd = -1;
							if (ConstructedInd < 0)
							{
								NewChunk->Ins.push_back(RegexNodeGhostIn<T>());
								ConstructedInd = NewChunk->Ins.back().Index = NewChunk->Ins.size() - 1;
							}
	
							FoundInd = ConstructedInd;

							NewChunk->Ins[FoundInd].Incorporate(currNext);
	
							OutsToInds[currInd].insert(FoundInd);
							NextIns.insert(FoundInd);
						}
						curr->GhostNexts.clear();
						ConstructedInd = -1;
					}
				};
	
			while (!CurrIns.empty())
			{
				CollapseIns(CurrIns, CurrNodes);
				CollapseNodes(CurrNodes, CurrOuts);
				CollapseOuts(CurrOuts, CurrIns);
			}
	
	
			// rewire all of the disparate nodes back together now that the brunt of the operation is over
	
			for (auto& currInPair : InsToInds)
			{
				RegexNodeGhostIn<T>& RetrievedIn = NewChunk->Ins[currInPair.first];
				for (const int& currInd : currInPair.second)
					RetrievedIn.Nexts.insert(NewChunk->Nodes[currInd]);
			}
	
			for (auto& currNodePair : NodesToInds)
			{
				RegexNode<T>* RetrievedNode = NewChunk->Nodes[currNodePair.first];
				for (const int& currInd : currNodePair.second)
					RetrievedNode->GhostNexts.insert(&NewChunk->Outs[currInd]);
			}
	
			for (auto& currOutPair : OutsToInds)
			{
				RegexNodeGhostOut<T>& RetrievedOut = NewChunk->Outs[currOutPair.first];
				for (const int& currInd : currOutPair.second)
					RetrievedOut.GhostNexts.insert(&NewChunk->Ins[currInd]);
			}
	
			Out.Ins.insert(&NewChunk->Ins[0]);
			Out.Outs.insert(&NewChunk->Outs[EndInd]);
	
			// delete all the old chunks
			for (RegexChunk<T>* currToDel : chunk.ChunksInvolved)
			{
				Chunks.erase(Chunks.find(currToDel));
				delete currToDel;
			}
	
			return Out;
		}
	
		// Constructs the operation "a{N}"
		inline RegexChunkLooseEnds<T> RepeatExact(RegexChunkLooseEnds<T>& chunk, int Times, CollapsePacket& CloneMaps, bool Lazy = false)
		{
			if (Times < 2)
				return chunk;
	
			Tickers.push_back(RegexTicker<T>(Times));
	
			RegexLoopNode<T>* NewLoop = new RegexLoopNode<T>(&Tickers.back(), Lazy);
	
			return AssembleGroup(chunk, NewLoop, CloneMaps);
		}
	
		// Constructs the operation "a{N,}"
		inline RegexChunkLooseEnds<T> RepeatMin(RegexChunkLooseEnds<T>& chunk, int MinTimes, CollapsePacket& CloneMaps, bool Lazy = false)
		{
			if (MinTimes < 2)
				return OccurOncePlus(chunk, CloneMaps);
	
			Tickers.push_back(RegexTicker<T>(MinTimes - 1));
	
			RegexLoopNode<T>* NewLoopA = new RegexLoopNode<T>(&Tickers.back());
			RegexLoopNode<T>* NewLoopB = new RegexLoopNode<T>(nullptr, Lazy);
	
			std::vector<RegexChunkLooseEnds<T>> Sides = AssembleGroups(chunk, { NewLoopA, NewLoopB }, CloneMaps);
	
			return Concat(Sides[0], Sides[1]);
		}
	
		// Constructs the operation "a{N,M}"
		inline RegexChunkLooseEnds<T> RepeatMinMax(RegexChunkLooseEnds<T>& chunk, int MinTimes, int MaxTimes,
			CollapsePacket& CloneMaps, bool Lazy = false)
		{
			MinTimes = (MinTimes < MaxTimes ? MinTimes : MaxTimes);
			MaxTimes = (MaxTimes > MinTimes ? MaxTimes : MinTimes);
	
			if (MinTimes == MaxTimes)
				return RepeatExact(chunk, MinTimes, CloneMaps);
	
			Tickers.push_back(RegexTicker<T>(MinTimes - 1));
			Tickers.push_back(RegexTicker<T>((MinTimes - 1) - MaxTimes));
	
			RegexLoopNode<T>* NewLoopA = new RegexLoopNode<T>(&(*(Tickers.end() - 2)));
			RegexLoopNode<T>* NewLoopB = new RegexLoopNode<T>(&Tickers.back(), Lazy);
	
			std::vector<RegexChunkLooseEnds<T>> Sides = AssembleGroups(chunk, { NewLoopA, NewLoopB }, CloneMaps);
	
			return Concat(Sides[0], Sides[1]);
		}

	public:

		// Retrieves a numbered capture. NOTE: Indices offset by 1! i.e. "\\1" or "\\k<1>" -> Captures[0]
		inline bool GetCapture(int Index, std::basic_string<T>& OutCapture, bool& OutCaptureSuccess)
		{
			if (Index < 1 || Index > (int)Captures.size())
				return false;
	
			RegexCapture<T>* Retrieved = dynamic_cast<RegexCapture<T>*>(Captures[Index - 1]);
	
			if (Retrieved)
			{
				OutCapture = Retrieved->CapturedInputs;
				OutCaptureSuccess = Retrieved->Succeeded;
				return true;
			}
	
			return false;
		}

		// Retrieves a numbered capture collection. NOTE: Indices offset by 1! i.e. "\\1" or "\\k<1> -> Captures[0]
		inline bool GetCaptureCollection(int Index, std::vector<std::basic_string<T>>& OutCaptures, bool& OutCaptureSuccess)
		{
			if (Index < 1 || Index > Captures.size())
				return false;
	
			RegexCaptureCollection<T>* Retrieved = dynamic_cast<RegexCaptureCollection<T>*>(Captures[Index - 1]);
	
			if (Retrieved)
			{
				OutCaptures = Retrieved->CapturedInputs;
				OutCaptureSuccess = Retrieved->Succeeded;
				return true;
			}
	
			return false;
		}

		// Retrieves a named capture, i.e. "\\g<Name>" -> NamesToCaptures[Name]
		inline bool GetCapture(std::basic_string<T> Name, std::basic_string<T>& OutCapture, bool& OutCaptureSuccess)
		{
			auto found = NamesToCaptures.find(Name);
	
			if (found == NamesToCaptures.end())
				return false;
	
			RegexCapture<T>* Retrieved = dynamic_cast<RegexCapture<T>*>(found->second);
	
			if (Retrieved)
			{
				OutCapture = Retrieved->CapturedInputs;
				OutCaptureSuccess = Retrieved->Succeeded;
				return true;
			}
	
			return false;
		}

		// Retrieves a named capture collection, i.e. "\\g<Name>" -> NamesToCaptures[Name]
		inline bool GetCaptureCollection(std::basic_string<T> Name, std::vector<std::basic_string<T>>& OutCaptures, bool& OutCaptureSuccess)
		{
			auto found = NamesToCaptures.find(Name);
	
			if (found == NamesToCaptures.end())
				return false;
	
			RegexCaptureCollection<T>* Retrieved = dynamic_cast<RegexCaptureCollection<T>*>(found->second);
	
			if (Retrieved)
			{
				OutCaptures = Retrieved->CapturedInputs;
				OutCaptureSuccess = Retrieved->Succeeded;
				return true;
			}
	
			return false;
		}
	
		// Pre-sets numbered captures. NOTE: Indices start at 1 for easier readability! i.e. 'index' 1 sets capture \1.
		inline void PreSetCaptures(const std::vector<std::pair<int, std::basic_string<T>>>& IndicesAndValues)
		{
			for (auto& currPair : IndicesAndValues)
			{
				if (currPair.first >= 1 && currPair.first <= int(Captures.size()))
					Captures[currPair.first - 1]->SetCapture(currPair.second, true);
			}
		}
	
		// Pre-sets named captures.
		inline void PreSetCaptures(const std::vector<std::pair<std::basic_string<T>,std::basic_string<T>>>& NamesAndValues)
		{
			for (auto& currPair : NamesAndValues)
			{
				auto found = NamesToCaptures.find(currPair.first);
	
				if (found != NamesToCaptures.end())
					found->second->SetCapture(currPair.second, true);
			}
		}
	
		// Resets numbered captures. NOTE: Indices start at 1 for easier readability! i.e. 'index' 1 sets capture \1.
		inline void PreResetCaptures(const std::vector<int>& CaptureIndices)
		{
			for (int& currInd : CaptureIndices)
			{
				if (currInd >= 1 && currInd <= int(Captures.size()))
					Captures[currInd - 1]->Reset();
			}
		}
	
		// Resets named captures.
		inline void PreResetCaptures(const std::vector<std::basic_string<T>>& CaptureNames)
		{
			for (std::basic_string<T>& currName : IndicesAndValues)
			{
				auto found = NamesToCaptures.find(currName);
	
				if (found != NamesToCaptures.end())
					found->second->Reset();
			}
		}
	
	private:
	
		// Gets the chunk within which a node resides
		RegexChunk<T>* GetChunkOfNode(const RegexNodeBase<T>* Node)
		{
			for (RegexChunk<T>* CurrChunk : Chunks)
			{
				if (CurrChunk->ContainsNode(Node))
					return CurrChunk;
			}
	
			return nullptr;
		}
	
		// Reroutes around unnecessary ghosts, i.e. ones that don't signify an end or start, in order to boost performance.
		void PruneIntermediaryGhosts(RegexChunkLooseEnds<T>& ToPrune)
		{
			std::unordered_set<RegexNode<T>*> Currs, Nexts;
			std::unordered_map<RegexChunk<T>*, std::unordered_set<RegexNodeGhostIn<T>*>> InsToDelete;
			std::unordered_map<RegexChunk<T>*, std::unordered_set<RegexNodeGhostOut<T>*>> OutsToDelete;
	
			for (RegexNodeGhostIn<T>* currIn : ToPrune.Ins)
				Currs.insert(currIn->Nexts.begin(), currIn->Nexts.end());
	
			while (!Currs.empty())
			{
				for (RegexNode<T>* currNode : Currs)
				{
					std::vector<RegexNodeGhostOut<T>*> LocalDels;
					for (RegexNodeGhostOut<T>* currOut : currNode->GhostNexts)
					{
						for (RegexNodeGhostIn<T>* currIn : currOut->GhostNexts)
						{
							currNode->Nexts.insert(currIn->Nexts.begin(), currIn->Nexts.end());
							Nexts.insert(currIn->Nexts.begin(), currIn->Nexts.end());
							InsToDelete[GetChunkOfNode(currIn)].insert(currIn);
						}
	
						if (ToPrune.Outs.find(currOut) == ToPrune.Outs.end())
						{
							OutsToDelete[GetChunkOfNode(currOut)].insert(currOut);
							LocalDels.push_back(currOut);
						}
					}
					if (LocalDels.size() == currNode->GhostNexts.size())
						currNode->GhostNexts.clear();
					else
					{
						for (RegexNodeGhostOut<T>* currLocDel : LocalDels)
							currNode->GhostNexts.erase(currLocDel);
					}
				}
				Currs = Nexts;
				Nexts.clear();
			}
	
			return;
		}
	};
	
	/*
		Takes a set of regex instructions in postfix format and constructs an automaton from them.
	*/
	template<typename T>
	class RegexAssembler
	{
	public:
		using HookFuncType = std::function<void(RegexRangeIterator<T>&)>;
		using FuncMapType = std::unordered_map<std::basic_string<T>, HookFuncType>;
	
		static void AssembleAutomaton(std::vector<RegexInstruction<T>>& Instructions, Regex<T>& Automaton, FuncMapType* Funcs)
		{
			std::vector<RegexChunkLooseEnds<T>> ChunkStack;
	
			std::unordered_map<RegexBackreferenceNode<T>*, int> ToConnect_Backs_Numbered;
			std::unordered_map<RegexBackreferenceNode<T>*, std::basic_string<T>> ToConnect_Backs_Named;
	
			std::unordered_map<RegexSubroutineNode<T>*, int> ToConnect_Subs_Numbered;
			std::unordered_map<RegexSubroutineNode<T>*, std::basic_string<T>> ToConnect_Subs_Named;
	
			std::unordered_set<RegexRecursionNode<T>*> RecursionNodes;
	
			std::unordered_map<RegexCaptureNode<T>*, int> ToConnect_Caps_Numbered;
			std::unordered_map<RegexCaptureNode<T>*, std::basic_string<T>> ToConnect_Caps_Named;
	
			Regex<T>::CollapsePacket packet
			{
				ToConnect_Backs_Numbered,
				ToConnect_Backs_Named,
				ToConnect_Subs_Numbered,
				ToConnect_Subs_Named,
				RecursionNodes,
				ToConnect_Caps_Numbered,
				ToConnect_Caps_Named
			};
	
			{
				int SymbReserve = 0, CCReserve = 0, TickerReserve = 0;
	
				for (RegexInstruction<T>& currInstruction : Instructions)
				{
					RegexInstructionType& currType = currInstruction.InstructionType;
	
					if (currType == RegexInstructionType::MakeCharClassSymbol ||
						currType == RegexInstructionType::MakeCharClassLigatureSymbol)
						++SymbReserve;
					else if (currType == RegexInstructionType::MakeLiteralCharClass ||
						currType == RegexInstructionType::MakeUnitedCharClass ||
						currType == RegexInstructionType::MakeSubtractedCharClass ||
						currType == RegexInstructionType::MakeIntersectedCharClass)
						++CCReserve;
					else if (currType == RegexInstructionType::Repeat ||
						currType == RegexInstructionType::RepeatLazy)
						++TickerReserve;
				}
	
				Automaton.CharClassSymbols.reserve(SymbReserve);
				Automaton.CharacterClasses.reserve(CCReserve);
				Automaton.Tickers.reserve(TickerReserve);
			}
	
			for (RegexInstruction<T>& currInstruction : Instructions)
			{
				std::vector<std::basic_string<T>>& Data = currInstruction.InstructionData;
	
				switch (currInstruction.InstructionType)
				{
				// std::string manual, std::string[] names (if empty then will just be reffed by index)
				case RegexInstructionType::MakeCapture:
					Automaton.Captures.push_back(new RegexCapture<T>());
					Automaton.Captures.back()->Manual = Data[0] == "t";
					if (!Data.empty()) // named
					{
						for (unsigned int i = 1; i < Data.size(); ++i)
						{
							std::basic_string<T>& currName = Data[i];
							if (Automaton.NamesToCaptures.find(currName) == Automaton.NamesToCaptures.end())
								Automaton.NamesToCaptures[currName] = Automaton.Captures.back();
						}
					}
					break;
	
				// std::string manual, std::string[] names (if empty then will just be reffed by index)
				case RegexInstructionType::MakeCaptureCollection:
					Automaton.Captures.push_back(new RegexCaptureCollection<T>());
					Automaton.Captures.back()->Manual = Data[0] == "t";
					if (!Data.empty()) // named
					{
						for (unsigned int i = 1; i < Data.size(); ++i)
						{
							std::basic_string<T>& currName = Data[i];
							if (Automaton.NamesToCaptures.find(currName) == Automaton.NamesToCaptures.end())
								Automaton.NamesToCaptures[currName] = Automaton.Captures.back();
						}
					}
					break;
	
	
				// std::string minChar, std::string maxChar
				case RegexInstructionType::MakeCharClassSymbol:
					{
						T& min = Data[0][0];
						T& max = Data[1][0];
						Automaton.CharClassSymbols.push_back(new RegexCharacterClassSymbol<T>(min, max));
					}
					break;
	
				// std::string[] charsInvolved
				case RegexInstructionType::MakeCharClassLigatureSymbol:
					{
						std::vector<T> ligChars;
						for (std::basic_string<T>& currChar : Data)
							ligChars.push_back(currChar[0]);
						Automaton.CharClassSymbols.push_back(new RegexCharacterClassSymbol<T>(ligChars));
					}
					break;
	
	
				// std::string negated, std::string caseinsensitive, std::string[] inds
				case RegexInstructionType::MakeLiteralCharClass:
					{
						bool Neg = Data[0] == "t";
						std::vector<RegexCharacterClassSymbol<T>*> Symbs;
						for (unsigned int i = 2; i < Data.size(); ++i)
							Symbs.push_back(Automaton.CharClassSymbols[std::stoi(Data[i])]);
	
						Automaton.CharacterClasses.push_back(new RegexCharacterClass<T>(Symbs, Neg, Data[1] == "t"));
					}
					break;
	
	
				// std::string lhsInd, std::string rhsInd
				case RegexInstructionType::MakeUnitedCharClass:
					{
						RegexCharacterClassBase<T>* lhs = Automaton.CharacterClasses[std::stoi(Data[0])];
						RegexCharacterClassBase<T>* rhs = Automaton.CharacterClasses[std::stoi(Data[1])];
						Automaton.CharacterClasses.push_back(new RegexUnionCharacterClass<T>(lhs, rhs));
					}
					break;
	
				// std::string lhsInd, std::string rhsInd
				case RegexInstructionType::MakeSubtractedCharClass:
					{
						RegexCharacterClassBase<T>* lhs = Automaton.CharacterClasses[std::stoi(Data[0])];
						RegexCharacterClassBase<T>* rhs = Automaton.CharacterClasses[std::stoi(Data[1])];
						Automaton.CharacterClasses.push_back(new RegexSubtractCharacterClass<T>(lhs, rhs));
					}
					break;
	
				// std::string lhsInd, std::string rhsInd
				case RegexInstructionType::MakeIntersectedCharClass:
					{
						RegexCharacterClassBase<T>* lhs = Automaton.CharacterClasses[std::stoi(Data[0])];
						RegexCharacterClassBase<T>* rhs = Automaton.CharacterClasses[std::stoi(Data[1])];
						Automaton.CharacterClasses.push_back(new RegexIntersectCharacterClass<T>(lhs, rhs));
					}
					break;
	
	
				// std::string[] characterclassindices
				case RegexInstructionType::Literal:
					{
						std::unordered_set<RegexCharacterClassBase<T>*> CCs;
						for (std::basic_string<T>& currCC : Data)
							CCs.insert(Automaton.CharacterClasses[std::stoi(currCC)]);
	
						RegexChunk<T>* NewChunk = RegexChunk<T>::Literal(CCs);
						ChunkStack.push_back(Automaton.GetLooseEnds(NewChunk));
						Automaton.Chunks.insert(NewChunk);
					}
					break;
	
				// std::string IsExclusive, std::string IsLastMatchEnd, std::string LineCharsInd
				case RegexInstructionType::StartCheck:
					{
						bool Exclusive = Data[0] == "t";
						bool LastMatchEnd = Data[1] == "t";
						RegexCharacterClassBase<T>* LineChars = Automaton.CharacterClasses[std::stoi(Data[2])];
	
						RegexAtBeginningNode<T>* NewNode = new RegexAtBeginningNode<T>(LineChars, Exclusive);
	
						if (LastMatchEnd)
							NewNode->LastMatchEnd = &Automaton.LastMatchEnd;
	
						RegexChunk<T>* NewChunk = RegexChunk<T>::Wrap(NewNode);
						ChunkStack.push_back(Automaton.GetLooseEnds(NewChunk));
						Automaton.Chunks.insert(NewChunk);
	
						Automaton.StartsWithLineCheck = true;
					}
					break;
	
				// std::string IsExclusive, std::string BeforeLastNewline, std::string LineCharsInd
				case RegexInstructionType::EndCheck:
					{
						bool Exclusive = Data[0] == "t";
						bool BeforeLastNL = Data[1] == "t";
						RegexCharacterClassBase<T>* LineChars = Automaton.CharacterClasses[std::stoi(Data[2])];
	
						RegexAtEndNode<T>* NewNode = new RegexAtEndNode<T>(LineChars, Exclusive, BeforeLastNL);
	
						RegexChunk<T>* NewChunk = RegexChunk<T>::Wrap(NewNode);
						ChunkStack.push_back(Automaton.GetLooseEnds(NewChunk));
						Automaton.Chunks.insert(NewChunk);
	
						Automaton.EndsWithLineCheck = true;
					}
					break;
	
	
				// std::string negated, std::string WordCharsInd
				case RegexInstructionType::WordBoundary:
					{
						bool Neg = Data[0] == "t";
						RegexCharacterClassBase<T>* WordChars = Automaton.CharacterClasses[std::stoi(Data[1])];
	
						RegexWordBoundaryNode<T>* NewNode = new RegexWordBoundaryNode<T>(WordChars, Neg);
	
						RegexChunk<T>* NewChunk = RegexChunk<T>::Wrap(NewNode);
						ChunkStack.push_back(Automaton.GetLooseEnds(NewChunk));
						Automaton.Chunks.insert(NewChunk);
	
						if (&*(Instructions.end() - 1) == &currInstruction || &*(Instructions.end() - 2) == &currInstruction)
							Automaton.EndsWithLineCheck = true;
					}
					break;
	
	
				// std::string capturenumber
				case RegexInstructionType::Backref_Numbered:
					{
						RegexBackreferenceNode<T>* NewNode = new RegexBackreferenceNode<T>(Data[0]);
	
						ToConnect_Backs_Numbered[NewNode] = std::stoi(Data[0]);
	
						RegexChunk<T>* NewChunk = RegexChunk<T>::Wrap(NewNode);
						ChunkStack.push_back(Automaton.GetLooseEnds(NewChunk));
						Automaton.Chunks.insert(NewChunk);
					}
					break;
	
				// std::string capturename
				case RegexInstructionType::Backref_Named:
					{
						RegexBackreferenceNode<T>* NewNode = new RegexBackreferenceNode<T>(Data[0]);
	
						ToConnect_Backs_Named[NewNode] = Data[0];
	
						RegexChunk<T>* NewChunk = RegexChunk<T>::Wrap(NewNode);
						ChunkStack.push_back(Automaton.GetLooseEnds(NewChunk));
						Automaton.Chunks.insert(NewChunk);
					}
					break;
	
	
				// std::string capturenumber, std::string maxdepth
				case RegexInstructionType::Subroutine_Numbered:
					{
						RegexSubroutineNode<T>* NewNode = new RegexSubroutineNode<T>(Data[0], std::stoi(Data[1]));
	
						ToConnect_Subs_Numbered[NewNode] = std::stoi(Data[0]);
	
						RegexChunk<T>* NewChunk = RegexChunk<T>::Wrap(NewNode);
						ChunkStack.push_back(Automaton.GetLooseEnds(NewChunk));
						Automaton.Chunks.insert(NewChunk);
					}
					break;
	
				// std::string capturename, std::string maxdepth
				case RegexInstructionType::Subroutine_Named:
					{
						RegexSubroutineNode<T>* NewNode = new RegexSubroutineNode<T>(Data[0], std::stoi(Data[1]));
	
						ToConnect_Subs_Named[NewNode] = Data[0];
	
						RegexChunk<T>* NewChunk = RegexChunk<T>::Wrap(NewNode);
						ChunkStack.push_back(Automaton.GetLooseEnds(NewChunk));
						Automaton.Chunks.insert(NewChunk);
					}
					break;
	
	
				// std::string maxdepth, std::string Lazy
				case RegexInstructionType::Recursion:
					{
						RegexRecursionNode<T>* NewNode = new RegexRecursionNode<T>(std::stoi(Data[0]));
						NewNode->LazyGroup = Data[1] == "t";
	
						RecursionNodes.insert(NewNode);
	
						RegexChunk<T>* NewChunk = RegexChunk<T>::Wrap(NewNode);
						ChunkStack.push_back(Automaton.GetLooseEnds(NewChunk));
						Automaton.Chunks.insert(NewChunk);
					}
					break;
	
	
				// std::string index, std::string Lazy. Pop one off stack, make group, put back in.
				case RegexInstructionType::CaptureGroup_Numbered:
					{
						RegexCaptureNode<T>* NewNode = new RegexCaptureNode<T>(Data[0]);
						NewNode->LazyGroup = Data[1] == "t";
	
						ToConnect_Caps_Numbered[NewNode] = std::stoi(Data[0]);
	
						RegexChunkLooseEnds<T> CaptureChunk = ChunkStack.back();
						ChunkStack.pop_back();
	
						ChunkStack.push_back(Automaton.AssembleGroup(CaptureChunk, NewNode, packet));
					}
					break;
	
				// std::string name, std::string Lazy. Pop one off stack, make group, put back in.
				case RegexInstructionType::CaptureGroup_Named:
					{
						RegexCaptureNode<T>* NewNode = new RegexCaptureNode<T>(Data[0]);
						NewNode->LazyGroup = Data[1] == "t";
	
						ToConnect_Caps_Named[NewNode] = Data[0];
	
						RegexChunkLooseEnds<T> CaptureChunk = ChunkStack.back();
						ChunkStack.pop_back();
	
						ChunkStack.push_back(Automaton.AssembleGroup(CaptureChunk, NewNode, packet));
					}
					break;
	
	
				// std::string Lazy. Pop one off stack, make group, put back in.
				case RegexInstructionType::NonCaptureGroup:
					{
						RegexGroupNode<T>* NewNode = new RegexGroupNode<T>();
						NewNode->LazyGroup = Data[0] == "t";
	
						RegexChunkLooseEnds<T> GroupChunk = ChunkStack.back();
						ChunkStack.pop_back();
	
						ChunkStack.push_back(Automaton.AssembleGroup(GroupChunk, NewNode, packet));
					}
					break;
	
	
				// std::string negated, std::string lazy. pop one off stack, make lookahead, put back in.
				case RegexInstructionType::LookAhead:
					{
						RegexLookAheadNode<T>* NewNode = new RegexLookAheadNode<T>(Data[0] == "t");
						NewNode->LazyGroup = Data[1] == "t";
	
						RegexChunkLooseEnds<T> GroupChunk = ChunkStack.back();
						ChunkStack.pop_back();
	
						ChunkStack.push_back(Automaton.AssembleGroup(GroupChunk, NewNode, packet));
					}
					break;
	
				// std::string negated, std::string lazy. pop one off stack, make lookbehind, put back in.
				case RegexInstructionType::LookBehind:
					{
						RegexLookBehindNode<T>* NewNode = new RegexLookBehindNode<T>(Data[0] == "t");
						NewNode->LazyGroup = Data[1] == "t";
	
						RegexChunkLooseEnds<T> GroupChunk = ChunkStack.back();
						ChunkStack.pop_back();
	
						ChunkStack.push_back(Automaton.AssembleGroup(GroupChunk, NewNode, packet));
					}
					break;
	
	
				// std::string name. pops one off the stack, and that's it. i.e. makes a chunk only accessible via subroutine call.
				case RegexInstructionType::DefineAsSubroutine:
					{
						RegexChunkLooseEnds<T> Popped = ChunkStack.back();
						ChunkStack.pop_back();
	
						Automaton.DefinedSubroutines.push_back(new RegexCapture<T>());
	
						Automaton.DefinedSubroutines.back()->InitialCapture = (*Popped.ChunksInvolved.begin())->Nodes[0];
	
						Automaton.NamesToCaptures[currInstruction.InstructionData[0]] = Automaton.DefinedSubroutines.back();
					}
					break;
	
	
				// std::string funcname
				case RegexInstructionType::CodeHook:
					{
						HookFuncType FoundFunc = nullptr;
						if (Funcs)
						{
							auto found = Funcs->find(Data[0]);
							if (found != Funcs->end())
								FoundFunc = found->second;
						}
	
						RegexCodeHookNode<T>* NewNode = new RegexCodeHookNode<T>(Data[0], FoundFunc);
	
						RegexChunk<T>* NewChunk = RegexChunk<T>::Wrap(NewNode);
						ChunkStack.push_back(Automaton.GetLooseEnds(NewChunk));
						Automaton.Chunks.insert(NewChunk);
					}
					break;
	
	
				// std::string NumConds, std::string Lazy. pop NumConds + 1, i.e. 2 or 3, off the top, create a conditional with them, place back in
				case RegexInstructionType::Conditional:
					{
						RegexChunk<T>* Cond = nullptr, *Then = nullptr, *Else = nullptr;
	
						if (Data[0] == "2")
						{
							Else = *ChunkStack.back().ChunksInvolved.begin();
							ChunkStack.pop_back();
							Then = *ChunkStack.back().ChunksInvolved.begin();
							ChunkStack.pop_back();
							Cond = *ChunkStack.back().ChunksInvolved.begin();
							ChunkStack.pop_back();
						}
						else
						{
							Then = *ChunkStack.back().ChunksInvolved.begin();
							ChunkStack.pop_back();
							Cond = *ChunkStack.back().ChunksInvolved.begin();
							ChunkStack.pop_back();
						}
	
						RegexConditionalNode<T>* NewNode = new RegexConditionalNode<T>(Cond, Then, Else);
						NewNode->LazyGroup = Data[1] == "t";
	
						RegexChunk<T>* NewChunk = RegexChunk<T>::Wrap(NewNode);
						ChunkStack.push_back(Automaton.GetLooseEnds(NewChunk));
						Automaton.Chunks.insert(NewChunk);
					}
					break;
	
	
				// pop one off stack, make none-or-once, put back in
				case RegexInstructionType::NOnce:
					{
						RegexChunkLooseEnds<T> Popped = ChunkStack.back();
						ChunkStack.pop_back();
	
						ChunkStack.push_back(Automaton.OccurNoneOrOnce(Popped, packet));
					}
					break;
	
				case RegexInstructionType::NOnceLazy:
					{
						RegexChunkLooseEnds<T> Popped = ChunkStack.back();
						ChunkStack.pop_back();
	
						ChunkStack.push_back(Automaton.OccurNoneOrOnce(Popped, packet, true));
					}
					break;
	
				// pop one off stack, make none-plus, put back in
				case RegexInstructionType::NPlus:
					{
						RegexChunkLooseEnds<T> Popped = ChunkStack.back();
						ChunkStack.pop_back();
	
						ChunkStack.push_back(Automaton.OccurNonePlus(Popped, packet));
					}
					break;
	
				case RegexInstructionType::NPlusLazy:
					{
						RegexChunkLooseEnds<T> Popped = ChunkStack.back();
						ChunkStack.pop_back();
	
						ChunkStack.push_back(Automaton.OccurNonePlus(Popped, packet, true));
					}
					break;
	
				// pop one off stack, make one-plus, put back in
				case RegexInstructionType::OPlus:
					{
						RegexChunkLooseEnds<T> Popped = ChunkStack.back();
						ChunkStack.pop_back();
	
						ChunkStack.push_back(Automaton.OccurOncePlus(Popped, packet));
					}
					break;
	
				case RegexInstructionType::OPlusLazy:
					{
						RegexChunkLooseEnds<T> Popped = ChunkStack.back();
						ChunkStack.pop_back();
	
						ChunkStack.push_back(Automaton.OccurOncePlus(Popped, packet, true));
					}
					break;
	
				// std::string type, std::string mintimes, std::string maxtimes. pop one off stack, make the respective repeat, put back in
				case RegexInstructionType::Repeat:
					{
						RegexChunkLooseEnds<T> Popped = ChunkStack.back();
						ChunkStack.pop_back();
	
						if (Data[0] == "Exact")
							ChunkStack.push_back(Automaton.RepeatExact(Popped, std::stoi(Data[1]), packet));
						else if (Data[0] == "Min")
							ChunkStack.push_back(Automaton.RepeatMin(Popped, std::stoi(Data[1]), packet));
						else if (Data[0] == "MinMax")
							ChunkStack.push_back(Automaton.RepeatMinMax(Popped, std::stoi(Data[1]), std::stoi(Data[2]), packet));
						else
						{
							Automaton.CompileError = "Regex Compile Error: \'" + Data[0] + "\'"
								" is not a valid type of repeat.";
							return;
						}
					}
					break;
	
				case RegexInstructionType::RepeatLazy:
					{
						RegexChunkLooseEnds<T> Popped = ChunkStack.back();
						ChunkStack.pop_back();
	
						if (Data[0] == "Exact")
							ChunkStack.push_back(Automaton.RepeatExact(Popped, std::stoi(Data[1]), packet, true));
						else if (Data[0] == "Min")
							ChunkStack.push_back(Automaton.RepeatMin(Popped, std::stoi(Data[1]), packet, true));
						else if (Data[0] == "MinMax")
							ChunkStack.push_back(Automaton.RepeatMinMax(Popped, std::stoi(Data[1]), std::stoi(Data[2]), packet, true));
						else
						{
							Automaton.CompileError = "Regex Compile Error: \'" + Data[0] + "\'"
								" is not a valid type of repeat.";
							return;
						}
					}
					break;
	
				// pop two off the top, concat them together, place back in.
				case RegexInstructionType::Concat:
					{
						RegexChunkLooseEnds<T> rhs = ChunkStack.back();
						ChunkStack.pop_back();
						RegexChunkLooseEnds<T> lhs = ChunkStack.back();
						ChunkStack.pop_back();
	
						ChunkStack.push_back(Automaton.Concat(lhs, rhs));
					}
					break;
	
				// pop two off the top, alt them together, place back in.
				case RegexInstructionType::Alternate:
					{
						RegexChunkLooseEnds<T> rhs = ChunkStack.back();
						ChunkStack.pop_back();
						RegexChunkLooseEnds<T> lhs = ChunkStack.back();
						ChunkStack.pop_back();
	
						ChunkStack.push_back(Automaton.Alternate(lhs, rhs));
					}
					break;
	
				default:
					Automaton.CompileError = "Regex Compile Error: Unrecognized Instruction.";
					return;
				}
			}
	
			if (ChunkStack.size() < 1)
			{
				Automaton.CompileError = "Regex Compile Error: No nodes constructed from given regex.";
				return;
			}


			for(auto& curr : ToConnect_Backs_Numbered)
			{
				if (curr.second >= 1 && size_t(curr.second) <= Automaton.Captures.size())
					curr.first->BoundCapture = Automaton.Captures[curr.second - 1];
			}
			
			for (auto& curr : ToConnect_Backs_Named)
			{
				auto found = Automaton.NamesToCaptures.find(curr.second);
				if (found != Automaton.NamesToCaptures.end())
					curr.first->BoundCapture = found->second;
			}
	
	
			for (auto& curr : ToConnect_Subs_Numbered)
			{
				if (curr.second >= 1 && size_t(curr.second) <= Automaton.Captures.size())
				{
					curr.first->BoundCapture = Automaton.Captures[curr.second - 1];
					if (nullptr == Automaton.Captures[curr.second - 1]->InitialCapture)
						Automaton.Captures[curr.second - 1]->InitialCapture = curr.first;
				}
			}
	
			for (auto& curr : ToConnect_Subs_Named)
			{
				auto found = Automaton.NamesToCaptures.find(curr.second);
				if (found != Automaton.NamesToCaptures.end())
				{
					curr.first->BoundCapture = found->second;
					if (nullptr == found->second->InitialCapture)
						found->second->InitialCapture = curr.first;
				}
			}
	
	
			for (auto& curr : ToConnect_Caps_Numbered)
			{
				if (curr.second >= 1 && size_t(curr.second) <= Automaton.Captures.size())
					curr.first->BoundCapture = Automaton.Captures[curr.second - 1];
			}
	
			for (auto& curr : ToConnect_Caps_Named)
			{
				auto found = Automaton.NamesToCaptures.find(curr.second);
				if (found != Automaton.NamesToCaptures.end())
					curr.first->BoundCapture = found->second;
			}


			// Collapse and prune the end result
			RegexChunkLooseEnds<T> Final = Automaton.Collapse(ChunkStack[0], packet);
			Automaton.PruneIntermediaryGhosts(Final);
	
			for (RegexRecursionNode<T>* curr : RecursionNodes)
			{
				curr->Ins = Final.Ins;
				curr->Outs = Final.Outs;
			}
	
			Automaton.StartNodes = Final.Ins;
			Automaton.EndNodes = Final.Outs;
		}
	};
	
	template<typename T>
	template<typename TranslatorType, typename AssemblerType>
	Regex<T>::Regex(std::basic_string<T> c, FuncMapType* Funcs, std::vector<RegexInstruction<T>>* OutInstructions,
		int MaxNestingDepth, RegexRangeIterator<T>* PresetLastMatchEnd)
	{
		std::vector<RegexInstruction<T>> PostfixInstructions = TranslatorType::Translate(c, CompileError, MaxNestingDepth);
	
		if (CompileError.empty())
		{
			if (OutInstructions)
				*OutInstructions = PostfixInstructions;
		
			AssemblerType::AssembleAutomaton(PostfixInstructions, *this, Funcs);
		}
	
		// Since we're done constructing, we don't need this data anymore
		for (RegexChunk<T>* currChunk : Chunks)
			currChunk->ConnectedTos.clear();
	
		if (PresetLastMatchEnd)
			LastMatchEnd = *PresetLastMatchEnd;
	}
	
	template<typename T>
	Regex<T>::Regex(std::vector<RegexInstruction<T>>& Instructions, FuncMapType* Funcs)
	{
		if (Instructions.empty())
		{
			CompileError = "Regex Compile Error: No instructions given. This may be caused by load-from-file failing.";
			return;
		}
	
		RegexAssembler<T>::AssembleAutomaton(Instructions, *this, Funcs);
	
		// Since we're done constructing, we don't need this data anymore
		for (RegexChunk<T>* currChunk : Chunks)
			currChunk->ConnectedTos.clear();
	}

	template<typename T> inline bool operator==(Evex::Regex<T>& lhs, std::string& rhs) { return lhs.Match(rhs); }
	template<typename T> inline bool operator==(Evex::Regex<T>& lhs, const T* rhs) { return lhs.Match(rhs); }

	template<typename T> inline bool operator==(std::string& lhs, Evex::Regex<T>& rhs) { return rhs.Match(lhs); }
	template<typename T> inline bool operator==(const T* lhs, Evex::Regex<T>& rhs) { return rhs.Match(lhs); }

	template<typename T> inline bool operator!=(Evex::Regex<T>& lhs, std::string& rhs) { return !lhs.Match(rhs); }
	template<typename T> inline bool operator!=(Evex::Regex<T>& lhs, const T* rhs) { return !lhs.Match(rhs); }

	template<typename T> inline bool operator!=(std::string& lhs, Evex::Regex<T>& rhs) { return !rhs.Match(lhs); }
	template<typename T> inline bool operator!=(const T* lhs, Evex::Regex<T>& rhs) { return !rhs.Match(lhs); }
}
