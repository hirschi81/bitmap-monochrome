/**
 * @file bitmap.cpp
 *
 * @author Kevin Buffardi, Ph.D
 * @author Joshua Petrin
 * @author Jan Hirsch, M. Sc.
 */

#ifndef BITMAP_CPP_
#define BITMAP_CPP_

#include <algorithm>
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <cstdint>
#include <cmath>
#include "bitmap.h"

typedef unsigned char uchar_t;  ///< Ensure only positive parsing

const int BMP_MAGIC_ID = 2;  ///< Length in bytes of the file identifier.


const uint8_t MONO_R_VAL_ON = 0;  ///< ON red val
const uint8_t MONO_G_VAL_ON = 0;  ///< ON green val
const uint8_t MONO_B_VAL_ON = 0;  ///< ON blue val

const uint8_t MONO_R_VAL_OFF = 255;  ///< OFF red val
const uint8_t MONO_G_VAL_OFF = 255;  ///< OFF green val
const uint8_t MONO_B_VAL_OFF = 255;  ///< OFF blue val

/// Windows BMP-specific format data
struct bmpfile_magic
{
    uchar_t magic[BMP_MAGIC_ID];  ///< 'B' and 'M'
};

/**
 * Generic 14-byte bitmap header
 */
struct bmpfile_header
{
    uint32_t file_size;  ///< The number of bytes in the bitmap file.
    uint16_t creator1;   ///< Two bytes reserved.
    uint16_t creator2;   ///< Two bytes reserved.
    uint32_t bmp_offset; ///< Offset from beginning to bitmap bits.
};

/**
 * @brief Mircosoft's defined header structure for Bitmap version 3.x.
 * 
 * https://msdn.microsoft.com/en-us/library/dd183376%28v=vs.85%29.aspx
 */
struct bmpfile_dib_info
{
  uint32_t header_size;           ///< The size of this header.
  int32_t  width;                 ///< Width of the image, in pixels.
  int32_t  height;                ///< Height of the image, in pixels.
  uint16_t num_planes;            ///< Number of planes. Almost always 1.
  uint16_t bits_per_pixel;        ///< Bits per pixel. Can be 0, 1, 4, 8, 16, 24, or 32.
  uint32_t compression;           ///< https://msdn.microsoft.com/en-us/library/cc250415.aspx
  uint32_t bmp_byte_size;         ///< The size of the image in bytes.
  int32_t  hres;                  ///< Horizontal resolution, pixels/meter
  int32_t  vres;                  ///< Vertical resolution, pixels/meter
  uint32_t num_colors;            ///< The number of color indices used in the color table.
  uint32_t num_important_colors;  ///< The number of colors used by the bitmap.
};

/**
 * @brief The color table for the monochrome image palette. 
 * 
 * Whatever 24-bit color is specified in the palette in the BMP will show up in
 * the actual image.
 */
struct bmpfile_color_table
{
    // I discovered on my system that the entire RGB num in the palette is 
    // parsed as little-endian. Not sure if this is the same on all systems, 
    // but here, the colors are in reverse order.
    uint8_t blue;     ///< Blue component
    uint8_t green;    ///< Green component
    uint8_t red;      ///< Red component
    uint8_t reserved; ///< Should be 0.
};


signed char Bitmap::open(const std::string& filename)
{
    std::ifstream file(filename.c_str(), std::ios::in | std::ios::binary);
    
    if (file.fail())
    {
		
		std::cout << filename << " could not be opened. Does it exist? " << "Is it already open by another program?\n";
        //std::remove(filename.c_str());
		return -1;        
    }
    else
    {
        bmpfile_magic magic;
        file.read((char*)(&magic), sizeof(magic));
        
        // Check to make sure that the first two bytes of the file are the "BM"
        // identifier that identifies a bitmap image.
        if (magic.magic[0] != 'B' || magic.magic[1] != 'M')
        {
            std::cout << filename << " is not in proper BMP format; it does " << "not begin with the magic bytes!\n";
			file.close();
			return -2;
        }
        else
        {
            // Read the file headers
            bmpfile_header header;
            file.read((char*)(&header), sizeof(header));

            bmpfile_dib_info dib_info;
            file.read((char*)(&dib_info), sizeof(dib_info));

            // Read the 2-color palette for monochrome
            bmpfile_color_table color1;
            file.read((char*)(&color1), sizeof(color1));

            bmpfile_color_table color2;
            file.read((char*)(&color2), sizeof(color2));


            // Only support for 1-bit images
            if (dib_info.bits_per_pixel != 1)
            {
                std::cout << filename << " uses " << dib_info.bits_per_pixel << " bits per pixel (bit depth). This implementation" << " of Bitmap only supports 1-bit (monochrome)." << std::endl;
				file.close();
				return -3;
            }
            // No support for compressed images
            else if (dib_info.compression != 0)
            {
                std::cout << filename << " is compressed. " << "Bitmap only supports uncompressed images." << std::endl;
				file.close();
				return -4;
            }
            // Check for the reserved bits in the color palette
            else if (color1.reserved != 0)
            {
                std::cout << filename << " does not have a good color palette" << " for monochrome display;" << " its first reserved bits are not 0." << std::endl;
				file.close();
				return -5;
            }
            else if (color2.reserved != 0)
            {
                std::cout << filename << " does not have a good color palette" << " for monochrome display;" << " its second reserved bits are not 0." << std::endl;
				file.close();
				return -6;
            }
            else  // All clear! Bitmap is (probably) in proper format.
            {		        
                // A negative height means the rows are stored top-to-bottom
                // in the file. We track this and reverse after loading if needed.
                bool flip = true;
                if (dib_info.height < 0)
                {
                    flip = false;
                    dib_info.height = -dib_info.height;
                }

                // Move to the data
                file.seekg(header.bmp_offset);

                // Bytes per row, padded to 4-byte boundary (BMP requirement).
                size_t stride = static_cast<size_t>(((dib_info.width + 31) / 32) * 4);
                size_t total_bytes = stride * dib_info.height;

                // Read the entire pixel data block directly into bitdata_.
                bitdata_.resize(total_bytes);
                file.read(reinterpret_cast<char*>(bitdata_.data()),
                          static_cast<std::streamsize>(total_bytes));
                width_ = static_cast<uint32_t>(dib_info.width);
                height_ = static_cast<uint32_t>(dib_info.height);

                // BMP rows are stored bottom-to-top by default.
                // Reverse row order in-place so bitdata_ is top-to-bottom.
                if (flip)
                {
                    std::vector<uint8_t> tmp(stride);
                    for (uint32_t lo = 0, hi = height_ - 1; lo < hi; ++lo, --hi)
                    {
                        uint8_t* lo_ptr = bitdata_.data() + lo * stride;
                        uint8_t* hi_ptr = bitdata_.data() + hi * stride;
                        std::copy(lo_ptr, lo_ptr + stride, tmp.data());
                        std::copy(hi_ptr, hi_ptr + stride, lo_ptr);
                        std::copy(tmp.data(), tmp.data() + stride, hi_ptr);
                    }
                }
            }
            file.close();
			return 0;
        }//end else (is an image)
    }//end else (can open file)
}


signed char Bitmap::save(const std::string& filename) const
{
    std::ofstream file(filename.c_str(), std::ios::out | std::ios::binary);

    if (file.fail())
    {
        std::cout << filename << " could not be opened for editing. "
                  << "Is it already open by another program, "
                  << "or is it read-only?"
                  << std::endl;
        return -1;
    }
    else if(!isImage())
    {
        std::cout << "Bitmap cannot be saved. It is not a valid image."
                  << std::endl;
		return -2;
    }
    else
    {
        // Write all the header information that the BMP file format requires.
        bmpfile_magic magic;
        magic.magic[0] = 'B';
        magic.magic[1] = 'M';
        file.write((char*)(&magic), sizeof(magic));

        size_t stride = static_cast<size_t>(((width_ + 31) / 32) * 4);

        bmpfile_header header = { 0 };
        header.bmp_offset = sizeof(bmpfile_magic) + sizeof(bmpfile_header)
                + sizeof(bmpfile_dib_info) + 2*sizeof(bmpfile_color_table);
        header.file_size = header.bmp_offset
                + static_cast<uint32_t>(stride) * static_cast<uint32_t>(height_);

        file.write((char*)(&header), sizeof(header));
        bmpfile_dib_info dib_info = { 0 };
        dib_info.header_size = sizeof(bmpfile_dib_info);
        dib_info.width = static_cast<int32_t>(width_);
        dib_info.height = static_cast<int32_t>(height_);
        dib_info.num_planes = 1;
        dib_info.bits_per_pixel = 1;  // monochrome
        dib_info.compression = 0;
        dib_info.bmp_byte_size = 0;
        dib_info.hres = 200;
        dib_info.vres = 200;
        dib_info.num_colors = 2;
        dib_info.num_important_colors = 0;
        file.write((char*)(&dib_info), sizeof(dib_info));

        // Color palettes.
        // First is the '0' color...
        bmpfile_color_table off_color;
        off_color.red = MONO_R_VAL_OFF;
        off_color.green = MONO_G_VAL_OFF;
        off_color.blue = MONO_B_VAL_OFF;
        off_color.reserved = 0;
        file.write((char*)(&off_color), sizeof(off_color));
        // ...then the '1' color
        bmpfile_color_table on_color;
        on_color.red = MONO_R_VAL_ON;
        on_color.green = MONO_G_VAL_ON;
        on_color.blue = MONO_B_VAL_ON;
        on_color.reserved = 0;
        file.write((char*)(&on_color), sizeof(on_color));

        // Write pixel rows bottom-to-top, as required by BMP format.
        // bitdata_ is stored top-to-bottom, so iterate in reverse.
        for (int32_t row = height_ - 1; row >= 0; --row)
        {
            file.write(reinterpret_cast<const char*>(bitdata_.data() + row * stride),
                       static_cast<std::streamsize>(stride));
        }
    }
    file.close();
	return 0;
}
    

bool Bitmap::isImage() const
{
    return width_ > 0 && height_ > 0;
}


PixelMatrix Bitmap::toPixelMatrix() const
{
    if( !isImage() )
        return PixelMatrix();

    size_t stride = static_cast<size_t>(((width_ + 31) / 32) * 4);
    PixelMatrix result(height_);

    for (uint32_t row = 0; row < height_; ++row)
    {
        const uint8_t* row_ptr = bitdata_.data() + row * stride;
        std::vector<Pixel>& row_pixels = result[row];
        row_pixels.resize(width_);
        uint32_t px = 0;

        for (uint32_t col = 0; col < width_ / 8; ++col)
        {
            for (int bit = 7; bit >= 0; --bit)
            {
                row_pixels[px++] = Pixel((row_ptr[col] & (1 << bit)) != 0);
            }
        }

        for (uint32_t rev_bit = 0; rev_bit < width_ % 8; ++rev_bit)
        {
            row_pixels[px++] = Pixel((row_ptr[width_ / 8]
                & (1 << (7 - rev_bit))) != 0);
        }
    }

    return result;
}


void Bitmap::fromPixelMatrix(const PixelMatrix & values)
{
    if (values.empty() || values[0].empty())
    {
        width_ = 0;
        height_ = 0;
        bitdata_.clear();
        return;
    }

    uint32_t w = static_cast<uint32_t>(values[0].size());
    uint32_t h = static_cast<uint32_t>(values.size());
    size_t stride = static_cast<size_t>(((w + 31) / 32) * 4);

    bitdata_.assign(stride * h, 0);

    for (uint32_t row = 0; row < h; ++row)
    {
        uint8_t* row_ptr = bitdata_.data() + row * stride;
        const std::vector<Pixel>& row_data = values[row];

        int bytes_written = 0;
        int bit = 7;

        for (size_t col = 0; col < row_data.size(); ++col)
        {
            if (row_data[col].on)
                row_ptr[bytes_written] |= static_cast<uint8_t>(1 << bit);

            if (bit > 0)
            {
                --bit;
            }
            else
            {
                ++bytes_written;
                bit = 7;
            }
        }
    }

    width_ = w;
    height_ = h;
}

#endif //BITMAP_CPP_