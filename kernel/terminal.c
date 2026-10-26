#include "terminal.h"
#include <stddef.h>

#define VGA_WIDTH 80
#define VGA_HEIGHT 25

static uint16_t* const VGA_MEMORY = (uint16_t*)0xB8000;

static size_t terminal_row;
static size_t terminal_column;
static uint8_t terminal_color;

static uint8_t make_color(enum terminal_color foreground,
                          enum terminal_color background)
{
    return (uint8_t)(foreground | (background << 4));
}

static uint16_t make_vga_entry(char c, uint8_t color)
{
    return (uint16_t)c | ((uint16_t)color << 8);
}

void terminal_set_color(enum terminal_color foreground,
                        enum terminal_color background)
{
    terminal_color = make_color(foreground, background);
}

void terminal_clear(void)
{
    for (size_t y = 0; y < VGA_HEIGHT; y++)
    {
        for (size_t x = 0; x < VGA_WIDTH; x++)
        {
            const size_t index = y * VGA_WIDTH + x;
            VGA_MEMORY[index] = make_vga_entry(' ', terminal_color);
        }
    }

    terminal_row = 0;
    terminal_column = 0;
}

void terminal_initialize(void)
{
    terminal_row = 0;
    terminal_column = 0;
    terminal_color = make_color(COLOR_LIGHT_GREY, COLOR_BLACK);

    terminal_clear();
}

void terminal_putchar(char c)
{
    if (c == '\n')
    {
        terminal_column = 0;
        terminal_row++;

        if (terminal_row >= VGA_HEIGHT)
        {
            terminal_row = 0;
        }

        return;
    }

    const size_t index = terminal_row * VGA_WIDTH + terminal_column;
    VGA_MEMORY[index] = make_vga_entry(c, terminal_color);

    terminal_column++;

    if (terminal_column >= VGA_WIDTH)
    {
        terminal_column = 0;
        terminal_row++;

        if (terminal_row >= VGA_HEIGHT)
        {
            terminal_row = 0;
        }
    }
}

void terminal_writestring(const char* data)
{
    for (size_t i = 0; data[i] != '\0'; i++)
    {
        terminal_putchar(data[i]);
    }
}

void terminal_backspace(void)
{
    if (terminal_column > 0)
    {
        terminal_column--;
    }
    else if (terminal_row > 0)
    {
        terminal_row--;
        terminal_column = VGA_WIDTH - 1;
    }
    else
    {
        return;
    }

    const size_t index =
        terminal_row * VGA_WIDTH + terminal_column;

    VGA_MEMORY[index] = make_vga_entry(' ', terminal_color);
}

void terminal_write_uint(uint32_t number)
{
    char buffer[10];
    int i = 0;

    if (number == 0)
    {
        terminal_putchar('0');
        return;
    }

    while (number > 0)
    {
        buffer[i++] = '0' + (number % 10);
        number /= 10;
    }

    while (i > 0)
    {
        terminal_putchar(buffer[--i]);
    }
}