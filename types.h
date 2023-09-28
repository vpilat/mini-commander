
#define CMD_MAX 16384

typedef enum {
    SORT_BY_NAME_ASC = 0,
    SORT_BY_SIZE_ASC,
    SORT_BY_TIME_ASC,
    SORT_BY_NAME_DESC,
    SORT_BY_SIZE_DESC,
    SORT_BY_TIME_DESC,
    SORT_BY_NAME_DIRSFIRST_ASC,
    SORT_BY_SIZE_DIRSFIRST_ASC,
    SORT_BY_TIME_DIRSFIRST_ASC,
    SORT_BY_NAME_DIRSFIRST_DESC,
    SORT_BY_SIZE_DIRSFIRST_DESC,
    SORT_BY_TIME_DIRSFIRST_DESC
} SortOrders;

typedef struct FileNode {
    char* name;
    time_t mtime;
    off_t size;
    mode_t chmod;
    uid_t chown;
    int is_dir;
    int is_executable;
    int is_link;
    int is_link_to_dir;  // link points to a directory
    int is_link_broken; // invalid link
    char* link_target;
    int is_selected; // with Insert key
    struct FileNode *next;
} FileNode;

typedef struct PanelProp {
    int selected_index;
    int scroll_index;
    SortOrders sort_order;
    char path[CMD_MAX];
    char file_under_cursor[CMD_MAX];
    int files_count;
    FileNode *files;
} PanelProp;

typedef struct viewer_lines {
    char *line; // Pointer to the start of the line in the mapped file
    struct viewer_lines *next; // Pointer to the next viewer_lines
} viewer_lines;

enum {
    COLOR_WHITE_ON_BLACK = 1,
    COLOR_BLACK_ON_WHITE,
    COLOR_WHITE_ON_BLUE,
    COLOR_BLACK_ON_CYAN,
    COLOR_YELLOW_ON_CYAN,
    COLOR_YELLOW_ON_BLUE,
    COLOR_GREEN_ON_BLUE,
    COLOR_RED_ON_BLUE
};

extern PanelProp left_panel;
extern PanelProp right_panel;
extern PanelProp* active_panel;

// Global windows
extern WINDOW *win1;
extern WINDOW *win2;

extern struct utsname unameData;
extern struct passwd *pw;
extern const char *username;

extern struct timeval last_click_time;
extern struct timeval current_time;
extern struct timeval diff_time;

extern int cursor_pos;
extern int cmd_offset;
extern int prompt_length;

extern char cmd[CMD_MAX];
extern int cmd_len;