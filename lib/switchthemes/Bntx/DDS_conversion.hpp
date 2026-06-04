#ifndef SWITCHTHEMESCOMMON_TESTS
#pragma once

#include <vector>
#include <string>

#include "../MyTypes.h"

namespace DDSConv
{	
	struct ConversionResult 
	{
		std::vector<u8> Data;
		std::string ErrorMessage;
		bool resized;

		bool IsSuccess() const { return ErrorMessage.empty(); }

		static ConversionResult Success(std::vector<u8> data, bool resized) 
		{
			return { data, "", resized };
		}

		static ConversionResult Fail(std::string error) 
		{
			return { {}, error, false };
		}
	};

	ConversionResult ConvertImage(const std::vector<u8>& imgData, 
		bool DXT5 = false, 
		int Width = 1280, 
		int Height = 720,
		bool ResizeIfNeeded = false);
}
#endif