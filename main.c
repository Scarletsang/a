#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

typedef char i8;
typedef unsigned char u8;
typedef unsigned short u16;
typedef int i32;
typedef unsigned u32;
typedef unsigned long u64;

struct string
{
    char*   data;
    u32     size;
};

#pragma pack(1)
struct bmp_header
{
    // Note: header
    i8  signature[2]; // should equal to "BM"
    u32 file_size;
    u32 unused_0;
    u32 data_offset;

    // Note: info header
    u32 info_header_size;
    u32 width; // in px
    u32 height; // in px
    u16 number_of_planes; // should be 1
    u16 bit_per_pixel; // 1, 4, 8, 16, 24 or 32
    u32 compression_type; // should be 0
    u32 compressed_image_size; // should be 0
    // Note: there are more stuff there but it is not important here
};

struct bmp_pixel
{
    u8  blue;
    u8  green;
    u8  red;
    u8  unused;
};

#define PRINT_ERROR(cstring) write(STDERR_FILENO, cstring, sizeof(cstring) - 1)
#include <assert.h>

struct string   read_entire_file(char* filename)
{
	char* file_data = 0;
	unsigned long	file_size = 0;
    int input_file_fd = open(filename, O_RDONLY);
    if (input_file_fd >= 0)
	{
		struct stat input_file_stat = {0};
		stat(filename, &input_file_stat);
		file_size = input_file_stat.st_size;
        file_data = mmap(0, file_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, input_file_fd, 0);
        close(input_file_fd);
	}
    return (struct string){file_data, file_size};
}

#define CODE_HEADER_SIZE    (8)
#define GET_PIXEL_RELATIVE_DOWN(current_pixel, width, y_offset) (current_pixel - (width * y_offset))
#define GET_PIXEL(start_pixel, width, x, y) (start_pixel + (width * y) + x)

int main(int argc, char** argv)
{
	if (argc != 2)
	{
		PRINT_ERROR("Usage: decoder <input_filename>\n");
		return 1;
	}
    struct string file_content = read_entire_file(argv[1]);
    if (file_content.data == NULL)
    {
		PRINT_ERROR("Error: Failed to read file\n");
        return 1;
    }
    struct bmp_header* header = (struct bmp_header*) file_content.data;
    if ((header->height < CODE_HEADER_SIZE) || (header->width < CODE_HEADER_SIZE))
    {
		PRINT_ERROR("Error: Image dimension too small to fit a code");
        return 1;
    }
    if (header->bit_per_pixel != 32)
    {
		PRINT_ERROR("Error: Image's pixel per bit is not 32");
        return 1;
    }
    struct bmp_pixel* pixels = (struct bmp_pixel*) ((char*)header + header->data_offset);
    struct bmp_pixel* pixel = pixels + (header->width * (CODE_HEADER_SIZE - 1));
    u32 x, y;
    for (y = CODE_HEADER_SIZE - 1; y < header->height; y++)
    {
        for (x = 0; x < (header->width - CODE_HEADER_SIZE); x++)
        {
            assert((pixel == GET_PIXEL(pixels, header->width, x, y)) && "wtf man");
            u32 i = 0;
            for (; i < (CODE_HEADER_SIZE - 1); i++)
            {
                struct bmp_pixel* pixel_new = pixel + i;
                if ((pixel_new->red != 217) || (pixel_new->green != 188) || (pixel_new->blue != 127))
                {
                    break;
                }
            }
            if (i == (CODE_HEADER_SIZE - 1))
            {
                for (i = 1; i < CODE_HEADER_SIZE; i++)
                {
                    struct bmp_pixel* pixel_new = GET_PIXEL_RELATIVE_DOWN(pixel, header->width, i);
                    if ((pixel_new->red != 217) || (pixel_new->green != 188) || (pixel_new->blue != 127))
                    {
                        break;
                    }
                }
                if (i == CODE_HEADER_SIZE)
                {
                    goto code_found;
                }
            }
            pixel++;
        }
        pixel += CODE_HEADER_SIZE;
    }
    if (0)
code_found:
    {
        static char message[512];
        struct bmp_pixel* pixel_message_length = pixel + CODE_HEADER_SIZE - 1;
        u32 message_length = pixel_message_length->blue + pixel_message_length->red;
        u32 message_index = 0;
        struct bmp_pixel* pixel_message_start = GET_PIXEL_RELATIVE_DOWN(pixel, header->width, 2) + 2;
        for (; message_index < message_length;)
        {
            for (u32 i = 0; (message_index < message_length) && (i < (CODE_HEADER_SIZE - 2)); i++)
            {
                message[message_index] = pixel_message_start->red;
                message[message_index + 1] = pixel_message_start->green;
                message[message_index + 2] = pixel_message_start->blue;
                message_index += 3;
                pixel_message_start++;
            }
            pixel_message_start = GET_PIXEL_RELATIVE_DOWN(pixel_message_start, header->width, 1) - (CODE_HEADER_SIZE - 2);
        }
        write(STDOUT_FILENO, message, message_index);
    }
    else
    {
        PRINT_ERROR("No code is found\n");
    }
	return 0;
}
