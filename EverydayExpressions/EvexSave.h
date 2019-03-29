#pragma once

#include "EvexTranslator.h"

#include <fstream>
#include <streambuf>

namespace Evex
{
	template<typename T>
	std::basic_ofstream<T>& operator<<(std::basic_ofstream<T>& lhs, std::vector<RegexInstruction<T>>& rhs)
	{
		std::basic_stringstream<T> Output;

		Output << rhs.size() << " \n";
		for (RegexInstruction<T>& currInst : rhs)
		{
			Output << T(currInst.InstructionType) << " " << currInst.InstructionData.size() << " ";
			for (std::basic_string<T> currData : currInst.InstructionData)
				Output << size_t(currData.length()) << " " << currData << " ";
			Output << "\n";
		}

		lhs << Output.str();
		return lhs;
	}

	template<typename T>
	std::basic_ifstream<T>& operator>>(std::basic_ifstream<T>& lhs, std::vector<RegexInstruction<T>>& rhs)
	{
		std::basic_string<T> Input;

		lhs.seekg(0, std::ios::end);
		Input.reserve(size_t(lhs.tellg()));
		lhs.seekg(0, std::ios::beg);

		Input.assign((std::istreambuf_iterator<char>(lhs)), std::istreambuf_iterator<char>());

		auto iter = Input.begin();

		std::basic_string<T> Munch;
		while (isdigit(*iter))
		{
			Munch += *iter;
			++iter;
		}
		++(++iter);
		rhs.reserve(std::stoi(Munch));

		while (iter < Input.end())
		{
			rhs.push_back({});
			RegexInstruction<T>& currInstruction = rhs.back();
			currInstruction.InstructionType = RegexInstructionType(uint8_t(*iter));
			++(++iter);

			Munch.clear();
			while (isdigit(*iter))
			{
				Munch += *iter;
				++iter;
			}
			++iter;

			size_t NumStrings = std::stoi(Munch);

			while (NumStrings-- > 0)
			{
				Munch.clear();
				while (isdigit(*iter))
				{
					Munch += *iter;
					++iter;
				}
				++iter;

				size_t NextStringLength = std::stoi(Munch);
				std::basic_string<T> NextString;
				while (NextStringLength-- > 0)
				{
					NextString += *iter;
					++iter;
				}
				++iter;

				currInstruction.InstructionData.push_back(NextString);
			}
			++iter;
		}

		return lhs;
	}

	/*
		Saves a given regex instruction list out to file so that it can be reloaded later.
	*/
	template<typename T>
	static bool SaveRegex(std::vector<RegexInstruction<T>> Instructions, const std::basic_string<T>& Filepath)
	{
		std::basic_ofstream<T> FileStream(Filepath.c_str(), std::ofstream::out | std::ofstream::trunc);
		if (FileStream.is_open())
		{
			FileStream << Instructions;
			FileStream.close();
			return true;
		}
		return false;
	}

	/*
		Loads a regex instruction list from a specified file, allowing you to skip the parsing phase and move straight to the assembly phase.
	*/
	template<typename T>
	static std::vector<RegexInstruction<T>> LoadRegex(const std::basic_string<T>& Filepath)
	{
		std::vector<RegexInstruction<T>> Instructions;
		std::basic_ifstream<T> FileStream(Filepath.c_str());
		if (FileStream.is_open())
		{
			FileStream >> Instructions;
			FileStream.close();
		}
		return Instructions;
	}
}
