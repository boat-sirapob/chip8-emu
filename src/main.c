#include <SDL2/SDL_assert.h>
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_keyboard.h>
#include <SDL2/SDL_pixels.h>
#include <SDL2/SDL_render.h>
#include <SDL2/SDL_scancode.h>
#include <SDL2/SDL_stdinc.h>
#include <SDL2/SDL_surface.h>
#include <SDL2/SDL_timer.h>
#include <SDL2/SDL_video.h>
#include <stdio.h>
#include <SDL2/SDL.h>
#include <stdbool.h>
#include <stdlib.h>
#include <tinyfiledialogs.h>

// display
#define DISPLAY_WIDTH 64
#define DISPLAY_HEIGHT 32

bool display[DISPLAY_HEIGHT][DISPLAY_WIDTH];

// window
const int WINDOW_WIDTH = 1280;
const int WINDOW_HEIGHT = 640;

const char* WINDOW_TITLE = "CHIP-8";

const double CELL_WIDTH = (double)WINDOW_WIDTH / DISPLAY_WIDTH;
const double CELL_HEIGHT = (double)WINDOW_HEIGHT / DISPLAY_HEIGHT;

const SDL_Color ON_COLOR = {0xF0, 0xED, 0xCC, 255};
const SDL_Color OFF_COLOR = {0x02, 0x34, 0x3F, 255};

// memory
#define MEMORY_SIZE 4096
#define PROGRAM_START_OFFSET 512

unsigned char memory[MEMORY_SIZE];

// stack
#define MAX_STACK_SIZE 16

unsigned short stack[MAX_STACK_SIZE];
unsigned char top_of_stack = 0;

// registers
unsigned char registers[16]; // general purpose registers

unsigned short program_counter = PROGRAM_START_OFFSET;
unsigned short index_register = 0;

// timers
#define TIMER_FREQ 60
#define PROCESSOR_FREQ 700

const double TIMER_INTERVAL = 1000.0/TIMER_FREQ;
const double PROCESSING_INTERVAL = 1000.0/PROCESSOR_FREQ;

unsigned char delay_timer = 0;
unsigned char sound_timer = 0;

// keypad
bool keypad_state[16];

// 1 2 3 C
// 4 5 6 D
// 7 8 9 E
// A 0 B F
SDL_Scancode keypad_map[] = {
    SDL_SCANCODE_1, SDL_SCANCODE_2, SDL_SCANCODE_3, SDL_SCANCODE_4,
    SDL_SCANCODE_Q, SDL_SCANCODE_W, SDL_SCANCODE_E, SDL_SCANCODE_R,
    SDL_SCANCODE_A, SDL_SCANCODE_S, SDL_SCANCODE_D, SDL_SCANCODE_F,
    SDL_SCANCODE_Z, SDL_SCANCODE_X, SDL_SCANCODE_C, SDL_SCANCODE_V
};

// SDL
SDL_Window* window;
SDL_Renderer* renderer;

bool paused = false;
bool step = false;
bool debug_info_on = false;

bool modern_flag = true;

// font
#define FONT_START_OFFSET 0
#define FONT_HEIGHT 5
const unsigned char FONT[] = {
    0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
    0x20, 0x60, 0x20, 0x20, 0x70, // 1
    0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
    0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
    0x90, 0x90, 0xF0, 0x10, 0x10, // 4
    0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
    0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
    0xF0, 0x10, 0x20, 0x40, 0x40, // 7
    0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
    0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
    0xF0, 0x90, 0xF0, 0x90, 0x90, // A
    0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
    0xF0, 0x80, 0x80, 0x80, 0xF0, // C
    0xE0, 0x90, 0x90, 0x90, 0xE0, // D
    0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
    0xF0, 0x80, 0xF0, 0x80, 0x80  // F
};

void dispose(void) {
    SDL_DestroyWindow(window);
    SDL_DestroyRenderer(renderer);
}

void push_stack(unsigned short data) {
    if (top_of_stack == MAX_STACK_SIZE) {
        printf("Error: stack overflow.");
        dispose();
        exit(1);
    }

    stack[top_of_stack] = data;
    top_of_stack++;
}

unsigned short pop_stack(void) {
    if (top_of_stack == 0) {
        printf("Error: stack is empty.");
        dispose();
        exit(1);
    }

    top_of_stack--;
    return stack[top_of_stack];
}

const char* open_file_dialog(void) {
    const char* filterPatterns[] = {"*.ch8"};
    const char* outPath = tinyfd_openFileDialog("Open Chip-8 ROM", ".", 1, filterPatterns, "Char-8 files", 0);

    if (outPath == NULL) {
        exit(0);
    }
    return outPath;
}

void load_file(const char *path) {
    FILE *ptr = fopen(path, "rb");

    // handle error
    if (ptr == NULL) {
        printf("Failed to load file.");
        exit(1);
    }

    // load into memory
    fread(memory+PROGRAM_START_OFFSET, sizeof(memory), 1, ptr);

    fclose(ptr);
}

void open_file() {
    // open file
    const char* outPath = open_file_dialog();
    
    // load into memory
    load_file(outPath);
}

void load_font() {
    for (size_t i = 0; i < sizeof(FONT); i++) {
        memory[i] = FONT[i];
    }
}

void clear_display(void) {
    if (debug_info_on) {
        puts("Clearing display.");
    }

    for (size_t row = 0; row < DISPLAY_HEIGHT; row++) {
        for (size_t col = 0; col < DISPLAY_WIDTH; col++) {
            display[row][col] = 0;
        }
    }
}

void reset() {

    // initialize memory
    for (size_t i = 0; i < MEMORY_SIZE; i++) {
        memory[i] = 0;
    }

    // initialize display
    clear_display();

    // initialize keypad state
    for (size_t i = 0; i < 16; i++) {
        keypad_state[i] = false;
    }

    // initialize stack
    for (size_t i = 0; i < MAX_STACK_SIZE; i++) {
        stack[i] = false;
    }
    top_of_stack = 0;

    // initialize registers
    for (size_t i = 0; i < 16; i++) {
        registers[i] = 0;
    }

    // initialize pointers
    program_counter = PROGRAM_START_OFFSET;
    index_register = 0;

    // initialize timers
    delay_timer = 0;
    sound_timer = 0;

    // open file
    open_file();

    // load font
    load_font();
}

void initialize(void) {

    reset();

    // initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("Failed to initialize. SDL_Error: %s\n", SDL_GetError());
        dispose();
        exit(1);
    }

    // initialize window and renderer
    if (SDL_CreateWindowAndRenderer(WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_RESIZABLE, &window, &renderer) < 0) {
        printf("Failed to create window and renderer. SDL_Error: %s\n", SDL_GetError());
        dispose();
        exit(1);
    }
}

void draw_pixel(size_t x, size_t y, SDL_Color color) {
    SDL_Rect rect;
    rect.x = x * CELL_WIDTH;
    rect.y = y * CELL_HEIGHT;
    rect.w = CELL_WIDTH;
    rect.h = CELL_HEIGHT;

    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_RenderFillRect(renderer, &rect);
}

void DEBUG_display() {
    for (size_t row = 0; row < DISPLAY_HEIGHT; row++) {
        for (size_t col = 0; col < DISPLAY_WIDTH; col++) {
            if (display[row][col] == 1) {
                printf("1");
            } else {
                printf("0");
            }
        }
        printf("\n");
    }
}

void draw_sprite(unsigned char x, unsigned char y, unsigned char n) {
    // sprites are 8-bit bytes from starting at I

    if (debug_info_on) {
        printf("Draw X: %d\n", x);
        printf("Draw Y: %d\n", y);
    }

    registers[0xF] = 0;

    for (int i = 0; i < n; i++) {
        if (y + i >= DISPLAY_HEIGHT) { break; }

        unsigned char sprite_row = memory[index_register+i];

        // go through each bit
        for (int j = 0; j < 8; j++) {
            if (x + j >= DISPLAY_WIDTH) { break; }

            unsigned char bit = ((sprite_row >> (8-j-1)) & 1);

            // if any pixels turned off, VF = 1 else 0
            if (bit && display[y+i][x+j]) { registers[0xF] = 1; } 
            
            // 0 - transparent, 1 - flip
            display[y+i][x+j] ^= bit;
        }
    }
}

void show_display(void) {
    if (debug_info_on) {
        puts("Updating display.");
    }

    SDL_Color color;
    for (size_t row = 0; row < DISPLAY_HEIGHT; row++) {
        for (size_t col = 0; col < DISPLAY_WIDTH; col++) {
            if (display[row][col] == 1) {
                color = ON_COLOR;
            } else {
                color = OFF_COLOR;
            }
            draw_pixel(col, row, color);
        }
    }
    SDL_RenderPresent(renderer);
}

void process_instruction(void) {
    // fetch
    unsigned char instruction_byte1 = memory[program_counter];
    unsigned char instruction_byte2 = memory[program_counter+1];

    // decode
    unsigned char nibble1 = instruction_byte1 >> 4;
    unsigned char nibble2 = instruction_byte1 & 0xF;
    unsigned char nibble3 = instruction_byte2 >> 4;
    unsigned char nibble4 = instruction_byte2 & 0xF;
    unsigned short nnn = (nibble2 << 8) + instruction_byte2;

    // debugging
    if (debug_info_on) {
        printf("\nRegisters: ");
        for (size_t i = 0; i < 16; i++) {
            printf("%x", registers[i]);
        }
        printf("\nProgram Counter: %x\n", program_counter);
        printf("Index Register: %x\n", index_register);
        printf("Current Instruction: %x%x%x%x\n", nibble1, nibble2, nibble3, nibble4);
    }

    program_counter += 2;

    bool valid = 1;

    switch (nibble1) {
        case 0x0: {
            if (nibble2 == 0x0 && nibble3 == 0xE) {
                switch (nibble4) {
                    case 0x0:
                        // 00E0 - clear screen
                        clear_display();
                        show_display();
                        break;
                    case 0xE:
                        // 00EE - subroutine return
                        program_counter = pop_stack();
                        break;
                    default:
                        valid = false;
                        break;
                }
            } else {
                valid = false;
            }
            break;
        }
        case 0x1: {
            // 1NNN - jump
            program_counter = nnn;
            break;
        }
        case 0x2: {
            // 2NNN - subroutine call
            push_stack(program_counter);
            program_counter = nnn;
            break;
        }
        case 0x3: {
            // 3XNN - skip if VX == NN
            if (registers[nibble2] == instruction_byte2) { program_counter += 2; }
            break;
        }
        case 0x4: {
            // 4XNN - skip if VX != NN
            if (registers[nibble2] != instruction_byte2) { program_counter += 2; }
            break;
        }
        case 0x5: {
            // 5XY0 - skip if VX == VY
            if (registers[nibble2] == registers[nibble3]) { program_counter += 2; }
            break;
        }
        case 0x6: {
            // 6XNN - set register VX
            unsigned char vx = nibble2;
            registers[vx] = instruction_byte2;
            break;
        }
        case 0x7: {
            // 7XNN - add value to register VX
            unsigned char vx = nibble2;
            registers[vx] += instruction_byte2;
            break;
        }
        case 0x8: {
            switch (nibble4) {
                case 0x0: {
                    // 8XY0 - set VX to VY value
                    registers[nibble2] = registers[nibble3];
                    break;
                }
                case 0x1: {
                    // 8XY1 - VX binary or VY
                    registers[nibble2] |= registers[nibble3];
                    break;
                }
                case 0x2: {
                    // 8XY2 - VX binary and VY
                    registers[nibble2] &= registers[nibble3];
                    break;
                }
                case 0x3: {
                    // 8XY3 - VX xor VY
                    registers[nibble2] ^= registers[nibble3];
                    break;
                }
                case 0x4: {
                    // 8XY4 - VX add VY with carry
                    if (registers[nibble2] > 255 - registers[nibble3]) { registers[0xF] = 1; }
                    else { registers[0xF] = 0; }

                    registers[nibble2] += registers[nibble3];
                    break;
                }
                case 0x5: {
                    // 8XY5 - VX subtract VY with carry
                    if (registers[nibble2] >= registers[nibble3]) { registers[0xF] = 1; }
                    else { registers[0xF] = 0; }

                    registers[nibble2] -= registers[nibble3];
                    break;
                }
                case 0x6: {
                    // 8XY6 - shift right

                    if (!modern_flag) {
                        registers[nibble2] = registers[nibble3];
                    }

                    registers[0xF] = registers[nibble2] & 1;

                    registers[nibble2] >>= 1;

                    break;
                }
                case 0x7: {
                    // 8XX7 - VY subtract VX with carry
                    if (registers[nibble3] >= registers[nibble2]) { registers[0xF] = 1; }
                    else { registers[0xF] = 0; }

                    registers[nibble2] = registers[nibble3] - registers[nibble2];
                    break;
                }
                case 0xE: {
                    // 8XYE - shift left

                    if (!modern_flag) {
                        registers[nibble2] = registers[nibble3];
                    }
                    registers[0xF] = (registers[nibble2] >> 7) & 1;

                    registers[nibble2] <<= 1;

                    break;
                }
            }
            break;
        }
        case 0x9: {
            // 9XY0 - skip if VX != VY
            if (registers[nibble2] != registers[nibble3]) { program_counter += 2; }
            break;
        }
        case 0xA: {
            // ANNN - set index register I
            index_register = nnn;
            break;
        }
        case 0xB: {
            if (!modern_flag) {
                // BNNN - jump to NNN with offset V0
                program_counter = nnn + registers[0];
            } else {
                // BXNN - jump to XNN with offset VX
                program_counter = nnn + registers[nibble2];
            }

            break;
        }
        case 0xC: {
            // CXNN - set VX to random
            registers[nibble2] = rand() & instruction_byte2;
            break;
        }
        case 0xD: {
            // DXYN - display/draw
            unsigned char x_coord = registers[nibble2] & (DISPLAY_WIDTH-1);
            unsigned char y_coord = registers[nibble3] & (DISPLAY_HEIGHT-1);
            unsigned char n_height = nibble4;

            draw_sprite(x_coord, y_coord, n_height);
            show_display();
            break;
        }
        case 0xE: {
            switch (instruction_byte2) {
                case 0x9E: {
                    // EX9E - skip if VX pressed
                    if (keypad_state[registers[nibble2]]) { program_counter += 2; }
                    break;
                }
                case 0xA1: {
                    // EXA1 - skip if VX not pressed
                    if (!keypad_state[registers[nibble2]]) { program_counter += 2; }
                    break;
                }
            }
            break;
        }
        case 0xF: {
            switch (instruction_byte2) {
                case 0x07: {
                    // FX07 - set VX to delay timer
                    registers[nibble2] = delay_timer;
                    break;
                }
                case 0x15: {
                    // FX15 - set delay timer to VX
                    delay_timer = registers[nibble2];
                    break;
                }
                case 0x18: {
                    // FX18 - set sound timer to VX
                    sound_timer = registers[nibble2];
                    break;
                }
                case 0x1E: {
                    // FX1E - add VX to I
                    if (modern_flag) {
                        // handle overflow
                        if (index_register > 0x1000 - registers[nibble2]) { registers[0xF] = 1; }
                    }
                    index_register += registers[nibble2];
                    break;
                }
                case 0x0A: {
                    // FX0A - get key
                    // FIXME: probably should be only on key press

                    bool pressed = false;
                    if (debug_info_on) {
                        printf("Keyboard State: ");
                    }
                    for (size_t key = 0; key < 16; key++) {
                        if (debug_info_on) {
                            printf("%d", keypad_state[key]);
                        }
                        
                        if (keypad_state[key]) {
                            registers[nibble2] = key;
                            pressed = true;
                            break;
                        }
                    }
                    if (debug_info_on) {
                        printf("\n");
                    }
                    if (!pressed) { program_counter -= 2; }
                    break;
                }
                case 0x29: {
                    // FX29 - font character
                    index_register = (registers[nibble2] & 0xF) * FONT_HEIGHT + FONT_START_OFFSET;
                    break;
                }
                case 0x33: {
                    // FX33 - BCD
                    unsigned char value = registers[nibble2];

                    memory[index_register] = value/100;
                    memory[index_register+1] = (value/10) % 10;
                    memory[index_register+2] = value % 10;
                    break;
                }
                case 0x55: {
                    // FX55 - store memory
                    for (size_t i = 0; i <= nibble2; i++) {
                        if (modern_flag) {
                            memory[index_register+i] = registers[i];
                        } else {
                            memory[index_register] = registers[i];
                            index_register++;
                        }
                    }
                    break;
                }
                case 0x65: {
                    // FX65 - load memory
                    for (size_t i = 0; i <= nibble2; i++) {
                        if (modern_flag) {
                            registers[i] = memory[index_register+i];
                        } else {
                            registers[i] = memory[index_register];
                            index_register++;
                        }
                    }
                    break;
                }
            }
            break;
        }
        default:
            valid = false;
            break;
    }
    if (!valid && debug_info_on) {
        puts("Warning: invalid instruction.");
    }
}

void handle_keypad() {
    const Uint8* keyboard = SDL_GetKeyboardState(NULL);

    // 1 2 3 C
    // 4 5 6 D
    // 7 8 9 E
    // A 0 B F
    keypad_state[0x1] = keyboard[keypad_map[0x0]];
    keypad_state[0x2] = keyboard[keypad_map[0x1]];
    keypad_state[0x3] = keyboard[keypad_map[0x2]];
    keypad_state[0xC] = keyboard[keypad_map[0x3]];

    keypad_state[0x4] = keyboard[keypad_map[0x4]];
    keypad_state[0x5] = keyboard[keypad_map[0x5]];
    keypad_state[0x6] = keyboard[keypad_map[0x6]];
    keypad_state[0xD] = keyboard[keypad_map[0x7]];

    keypad_state[0x7] = keyboard[keypad_map[0x8]];
    keypad_state[0x8] = keyboard[keypad_map[0x9]];
    keypad_state[0x9] = keyboard[keypad_map[0xA]];
    keypad_state[0xE] = keyboard[keypad_map[0xB]];

    keypad_state[0xA] = keyboard[keypad_map[0xC]];
    keypad_state[0x0] = keyboard[keypad_map[0xD]];
    keypad_state[0xB] = keyboard[keypad_map[0xE]];
    keypad_state[0xF] = keyboard[keypad_map[0xF]];
}

void handle_keyevents(SDL_KeyboardEvent* event) {

    switch (event->type) {
        case SDL_KEYDOWN: {
            if (debug_info_on) {
                printf("Key Pressed: %s\n", SDL_GetKeyName(event->keysym.sym));
            }
            switch (event->keysym.scancode) {
                case SDL_SCANCODE_P:
                    printf("Paused: %d -> %d\n", paused, !paused);
                    paused = !paused;
                    break;
                case SDL_SCANCODE_N:
                    debug_info_on = true;
                    paused = true;
                    step = true;
                    break;
                case SDL_SCANCODE_O:
                    reset();
                    break;
                case SDL_SCANCODE_M:
                    printf("Modern: %d -> %d\n", modern_flag, !modern_flag);
                    modern_flag = !modern_flag;
                    break;
                case SDL_SCANCODE_I:
                    debug_info_on = !debug_info_on;
                    break;
                default:
                    break;
            }
            break;
        }
    }

    handle_keypad();
}

void run(void) {

    // main loop
    SDL_Event event;
    Uint64 last_processor = SDL_GetTicks64(); // 700hz
    Uint64 last_display = SDL_GetTicks64(); // 60hz

    bool running = true;
    while (running) {
        // event loop
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    running = false;
                    break;
                case SDL_KEYDOWN:
                case SDL_KEYUP:
                    handle_keyevents(&event.key);
                default:
                    break;
            }
        }

        // for calculating time
        Uint64 now = SDL_GetTicks64();

        // processor timer
        if (now - last_processor >= PROCESSING_INTERVAL) {
            last_processor = now;

            if (!paused || step) {
                process_instruction();
                step = false;
            }
        }
        
        // display timer
        if (now - last_display >= TIMER_INTERVAL) {
            last_display = now;

            // decrement timers
            if (delay_timer > 0) { delay_timer--; }
            if (sound_timer > 0) { sound_timer--; }
        }
    }
}

int main(void) {

    initialize();

    run();

    dispose();

    return 0;
}