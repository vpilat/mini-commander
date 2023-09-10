#include <ncurses.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>
#include <sys/utsname.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>

#define CMD_MAX 16384

typedef struct FileNode {
    char name[256];
    time_t last_access_time;
    off_t size;
    mode_t chmod;
    uid_t chown;
    int is_dir;
    int is_executable;
    int is_link;
    int is_selected;
    struct FileNode *next;
} FileNode;

FileNode *left_files = NULL;
FileNode *right_files = NULL;

// Global windows
WINDOW *win1;
WINDOW *win2;

struct utsname unameData;
struct passwd *pw;
const char *username;

int cursor_pos = 0;
int cmd_offset = 0;
int prompt_length = 0;

char cmd[CMD_MAX] = {0};
int cmd_len = 0;

char left_path[CMD_MAX] = "cwd";
char right_path[CMD_MAX] = "/";
char *current_path = left_path;


FileNode* read_directory(const char *path) {
    DIR *dir;
    struct dirent *entry;
    struct stat file_stat;
    FileNode *head = NULL, *current = NULL;

    if ((dir = opendir(path)) == NULL) {
        return NULL;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0) continue;
        if (strcmp(entry->d_name, "..") == 0 && strcmp(path, "/") == 0) continue;

        FileNode *new_node = (FileNode*) malloc(sizeof(FileNode));
        snprintf(new_node->name, sizeof(new_node->name), "%s", entry->d_name);

        char full_path[CMD_MAX];
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);
        stat(full_path, &file_stat);

        new_node->last_access_time = file_stat.st_atime;
        new_node->size = file_stat.st_size;
        new_node->chmod = file_stat.st_mode;
        new_node->chown = file_stat.st_uid;
        new_node->is_dir = S_ISDIR(file_stat.st_mode);
        new_node->is_link = S_ISLNK(file_stat.st_mode);
        new_node->is_executable = (file_stat.st_mode & S_IXUSR) || (file_stat.st_mode & S_IXGRP) || (file_stat.st_mode & S_IXOTH);
        new_node->is_selected = false;
        new_node->next = NULL;

        if (head == NULL) {
            head = new_node;
            current = head;
        } else {
            current->next = new_node;
            current = new_node;
        }
    }

    closedir(dir);
    return head;
}

void free_file_nodes(FileNode *head) {
    FileNode *tmp;
    while (head != NULL) {
        tmp = head;
        head = head->next;
        free(tmp);
    }
}



void draw_buttons(int maxY, int maxX) {
    move(maxY - 1, 0);
    clrtoeol();

    char *buttons[] = {"List", "View", "Edit", "Copy", "Move", "Mkdir", "Del", "Refresh", "Quit"};
    int num_buttons = sizeof(buttons) / sizeof(char *);

    int total_width = maxX - (num_buttons - 1);  // Subtract (num_buttons - 1) to account for spaces between buttons
    int button_width = (total_width - 1) / num_buttons;  // -1 to account for the extra character in "F10"

    int extra_space = total_width - (button_width * num_buttons) - 1;  // -1 to account for the extra character in "F10"

    int x = 0;
    for (int i = 0; i < num_buttons; ++i) {
        int extra = 0;
        if (extra_space > 0) {
            extra = 1;
            extra_space--;
        }

        attrset(A_NORMAL);
        if (i == num_buttons - 1) {  // Last button (F10)
            mvprintw(maxY - 1, x, "F%d ", i + 2);
        } else {
            mvprintw(maxY - 1, x, "F%d", i + 2);
        }

        attron(COLOR_PAIR(3));
        attron(A_REVERSE);
        mvprintw(maxY - 1, x + 2 + (i == num_buttons - 1), "%-*s", button_width - 2 + extra, buttons[i]);
        attroff(A_REVERSE);

        x += button_width + extra + 1 + (i == num_buttons - 1);  // +1 spacer between buttons, +1 for the last button (F10)
    }
}

void draw_windows(int maxY, int maxX) {
    // Refresh stdscr to ensure it's updated
    refresh();

    // Calculate window dimensions
    int winHeight = maxY - 2;
    int winWidth1 = maxX / 2;
    int winWidth2 = maxX / 2;

    // Adjust for odd COLS
    if (maxX % 2 != 0) {
        winWidth1 += 1;
    }

    // Delete old windows
    delwin(win1);
    delwin(win2);

    // Create new windows
    win1 = newwin(winHeight, winWidth1, 0, 0);
    win2 = newwin(winHeight, winWidth2, 0, winWidth1);

    // Apply the color pair to the window
    wbkgd(win1, COLOR_PAIR(2));
    wbkgd(win2, COLOR_PAIR(2));

    // Add borders to windows using wborder()
    wborder(win1, '|', '|', '-', '-', '+', '+', '+', '+');
    wborder(win2, '|', '|', '-', '-', '+', '+', '+', '+');

    // Refresh windows to make borders visible
    wrefresh(win1);
    wrefresh(win2);
}

void cursor_to_cmd() {
    // move cursor where it belongs
    move(LINES - 2, prompt_length + cursor_pos - cmd_offset);
    curs_set(1);
}

void update_cmd() {

    attron(COLOR_PAIR(1));

    // Print username, hostname, and current directory path
    move(LINES - 2, 0);
    clrtoeol();
    printw("%s@%s:%s# ", username, unameData.nodename, current_path);

    // Calculate max command display length
    prompt_length = strlen(username) + strlen(unameData.nodename) + strlen(current_path) + 4;  // 5 accounts for '@', ':', '#', and spaces.
    int max_cmd_display = COLS - prompt_length;

    // Print the visible part of the command, limited to max_cmd_display characters
    printw("%.*s", max_cmd_display, cmd + cmd_offset);

    cursor_to_cmd();

    // Refresh only the changed parts
    refresh();

    return;
}


void update_panel(WINDOW *win, FileNode *head) {
    FileNode *current = head;
    int line = 1;  // Start from the second row to avoid the border

    int width = getmaxx(win) - 2;
    int height = getmaxy(win);

    wattron(win, COLOR_PAIR(4));
    mvwprintw(win, line, 1, "%s", "Name");
    mvwprintw(win, line, width - 12 - 7 + 2, "%s", "Size");
    mvwprintw(win, line, width - 7 - 4, "%s", "Modify time");
    wattron(win, COLOR_PAIR(2));
    mvwprintw(win, line, width - 7 - 5, "%s", "|");
    mvwprintw(win, line, width - 12 - 7 - 1, "%s", "|");

    line++;

    while (current != NULL) {
        char prefix = ' ';
        if (current->is_dir) {
            prefix = '/';
        } else if (current->is_link) {
            prefix = '@';
        } else if (current->is_executable) {
            prefix = '*';
        }

        struct tm *tm = localtime(&current->last_access_time);
        char date_str[13];
        strftime(date_str, sizeof(date_str), "%b %d %H:%M", tm);

        long long size = current->size;
        char size_str[50];  // Buffer to hold the size and suffix
        snprintf(size_str, sizeof(size_str), "%lld", size);

        if (strlen(size_str) > 7) {
            size /= 1024;
            snprintf(size_str, sizeof(size_str), "%lldK", size);
            if (strlen(size_str) > 7) {
                size /= 1024;
                snprintf(size_str, sizeof(size_str), "%lldM", size);
                if (strlen(size_str) > 7) {
                    size /= 1024;
                    snprintf(size_str, sizeof(size_str), "%lldG", size);
                    if (strlen(size_str) > 7) {
                       size /= 1024;
                       snprintf(size_str, sizeof(size_str), "%lldT", size);
                    }
                }
            }
        }

        if (current->is_dir && strcmp(current->name, "..") == 0) {
           snprintf(size_str, sizeof(size_str), "UP--DIR");
        }

        mvwprintw(win, line, 1, "%c%-*s|%7s|%12s", prefix, width - 12 - 7 - 2 - 1, current->name, size_str, date_str);
        line++;
        current = current->next;
    }

    // Fill remaining lines with empty strings to maintain columns
    for (; line < height - 3; ++line) {
        mvwprintw(win, line, 1, "%-*s|        |           ", width - 12 - 7 - 2, " ");
    }

    mvwhline(win, height - 3, 1, '-', width);

    wrefresh(win);
    cursor_to_cmd();
}



void init_all() {
    initscr();
    start_color();
    raw();
    keypad(stdscr, TRUE);
    noecho();
    curs_set(1);

    init_pair(1, COLOR_WHITE, COLOR_BLACK);
    init_pair(2, COLOR_WHITE, COLOR_BLUE);
    init_pair(3, COLOR_CYAN, COLOR_BLACK);
    init_pair(4, COLOR_YELLOW, COLOR_BLUE);
}

void cleanup() {
    delwin(win1);
    delwin(win2);
    endwin();
}

void redraw_ui() {
   // Get screen dimensions
   int maxY, maxX;
   getmaxyx(stdscr, maxY, maxX);

   refresh();
   draw_windows(maxY, maxX);
   draw_buttons(maxY, maxX);
   update_cmd();
   refresh();
}

int main() {
    init_all();

    getcwd(left_path, sizeof(left_path));

    left_files = read_directory(left_path);
    right_files = read_directory(right_path);


    uname(&unameData);
    pw = getpwuid(getuid());
    username = pw->pw_name;

    while (1) {
        redraw_ui();

    // Print file names in left and right windows
    update_panel(win1, left_files);
    update_panel(win2, right_files);


        int ch = getch();
        if (ch == KEY_F(10)) {
            break;
        } else if (ch == KEY_RESIZE) {  // Handle terminal resize
            endwin();
            init_all();
            update_cmd();
        } else if (ch == '\n' && cmd_len > 0) {
            endwin();  // End ncurses mode
            if (strcmp(cmd, "exit") == 0) exit(0);
            printf("%s@%s:%s# %s\n", username, unameData.nodename, current_path, cmd);
            system(cmd);  // Execute the command
            init_all();
            memset(cmd, 0, CMD_MAX);
            cmd_len = cursor_pos = cmd_offset = prompt_length = 0;
        } else if (ch == 12) {  // Ctrl+L
            endwin();
            init_all();
        } else if (ch == 15) {  // Ctrl+O
            endwin();
            initscr();
            raw();
            getch();
            init_all();
        } else if (ch == KEY_BACKSPACE && cursor_pos > 0) {
            memmove(cmd + cursor_pos - 1, cmd + cursor_pos, cmd_len - cursor_pos);
            cmd[--cmd_len] = '\0';
            cursor_pos--;
        } else if (ch == KEY_DC && cursor_pos < cmd_len) {
            memmove(cmd + cursor_pos, cmd + cursor_pos + 1, cmd_len - cursor_pos - 1);
            cmd[--cmd_len] = '\0';
        } else if (ch == KEY_LEFT) {
            if (cursor_pos > 0) {
                cursor_pos--;
            } else if (cmd_offset > 0) {
                cmd_offset--;
            }
        } else if (ch == KEY_RIGHT && cursor_pos < cmd_len) {
            cursor_pos++;
        } else if (ch >= 32 && ch <= 126 && cmd_len < CMD_MAX - 1) {
            memmove(cmd + cursor_pos + 1, cmd + cursor_pos, cmd_len - cursor_pos);
            cmd[cursor_pos] = ch;
            cmd[++cmd_len] = '\0';
            cursor_pos++;
        } else if (ch == '\t') {
            /// TBD
        }


        // Handle scrolling in command line
        int max_cmd_display = COLS - (strlen(username) + strlen(unameData.nodename) + strlen(current_path) + 6) - 1;
        if (cursor_pos - cmd_offset >= max_cmd_display) {
            cmd_offset++;
        } else if (cursor_pos - cmd_offset < 0 && cmd_offset > 0) {
            cmd_offset--;
        }

        if (cmd_offset < 0) {
                cmd_offset = 0;
        }
    }

    endwin();
    cleanup();
    return 0;
}
