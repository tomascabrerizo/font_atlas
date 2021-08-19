#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

typedef uint8_t u8;
typedef int8_t i8;

typedef uint16_t u16;
typedef int16_t i16;

typedef uint32_t u32;
typedef int32_t i32;

typedef uint64_t u64;
typedef int64_t i64;

typedef float f32;

typedef struct
{
    void *base;
    size_t used;
    size_t size;
} Arena;

typedef struct
{
    size_t old_used;
    Arena *arena;
} TempArena;

#define KYLOBYTES(a) ((a)*1024LL)
#define MEGABYTES(a) (KYLOBYTES(a)*1024LL)

void InitArena(Arena *arena, size_t size)
{
    arena->size = size;
    arena->base = (void *)malloc(arena->size);
    arena->used = 0;
}

#define PushStruct(arena, type) (type *)ArenaPush((arena), (sizeof(type)))

void *ArenaPush(Arena *arena, size_t size)
{
    assert(arena->used + size < arena->size); 
    void *result = arena->base + arena->used;
    arena->used += size;
    return result;
}

TempArena BeginTempArena(Arena *arena)
{
    TempArena result = {};
    result.old_used = arena->used;
    result.arena = arena;
    return result;
}

void EndTempArena(TempArena *temp_arena)
{
    size_t number_bytes = temp_arena->arena->used - temp_arena->old_used;
    memset(temp_arena->arena->base+temp_arena->old_used, 0, number_bytes);
    temp_arena->arena->used = temp_arena->old_used;
}

void PrintIntArena(Arena *arena)
{
    printf("Dump arena\n");
    for(i32 i = 0; i < arena->used/4; ++i)
    {
        i32 *num = (i32 *)arena->base + i;
        printf("%d ", *num);
    }
    printf("\n");
}

void WriteU8Texture(u32 *buffer, u32 pitch_in_pixels, u8 *bitmap, u32 bitmap_width, u32 bitmap_height)
{
    u32 *row_ptr = buffer;
    u8 *row_bitmap = bitmap;
    for(u32 y = 0; y < bitmap_height; ++y)
    {
        u32 *col_ptr = row_ptr;
        u8 *col_bitmap = row_bitmap;
        for(u32 x = 0; x < bitmap_width; ++x)
        {
            u32 *pixel = col_ptr;
            u32 color = *col_bitmap;
            *pixel = (color << 24 |
                      color << 16 |
                      color << 8 |
                      color << 0);
            
            col_ptr++;
            col_bitmap++;

        }
        row_ptr += pitch_in_pixels;
        row_bitmap += bitmap_width;
    }
}

#define ASCII_OFFSET 32
#define NUM_OF_CHAR 96

int main()
{
    Arena memory = {};
    InitArena(&memory, MEGABYTES(40));
    
    // NOTE: Generate bitmap_glyps from file
    TempArena file_arena = BeginTempArena(&memory); 
    
    FILE *font_file = fopen("fonts/UbuntuMono-Regular.ttf", "rb");
    fseek(font_file, 0, SEEK_END);
    int file_size = ftell(font_file);
    fseek(font_file, 0, SEEK_SET);

    u8 *ttf_buffer = ArenaPush(&memory, file_size);
    fread(ttf_buffer, 1, file_size, font_file);

    fclose(font_file);
    
    stbtt_fontinfo font;
    stbtt_InitFont(&font, ttf_buffer, stbtt_GetFontOffsetForIndex(ttf_buffer, 0));
    
    u8 *bitmap[NUM_OF_CHAR];
    i32 bitmap_width[NUM_OF_CHAR];
    i32 bitmap_height[NUM_OF_CHAR];
    
    u32 max_bitmap_width = 0;
    u32 max_bitmap_height = 0;

    for(int i = 0; i < NUM_OF_CHAR; ++i)
    {
        bitmap[i] = stbtt_GetCodepointBitmap(&font, 0, stbtt_ScaleForPixelHeight(&font, 200), (char)(i+ASCII_OFFSET), &bitmap_width[i], &bitmap_height[i], 0, 0);
        max_bitmap_width = bitmap_width[i] > max_bitmap_width ? bitmap_width[i] : max_bitmap_width;
        max_bitmap_height = bitmap_height[i] > max_bitmap_height ? bitmap_height[i] : max_bitmap_height;
    }

    EndTempArena(&file_arena);
    
    i32 atlas_col = 16;
    i32 atlas_rows = 6;
    u32 atlas_width = atlas_col *  max_bitmap_width; 
    u32 atlas_height = atlas_rows * max_bitmap_height; 
    size_t atlas_size = atlas_width*atlas_height*sizeof(u32);
    
    printf("glyph bitmap w:%d, h:%d\n", max_bitmap_width, max_bitmap_height);
    printf("atlas w:%d, h:%d\n", atlas_width, atlas_height);
    printf("size: %zu\n", atlas_size);
    
    // NOTE: Alloc space for the final font atlas
    u32 *font_atlas = (u32 *)ArenaPush(&memory, atlas_size);
    
    // NOTE: Generate the final font atlas texture
    u32 *row_ptr = font_atlas;
    for(i32 row = 0; row < atlas_rows; ++row)
    {
        u32 *col_ptr = row_ptr;
        for(i32 col = 0; col < atlas_col; ++col)
        {
            i32 bitmap_index = (row*atlas_col+col);
            WriteU8Texture(col_ptr, atlas_width, bitmap[bitmap_index], bitmap_width[bitmap_index], bitmap_height[bitmap_index]);
            
            col_ptr += max_bitmap_width;
        }
        row_ptr += (atlas_width * max_bitmap_height);
    }

    stbi_write_bmp("font_atlas.bmp", atlas_width, atlas_height, 4, font_atlas);
    printf("atlas was generated\n");

    return 0;
}
