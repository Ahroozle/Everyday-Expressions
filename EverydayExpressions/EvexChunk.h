#pragma once

#include "EvexNode.h"


namespace Evex
{
	template<typename T>
	struct RegexChunk
	{
		std::vector<RegexNode<T>*> Nodes;
		std::vector<RegexNodeGhostIn<T>> Ins;
		std::vector<RegexNodeGhostOut<T>> Outs;

		~RegexChunk() { for (RegexNode<T>* curr : Nodes) delete curr; }

		std::unordered_set<RegexChunk*> ConnectedTos;

		using IterType = RegexRangeIterator<T>;
		static bool Match(const IterType& Input,
			std::unordered_set<RegexNodeGhostIn<T>*>& Ins,
			std::unordered_set<RegexNodeGhostOut<T>*> Outs,
			bool Lazy,
			IterType& OutMatchEnd,
			std::vector<RegexNode<T>*>* Outers = nullptr,
			bool IterateReverse = false)
		{
			OutMatchEnd = Input;

			std::vector<RegexNode<T>*> CurrentNexts;

			for (RegexNodeGhostIn<T>* currIn : Ins)
			{
				std::vector<RegexNode<T>*> NextReals = currIn->GetNexts();
				CurrentNexts.insert(CurrentNexts.end(), NextReals.begin(), NextReals.end());
			}

			RegexNode<T>* CurrNode = nullptr;
			IterType LastMatch = OutMatchEnd;
			while (CurrentNexts.size() > 0 && !(IterateReverse ? OutMatchEnd.IsPreBegin() : OutMatchEnd.IsEnd()))
			{
				bool NewNodeFound = false;
				for (RegexNode<T>* currNext : CurrentNexts)
				{
					if (currNext->CanEnter(OutMatchEnd, Outers))
					{
						CurrNode = currNext;
						NewNodeFound = true;
						break;
					}
				}

				if (!NewNodeFound)
					break;

				CurrentNexts = CurrNode->GetNexts();

				for (RegexNodeGhostOut<T>* currGhost : CurrNode->GhostNexts)
				{
					if (Outs.find(currGhost) != Outs.end())
					{
						if (Lazy)
							return true;
						else
							LastMatch = OutMatchEnd;
						break;
					}
				}

				if (IterateReverse)
					--OutMatchEnd;
				else
					++OutMatchEnd;
			}

			if (IterateReverse)
				++OutMatchEnd;
			else
				--OutMatchEnd;

			if (CurrNode)
			{
				for (RegexNodeGhostOut<T>* CurrOut : CurrNode->GhostNexts)
				{
					if (Outs.find(CurrOut) != Outs.end())
						return true;
				}
			}

			if (LastMatch != Input)
			{
				OutMatchEnd = LastMatch;
				return true;
			}

			return false;
		}

		using StringType = std::basic_string<T>;
		static void Draw(std::unordered_set<RegexNodeGhostIn<T>*>& Ins,
			std::unordered_map<StringType, int>& TypeNumbers,
			StringType& OutStr,
			const std::unordered_set<RegexNodeBase<T>*> Ends,
			std::unordered_map<RegexNodeBase<T>*, StringType>& NodeNames,
			StringType Indent,
			StringType& MyName)
		{
			std::basic_ostringstream<T> oss;
			oss << TypeNumbers["Cluster"]++;
			StringType MetaChunkName = "cluster_" + oss.str();

			OutStr += Indent + "subgraph " + MetaChunkName + "\n" +
				Indent + "{\n" +
				Indent + "\tlabel=\"" + MyName + "\"\n" +
				Indent + "\tstyle=filled\n" +
				Indent + "\tfillcolor=lightgrey\n";

			for (RegexNodeGhostIn<T>* currIn : Ins)
				currIn->Draw(TypeNumbers, OutStr, Ends, NodeNames, Indent + '\t');

			OutStr += Indent + "}\n";
		}

		static void Draw(std::vector<RegexNodeGhostIn<T>>& Ins,
			std::unordered_map<StringType, int>& TypeNumbers,
			StringType& OutStr,
			const std::unordered_set<RegexNodeBase<T>*> Ends,
			std::unordered_map<RegexNodeBase<T>*, StringType>& NodeNames,
			StringType Indent,
			StringType& MyName)
		{
			std::basic_ostringstream<T> oss;
			oss << TypeNumbers["Cluster"]++;
			StringType MetaChunkName = "cluster_" + oss.str();

			OutStr += Indent + "subgraph " + MetaChunkName + "\n" +
				Indent + "{\n" +
				Indent + "\tlabel=\"" + MyName + "\"\n" +
				Indent + "\tstyle=filled\n" +
				Indent + "\tfillcolor=lightgrey\n";

			for (RegexNodeGhostIn<T>& currIn : Ins)
				currIn.Draw(TypeNumbers, OutStr, Ends, NodeNames, Indent + '\t');

			OutStr += Indent + "}\n";
		}

		bool ContainsNode(const RegexNodeBase<T>* Node)
		{
			if (const RegexNode<T>* AsNode = dynamic_cast<const RegexNode<T>*>(Node))
				return Node->Index >= 0 && Node->Index < Nodes.size() && Nodes[Node->Index] == AsNode;
			else if (const RegexNodeGhostIn<T>* AsGhostIn = dynamic_cast<const RegexNodeGhostIn<T>*>(Node))
				return Node->Index >= 0 && Node->Index < Ins.size() && &Ins[Node->Index] == AsGhostIn;
			else if (const RegexNodeGhostOut<T>* AsGhostOut = dynamic_cast<const RegexNodeGhostOut<T>*>(Node))
				return Node->Index >= 0 && Node->Index < Outs.size() && &Outs[Node->Index] == AsGhostOut;

			return false;
		}

		RegexChunk* Clone()
		{
			RegexChunk* Out = new RegexChunk();
			Out->Ins = Ins;
			Out->Outs = Outs;
			for (RegexNode<T>* currNode : Nodes)
				Out->Nodes.push_back(currNode->Clone());
			return Out;
		}

		// "a", "a-z", ".", etc.
		static inline RegexChunk* Literal(const std::unordered_set<RegexCharacterClassBase<T>*>& CharClasses)
		{
			RegexChunk* Out = new RegexChunk();

			Out->Nodes.push_back(new RegexNode<T>(CharClasses));

			Out->Ins.push_back(RegexNodeGhostIn<T>());
			Out->Outs.push_back(RegexNodeGhostOut<T>());

			Out->Ins[0].Nexts.insert(Out->Nodes[0]);
			Out->Nodes[0]->GhostNexts.insert(&Out->Outs[0]);

			Out->Nodes[0]->Index = 0;
			Out->Ins[0].Index = 0;
			Out->Outs[0].Index = 0;

			return Out;
		}

		// wrapping a lone node, usually a group node, with a chunk
		static inline RegexChunk* Wrap(RegexNode<T>* Node)
		{
			RegexChunk* Out = new RegexChunk();

			Out->Nodes.push_back(Node);
			Out->Ins.push_back(RegexNodeGhostIn<T>());
			Out->Outs.push_back(RegexNodeGhostOut<T>());

			Out->Ins[0].Nexts.insert(Out->Nodes[0]);
			Out->Nodes[0]->GhostNexts.insert(&Out->Outs[0]);

			Out->Nodes[0]->Index = 0;
			Out->Ins[0].Index = 0;
			Out->Outs[0].Index = 0;

			return Out;
		}
	};
}
