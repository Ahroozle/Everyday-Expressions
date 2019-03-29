#include "Evex.h"
#include "EvexDraw.h"
#include "EvexSave.h"

#include <iostream>

int main()
{
	{
		/*
			Creating an Evex regex
		*/
		Evex::Regex<char> Regex("^[Hh]ello!$");


		/*
			Evaluating an Evex regex
		*/
		if ("Hello" == Regex) {}

		Regex.Match("Hello");

		std::string FromResult;
		Regex.MatchFrom("Hello", 0, FromResult);

		std::vector<std::string> AllResult;
		Regex.MatchAll("Hello", AllResult);


		/*
			Using Evex::DrawRegex to draw a debug representation
			of a regex's internal automaton.
		*/
		Evex::DrawRegex<char>(Regex, "../GraphOut.txt");
	}

	{
		/*
			Saving an Evex regex instruction list out to a file with Evex::SaveRegex
		*/
		std::vector<Evex::RegexInstruction<char>> InstructionsToSave;
		Evex::Regex<char> SaveExample("Sample Text", nullptr, &InstructionsToSave);

		Evex::SaveRegex<char>(InstructionsToSave, "../InstructionsOut.txt");


		/*
			Loading an Evex regex instruction list from a file with Evex::LoadRegex
		*/
		std::vector<Evex::RegexInstruction<char>> LoadedInstructions = Evex::LoadRegex<char>("../InstructionsOut.txt");
		Evex::Regex<char> FromInstructions(LoadedInstructions);

		// Alternatively:
		Evex::Regex<char> DirectlyLoaded(Evex::LoadRegex<char>("../InstructionsOut.txt"));

		Evex::DrawRegex<char>(FromInstructions, "../GraphOut.txt");
	}

	system("pause");

	return 0;
}
