#pragma once

#include "Evex.h"

#include <fstream>


namespace Evex
{
	/*
		Outputs a text representation of the given Regex's graph form to a file of choice,
		ready to be input into GraphViz ( https://dreampuf.github.io/GraphvizOnline/ ) for
		studying. These graphs are specifically for the Dot engine.

		Returns false if the filestream could not be opened for some reason, or if the given
		Regex is invalid.
	*/
	template<typename T>
	bool DrawRegex(Regex<T>& RegexGraph, const std::basic_string<T>& Filepath)
	{
		if (RegexGraph.IsValidForMatching())
		{
			std::basic_ofstream<T> FileStream(Filepath.c_str(), std::ofstream::out | std::ofstream::trunc);
			if (FileStream.is_open())
			{
				std::basic_string<T> FinalOutput = "digraph G\n{\n\tcompound=true\n\trankdir=\"LR\"\n\tlabelloc=b\n";

				std::unordered_set<RegexNodeBase<T>*> Ends;
				for (RegexNodeGhostOut<T>* currOut : RegexGraph.EndNodes)
					Ends.insert(currOut);

				std::unordered_map<std::basic_string<T>, int> TypeNums;
				std::unordered_map<RegexNodeBase<T>*, std::basic_string<T>> KeptNames;
				for (RegexNodeGhostIn<T>* currIn : RegexGraph.StartNodes)
					currIn->Draw(TypeNums, FinalOutput, Ends, KeptNames, "\t");

				if (!RegexGraph.DefinedSubroutines.empty())
				{
					FinalOutput += "\tsubgraph cluster_" + std::to_string(TypeNums["Cluster"]++) + "\n\t{\n\t\tlabel=\"Defined Subroutines\"\n";

					for (RegexCaptureBase<T>* currDeffedSub : RegexGraph.DefinedSubroutines)
					{
						std::basic_string<T> DefSubName = "???";
						for (auto &currNamePair : RegexGraph.NamesToCaptures)
						{
							if (currNamePair.second == currDeffedSub)
							{
								DefSubName = currNamePair.first;
								break;
							}
						}
						FinalOutput += "\t\tsubgraph cluster_" + std::to_string(TypeNums["Cluster"]++) + "\n\t\t{\n\t\t\tlabel=\"" + DefSubName + "\"\n";
						if (currDeffedSub->InitialCapture)
							currDeffedSub->InitialCapture->Draw(TypeNums, FinalOutput, Ends, KeptNames, "\t\t\t");
						FinalOutput += "\t\t}\n";
					}

					FinalOutput += "\t}\n";
				}

				FileStream << FinalOutput << "}";

				FileStream.close();

				return true;
			}
		}
		return false;
	}
}
