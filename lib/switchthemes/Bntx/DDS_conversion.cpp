#ifndef SWITCHTHEMESCOMMON_TESTS

#include <string>
#include <vector>
#include <span>
#include "DDS_conversion.hpp"
#include "../MyTypes.h"
#include "../BinaryReadWrite/Buffer.hpp"
// Vendored: upstream pulled Platform.hpp (GLFW glue) but used no symbol from it.
// stb headers live in lib/switchthemes/third_party. The implementations are
// emitted here (the only consumer) with STATIC linkage so they never clash with
// borealis's own bundled stb_image (nanovg) at link time.
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_DXT_STATIC
#define STB_DXT_IMPLEMENTATION
#include "stb_dxt.h"
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize2.h"
#include <utility>

namespace 
{
	int imin(int x, int y) { return (x < y) ? x : y; }

	void extractBlock(const unsigned char* src, int x, int y, int w, int h, unsigned char* block)
	{
		int i, j;

		if ((w - x >= 4) && (h - y >= 4))
		{
			// Full Square shortcut
			src += x * 4;
			src += y * w * 4;
			for (i = 0; i < 4; ++i)
			{
				*(unsigned int*)block = *(unsigned int*)src; block += 4; src += 4;
				*(unsigned int*)block = *(unsigned int*)src; block += 4; src += 4;
				*(unsigned int*)block = *(unsigned int*)src; block += 4; src += 4;
				*(unsigned int*)block = *(unsigned int*)src; block += 4;
				src += (w * 4) - 12;
			}
			return;
		}

		int bw = imin(w - x, 4);
		int bh = imin(h - y, 4);
		int bx, by;

		const int rem[] =
		{
		   0, 0, 0, 0,
		   0, 1, 0, 1,
		   0, 1, 2, 0,
		   0, 1, 2, 3
		};

		for (i = 0; i < 4; ++i)
		{
			by = rem[(bh - 1) * 4 + i] + y;
			for (j = 0; j < 4; ++j)
			{
				bx = rem[(bw - 1) * 4 + j] + x;
				block[(i * 4 * 4) + (j * 4) + 0] =
					src[(by * (w * 4)) + (bx * 4) + 0];
				block[(i * 4 * 4) + (j * 4) + 1] =
					src[(by * (w * 4)) + (bx * 4) + 1];
				block[(i * 4 * 4) + (j * 4) + 2] =
					src[(by * (w * 4)) + (bx * 4) + 2];
				block[(i * 4 * 4) + (j * 4) + 3] =
					src[(by * (w * 4)) + (bx * 4) + 3];
			}
		}
	}

	struct StbImageTmp
	{
		u8* data = nullptr;
		int width = 0;
		int height = 0;
		int channels = 0;
		std::string error = "";

		StbImageTmp(const StbImageTmp&) = delete;		
		StbImageTmp& operator=(const StbImageTmp&) = delete;
		
		StbImageTmp& operator=(StbImageTmp&& other)
		{
			data = other.data;
			width = other.width;
			height = other.height;
			error = other.error;
			isStb = other.isStb;

			other.data = nullptr;
			return *this;
		}

		StbImageTmp(StbImageTmp&& other) : 
			data(other.data), width(other.width), height(other.height), 
			channels(other.height), error(other.error), isStb(other.isStb)
		{
			other.data = nullptr;
		}

		StbImageTmp(int width, int height, int channels) : 
			width(width), height(height), channels(channels)
		{
			data = new u8[width * height * channels];
			isStb = false;
		}

		StbImageTmp(std::span<const u8> image)
		{
			data = stbi_load_from_memory(image.data(), image.size(), &width, &height, nullptr, 4);
			channels = 4;
			isStb = true;

			if (!data)
				error = stbi_failure_reason();
		}

		StbImageTmp(std::string error) : error(error) {}

		~StbImageTmp()
		{
			if (data)
			{
				if (isStb) stbi_image_free(data);
				else delete[] data;
			}

			data = nullptr;
		}

		StbImageTmp Resize(int nextWidth, int nextHeight)
		{
			if (channels != 4)
				return StbImageTmp("Only ARGB images are supported for resizing");

			StbImageTmp result(nextWidth, nextHeight, channels);

			stbir_resize_uint8_linear(
				data, width, height, width * channels, 
				result.data, result.width, result.height, result.width * result.channels, 
				STBIR_RGBA);

			return result;
		}

	private:
		bool isStb = false;
	};
}

DDSConv::ConversionResult DDSConv::ConvertImage(const std::vector<u8>& imgData, bool DXT5, int Width, int Height, bool ResizeIfNeeded)
{
	if ((Width % 4) || (Height % 4))
		return DDSConv::ConversionResult::Fail("Width and height must be multiples of 4");

	StbImageTmp image{ imgData };

	if (image.error.size())
		return DDSConv::ConversionResult::Fail("Failed to load the source image: " + image.error);

	bool imageResized = false;
	if (image.width != Width || image.height != Height)
	{
		if (!ResizeIfNeeded)
			return DDSConv::ConversionResult::Fail("Image dimensions don't match the required ones.");

		image = image.Resize(Width, Height);
		if (image.error.size())
			return DDSConv::ConversionResult::Fail("Failed to resize the source image: " + image.error);

		imageResized = true;
	}

	if (image.width != Width || image.height != Height)
		return DDSConv::ConversionResult::Fail("Resize failure");

	const int BytePerBlock = DXT5 ? 16 : 8;

	//Hacky af but works(TM)
	Buffer bin;
	bin.ByteOrder = Endianness::LittleEndian;
	bin.Write("DDS ");
	bin.Write((u32)0x7c);
	bin.Write((u32)0xA1007);
	bin.Write((u32)image.height);
	bin.Write((u32)image.width);
	bin.Write((u32)((image.width * image.height / 16) * BytePerBlock)); //Linear size
	bin.Write((u32)0);
	bin.Write((u32)0); //Mipmap count (?)
	for (int i = 0; i < 11; i++)
		bin.Write((u32)0);
	bin.Write((u32)0x20);
	bin.Write((u32)0x4);
	bin.Write(DXT5 ? "DXT5" : "DXT1"); //Not sure about the difference between DXT3 and 5
	for (int i = 0; i < 5; i++)
		bin.Write((u32)0);
	bin.Write((u32)0x401008);
	for (int i = 0; i < 4; i++)
		bin.Write((u32)0);

	unsigned char block[64];
	std::vector<u8> dst(BytePerBlock);
	int x, y;

	for (y = 0; y < image.height; y += 4)
	{
		for (x = 0; x < image.width; x += 4)
		{
			extractBlock(image.data, x, y, image.width, image.height, block);
			stb_compress_dxt_block(dst.data(), block, DXT5, STB_DXT_DITHER | STB_DXT_HIGHQUAL);
			bin.Write(dst);
		}
	}

	return DDSConv::ConversionResult::Success(bin.getBuffer(), imageResized);
}
#endif