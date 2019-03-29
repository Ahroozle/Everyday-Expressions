#pragma once

#include "EvexChunk.h"

#include <exception>
#include <functional>


namespace Evex
{
	/*
		Non-capturing group node. Moves Input(!) forward on success.
	*/
	template<typename T>
	struct RegexGroupNode : public RegexNode<T>
	{
		std::vector<RegexChunk<T>*> Chunks;
		std::unordered_set<RegexNodeGhostIn<T>*> Ins;
		std::unordered_set<RegexNodeGhostOut<T>*> Outs;
	
		bool LazyGroup = false;
	
		inline virtual RegexNode* Clone()
		{
			RegexGroupNode* Out = new RegexGroupNode();
			Out->Chunks = Chunks;
			Out->Ins = Ins;
			Out->Outs = Outs;
			Out->LazyGroup = LazyGroup;
			return Out;
		}
	
		// Groups should never be similar to other groups.
		inline bool SimilarTo(const RegexNodeBase<T>* o) const { return false; }
	
		inline bool CanEnter(IterType& Input, std::vector<RegexNode<T>*>* Outers = nullptr)
		{
			IterType Copy;
	
			std::vector<RegexNode<T>*> AppendOuters;
			if (nullptr != Outers)
				AppendOuters = *Outers;
			AppendOuters.push_back(this);
			if (RegexChunk<T>::Match(Input, Ins, Outs, LazyGroup, Copy, &AppendOuters))
			{
				Input = Copy;
				return true;
			}
	
			return false;
		}
	
		StringType Draw(std::unordered_map<StringType, int>& TypeNumbers,
						 StringType& OutStr,
						 const std::unordered_set<RegexNodeBase<T>*> Ends,
						 std::unordered_map<RegexNodeBase<T>*, StringType>& NodeNames,
						 StringType Indent)
		{
			StringType MyName;
			{
				std::basic_ostringstream<T> oss;
				oss << "NonCapturingGroup_" << TypeNumbers["NonCapturingGroup"]++;
				MyName = oss.str();
	
				OutStr += Indent + MyName + "[label=\"" + MyName + "\"]" + '\n';
			}
	
			RegexChunk<T>::Draw(Ins, TypeNumbers, OutStr, Ends, NodeNames, Indent, MyName);
			DrawNexts(TypeNumbers, OutStr, Ends, NodeNames, Indent, MyName);
	
			NodeNames[this] = MyName;
			return MyName;
		}
	};
	
	
	// Lookaround nodes
	
	/*
		Marches forward to determine whether the given capture is met later in the string.
		Does not modify Input.
	*/
	template<typename T>
	struct RegexLookAheadNode : public RegexGroupNode<T>
	{
		bool Negative = false;
	
		RegexLookAheadNode(bool Negate) : Negative(Negate) {}
		inline virtual RegexNode* Clone()
		{
			RegexLookAheadNode* Out = new RegexLookAheadNode(Negative);
			Out->Chunks = Chunks;
			Out->Ins = Ins;
			Out->Outs = Outs;
			Out->LazyGroup = LazyGroup;
			return Out;
		}
	
		// lookaheads should never be similar to other lookaheads
		inline bool SimilarTo(const RegexNodeBase<T>* o) const final { return false; }
	
		inline bool CanEnter(IterType& Input, std::vector<RegexNode<T>*>* Outers = nullptr) final
		{
			IterType Copy;
	
			bool Success = RegexChunk<T>::Match(Input, Ins, Outs, LazyGroup, Copy);
	
			return (Negative ? !Success : Success);
		}
	
		StringType Draw(std::unordered_map<StringType, int>& TypeNumbers,
						 StringType& OutStr,
						 const std::unordered_set<RegexNodeBase<T>*> Ends,
						 std::unordered_map<RegexNodeBase<T>*, StringType>& NodeNames,
						 StringType Indent)
		{
			StringType MyName;
			{
				std::basic_ostringstream<T> oss;
				std::basic_string<T> MyType = (Negative ? "NegativeLookAhead" : "PositiveLookAhead");
				oss << MyType <<"_" << TypeNumbers[MyType]++;
				MyName = oss.str();
	
				OutStr += Indent + MyName + '\n';
			}
	
			RegexChunk<T>::Draw(Ins, TypeNumbers, OutStr, Ends, NodeNames, Indent, MyName);
			DrawNexts(TypeNumbers, OutStr, Ends, NodeNames, Indent, MyName);
	
			NodeNames[this] = MyName;
			return MyName;
		}
	};
	
	/*
		Essentially a RegexLookAheadNode but everything is backwards.
		i.e. The participating chunks are constructed backwards, and the iterator is moved backwards.
		Does not modify Input.
	*/
	template<typename T>
	struct RegexLookBehindNode : public RegexGroupNode<T>
	{
		bool Negative = false;
	
		RegexLookBehindNode(bool Negate) : Negative(Negate) {}
	
		inline virtual RegexNode* Clone()
		{
			RegexLookBehindNode* Out = new RegexLookBehindNode(Negative);
			Out->Chunks = Chunks;
			Out->Ins = Ins;
			Out->Outs = Outs;
			Out->LazyGroup = LazyGroup;
			return Out;
		}
	
		// lookbehinds should never be similar to other lookbehinds
		inline bool SimilarTo(const RegexNodeBase<T>* o) const final { return false; }
	
		inline bool CanEnter(IterType& Input, std::vector<RegexNode<T>*>* Outers = nullptr) final
		{
			IterType InputBackOne = Input, Copy;
			--InputBackOne;
	
			std::vector<RegexNode<T>*> DummyOuters;
			bool Success = RegexChunk<T>::Match(InputBackOne, Ins, Outs, LazyGroup, Copy, &DummyOuters, true);
	
			Success = (Negative ? !Success : Success);
	
			if (Success)
				--Input;
	
			return Success;
		}
	
		StringType Draw(std::unordered_map<StringType, int>& TypeNumbers,
						 StringType& OutStr,
						 const std::unordered_set<RegexNodeBase<T>*> Ends,
						 std::unordered_map<RegexNodeBase<T>*, StringType>& NodeNames,
						 StringType Indent)
		{
			StringType MyName;
			{
				std::basic_ostringstream<T> oss;
				std::basic_string<T> MyType = (Negative ? "NegativeLookBehind" : "PositiveLookBehind");
				oss << MyType << "_" << TypeNumbers[MyType]++;
				MyName = oss.str();
	
				OutStr += Indent + MyName + '\n';
			}
	
			RegexChunk<T>::Draw(Ins, TypeNumbers, OutStr, Ends, NodeNames, Indent, MyName);
			DrawNexts(TypeNumbers, OutStr, Ends, NodeNames, Indent, MyName);
	
			NodeNames[this] = MyName;
			return MyName;
		}
	};
	
	
	// Basic Capture-based nodes, i.e. Capture Group and Backreference.
	
	template<typename T>
	struct RegexCaptureBase
	{

		// The initial value for LastCapture. Always equals the first node which touches the group.
		RegexNode<T>* InitialCapture = nullptr;

		// The last group that set this capture. Used exclusively by subroutine nodes.
		RegexNode<T>* LastCapture = nullptr;
	
		// Indicates whether the capture was successful, i.e. that it was both reached and written to.
		bool Succeeded = false;

		// Indicates that this capture is to be set by the user. Used in pre-match reset functionality within Evex::Regex.
		bool Manual = false;
	
		virtual std::basic_string<T> GetCapture() const = 0;
		virtual void SetCapture(std::basic_string<T> NewCapture, bool Reset = false) = 0;
		virtual void Reset() { LastCapture = InitialCapture; }
	};
	
	template<typename T>
	struct RegexCapture : public RegexCaptureBase<T>
	{
		/*
			Since the line of nodes this produces will only ever be a straight line,
			we can save a little bit of space by having the captured inputs also be
			the (pretend) node list we traverse when within a backreference.
		*/
		std::basic_string<T> CapturedInputs;
	
		std::basic_string<T> GetCapture() const final { return CapturedInputs; }
		void SetCapture(std::basic_string<T> NewCapture, bool Reset = false) final { CapturedInputs = NewCapture; Succeeded = true; }
		void Reset() final { RegexCaptureBase<T>::Reset(); CapturedInputs.clear(); }
	};
	
	template<typename T>
	struct RegexCaptureCollection : public RegexCaptureBase<T>
	{
		std::vector<std::basic_string<T>> CapturedInputs;
	
		// NOTE: gets only the latest(!) capture in a capture collection.
		std::basic_string<T> GetCapture() const final { return (CapturedInputs.empty() ? "" : CapturedInputs.back()); }
		void SetCapture(std::basic_string<T> NewCapture, bool Reset = false) final
		{
			if (Reset)
				CapturedInputs.clear();
	
			CapturedInputs.push_back(NewCapture);
	
			Succeeded = true;
		}
		void Reset() final { RegexCaptureBase<T>::Reset(); CapturedInputs.clear(); }
	};
	
	/*
		Marches forward to determine whether a capture is met.
		If capture is met, the resulting data is applied to a bound RegexCapture for later backreferencing,
		and moves Input(!) to the latest checked position.
	*/
	template<typename T>
	struct RegexCaptureNode : public RegexGroupNode<T>
	{
		std::basic_string<T> CaptureName;
		RegexCaptureBase<T>* BoundCapture = nullptr;
	
		RegexCaptureNode(std::basic_string<T>& CapName) : CaptureName(CapName) {}
	
		inline virtual RegexNode* Clone()
		{
			RegexCaptureNode* Out = new RegexCaptureNode(CaptureName);
			Out->BoundCapture = BoundCapture;
			Out->Chunks = Chunks;
			Out->Ins = Ins;
			Out->Outs = Outs;
			Out->LazyGroup = LazyGroup;
			return Out;
		}
	
		// Captures should never be similar to other captures.
		inline bool SimilarTo(const RegexNodeBase<T>* o) const final { return false; }
	
		inline bool CanEnter(IterType& Input, std::vector<RegexNode<T>*>* Outers = nullptr) final
		{
			if (BoundCapture)
			{
				BoundCapture->Succeeded = false;
	
				IterType Copy;
	
				std::vector<RegexNode<T>*> AppendOuters;
				if (nullptr != Outers)
					AppendOuters = *Outers;
				AppendOuters.push_back(this);
				if (RegexChunk<T>::Match(Input, Ins, Outs, LazyGroup, Copy, &AppendOuters))
				{
					std::basic_string<T> NewCappedInputs;
					for (IterType iter = Input; iter < Copy; ++iter)
						NewCappedInputs += *iter;
	
					if (!Copy.IsEnd() && Copy >= Input)
						NewCappedInputs += *Copy;
	
					BoundCapture->SetCapture(NewCappedInputs);
					BoundCapture->LastCapture = this;
	
					if (Copy < Input) // zero-width match
						--Input;
					else
						Input = Copy;
	
					return true;
				}
			}
	
			return false;
		}
	
		StringType Draw(std::unordered_map<StringType, int>& TypeNumbers,
						 StringType& OutStr,
						 const std::unordered_set<RegexNodeBase<T>*> Ends,
						 std::unordered_map<RegexNodeBase<T>*, StringType>& NodeNames,
						 StringType Indent)
		{
			StringType MyName;
			{
				std::basic_ostringstream<T> oss;
				oss << "CaptureGroup_" << TypeNumbers["CaptureGroup"]++;
				MyName = oss.str();
	
				StringType CapStr = (BoundCapture ? CaptureName : "invalid");
	
				OutStr += Indent + MyName + "[label=\""+ MyName +"\\n(\\" + CapStr + ")\"]" + '\n';
			}
	
			RegexChunk<T>::Draw(Ins, TypeNumbers, OutStr, Ends, NodeNames, Indent, MyName);
			DrawNexts(TypeNumbers, OutStr, Ends, NodeNames, Indent, MyName);
	
			NodeNames[this] = MyName;
			return MyName;
		}
	};
	
	/*
		Marches forward to determine whether a previously-constructed capture is met.
		If capture is met, the node can be entered, and it moves Input(!) to the latest checked position.
	*/
	template<typename T>
	struct RegexBackreferenceNode : public RegexNode<T>
	{
		std::basic_string<T> CaptureName;
		const RegexCaptureBase<T>* BoundCapture = nullptr;
	
		RegexBackreferenceNode(std::basic_string<T>& CapName) : CaptureName(CapName) {}
	
		inline virtual RegexNode* Clone()
		{
			RegexBackreferenceNode* Out = new RegexBackreferenceNode(CaptureName);
			Out->BoundCapture = BoundCapture;
			return Out;
		}
	
		inline bool SimilarTo(const RegexNodeBase<T>* o) const final
		{
			const RegexBackreferenceNode* AsBackrefNode = dynamic_cast<const RegexBackreferenceNode*>(o);
	
			if (AsBackrefNode)
				return CaptureName == AsBackrefNode->CaptureName;
	
			return false;
		}
	
		inline bool CanEnter(IterType& Input, std::vector<RegexNode<T>*>* Outers = nullptr) final
		{
			if (BoundCapture && BoundCapture->Succeeded)
			{
				IterType Copy = Input;
	
				std::basic_string<T> CappedInputs = BoundCapture->GetCapture();
				for (const T& currCapturedInput : CappedInputs)
				{
					if (Copy.IsEnd() || *Copy != currCapturedInput)
						return false;
					++Copy;
				}
	
				Input = --Copy;
				return true;
			}
	
			return false;
		}
	
		StringType Draw(std::unordered_map<StringType, int>& TypeNumbers,
						 StringType& OutStr,
						 const std::unordered_set<RegexNodeBase<T>*> Ends,
						 std::unordered_map<RegexNodeBase<T>*, StringType>& NodeNames,
						 StringType Indent)
		{
			StringType MyName;
			{
				std::basic_ostringstream<T> oss;
				oss << "BackRef_" << TypeNumbers["BackRef"]++;
				MyName = oss.str();
	
				StringType CapStr = (BoundCapture ? CaptureName : "invalid");
	
				OutStr += Indent + MyName + "[label=\"" + MyName + "\\n(\\" + CapStr + ")\"]" + '\n';
			}
	
			DrawNexts(TypeNumbers, OutStr, Ends, NodeNames, Indent, MyName);
	
			NodeNames[this] = MyName;
			return MyName;
		}
	};
	
	
	// Loop nodes, i.e. None-Or-More and Repeat/Loop
	
	/*
		Represents the ?, ??, *, and *? operators.
		Moves Input(!) during operation.
	*/
	template<typename T>
	struct RegexNoneOrMoreNode : public RegexGroupNode<T>
	{
		bool OnceOnly = true;
		bool Lazy = false;
	
		RegexNoneOrMoreNode(bool Once, bool Reluctant = false) : OnceOnly(Once), Lazy(Reluctant) {}
	
		inline virtual RegexNode* Clone()
		{
			RegexNoneOrMoreNode* Out = new RegexNoneOrMoreNode(OnceOnly, Lazy);
			Out->Chunks = Chunks;
			Out->Ins = Ins;
			Out->Outs = Outs;
			return Out;
		}
	
		// None-Or-Mores should never be similar to each other.
		inline bool SimilarTo(const RegexNodeBase<T>* o) const final { return false; }
	
		void TryAnyTakers(IterType& Input, std::vector<RegexNode<T>*>* Outers)
		{
			IterType FinalCopy = Input;
	
			std::vector<RegexNode<T>*> NextCandidates;
			if (nullptr != Outers)
				NextCandidates = *Outers;
			NextCandidates.push_back(this);
	
			bool AnyTakers = false;
			for (int i = NextCandidates.size() - 1; i >= 0 && !AnyTakers; --i)
			{
				std::vector<RegexNode<T>*> RetrievedNexts = NextCandidates[i]->GetNexts();
	
				for (RegexNode<T>* currNext : RetrievedNexts)
				{
					if (currNext != this)
					{
						AnyTakers = currNext->CanEnter(FinalCopy, &NextCandidates);
	
						if (AnyTakers)
						{
							--Input;
							break;
						}
	
						FinalCopy = Input;
					}
				}
			}
		}
	
		inline bool CanEnter(IterType& Input, std::vector<RegexNode<T>*>* Outers = nullptr) final
		{
			IterType Copy;
			std::vector<RegexNode<T>*> AppendOuters;
			if (nullptr != Outers)
				AppendOuters = *Outers;
			AppendOuters.push_back(this);
			if (RegexChunk<T>::Match(Input, Ins, Outs, false, Copy, &AppendOuters))
			{
				if (Lazy)
					TryAnyTakers(Input, Outers);
				else
					Input = Copy;
			}
			else
				TryAnyTakers(Input, Outers);
	
			return true;
		}
	
		inline std::vector<RegexNode<T>*> GetNexts() final
		{
			std::vector<RegexNode<T>*> OtherNexts = RegexNode<T>::GetNexts();
	
			if (!OnceOnly)
			{
				if (Lazy)
					OtherNexts.insert(OtherNexts.end(), this);
				else
					OtherNexts.insert(OtherNexts.begin(), this);
			}
	
			return OtherNexts;
		}
	
		StringType Draw(std::unordered_map<StringType, int>& TypeNumbers,
			StringType& OutStr,
			const std::unordered_set<RegexNodeBase<T>*> Ends,
			std::unordered_map<RegexNodeBase<T>*, StringType>& NodeNames,
			StringType Indent)
		{
			StringType MyName;
			std::basic_ostringstream<T> oss;
	
			std::basic_string<T> MyType((OnceOnly ? "NoneOrOnce" : "NoneOrMore"));
			MyType = (Lazy ? "Lazy" : "") + MyType;
	
			oss << MyType << "_" << TypeNumbers[MyType]++;
	
			MyName = oss.str();
	
			oss.str(std::basic_string<T>());
	
			OutStr += Indent + MyName + "[label=\"" + (OnceOnly ? "None or Once" : "None or More") + "\\n(" + MyName + ")\"]" + '\n';
	
			if (!OnceOnly)
				OutStr += Indent + MyName + " -> " + MyName + "[dir=back]\n";
	
			RegexChunk<T>::Draw(Ins, TypeNumbers, OutStr, Ends, NodeNames, Indent, MyName);
			DrawNexts(TypeNumbers, OutStr, Ends, NodeNames, Indent, MyName);
	
			NodeNames[this] = MyName;
			return MyName;
		}
	};
	
	
	/*
		Ticker class exclusively used by RegexLoopNodes.
	*/
	template<typename T>
	struct RegexTicker
	{
		// Positive times indicate mandatory loops, negative times indicate skippable loops.
		const int MaxTimes = 0;
		int CurrTimes = 0;
	
		RegexTicker(int Max) : MaxTimes(Max), CurrTimes(Max) {}
	
		inline bool IsExhausted() { return CurrTimes == 0; }
	
		inline bool IsMandatory() { return MaxTimes > 0; }
	
		inline void Tick() { (MaxTimes > 0 ? --CurrTimes : ++CurrTimes); }
	
		inline void Reset() { CurrTimes = MaxTimes; }
	};
	
	/*
		Essentially a non-capturing group which points to itself as a potential next.
		May move Input(!) under some circumstances.
	*/
	template<typename T>
	struct RegexLoopNode : public RegexGroupNode<T>
	{
		RegexTicker<T>* BoundTicker = nullptr;
		bool Lazy = false;
	
		RegexLoopNode(RegexTicker<T>* Ticker, bool Reluctant = false) : BoundTicker(Ticker), Lazy(Reluctant) {}
	
		inline virtual RegexNode* Clone()
		{
			RegexLoopNode* Out = new RegexLoopNode(BoundTicker, Lazy);
			Out->Chunks = Chunks;
			Out->Ins = Ins;
			Out->Outs = Outs;
			return Out;
		}
	
		// loops should never be similar to each other.
		inline bool SimilarTo(const RegexNodeBase<T>* o) const final { return false; }
	
		inline bool CanEnter(IterType& Input, std::vector<RegexNode<T>*>* Outers = nullptr) final
		{
			if (nullptr == BoundTicker || !BoundTicker->IsExhausted())
			{
				IterType Copy;
	
				std::vector<RegexNode<T>*> AppendOuters;
				if (nullptr != Outers)
					AppendOuters = *Outers;
				AppendOuters.push_back(this);
				if (RegexChunk<T>::Match(Input, Ins, Outs, false, Copy, &AppendOuters))
				{
					if (BoundTicker)
						BoundTicker->Tick();
					Input = Copy;
					return true;
				}
			}
	
			return false;
		}
	
		inline std::vector<RegexNode<T>*> GetNexts() final
		{
			std::vector<RegexNode<T>*> OtherNexts = RegexNode<T>::GetNexts();
	
			if (BoundTicker && !BoundTicker->IsExhausted() && BoundTicker->IsMandatory())
				return{ this };
	
			if (Lazy)
				OtherNexts.insert(OtherNexts.end(), this);
			else
				OtherNexts.insert(OtherNexts.begin(), this);
	
			return OtherNexts;
		}
	
		StringType Draw(std::unordered_map<StringType, int>& TypeNumbers,
						 StringType& OutStr,
						 const std::unordered_set<RegexNodeBase<T>*> Ends,
						 std::unordered_map<RegexNodeBase<T>*, StringType>& NodeNames,
						 StringType Indent)
		{
			StringType MyName;
			std::basic_ostringstream<T> oss;
	
			std::basic_string<T> MyType = (Lazy ? "Lazy" : "") + std::basic_string<T>("Loop");
	
			oss << MyType << "_" << TypeNumbers[MyType]++;
			MyName = oss.str();
	
			oss.str(std::basic_string<T>());
	
			OutStr += Indent + MyName + "[label=\"Loop\\n(" + MyName + ")\"]" + '\n';
	
			StringType AppendLabel = "[dir=back]\n";
			if (BoundTicker)
			{
				oss << std::abs(BoundTicker->MaxTimes);
				if (BoundTicker)
					AppendLabel = StringType("[xlabel=\"") + (BoundTicker->MaxTimes > 0 ? "Mandatory " : "")
					+ "x" + oss.str() + "\",dir=back]\n";
			}
	
			OutStr += Indent + MyName + " -> " + MyName + AppendLabel;
	
			RegexChunk<T>::Draw(Ins, TypeNumbers, OutStr, Ends, NodeNames, Indent, MyName);
			DrawNexts(TypeNumbers, OutStr, Ends, NodeNames, Indent, MyName);
	
			NodeNames[this] = MyName;
			return MyName;
		}
	};
	
	
	// Recursive nodes, i.e. Recursion and Subroutines.
	
	class RegexRuntimeException : public std::exception
	{
	private:
		std::string msg;
	public:
		explicit RegexRuntimeException(const std::string& inMsg) : msg(inMsg) {}
		virtual const char* what() const throw() { return msg.c_str(); }
	};
	
	/*
		Essentially a non-capturing group which contains the entire match automaton, including itself.
		Moves Input(!) under certain circumstances.
		NOTE: MaxDepth should always be below the stack overflow threshold for the safety to work properly!
	*/
	template<typename T>
	struct RegexRecursionNode : public RegexGroupNode<T>
	{
		const int MaxDepth = 0;
		int CurrDepth = 0;
	
		RegexRecursionNode(int maxDepth) : MaxDepth(maxDepth) {}
	
		inline virtual RegexNode* Clone()
		{
			RegexRecursionNode* Out = new RegexRecursionNode(MaxDepth);
			Out->Chunks = Chunks;
			Out->Ins = Ins;
			Out->Outs = Outs;
			Out->LazyGroup = LazyGroup;
			return Out;
		}
	
		// Recursion nodes should never be similar to each other.
		inline bool SimilarTo(const RegexNodeBase<T>* o) const final { return false; }
	
		inline bool CanEnter(IterType& Input, std::vector<RegexNode<T>*>* Outers = nullptr) final
		{
			IterType Copy;
	
			int KeptDepth = CurrDepth++;
			if (KeptDepth < MaxDepth)
			{
				std::vector<RegexNode<T>*> AppendOuters;
				if (nullptr != Outers)
					AppendOuters = *Outers;
				AppendOuters.push_back(this);
				if (RegexChunk<T>::Match(Input, Ins, Outs, LazyGroup, Copy, &AppendOuters))
				{
					Input = Copy;
					return true;
				}
			}
			else
				throw RegexRuntimeException("Maximum recursion depth exceeded during match!");
	
			CurrDepth = KeptDepth;
	
			return false;
		}
	
		StringType Draw(std::unordered_map<StringType, int>& TypeNumbers,
						 StringType& OutStr,
						 const std::unordered_set<RegexNodeBase<T>*> Ends,
						 std::unordered_map<RegexNodeBase<T>*, StringType>& NodeNames,
						 StringType Indent)
		{
			StringType MyName;
			{
				std::basic_ostringstream<T> oss;
				oss << "Recursion_" << TypeNumbers["Recursion"]++;
				MyName = oss.str();
	
				OutStr += Indent + MyName + "[label=\"Recursion\"]" + '\n';
			}
			DrawNexts(TypeNumbers, OutStr, Ends, NodeNames, Indent, MyName);
	
			NodeNames[this] = MyName;
			return MyName;
		}
	};
	
	/*
		A node which directly calls the last capture node to set its bound capture.
		Moves input(!).
		NOTE: MaxDepth should always be below the stack overflow threshold for the safety to work properly!
	*/
	template<typename T>
	struct RegexSubroutineNode : public RegexNode<T>
	{
		const int MaxDepth = 0;
		int CurrDepth = 0;
	
		std::basic_string<T> CaptureName;
		const RegexCaptureBase<T>* BoundCapture = nullptr;
	
		RegexSubroutineNode(std::basic_string<T>& CapName, int maxDepth) : CaptureName(CapName), MaxDepth(maxDepth) {}
	
		inline virtual RegexNode* Clone()
		{
			RegexSubroutineNode* Out = new RegexSubroutineNode(CaptureName, MaxDepth);
			Out->BoundCapture = BoundCapture;
			return Out;
		}
	
		inline bool SimilarTo(const RegexNodeBase<T>* o) const final
		{
			const RegexSubroutineNode* AsSubroutineNode = dynamic_cast<const RegexSubroutineNode*>(o);
	
			if (AsSubroutineNode)
				return CaptureName == AsSubroutineNode->CaptureName;
	
			return false;
		}
	
		inline bool CanEnter(IterType& Input, std::vector<RegexNode<T>*>* Outers = nullptr) final
		{
			if (BoundCapture && BoundCapture->LastCapture)
			{
				int KeptDepth = CurrDepth++;
				if (KeptDepth < MaxDepth)
					return BoundCapture->LastCapture->CanEnter(Input, Outers);
				else
					throw RegexRuntimeException("Maximum recursion depth exceeded during match!");
	
				CurrDepth = KeptDepth;
			}
	
			return false;
		}
	
		StringType Draw(std::unordered_map<StringType, int>& TypeNumbers,
			StringType& OutStr,
			const std::unordered_set<RegexNodeBase<T>*> Ends,
			std::unordered_map<RegexNodeBase<T>*, StringType>& NodeNames,
			StringType Indent)
		{
			StringType MyName;
			{
				std::basic_ostringstream<T> oss;
				oss << "Subroutine_" << TypeNumbers["Subroutine"]++;
				MyName = oss.str();
	
				StringType CapStr = (BoundCapture ? CaptureName : "invalid");
	
				OutStr += Indent + MyName + "[label=\"" + MyName + "\\n(\\" + CapStr + ")\"]" + '\n';
			}
	
			DrawNexts(TypeNumbers, OutStr, Ends, NodeNames, Indent, MyName);
	
			NodeNames[this] = MyName;
			return MyName;
		}
	};
	
	
	// Zero-width match nodes, i.e. String-Beginning, String-End, Word Boundary
	
	/*
		Start-of-line/string/match.
	*/
	template<typename T>
	struct RegexAtBeginningNode : public RegexNode<T>
	{
		// When false, represents "^". When true, represents "\A".
		bool ExclusivelyBeginning = false;
	
		// If ExclusivelyBeginning is true and this is set, represents "\G"
		IterType* LastMatchEnd = nullptr;
	
		RegexAtBeginningNode(RegexCharacterClassBase<T>* LineChars, bool Exclusive)
			: RegexNode<T>({ LineChars }), ExclusivelyBeginning(Exclusive) {}
	
		inline virtual RegexNode* Clone()
		{
			RegexAtBeginningNode* Out = new RegexAtBeginningNode(*Comparators.begin(), ExclusivelyBeginning);
			Out->LastMatchEnd = LastMatchEnd;
			return Out;
		}
	
		// There should never be another AtBeginning node in the first place, so it should never be similar to anything.
		inline bool SimilarTo(const RegexNodeBase<T>* o) const final { return false; }
	
		inline bool CanEnter(IterType& Input, std::vector<RegexNode<T>*>* Outers = nullptr) final
		{
			IterType Copy = Input;
	
			if (ExclusivelyBeginning && LastMatchEnd)
				return Input.IsBegin() || --Copy == *LastMatchEnd;
	
			return Input.IsBegin() || (!ExclusivelyBeginning && RegexNode<T>::CanEnter(--Copy));
		}
	
		StringType Draw(std::unordered_map<StringType, int>& TypeNumbers,
						 StringType& OutStr,
						 const std::unordered_set<RegexNodeBase<T>*> Ends,
						 std::unordered_map<RegexNodeBase<T>*, StringType>& NodeNames,
						 StringType Indent)
		{
			StringType MyName;
			{
				std::basic_ostringstream<T> oss;
				oss << "AtStart_" << TypeNumbers["AtStart"]++;
				MyName = oss.str();
	
				OutStr += Indent + MyName + "[label=\"" + (ExclusivelyBeginning ? "\\\\A" : "^") + "\"]" + '\n';
			}
			DrawNexts(TypeNumbers, OutStr, Ends, NodeNames, Indent, MyName);
	
			NodeNames[this] = MyName;
			return MyName;
		}
	};
	
	/*
		End of line/string.
	*/
	template<typename T>
	struct RegexAtEndNode : public RegexNode<T>
	{
		// When false, represents "$". When true, represents "\z".
		bool ExclusivelyEnd = false;
	
		// when ExclusivelyEnd and true, represents "\Z".
		bool LastNewline = false;
	
		RegexAtEndNode(RegexCharacterClassBase<T>* LineChars, bool Exclusive, bool LastNL)
			: RegexNode<T>({ LineChars }), ExclusivelyEnd(Exclusive), LastNewline(LastNL) {}
	
		inline virtual RegexNode* Clone()
		{
			RegexAtEndNode* Out = new RegexAtEndNode(*Comparators.begin(), ExclusivelyEnd, LastNewline);
			return Out;
		}
	
		// There should never be another AtEnd node in the first place, so it should never be similar to anything.
		inline bool SimilarTo(const RegexNodeBase<T>* o) const final { return false; }
	
		inline bool CanEnter(IterType& Input, std::vector<RegexNode<T>*>* Outers = nullptr) final
		{
			IterType Copy = Input;
	
			bool Success = false;
			IterType TrueEnd = Input.CloneAtEnd();
			if (LastNewline)
			{
				while (TrueEnd != Input)
				{
					--TrueEnd;
					if (!RegexNode<T>::CanEnter(TrueEnd))
					{
						++TrueEnd;
						break;
					}
				}
	
				if (TrueEnd == Input && RegexNode<T>::CanEnter(TrueEnd))
					Success = true;
			}
	
			if (!Success)
				Success = Input == TrueEnd || (!ExclusivelyEnd && RegexNode<T>::CanEnter(Copy));
	
			return Success;
		}
	
		StringType Draw(std::unordered_map<StringType, int>& TypeNumbers,
						 StringType& OutStr,
						 const std::unordered_set<RegexNodeBase<T>*> Ends,
						 std::unordered_map<RegexNodeBase<T>*, StringType>& NodeNames,
						 StringType Indent)
		{
			StringType MyName;
			{
				std::basic_ostringstream<T> oss;
				oss << "AtEnd_" << TypeNumbers["AtEnd"]++;
				MyName = oss.str();
	
				OutStr += Indent + MyName + "[label=\"" + (ExclusivelyEnd ? (LastNewline ? "\\\\Z" : "\\\\z") : "$") + "\"]" + '\n';
			}
			DrawNexts(TypeNumbers, OutStr, Ends, NodeNames, Indent, MyName);
	
			NodeNames[this] = MyName;
			return MyName;
		}
	};
	
	/*
		Represents a boundary between two characters where one character matches
		the character class [A-Za-z0-9_] and the other does not.
		Moves Input(!) in some circumstances.
	*/
	template<typename T>
	struct RegexWordBoundaryNode : public RegexNode<T>
	{
		bool Negated = false;
	
		RegexWordBoundaryNode(RegexCharacterClassBase<T>* wordChars, bool Negate)
			: RegexNode<T>({ wordChars }), Negated(Negate) {}
	
		inline virtual RegexNode* Clone()
		{
			RegexWordBoundaryNode* Out = new RegexWordBoundaryNode(*Comparators.begin(), Negated);
			return Out;
		}
	
		inline bool SimilarTo(const RegexNodeBase<T>* o) const final
		{
			const RegexWordBoundaryNode* AsWB = dynamic_cast<const RegexWordBoundaryNode*>(o);
	
			if (AsWB)
				return AsWB->Negated == Negated;
	
			return false;
		}
	
		inline bool CanEnter(IterType& Input, std::vector<RegexNode<T>*>* Outers = nullptr) final
		{
			IterType Copy = Input;
			bool Success = Copy.IsBegin() || Copy.IsEnd() || (++Copy).IsEnd();
	
			if (!Success)
			{
				Copy = Input;
				if (RegexNode<T>::CanEnter(Input))
					Success = !(RegexNode<T>::CanEnter(--Copy) || RegexNode<T>::CanEnter(++(++Copy)));
				else
					Success = (RegexNode<T>::CanEnter(--Copy) || RegexNode<T>::CanEnter(++(++Copy)));
			}
			else if (Input.IsBegin())
				--Input;
	
			return (Negated ? !Success : Success);
		}
	
		StringType Draw(std::unordered_map<StringType, int>& TypeNumbers,
						 StringType& OutStr,
						 const std::unordered_set<RegexNodeBase<T>*> Ends,
						 std::unordered_map<RegexNodeBase<T>*, StringType>& NodeNames,
						 StringType Indent)
		{
			StringType MyName;
			{
				std::basic_ostringstream<T> oss;
				oss << "WordBoundary_" << TypeNumbers["WordBoundary"]++;
				MyName = oss.str();
	
				OutStr += Indent + MyName + "[label=\"\\\\b\"]" + '\n';
			}
			DrawNexts(TypeNumbers, OutStr, Ends, NodeNames, Indent, MyName);
	
			NodeNames[this] = MyName;
			return MyName;
		}
	};
	
	
	// Conditional Node
	
	/*
		If the relevant condition has been met, matches IfTrue, otherwise matches IfFalse if it exists.
		There are two distinct behaviors a conditional node can exhibit:
			- Regular
				- When the condition is a regex, the conditional node checks if it has been satisfied.
			- Referential
				- When the condition is a lone backreference node, the conditional checks if its capture has succeeded.
		Moves Input(!).
	*/
	template<typename T>
	struct RegexConditionalNode : public RegexGroupNode<T>
	{
		RegexChunk<T>* Cond, *IfTrue, *IfFalse;
	
		RegexConditionalNode(RegexChunk<T>* c, RegexChunk<T>* t, RegexChunk<T>* f) : Cond(c), IfTrue(t), IfFalse(f) {}
	
		inline virtual RegexNode* Clone()
		{
			RegexConditionalNode* Out = new RegexConditionalNode(Cond, IfTrue, IfFalse);
			Out->Chunks = Chunks;
			Out->Ins = Ins;
			Out->Outs = Outs;
			Out->LazyGroup = LazyGroup;
			return Out;
		}
	
		// conditionals should not be similar to each other
		inline bool SimilarTo(const RegexNodeBase<T>* o) const final { return false; }
	
		inline bool CanEnter(IterType& Input, std::vector<RegexNode<T>*>* Outers = nullptr) final
		{
			const RegexCaptureBase<T>* Cap = nullptr;
			if (!Cond->Nodes.empty() && Cond->Nodes.size() < 2)
			{
				RegexBackreferenceNode<T>* AsBackref = dynamic_cast<RegexBackreferenceNode<T>*>(Cond->Nodes[0]);
	
				if (AsBackref)
					Cap = AsBackref->BoundCapture;
			}
	
			IterType Copy;
	
			std::vector<RegexNode<T>*> AppendOuters;
			if (nullptr != Outers)
				AppendOuters = *Outers;
			AppendOuters.push_back(this);
	
			std::unordered_set<RegexNodeGhostIn<T>*> currIns;
			std::unordered_set<RegexNodeGhostOut<T>*> currOuts;
	
			for (RegexNodeGhostIn<T>& currIn : Cond->Ins) currIns.insert(&currIn);
			for (RegexNodeGhostOut<T>& currOut : Cond->Outs) currOuts.insert(&currOut);
			if ((Cap ? Cap->Succeeded : RegexChunk<T>::Match(Input, currIns, currOuts, LazyGroup, Copy, &AppendOuters)))
			{
				currIns.clear();
				currOuts.clear();
				for (RegexNodeGhostIn<T>& currIn : IfTrue->Ins) currIns.insert(&currIn);
				for (RegexNodeGhostOut<T>& currOut : IfTrue->Outs) currOuts.insert(&currOut);
				if (RegexChunk<T>::Match(Input, currIns, currOuts, LazyGroup, Copy, &AppendOuters))
				{
					Input = Copy;
					return true;
				}
			}
			else if (nullptr != IfFalse)
			{
				currIns.clear();
				currOuts.clear();
				for (RegexNodeGhostIn<T>& currIn : IfFalse->Ins) currIns.insert(&currIn);
				for (RegexNodeGhostOut<T>& currOut : IfFalse->Outs) currOuts.insert(&currOut);
				if (RegexChunk<T>::Match(Input, currIns, currOuts, LazyGroup, Copy, &AppendOuters))
				{
					Input = Copy;
					return true;
				}
			}
	
			return false;
		}
	
		StringType Draw(std::unordered_map<StringType, int>& TypeNumbers,
			StringType& OutStr,
						 const std::unordered_set<RegexNodeBase<T>*> Ends,
						 std::unordered_map<RegexNodeBase<T>*, StringType>& NodeNames,
						 StringType Indent)
		{
			StringType MyName;
			{
				std::basic_ostringstream<T> oss;
				oss << "Conditional_" << TypeNumbers["Conditional"]++;
				MyName = oss.str();
	
				OutStr += Indent + MyName + "[label=\"Conditional\\n(\'" + MyName + "\')\"]" + '\n';
			}
	
			RegexChunk<T>::Draw(Cond->Ins, TypeNumbers, OutStr, Ends, NodeNames, Indent, MyName + " (Condition)");
			RegexChunk<T>::Draw(IfTrue->Ins, TypeNumbers, OutStr, Ends, NodeNames, Indent, MyName + " (If True)");
			if (IfFalse)
				RegexChunk<T>::Draw(IfFalse->Ins, TypeNumbers, OutStr, Ends, NodeNames, Indent, MyName + " (If False)");
	
			DrawNexts(TypeNumbers, OutStr, Ends, NodeNames, Indent, MyName);
	
			NodeNames[this] = MyName;
			return MyName;
		}
	};
	
	
	// Code Hook Node
	
	/*
		A node which calls a user-specified function when reached.
		Does not move Input on its own, but there's nothing stopping
		the user function from moving it.
	*/
	template<typename T>
	struct RegexCodeHookNode : public RegexNode<T>
	{
		using FuncType = std::function<void(IterType&)>;
	
		std::basic_string<T> HookedName;
		FuncType Hooked;
	
		RegexCodeHookNode(std::basic_string<T> name, FuncType func) : HookedName(name), Hooked(func) {}
	
		inline virtual RegexNode* Clone()
		{
			RegexCodeHookNode* Out = new RegexCodeHookNode(HookedName, Hooked);
			return Out;
		}
	
		inline bool SimilarTo(const RegexNodeBase<T>* o) const final
		{
			const RegexCodeHookNode* AsHookNode = dynamic_cast<const RegexCodeHookNode*>(o);
	
			if (AsHookNode)
				return HookedName == AsHookNode->HookedName;
	
			return false;
		}
	
		inline bool CanEnter(IterType& Input, std::vector<RegexNode<T>*>* Outers = nullptr) final
		{
			if (Hooked)
				Hooked(Input);
			--Input;
			return true;
		}
	
		StringType Draw(std::unordered_map<StringType, int>& TypeNumbers,
						 StringType& OutStr,
						 const std::unordered_set<RegexNodeBase<T>*> Ends,
						 std::unordered_map<RegexNodeBase<T>*, StringType>& NodeNames,
						 StringType Indent)
		{
			StringType MyName;
			{
				std::basic_ostringstream<T> oss;
				oss << "CodeHook_" << TypeNumbers["CodeHook"]++;
				MyName = oss.str();
	
				OutStr += Indent + MyName + "[label=\"Code Hook\\n(func \'" + HookedName +"\')\"]" + '\n';
			}
	
			DrawNexts(TypeNumbers, OutStr, Ends, NodeNames, Indent, MyName);
	
			NodeNames[this] = MyName;
			return MyName;
		}
	};
}
