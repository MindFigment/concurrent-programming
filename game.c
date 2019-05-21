#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <time.h>

#include <game.h>

#define BOARD_WIDTH 15
#define BOARD_HEIGHT 8
#define CELL_SIZE_PX 50
#define SHROOM_SIZE_PX CELL_SIZE_PX / 3
#define BOARD_WIDTH_SIZE_PX BOARD_WIDTH * CELL_SIZE_PX
#define BOARD_HEIGHT_SIZE_PX BOARD_HEIGHT * CELL_SIZE_PX
#define BOARD_X_MARGIN 50 
#define BOARD_Y_MARGIN 50

#define FIELD_START 1

#define RED_SHROOM 3
#define ORANGE_SHROOM 4
#define YELLOW_SHROOM 5

#define STEP_UP 0
#define STEP_DOWN 1
#define STEP_RIGHT 2

#define TRUE 1
#define FALSE 0

#define MAX_PLAYERS 6

typedef struct button_cords_st {
    int x1;
    int y1;
    int x2;
    int y2;
} button_cords_t;

typedef struct path_cell_st {
    int x;
    int y;
    int state;
} path_cell_t;

typedef struct cell_update_st {
    int updatex;
    int updatey;
} cell_update_t;

typedef struct player_st {
    int number;
    int cell;
    int score;
} player_t;

typedef struct path_st {
    path_cell_t *path;
    int path_len;
    player_t *players;
} path_t;

Display *display;
int screen;
GC gc;
Window window;
Colormap colormap;
int x11_file_descriptor;
XColor color, exact_color;
XEvent event;

int already_rolled_dice = FALSE;
path_t *path_struct;
int num_players = 6;
int current_player = 0;
button_cords_t *roll_dice_button_cords;


int main() {
    init_display();
    init_game();
    game_loop();
}


// Wait for Expose event and display game board
void init_game() {
    roll_dice_button_cords = (button_cords_t *)malloc(sizeof(button_cords_t));
    do { XNextEvent(display, &event); } while(event.type != Expose);
    printf("Event type %i\n", event.type);
    if (event.type == Expose) {
        printf("FIRST EXPOSE\n");
        draw_grid();
        draw_board();
        srand(time(NULL));
        int starty, endy;
        starty = rand() % (BOARD_HEIGHT - 1);
        endy = rand() % (BOARD_HEIGHT - 1);
        path_struct = generate_path(0, starty, BOARD_WIDTH - 1, endy);
        draw_path(path_struct);
        draw_current_player_title(current_player);
        draw_players_scores();
        draw_roll_dice_button("ROLL DICE");
        XFlush(display);
    }
}


// Some initialization for xlib display
void init_display() {
    // Open connection to the server
    display = XOpenDisplay(NULL);
    if (display == NULL) {
        printf("Cannot open display\n");
    }
    // Set screen
    screen = DefaultScreen(display);
    // Set GC
    gc = DefaultGC(display, screen);
    // Create window
    window = XCreateSimpleWindow(
        display,
        RootWindow(display, screen),
        0,
        0,
        BOARD_WIDTH_SIZE_PX + 4 * BOARD_X_MARGIN,
        BOARD_HEIGHT_SIZE_PX + 3 * BOARD_Y_MARGIN,
        1,
        BlackPixel(display, screen),
        WhitePixel(display, screen)
    );
    // Procces window close event through event hander so XNextEvent does not fail
    Atom delete_window = XInternAtom(display, "WM_DELETE_WINDOW", 0);
    XSetWMProtocols(display, window, &delete_window, 1);
    // Grab mouse pointer location
    XGrabPointer(display, window, False, ButtonPressMask, GrabModeAsync, GrabModeAsync, None, None, CurrentTime);
    // Select kind of events we are interested in
    XSelectInput(display, window, ExposureMask | KeyPressMask | ButtonPressMask | StructureNotifyMask);
    // Map (show) thw window
    XMapWindow(display, window);
    // Get display colormap
    colormap = DefaultColormap(display, screen);
    // Display file descriptor
    x11_file_descriptor = ConnectionNumber(display);
}


// Function handilng closing the game 
void dispose_display() {
    // Destroy our window
    XDestroyWindow(display, window);
    // Close connection to the server
    XCloseDisplay(display);
    exit(0);
}


// Main game loop
void game_loop() {

    while(1) {
        XNextEvent(display, &event);
        switch (event.type) {

            case Expose:
                printf("Expose %i\n", event.type);
                draw_grid();
                draw_board();
                srand(time(NULL));
                draw_path(path_struct);
                draw_current_player_title(current_player);
                draw_players_scores();
                draw_roll_dice_button("ROLL DICE");
                XFlush(display);
                break;

            case ButtonPress:
                printf("Event: mouse pressed\n");
                // CHECK IF ROLL DICE BUTTON IS PRESSED FOR THE FIRST TIME IN THIS TURN
                int result = check_if_roll_dice(event.xbutton.x, event.xbutton.y);
                printf("Already diced: %i\n", already_rolled_dice);
                printf("Click cords: (%i, %i)\n", event.xbutton.x, event.xbutton.y);
                printf("Result: %i\n", result);
                if (result == 1) {
                    int draw = rand() % 6 + 1; 
                    char *button_string = (char*)malloc(15 * sizeof(char));
                    sprintf(button_string, "%s %i", "You draw:", draw);
                    draw_roll_dice_button(button_string);
                    // XFlush to make sure new button string gets printed before thread goes to sleep
                    XFlush(display);
                    // sleep(1);
                    update_game_state(draw, current_player);
                    draw_roll_dice_button("ROLL DICE");
                    already_rolled_dice = FALSE;
                    current_player = (current_player + 1) % num_players;
                    draw_current_player_title(current_player);
                    XFlush(display);
                }
                break;

            case ClientMessage:
                free(roll_dice_button_cords);
                free(path_struct);
                printf("Event: window closed\n");
                dispose_display();
                break;
        }
    }
}

// Update game state by moving current player by number
// of cells drew from dice, if this cell contains shroom, update score
// and delete this shroom 
void update_game_state(int draw, int player_number) {
    int next_cell_number, current_cell_number, path_len;
    player_t *player = &path_struct->players[player_number];
    current_cell_number = player->cell;
    next_cell_number =  current_cell_number + draw;
    path_len = path_struct->path_len;
    // If next cell is greater then path len, take player to the end of the path
    if (next_cell_number >= path_len) {
        next_cell_number = path_len - 1;
    }
    path_cell_t *current_cell = &path_struct->path[current_cell_number];
    path_cell_t *next_cell = &path_struct->path[next_cell_number];
    if (next_cell->state != 0) {
        if (next_cell->state == RED_SHROOM) { player->score += 3; };
        if (next_cell->state == ORANGE_SHROOM) { player->score += 2; };
        if (next_cell->state == YELLOW_SHROOM) { player->score += 1; };
        next_cell->state = 0;
        draw_player_score(*player);
    }
    if (next_cell_number == path_len - 1) {
        draw_path_cell("grey", *next_cell);
    }
    else {
        draw_path_cell("green", *next_cell);
    }
    if (current_cell_number == 0 || current_cell_number == path_len - 1) {
        draw_path_cell("grey", *current_cell);
    }
    else {
        draw_path_cell("green", *current_cell);
    }
    player->cell = next_cell_number;
    draw_players_positions();
}


// Draw board grid
void draw_grid() {
    XAllocNamedColor(display, colormap, "black", &color, &exact_color);
    XSetForeground(display, gc, color.pixel);

    int x1, y1, x2, y2;
    // int offset_x = BOARD_WIDTH_SIZE_PX + BOARD_X_MARGIN;
    for (int i = 0; i < BOARD_HEIGHT + 1; i++) {
        // Horizontal lines
        x1 = BOARD_X_MARGIN;
        y1 = BOARD_Y_MARGIN + i * CELL_SIZE_PX;
        x2 = BOARD_X_MARGIN + BOARD_WIDTH_SIZE_PX;
        y2 = BOARD_Y_MARGIN + i * CELL_SIZE_PX;
        XDrawLine(display, window, gc, x1, y1, x2, y2);   
        // XDrawLine(display, window, gc, x1 + offset_x, y1, x2 + offset_x, y2); 
    }

    for (int i = 0; i < BOARD_WIDTH + 1; i++) {
        // Vertical lines
        x1 = BOARD_X_MARGIN + i * CELL_SIZE_PX;
        y1 = BOARD_Y_MARGIN;
        x2 = BOARD_X_MARGIN + i * CELL_SIZE_PX;
        y2 = BOARD_Y_MARGIN + BOARD_HEIGHT_SIZE_PX;
        XDrawLine(display, window, gc, x1, y1, x2, y2);
        XDrawLine(display, window, gc, x1, y1, x2, y2);
    }
}


// Draw board with all cells brown, we call this only once at the begining
// and once we generate path it stays unchanged throught the whole game
// so there is no need to keep whole board as 2d array in memory
// we only need to remember path cells
void draw_board() {
    for (int i = 0; i < BOARD_HEIGHT; i++) {
        for (int j = 0; j < BOARD_WIDTH; j++) {
            XAllocNamedColor(display, colormap, "brown", &color, &exact_color);
            XSetForeground(display, gc, color.pixel);
            int x1 = BOARD_Y_MARGIN + j * CELL_SIZE_PX + 1;
            int y1 = BOARD_X_MARGIN + i * CELL_SIZE_PX + 1;
            int width = CELL_SIZE_PX - 1;
            int height = CELL_SIZE_PX - 1;
            XFillRectangle(display, window, gc, x1, y1, width, height);
        }
    }
}


// Draw positions of all players of the path
void draw_players_positions() {
    player_t *players =  path_struct->players;
    path_cell_t *path = path_struct->path;
    for (int i = 0; i < num_players; i++) {
        draw_player(players[i].number, path[players[i].cell]);
    }
}


// Draw path
void draw_path(path_t *path_struct) {
    int n = path_struct->path_len;
    path_cell_t *path = path_struct->path;
    draw_path_cell("grey", path[0]);
    draw_path_cell("grey", path[n - 1]);
    for(int i = 1; i < n - 1; i++) {
        draw_path_cell("green", path[i]);
        if (path[i].state  == 3) {    
            draw_shroom("red", path[i]);
        }
        if (path[i].state == 4) {
            draw_shroom("orange", path[i]);
        }
        if (path[i].state == 5) {
            draw_shroom("yellow", path[i]);
        }
    }
    draw_players_positions();
}


// Draw figure representing player in his current cell
void draw_player(int player, path_cell_t cell) {
    int x1, y1, width, height, offset_x, offset_y;
    width = CELL_SIZE_PX / MAX_PLAYERS * 2;
    height = CELL_SIZE_PX / MAX_PLAYERS * 2;
    char *player_color;
    if (player == 1) {
        offset_x = 1;
        offset_y = height + 1;
    }
    if (player == 2) {
        offset_x = width + 1;
        offset_y = height + 1;
    }
    if (player == 3) {
        offset_x = 2 * width + 1;
        offset_y = height + 1;
    }
    if (player == 4) {
        offset_x = 1;
        offset_y = 2 * height + 1;
    }   
    if (player == 5) {
        offset_x = width + 1;
        offset_y = 2 * height + 1;
    }
    if (player == 6) {
        offset_x = 2 * width + 1;
        offset_y = 2 * height + 1;
    }
    XAllocNamedColor(display, colormap, "white", &color, &exact_color);
    XSetForeground(display, gc, color.pixel);
    x1 = BOARD_Y_MARGIN + cell.x * CELL_SIZE_PX + 1 + offset_x;
    y1 = BOARD_X_MARGIN + cell.y * CELL_SIZE_PX + 1 + offset_y;
    XFillRectangle(display, window, gc, x1, y1, width, height);

    XAllocNamedColor(display, colormap, "black", &color, &exact_color);
    XSetForeground(display, gc, color.pixel);
    char *current_player = (char*)malloc(1 * sizeof(char));
    sprintf(current_player, "%i", player);
    XDrawString(display, window, gc, x1 + height / 3, y1 + 10 + width / 3, current_player, strlen(current_player));
    free(current_player);
}


// Draw one cell of the path
void draw_path_cell(char *cell_color, path_cell_t cell) {
    int x1, y1, width, height;
    XAllocNamedColor(display, colormap, cell_color, &color, &exact_color);
    XSetForeground(display, gc, color.pixel);
    x1 = BOARD_Y_MARGIN + cell.x * CELL_SIZE_PX + 1;
    y1 = BOARD_X_MARGIN + cell.y * CELL_SIZE_PX + 1;
    width = CELL_SIZE_PX - 1;
    height = CELL_SIZE_PX - 1;
    XFillRectangle(display, window, gc, x1, y1, width, height);
}


// Draw one shroom on one of the path cells
void draw_shroom(char *shroom_color, path_cell_t cell) {
    int x1, y1, width, height, angle1, angle2, margin;
    margin = 3;
    XAllocNamedColor(display, colormap, "white", &color, &exact_color);
    XSetForeground(display, gc, color.pixel);
    x1 = BOARD_Y_MARGIN + cell.x * CELL_SIZE_PX + SHROOM_SIZE_PX / 3 + margin;
    y1 = BOARD_X_MARGIN + cell.y * CELL_SIZE_PX + SHROOM_SIZE_PX / 2 + margin;
    width = SHROOM_SIZE_PX / 3;
    height = SHROOM_SIZE_PX / 2;
    XFillRectangle(display, window, gc, x1, y1, width, height);

    XAllocNamedColor(display, colormap, shroom_color, &color, &exact_color);
    XSetForeground(display, gc, color.pixel);
    x1 = BOARD_Y_MARGIN + cell.x * CELL_SIZE_PX + margin;
    y1 = BOARD_X_MARGIN + cell.y * CELL_SIZE_PX + margin;
    width = SHROOM_SIZE_PX;
    height = SHROOM_SIZE_PX;
    angle1 = 0;
    angle2 = 180 * 64;
    XFillArc(display, window, gc, x1, y1, width, height, angle1, angle2);    
}


// Draw which players turn is now
void draw_current_player_title(int player) {
    int x = BOARD_X_MARGIN + BOARD_WIDTH_SIZE_PX / 2 - 30;
    int y = BOARD_Y_MARGIN - 5;

    XAllocNamedColor(display, colormap, "white", &color, &exact_color);
    XSetForeground(display, gc, color.pixel);
    XFillRectangle(display, window, gc, x, y - 10, 100, 10);

    XAllocNamedColor(display, colormap, "black", &color, &exact_color);
    XSetForeground(display, gc, color.pixel);
    char *current_player = (char*)malloc(14 * sizeof(char));
    sprintf(current_player, "%s %i %s", "Player", player + 1, "turn");
    XDrawString(display, window, gc, x, y, current_player, strlen(current_player));
    free(current_player);
}


// Draw legend containing score of every player
void draw_players_scores() {
    player_t *players =  path_struct->players;
    for (int i = 0; i < num_players; i++){
        draw_player_score(players[i]);
    }
}


// Draw row in legend representing this player current score
void draw_player_score(player_t player) {
    int x = BOARD_X_MARGIN + BOARD_WIDTH_SIZE_PX + 5;
    int y = BOARD_Y_MARGIN + player.number * 10 ;
    XAllocNamedColor(display, colormap, "white", &color, &exact_color);
    XSetForeground(display, gc, color.pixel);
    XFillRectangle(display, window, gc, x, y - 10, 200, 10);

    XAllocNamedColor(display, colormap, "black", &color, &exact_color);
    XSetForeground(display, gc, color.pixel);
    char *player_status = (char*)malloc(35 * sizeof(char));
    sprintf(player_status, "%s %i%s %i %s", "Player", player.number, ":", player.score, "points");
    XDrawString(display, window, gc, x, y, player_status, strlen(player_status));
    free(player_status);
}


// Draw button for rolling dice
void draw_roll_dice_button(char *button_string) {
    XAllocNamedColor(display, colormap, "pink", &color, &exact_color);
    XSetForeground(display, gc, color.pixel);
    roll_dice_button_cords->x1 = BOARD_Y_MARGIN + BOARD_WIDTH_SIZE_PX / 2 - 50;
    roll_dice_button_cords->y1 = BOARD_Y_MARGIN + BOARD_HEIGHT_SIZE_PX + 10;
    int width = 100;
    int height = 50;
    roll_dice_button_cords->x2 = roll_dice_button_cords->x1 + width;
    roll_dice_button_cords->y2 = roll_dice_button_cords->y1 + height;
    XFillRectangle(display, window, gc, roll_dice_button_cords->x1, roll_dice_button_cords->y1, width, height);

    XAllocNamedColor(display, colormap, "black", &color, &exact_color);
    XSetForeground(display, gc, color.pixel);
    int x = BOARD_X_MARGIN + BOARD_WIDTH_SIZE_PX / 2 - 30;
    int y = BOARD_Y_MARGIN + BOARD_HEIGHT_SIZE_PX + 40;
    XDrawString(display, window, gc, x, y, button_string, strlen(button_string));
}


// Check if this player already roled dice in this turn
int check_if_roll_dice(int x, int y) {
    if (x >= roll_dice_button_cords->x1 && x <= roll_dice_button_cords->x2)  {
        if (y >= roll_dice_button_cords->y1 && y <= roll_dice_button_cords->y2) {
            if (already_rolled_dice == 0) {
                already_rolled_dice = TRUE;
                return 1;
            }
        }
    }
    return 0;
}


// Path generation at the start of the game
path_t *generate_path(int startx, int starty, int endx, int endy) {
    int size, last_step, last_x, last_y, current_step, r, range, down_range, up_range, right_range, n;
    size = 300;
    path_cell_t *path = (path_cell_t *)malloc(size * sizeof(path_cell_t));
    int *step_history = (int *)malloc(size * sizeof(int));
    path_cell_t current_cell = {startx, starty, FIELD_START};
    cell_update_t cell_update;
    path[0] = current_cell;
    step_history[0] = -1;
    last_step = -1;
    current_step = 1;
    n = 1;
    
    // Main while loop for generating path from start cordinates to end cordinates
    do {
        // Initial probability ranges for each of three possible steps
        // We give down and up range righer probability cause we prefer
        // the path to be longer by going up and down
        down_range = 50;
        up_range = 50;
        right_range = 10;
        // Check if at the bottom boarder of the board
        if (last_y == BOARD_HEIGHT - 1) {
            down_range = 0;
        }
        // Check if at the upper boarder of the board
        if (last_y == 0) {
            up_range = 0;
        }
        // Check if at the x end of the board, if so navigate to endy
        if (last_x == BOARD_WIDTH - 1) {
            if (last_y < endy) {
                up_range = 0;
                down_range = 10;
                right_range = 0;
            }
            if (last_y > endy) {
                up_range = 10;
                down_range = 0;
                right_range = 0;
            }
        }
        // Prevent from going down if previously generated step was up
        if (last_step == STEP_UP) {
            down_range = 0;
        }
        // Prevent from going up if previously generated step was down
        if (last_step == STEP_DOWN) {
            up_range = 0;
        }
        // This block of code ensures that vertical paths are at least one cell apart 
        // if previously generated cell was further then 1 cell from width boarder
        if (last_step == STEP_RIGHT && last_x < BOARD_WIDTH - 2) {
            if (current_step >= 2) {
                if (step_history[current_step - 2] == STEP_UP) {
                    down_range = 0;
                }
                if (step_history[current_step - 2] == STEP_DOWN) {
                    up_range = 0;
                }
            }
        }

        // This block of code ensures that vertical paths are at least one cell apart
        // in the case when we are at the last 2 cells on x axis 
        if (last_step == STEP_RIGHT && last_x == BOARD_WIDTH - 2) {
            if (last_y < endy) {
                up_range = 0;
            }
            if (last_y > endy) {
                down_range = 0;
            }
            if (last_y == endy) {
                up_range = 0;
                down_range = 0;
                right_range = 10;
            }
            if (step_history[current_step - 2] == STEP_UP && endy > last_y) {
                down_range = 0;
                up_range = 0;
                right_range = 10;
            }
            if (step_history[current_step - 2] == STEP_DOWN && endy < last_y) {
                down_range = 0;
                up_range = 0;
                right_range = 10;
            }
        }
        if (last_x == BOARD_WIDTH - 2 && endy == last_y) {
            down_range = 0;
            up_range = 0;
            right_range = 10;
        }

        // Here we actually use range probabilities to generate next cell in the path
        range = up_range + down_range + right_range;
        r = rand() % range;
        last_step = get_step_from_random_number(r, up_range, down_range, right_range);
        cell_update = update_cell_from_step(last_step);
        current_cell.x += cell_update.updatex;
        current_cell.y += cell_update.updatey;
        step_history[current_step] = last_step;
        path[current_step] = current_cell;
        current_step += 1;
        last_x = current_cell.x;
        last_y = current_cell.y;
        n += 1;

    } while(last_x != endx || last_y != endy);

    free(step_history);
    path = realloc(path, n * sizeof(path_cell_t));

    // Generate shroom placement, numbers from 0 to 2 means no shroom
    // 3 - red shroom, 4 - orannge shroom, 5 - yellow shroom
    path[0].state = 0;
    for(int i = 1; i < n; i++) {
        path[i].state = rand() % 6;
    }

    // Place all players at the begining of the path with 0 score
    player_t *players = (player_t *)malloc(num_players * sizeof(player_t));
    for(int i = 0; i < num_players; i++) {
        players[i].number = i + 1;
        players[i].cell = 0;
        players[i].score = 0;
    }

    // Create struct representing our path
    path_t *path_struct = (path_t *)malloc(sizeof(path_t));
    path_struct->path = path;
    path_struct->path_len = n;
    path_struct->players = players;

    return path_struct;
}


// Generate randomly next cell in the path
int get_step_from_random_number(int number, int up_range, int down_range, int right_range) {
    if (number >= 0 && number < up_range) {
        return STEP_UP;
    }
    else if (number >= up_range && number < up_range + down_range) {
        return STEP_DOWN;
    }
    else if (number >= up_range + down_range && number < up_range + down_range + right_range) {
        return STEP_RIGHT;
    }
    else {
        printf("Number %i is to big!", number);
        return EXIT_FAILURE;
    }
}


// Helper function for transforming one of three possible steps
// during path generation; STEP_UP, STEP_DOWN, STEP_RIGHT
// into x and y cordinates
cell_update_t update_cell_from_step(int step) {
    cell_update_t update;
    if (step == STEP_UP) {
        update.updatex = 0;
        update.updatey = -1;
    }
    if (step == STEP_DOWN) {
        update.updatex = 0;
        update.updatey = 1;
    }
    if (step == STEP_RIGHT) {
        update.updatex = 1;
        update.updatey = 0;
    }
    return update;
}



