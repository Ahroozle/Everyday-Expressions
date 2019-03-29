#pragma once

#include "EvexCharacterClass.h"

#include <unordered_set>
#include <unordered_map>
#include <sstream>


namespace Evex
{
	template<typename T> struct RegexNode;

	template<typename T>
	struct RegexNodeBase
	{
		unsigned int Index = -1;

		using IterType = RegexRangeIterator<T>;
		using StringType = std::basic_string<T>;

		// Used to determine whether or not a node can be collapsed into another node
		inline virtual bool SimilarTo(const RegexNodeBase* o) const = 0;

		// Used to merge the nexts of another node with this one during a collapse.
		inline virtual void Incorporate(const RegexNodeBase* o) = 0;

		// Can this node be entered with the given input data?
		inline virtual bool CanEnter(IterType& Input, std::vector<RegexNode<T>*>* Outers = nullptr) = 0;

		// Gets all non-ghost nodes in line to be the next destination during traversal.
		inline virtual std::vector<RegexNode<T>*> GetNexts() = 0;

		virtual StringType Draw(std::unordered_map<StringType, int>& TypeNumbers,
			StringType& OutStr,
			const std::unordered_set<RegexNodeBase<T>*> Ends,
			std::unordered_map<RegexNodeBase<T>*, StringType>& NodeNames,
			StringType Indent) = 0;
	};

	template<typename T> struct RegexNodeGhostOut;
	template<typename T> struct RegexGroupNode;

	template<typename T>
	struct RegexNode : public RegexNodeBase<T>
	{
		std::unordered_set<RegexNode*> Nexts;

		std::unordered_set<RegexNodeGhostOut<T>*> GhostNexts;

		// The conditions against which incoming inputs are compared to.
		std::unordered_set<RegexCharacterClassBase<T>*> Comparators;

		RegexNode() {}
		RegexNode(const std::unordered_set<RegexCharacterClassBase<T>*> inComps) : Comparators(inComps) {}

		inline virtual RegexNode* Clone()
		{
			RegexNode* Out = new RegexNode(Comparators);
			return Out;
		}

		inline bool SimilarTo(const RegexNodeBase<T>* o) const override
		{
			const RegexNode* AsType = dynamic_cast<const RegexNode*>(o);

			if (AsType)
			{
				for (RegexCharacterClassBase<T>* currCClass : AsType->Comparators)
				{
					if (Comparators.find(currCClass) == Comparators.end())
						return false;
				}

				return nullptr == dynamic_cast<const RegexGroupNode<T>*>(o);//o->SimilarTo(this);
			}

			return false;
		}

		inline void Incorporate(const RegexNodeBase* o) final
		{
			const RegexNode* AsType = dynamic_cast<const RegexNode*>(o);

			if (AsType)
			{
				Nexts.insert(AsType->Nexts.begin(), AsType->Nexts.end());
				GhostNexts.insert(AsType->GhostNexts.begin(), AsType->GhostNexts.end());
			}
		}

		inline bool CanEnter(IterType& Input, std::vector<RegexNode<T>*>* Outers = nullptr)
		{
			for (RegexCharacterClassBase<T>* curr : Comparators)
			{
				if (!curr->Matches(Input))
					return false;
			}

			return true;
		}

		inline std::vector<RegexNode<T>*> GetNexts();

		void DrawNexts(std::unordered_map<StringType, int>& TypeNumbers,
			StringType& OutStr,
			const std::unordered_set<RegexNodeBase<T>*> Ends,
			std::unordered_map<RegexNodeBase<T>*, StringType>& NodeNames,
			StringType Indent,
			StringType& MyName)
		{
			for (RegexNode* currNext : Nexts)
			{
				std::unordered_map<RegexNodeBase<T>*, StringType>::iterator Found = NodeNames.find(currNext);
				if (Found != NodeNames.end())
					OutStr += Indent + MyName + " -> " + Found->second + '\n';
				else
					OutStr += Indent + MyName + " -> " + currNext->Draw(TypeNumbers, OutStr, Ends, NodeNames, Indent) + '\n';
			}

			for (RegexNodeGhostOut<T>* currGhostNext : GhostNexts)
			{
				std::unordered_map<RegexNodeBase<T>*, StringType>::iterator Found = NodeNames.find(currGhostNext);
				if (Found != NodeNames.end())
					OutStr += Indent + MyName + " -> " + Found->second + '\n';
				else
					OutStr += Indent + MyName + " -> " + currGhostNext->Draw(TypeNumbers, OutStr, Ends, NodeNames, Indent) + '\n';
			}
		}

		StringType Draw(std::unordered_map<StringType, int>& TypeNumbers,
			StringType& OutStr,
			const std::unordered_set<RegexNodeBase<T>*> Ends,
			std::unordered_map<RegexNodeBase<T>*, StringType>& NodeNames,
			StringType Indent) override
		{
			StringType MyName;
			{
				std::basic_ostringstream<T> oss;
				oss << "Node_" << TypeNumbers["Node"]++;
				MyName = oss.str();
				oss << "[label=\"";
				for (RegexCharacterClassBase<T>* currCC : Comparators)
					oss << "[" << currCC->WrittenForm() << "]\\n";
				oss << "\"]\n";
				OutStr += Indent + oss.str();
			}

			for (RegexNode* currNext : Nexts)
			{
				std::unordered_map<RegexNodeBase<T>*, StringType>::iterator Found = NodeNames.find(currNext);
				if (Found != NodeNames.end())
					OutStr += Indent + MyName + " -> " + Found->second + '\n';
				else
					OutStr += Indent + MyName + " -> " + currNext->Draw(TypeNumbers, OutStr, Ends, NodeNames, Indent) + '\n';
			}

			for (RegexNodeGhostOut<T>* currGhostNext : GhostNexts)
			{
				std::unordered_map<RegexNodeBase<T>*, StringType>::iterator Found = NodeNames.find(currGhostNext);
				if (Found != NodeNames.end())
					OutStr += Indent + MyName + " -> " + Found->second + '\n';
				else
					OutStr += Indent + MyName + " -> " + currGhostNext->Draw(TypeNumbers, OutStr, Ends, NodeNames, Indent) + '\n';
			}

			NodeNames[this] = MyName;
			return MyName;
		}
	};

	template<typename T>
	struct RegexNodeGhostIn : public RegexNodeBase<T>
	{
		std::unordered_set<RegexNode<T>*> Nexts;

		inline bool SimilarTo(const RegexNodeBase<T>* o) const final { return nullptr != dynamic_cast<const RegexNodeGhostIn*>(o); }

		inline void Incorporate(const RegexNodeBase* o) final
		{
			const RegexNodeGhostIn* AsType = dynamic_cast<const RegexNodeGhostIn*>(o);

			if (AsType)
				Nexts.insert(AsType->Nexts.begin(), AsType->Nexts.end());
		}

		inline bool CanEnter(IterType& Input, std::vector<RegexNode<T>*>* Outers = nullptr) final { return true; }
		inline std::vector<RegexNode<T>*> GetNexts() final;

		StringType Draw(std::unordered_map<StringType, int>& TypeNumbers,
			StringType& OutStr,
			const std::unordered_set<RegexNodeBase<T>*> Ends,
			std::unordered_map<RegexNodeBase<T>*, StringType>& NodeNames,
			StringType Indent) final
		{
			StringType MyName;
			{
				std::basic_ostringstream<T> oss;
				oss << "GhostIn_" << TypeNumbers["GhostIn"]++;
				MyName = oss.str();

				StringType Style = "[style=dashed,label=\"\"]";
				if (NodeNames.find(this) != NodeNames.end())
					Style = "[style=dotted,label=\"\"]";

				OutStr += Indent + MyName + Style + '\n';
			}

			for (RegexNode<T>* currNext : Nexts)
			{
				std::unordered_map<RegexNodeBase<T>*, StringType>::iterator Found = NodeNames.find(currNext);
				if (Found != NodeNames.end())
					OutStr += Indent + MyName + " -> " + Found->second + '\n';
				else
					OutStr += Indent + MyName + " -> " + currNext->Draw(TypeNumbers, OutStr, Ends, NodeNames, Indent) + '\n';
			}

			NodeNames[this] = MyName;
			return MyName;
		}
	};

	template<typename T>
	struct RegexNodeGhostOut : public RegexNodeBase<T>
	{
		std::unordered_set<RegexNodeGhostIn<T>*> GhostNexts;

		inline bool SimilarTo(const RegexNodeBase<T>* o) const final { return nullptr != dynamic_cast<const RegexNodeGhostOut*>(o); }

		inline void Incorporate(const RegexNodeBase* o) final
		{
			const RegexNodeGhostOut* AsType = dynamic_cast<const RegexNodeGhostOut*>(o);

			if (AsType)
				GhostNexts.insert(AsType->GhostNexts.begin(), AsType->GhostNexts.end());
		}

		inline bool CanEnter(IterType& Input, std::vector<RegexNode<T>*>* Outers = nullptr) final { return true; }

		inline std::vector<RegexNode<T>*> GetNexts() final
		{
			std::unordered_set<RegexNode<T>*> Out;
			for (RegexNodeGhostIn<T>* currNext : GhostNexts)
			{
				std::vector<RegexNode<T>*> NextNexts = currNext->GetNexts();
				Out.insert(NextNexts.begin(), NextNexts.end());
			}

			std::vector<RegexNode<T>*> FinalOut;
			FinalOut.insert(FinalOut.end(), Out.begin(), Out.end());

			return FinalOut;
		}

		StringType Draw(std::unordered_map<StringType, int>& TypeNumbers,
			StringType& OutStr,
			const std::unordered_set<RegexNodeBase<T>*> Ends,
			std::unordered_map<RegexNodeBase<T>*, StringType>& NodeNames,
			StringType Indent) final
		{
			StringType MyName;
			{
				std::basic_ostringstream<T> oss;
				oss << "GhostOut_" << TypeNumbers["GhostOut"]++;
				MyName = oss.str();

				StringType style = "[style=dashed,label=\"\"]";
				if (Ends.find(this) != Ends.end())
					style = "[shape=doublecircle,style=dashed,label=\"\"]";

				OutStr += Indent + MyName + style + '\n';
			}

			for (RegexNodeGhostIn<T>* currGhostNext : GhostNexts)
			{
				std::unordered_map<RegexNodeBase<T>*, StringType>::iterator Found = NodeNames.find(currGhostNext);
				if (Found != NodeNames.end())
					OutStr += Indent + MyName + " -> " + Found->second + '\n';
				else
					OutStr += Indent + MyName + " -> " + currGhostNext->Draw(TypeNumbers, OutStr, Ends, NodeNames, Indent) + '\n';
			}

			NodeNames[this] = MyName;
			return MyName;
		}
	};

	template<typename T>
	inline std::vector<RegexNode<T>*> RegexNode<T>::GetNexts()
	{
		std::unordered_set<RegexNode<T>*> Out;
		for (RegexNode<T>* currNext : Nexts)
			Out.insert(currNext);

		for (RegexNodeGhostOut<T>* currGhostNext : GhostNexts)
		{
			std::vector<RegexNode<T>*> Throughs = currGhostNext->GetNexts();
			Out.insert(Throughs.begin(), Throughs.end());
		}

		std::vector<RegexNode<T>*> FinalOut;
		FinalOut.insert(FinalOut.end(), Out.begin(), Out.end());

		return FinalOut;
	}

	template<typename T>
	inline std::vector<RegexNode<T>*> RegexNodeGhostIn<T>::GetNexts()
	{
		std::vector<RegexNode<T>*> FinalOut;
		FinalOut.insert(FinalOut.end(), Nexts.begin(), Nexts.end());

		return FinalOut;
	}
}
