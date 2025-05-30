// Global Definitions and Structures
#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_BUFFER 0xB8000
#define PAGE_SIZE 4096
#define MAX_PROCESSES 8
#define MAX_FILES 32 // Reduced from 32 to 16 to test memory constraints
#define MAX_INODES 8
#define KERNEL_BASE 0xC0000000
#define USER_BASE 0x100000
#define FILE_WRITE_MAX 2048

// System call numbers
#define SYS_WRITE 1
#define SYS_OPEN  2
#define SYS_EXIT  3
#define SYS_PS    4
#define SYS_KILL  5
#define SYS_READ  6
#define SYS_CLOSE 7
#define SYS_CREATE 8
#define SYS_LS    9

// Diary Global Variables
static char diary_buffer[256];
static int diary_index = 0;
static int diary_active = 0;

// File Write Buffer
static char file_write_buffer[FILE_WRITE_MAX];
static int file_write_index = 0;
static int file_write_active = 0;
static int current_file_fd = -1;



// Process structure for task management
typedef struct {
    void (*task)();           // Task function pointer
    int state;                // 0: ready, 1: running, 2: terminated
    int esp;                  // Stack pointer
    int pid;                  // Process ID
    int priority;             // Process priority (1-10)
    unsigned int user_stack;  // User stack address
    unsigned int code_segment;// Code segment
    int privilege;            // 0: kernel, 3: user
    unsigned int page_dir;    // Page directory address
} Process;

// Inode structure for file system
typedef struct {
    int id;                   // Inode ID
    char name[32];            // File name
    int size;                 // File size in bytes
    int used;                 // 1: in use, 0: free
    char data[128];           // File data buffer
} Inode;

// Virtual File System mount structure
typedef struct {
    char device[16];          // Device name
    char mount_point[16];     // Mount point path
    char fs_type[16];         // Filesystem type
    int inodes_used;          // Number of used inodes
    int files;                // Number of files
    Inode inodes[MAX_INODES]; // Inode table
} VFS_Mount;

// File descriptor structure
typedef struct {
    int inode_id;             // Associated inode
    int used;                 // 1: in use, 0: free
    int offset;               // Current file offset
} FileDescriptor;

// Global Variables
Process processes[MAX_PROCESSES]; // Array of processes
int current_process = 0;          // Index of currently running process
char keyboard_buffer[256];         // Buffer for keyboard input
int buffer_index = 0;             // Current index in keyboard buffer
char shell_buffer[256];           // Buffer for shell commands
int shell_index = 0;              // Current index in shell buffer
char command_log[512];            // Buffer for command history
int log_index = 0;                // Current index in command log
volatile int schedule_flag = 0;   // Flag to trigger scheduling
int menu_active = 0;              // Menu state: 0 (off), 1 (on)
int shell_active = 0;             // Shell state: 0 (off), 1 (on)
VFS_Mount vfs;                    // Single VFS mount
FileDescriptor fds[MAX_FILES];    // File descriptor table
unsigned int* kernel_page_dir;    // Kernel page directory
int vfs_initialized = 0;          // Flag to track VFS initialization

// Function Prototypes
void DiaryNote(void);
void FileWrite(const char* filename);
void display_shell_prompt(void);

// String manipulation functions
void custom_strcpy(char* dest, const char* src) {
    while (*src) {
        *dest++ = *src++;
    }
    *dest = 0;
}

int strcmp(const char* s1, const char* s2) {
    while (*s1 && *s2 && *s1 == *s2) {
        s1++;
        s2++;
    }
    return *s1 - *s2;
}

int strncmp(const char* s1, const char* s2, int n) {
    while (n > 0 && *s1 && *s2 && *s1 == *s2) {
        s1++;
        s2++;
        n--;
    }
    if (n == 0) return 0;
    return *s1 - *s2;
}

// VGA Display Functions
void clear_screen() {
    unsigned short* vga = (unsigned short*)VGA_BUFFER;
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        vga[i] = 0x0700; // White on black
    }
}

void print_string(const char* str, int row, int col) {
    unsigned short* vga = (unsigned short*)VGA_BUFFER;
    int index = row * VGA_WIDTH + col;
    while (*str && index < VGA_WIDTH * VGA_HEIGHT) {
        vga[index] = 0x0700 | (*str & 0xFF); // Ensure ASCII
        str++;
        index++;
    }
}

void print_string_with_attr(const char* str, int row, int col, unsigned char attr) {
    unsigned short* vga = (unsigned short*)VGA_BUFFER;
    int index = row * VGA_WIDTH + col;
    while (*str && index < VGA_WIDTH * VGA_HEIGHT) {
        vga[index] = (attr << 8) | (*str & 0xFF);
        str++;
        index++;
    }
}

void print_hex_byte(unsigned char value, int row, int col) {
    unsigned short* vga = (unsigned short*)VGA_BUFFER;
    const char hex[] = "0123456789ABCDEF";
    if (row * VGA_WIDTH + col + 1 < VGA_WIDTH * VGA_HEIGHT) {
        vga[row * VGA_WIDTH + col] = 0x4F00 | hex[(value >> 4) & 0xF];
        vga[row * VGA_WIDTH + col + 1] = 0x4F00 | hex[value & 0xF];
    }
}

void print_number(int value, int row, int col) {
    char buf[16];
    int i = 0, temp = value;
    if (temp == 0) {
        buf[i++] = '0';
    } else {
        while (temp) {
            buf[i++] = (temp % 10) + '0';
            temp /= 10;
        }
    }
    buf[i] = 0;
    for (int j = 0; j < i / 2; j++) {
        char t = buf[j];
        buf[j] = buf[i - j - 1];
        buf[i - j - 1] = t;
    }
    print_string(buf, row, col);
}

void redraw_write_buffer(int text_row_start, int text_col, int rect_width, int rect_height) {
    unsigned short* vga = (unsigned short*)VGA_BUFFER;
    const int visible_rows = rect_height - 6;
    const int visible_cols = rect_width - 4;

    int start_pos = file_write_index - (visible_rows * visible_cols);
    if (start_pos < 0) start_pos = 0;

    for (int i = 0; i < visible_rows * visible_cols; i++) {
        int buffer_pos = start_pos + i;
        int r = i / visible_cols;
        int c = i % visible_cols;
        char ch = (buffer_pos < file_write_index) ? file_write_buffer[buffer_pos] : ' ';
        vga[(text_row_start + r) * VGA_WIDTH + text_col + c] = 0x2F00 | ch;
    }
}

// Virtual Memory Management
void init_paging() {
    print_string("Initializing paging...", 1, 0);
    kernel_page_dir = (unsigned int*)0x100000; // Page directory at 1MB
    for (int i = 0; i < 1024; i++) {
        kernel_page_dir[i] = 0;
    }
    unsigned int* page_table = (unsigned int*)0x101000; // First page table
    for (int i = 0; i < 1024; i++) {
        page_table[i] = (i * PAGE_SIZE) | 0x3; // Present, R/W, Supervisor
    }
    kernel_page_dir[0] = (unsigned int)page_table | 0x3;
    kernel_page_dir[768] = (unsigned int)page_table | 0x3;
    unsigned int vga_addr = 0xB8000;
    int pt_index = vga_addr / PAGE_SIZE;
    page_table[pt_index] = vga_addr | 0x3;
    //print_string("Page directory set up", 2, 0);
    asm volatile(
        "mov %0, %%cr3\n\t"
        "mov %%cr0, %%eax\n\t"
        "or $0x80000000, %%eax\n\t"
        "mov %%eax, %%cr0"
        : : "r"(kernel_page_dir) : "eax"
    );
    print_string("Paging enabled", 2, 0);
}

unsigned int* create_user_page_dir() {
    unsigned int* page_dir = (unsigned int*)0x200000;
    for (int i = 0; i < 1024; i++) {
        page_dir[i] = 0;
    }
    unsigned int* page_table = (unsigned int*)0x201000;
    for (int i = 0; i < 1024; i++) {
        page_table[i] = (i * PAGE_SIZE + USER_BASE) | 0x7; // Present, R/W, User
    }
    page_dir[0] = (unsigned int)page_table | 0x7;
    page_dir[768] = kernel_page_dir[768];
    return page_dir;
}

// Virtual File System
void init_vfs() {
    print_string("Initializing VFS...", 3, 0);
    
    // Debug: Step 1
    //print_string("Step 1: Setting device", 4, 0);
    custom_strcpy(vfs.device, "hda");
    
    // Debug: Step 2
    //print_string("Step 2: Setting mount point", 5, 0);
    custom_strcpy(vfs.mount_point, "/");
    
    // Debug: Step 3
    //print_string("Step 3: Setting fs type", 6, 0);
    custom_strcpy(vfs.fs_type, "ext2");
    
    // Debug: Step 4
    //print_string("Step 4: Initializing counters", 7, 0);
    vfs.inodes_used = 0;
    vfs.files = 0;
    
    // Debug: Step 5
    //print_string("Step 5: Initializing inodes", 8, 0);
    for (int i = 0; i < MAX_INODES; i++) {
        vfs.inodes[i].used = 0;
        vfs.inodes[i].id = 0;
        vfs.inodes[i].size = 0;
        for (int j = 0; j < 32; j++) {
            vfs.inodes[i].name[j] = 0;
        }
        for (int j = 0; j < 128; j++) {
            vfs.inodes[i].data[j] = 0;
        }
    }
    
    // Debug: Step 6
    //print_string("Step 6: Initializing file descriptors", 9, 0);
    for (int i = 0; i < MAX_FILES; i++) {
        fds[i].used = 0;
        fds[i].inode_id = -1;
        fds[i].offset = 0;
    }
    
    // Debug: Step 7
    //print_string("Step 7: Setting VFS flag", 10, 0);
    vfs_initialized = 1;
    
    // Final confirmation
    print_string("VFS initialized", 4, 0);
}

int vfs_create_file(const char* name) {
    if (!vfs_initialized) {
        print_string("VFS not initialized in create", 16, 0);
        return -1;
    }
    if (vfs.inodes_used >= MAX_INODES) {
        print_string("No free inodes", 16, 0);
        return -1;
    }
    for (int i = 0; i < MAX_INODES; i++) {
        if (!vfs.inodes[i].used) {
            vfs.inodes[i].used = 1;
            vfs.inodes[i].id = i;
            int j = 0;
            while (name[j] && j < 31) {
                vfs.inodes[i].name[j] = name[j];
                j++;
            }
            vfs.inodes[i].name[j] = 0;
            vfs.inodes[i].size = 0;
            vfs.inodes_used++;
            vfs.files++;
            print_string("Created inode: ", 16, 0);
            print_number(i, 16, 15);
            return i;
        }
    }
    print_string("No free inodes found", 16, 0);
    return -1;
}

int vfs_open_file(const char* name) {
    if (!vfs_initialized) {
        print_string("VFS not initialized in open", 16, 0);
        return -1;
    }
    for (int i = 0; i < MAX_INODES; i++) {
        if (vfs.inodes[i].used && strcmp(vfs.inodes[i].name, name) == 0) {
            for (int j = 0; j < MAX_FILES; j++) {
                if (!fds[j].used) {
                    fds[j].used = 1;
                    fds[j].inode_id = i;
                    fds[j].offset = 0;
                    print_string("Opened fd: ", 16, 0);
                    print_number(j, 16, 11);
                    return j;
                }
            }
            print_string("No free file descriptors", 16, 0);
            return -1;
        }
    }
    print_string("File not found: ", 16, 0);
    print_string(name, 16, 16);
    return -1;
}

int vfs_read_file(int fd, char* buf, int len) {
    if (!vfs_initialized) {
        print_string("VFS not initialized in read", 17, 0);
        return -1;
    }
    if (fd < 0 || fd >= MAX_FILES || !fds[fd].used) {
        print_string("Invalid file descriptor: ", 17, 0);
        print_number(fd, 17, 25);
        return -1;
    }
    Inode* inode = &vfs.inodes[fds[fd].inode_id];
    if (!inode->used) {
        print_string("Inode not used: ", 17, 0);
        print_number(fds[fd].inode_id, 17, 16);
        return -1;
    }
    if (inode->size == 0 || fds[fd].offset >= inode->size) {
        print_string("File empty or offset beyond size", 17, 0);
        return 0;
    }
    int bytes = 0;
    while (bytes < len && fds[fd].offset < inode->size && fds[fd].offset < 128) {
        buf[bytes] = inode->data[fds[fd].offset];
        bytes++;
        fds[fd].offset++;
    }
    print_string("Read bytes: ", 17, 0);
    print_number(bytes, 17, 12);
    return bytes;
}

int vfs_write_file(int fd, const char* buf, int len) {
    if (!vfs_initialized) {
        print_string("VFS not initialized in write", 18, 0);
        return -1;
    }
    if (fd < 0 || fd >= MAX_FILES || !fds[fd].used) {
        print_string("Invalid file descriptor in write: ", 18, 0);
        print_number(fd, 18, 34);
        return -1;
    }
    Inode* inode = &vfs.inodes[fds[fd].inode_id];
    if (!inode->used) {
        print_string("Inode not used in write: ", 18, 0);
        print_number(fds[fd].inode_id, 18, 25);
        return -1;
    }
    int bytes = 0;
    while (bytes < len && fds[fd].offset < 128) {
        inode->data[fds[fd].offset] = buf[bytes];
        bytes++;
        fds[fd].offset++;
    }
    if (fds[fd].offset > inode->size) {
        inode->size = fds[fd].offset;
    }
    print_string("Wrote bytes: ", 18, 0);
    print_number(bytes, 18, 13);
    return bytes;
}

void vfs_close_file(int fd) {
    if (!vfs_initialized) {
        print_string("VFS not initialized in close", 19, 0);
        return;
    }
    if (fd >= 0 && fd < MAX_FILES && fds[fd].used) {
        fds[fd].used = 0;
        fds[fd].inode_id = -1;
        fds[fd].offset = 0;
        print_string("Closed fd: ", 19, 0);
        print_number(fd, 19, 11);
    }
}

void vfs_list_files(char* buf, int* len) {
    if (!vfs_initialized) {
        print_string("VFS not initialized in ls", 16, 0);
        *len = 0;
        buf[0] = 0;
        return;
    }
    int pos = 0;
    for (int i = 0; i < MAX_INODES; i++) {
        if (vfs.inodes[i].used) {
            int j = 0;
            while (vfs.inodes[i].name[j] && j < 31 && pos < 255) {
                char c = vfs.inodes[i].name[j];
                if (c >= 32 && c <= 126) {
                    buf[pos++] = c;
                }
                j++;
            }
            if (pos < 255) {
                buf[pos++] = ' ';
            }
        }
    }
    buf[pos] = 0;
    *len = pos;
}

// File Write Interface
void FileWrite(const char* filename) {
    if (!vfs_initialized) {
        clear_screen();
        print_string("VFS not initialized. File operations disabled.", 12, 10);
        for (volatile int i = 0; i < 1000000; i++);
        clear_screen();
        display_shell_prompt();
        shell_active = 1;
        return;
    }
    unsigned short* vga = (unsigned short*)VGA_BUFFER;
    const int rect_width = 60;
    const int rect_height = 15;
    const int rect_start_row = (VGA_HEIGHT - rect_height) / 2;
    const int rect_start_col = (VGA_WIDTH - rect_width) / 2;
    const int rect_end_row = rect_start_row + rect_height - 1;
    const int rect_end_col = rect_start_col + rect_width - 1;

    clear_screen();

    for (int row = rect_start_row; row <= rect_end_row; row++) {
        for (int col = rect_start_col; col <= rect_end_col; col++) {
            vga[row * VGA_WIDTH + col] = 0x2F00 | ' ';
        }
    }

    for (int col = rect_start_col; col <= rect_end_col; col++) {
        vga[rect_start_row * VGA_WIDTH + col] = 0x2F00 | '-';
        vga[rect_end_row * VGA_WIDTH + col] = 0x2F00 | '-';
    }
    for (int row = rect_start_row + 1; row < rect_end_row; row++) {
        vga[row * VGA_WIDTH + rect_start_col] = 0x2F00 | '|';
        vga[row * VGA_WIDTH + rect_end_col] = 0x2F00 | '|';
    }
    vga[rect_start_row * VGA_WIDTH + rect_start_col] = 0x2F00 | '+';
    vga[rect_start_row * VGA_WIDTH + rect_end_col] = 0x2F00 | '+';
    vga[rect_end_row * VGA_WIDTH + rect_start_col] = 0x2F00 | '+';
    vga[rect_end_row * VGA_WIDTH + rect_end_col] = 0x2F00 | '+';

    int text_row = rect_start_row + 1;
    int text_col = rect_start_col + 2;
    print_string_with_attr("Write to File", text_row++, text_col, 0x2F);
    print_string_with_attr("File: ", text_row, text_col, 0x2F);
    print_string_with_attr(filename, text_row++, text_col + 6, 0x2F);
    print_string_with_attr("Press Enter to save, Esc to cancel.", text_row++, text_col, 0x2F);
    print_string_with_attr("----------------------", text_row++, text_col, 0x2F);
    //print_string_with_attr("Enter text:", text_row++, text_col, 0x2F);

    for (int i = 0; i < 128; i++) {
        file_write_buffer[i] = 0;
    }
    file_write_index = 0;
    file_write_active = 1;
}

// Shell Display Functions
void clear_shell_input() {
    unsigned short* vga = (unsigned short*)VGA_BUFFER;
    for (int i = 0; i < VGA_WIDTH; i++) {
        vga[23 * VGA_WIDTH + i] = 0x0700;
    }
    shell_index = 0;
    for (int i = 0; i < 256; i++) {
        shell_buffer[i] = 0;
    }
}

void clear_shell_output() {
    unsigned short* vga = (unsigned short*)VGA_BUFFER;
    for (int i = 0; i < VGA_WIDTH; i++) {
        vga[15 * VGA_WIDTH + i] = 0x0700;
        vga[22 * VGA_WIDTH + i] = 0x0700;
    }
}

void clear_shell_command_prompt() {
    unsigned short* vga = (unsigned short*)VGA_BUFFER;
    for (int i = 0; i < VGA_WIDTH; i++) {
        vga[20 * VGA_WIDTH + i] = 0x0700;
    }
}

void clear_shell() {
    unsigned short* vga = (unsigned short*)VGA_BUFFER;
    for (int i = 15; i <= 23; i++) {
        for (int j = 0; j < VGA_WIDTH; j++) {
            vga[i * VGA_WIDTH + j] = 0x0700;
        }
    }
    shell_index = 0;
    for (int i = 0; i < 256; i++) {
        shell_buffer[i] = 0;
    }
}

void display_shell_prompt() {
    clear_shell_input();
    print_string_with_attr("SHELL>> ", 23, 0, 0x2F);
}

// Menu Functions
void display_menu() {
    unsigned short* vga = (unsigned short*)VGA_BUFFER;
    for (int i = 0; i < VGA_WIDTH * 2; i++) {
        vga[(VGA_HEIGHT - 2) * VGA_WIDTH + i] = 0x0700;
    }
    print_string("Menu: ", 24, 0);
    print_string_with_attr("1", 24, 6, 0x0F);
    print_string(".Show/Hide ", 24, 7);
    print_string_with_attr("2", 24, 18, 0x0F);
    print_string(".Exit ", 24, 19);
    print_string_with_attr("3", 24, 25, 0x0F);
    print_string(".Crash (BSOD) ", 24, 26);
    print_string_with_attr("S", 24, 40, 0x0F);
    print_string(".Shell ", 24, 41);
    print_string_with_attr("V", 24, 48, 0x0F);
    print_string(".Virtual Memory ", 24, 49);
}

void hide_menu() {
    unsigned short* vga = (unsigned short*)VGA_BUFFER;
    for (int i = 0; i < VGA_WIDTH * 2; i++) {
        vga[(VGA_HEIGHT - 2) * VGA_WIDTH + i] = 0x0700;
    }
}

// System Control Functions
void halt_system() {
    clear_screen();
    print_string("System halted.", 12, 33);
    asm volatile("hlt");
    while (1);
}

void display_bsod() {
    unsigned short* vga = (unsigned short*)VGA_BUFFER;
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        vga[i] = 0x2F00;
    }
    print_string_with_attr("*** A fatal error has occurred ***", 5, 24, 0x2F);
    print_string_with_attr("Sebria OS has encountered a critical error and must halt.", 7, 12, 0x2F);
    print_string_with_attr("Error Code: 0xDEADBEEF", 9, 29, 0x2F);
    asm volatile("hlt");
    while (1);
}

// Screen Dump Function
void dump_screen() {
    unsigned short* vga = (unsigned short*)VGA_BUFFER;
    char screen_buffer[VGA_HEIGHT * (VGA_WIDTH + 1)];
    int buffer_pos = 0;

    for (int row = 0; row < VGA_HEIGHT; row++) {
        for (int col = 0; col < VGA_WIDTH; col++) {
            char c = (char)(vga[row * VGA_WIDTH + col] & 0xFF);
            if (c == 0) c = ' ';
            if (buffer_pos < VGA_HEIGHT * (VGA_WIDTH + 1) - 1) {
                screen_buffer[buffer_pos++] = c;
            }
        }
        if (buffer_pos < VGA_HEIGHT * (VGA_WIDTH + 1) - 1) {
            screen_buffer[buffer_pos++] = '\n';
        }
    }
    screen_buffer[buffer_pos ? buffer_pos - 1 : 0] = 0;

    clear_shell_output();
    print_string("Dumping screen contents...", 18, 0);
    for (volatile int i = 0; i < 100000; i++);

    const int rect_width = 60;
    const int rect_height = 15;
    const int rect_start_row = (VGA_HEIGHT - rect_height) / 2;
    const int rect_start_col = (VGA_WIDTH - rect_width) / 2;
    const int rect_end_row = rect_start_row + rect_height - 1;
    const int rect_end_col = rect_start_col + rect_width - 1;

    clear_screen();

    for (int col = rect_start_col; col <= rect_end_col; col++) {
        vga[rect_start_row * VGA_WIDTH + col] = 0x2F00 | '-';
        vga[rect_end_row * VGA_WIDTH + col] = 0x2F00 | '-';
    }
    for (int row = rect_start_row + 1; row < rect_end_row; row++) {
        vga[row * VGA_WIDTH + rect_start_col] = 0x2F00 | '|';
        vga[row * VGA_WIDTH + rect_end_col] = 0x2F00 | '|';
    }
    vga[rect_start_row * VGA_WIDTH + rect_start_col] = 0x2F00 | '+';
    vga[rect_start_row * VGA_WIDTH + rect_end_col] = 0x2F00 | '+';
    vga[rect_end_row * VGA_WIDTH + rect_start_col] = 0x2F00 | '+';
    vga[rect_end_row * VGA_WIDTH + rect_end_col] = 0x2F00 | '+';

    int display_row = rect_start_row + 1;
    int display_col = rect_start_col + 1;
    int scrollAddr = 0;

    for (int row = scrollAddr; row < scrollAddr + (rect_height - 2) && row < VGA_HEIGHT; row++) {
        for (int col = 0; col < rect_width - 2 && col < VGA_WIDTH; col++) {
            int buf_index = row * (VGA_WIDTH + 1) + col;
            char c = (buf_index < buffer_pos) ? screen_buffer[buf_index] : ' ';
            if (c == '\n' || c == 0) c = ' ';
            vga[display_row * VGA_WIDTH + display_col + col] = 0x2F00 | c;
        }
        display_row++;
    }

    print_string_with_attr("Press S to return to shell, Q to exit.", 21, 10, 0x2F);

    while (1) {
        unsigned char status, scancode;
        asm volatile("inb $0x64, %0" : "=a"(status));
        if (status & 0x01) {
            asm volatile("inb $0x60, %0" : "=a"(scancode));
            if (!(scancode & 0x80)) {
                if (scancode == 0x1F) {
                    shell_index = 0;
                    for (int i = 0; i < 256; i++) {
                        shell_buffer[i] = 0;
                    }
                    clear_screen();
                    clear_shell();
                    display_shell_prompt();
                    shell_active = 1;
                    return;
                }
                if (scancode == 0x10) {
                    halt_system();
                    return;
                }
            }
        }
        for (volatile int i = 0; i < 10000; i++);
    }
}

// Diary Note Function
void DiaryNote() {
    if (!vfs_initialized) {
        clear_screen();
        print_string("VFS not initialized. Diary feature disabled.", 12, 10);
        for (volatile int i = 0; i < 1000000; i++);
        clear_screen();
        display_shell_prompt();
        shell_active = 1;
        return;
    }
    unsigned short* vga = (unsigned short*)VGA_BUFFER;
    const int rect_width = 60;
    const int rect_height = 15;
    const int rect_start_row = (VGA_HEIGHT - rect_height) / 2;
    const int rect_start_col = (VGA_WIDTH - rect_width) / 2;
    const int rect_end_row = rect_start_row + rect_height - 1;
    const int rect_end_col = rect_start_col + rect_width - 1;

    clear_screen();

    for (int row = rect_start_row; row <= rect_end_row; row++) {
        for (int col = rect_start_col; col <= rect_end_col; col++) {
            vga[row * VGA_WIDTH + col] = 0x2F00 | ' ';
        }
    }

    for (int col = rect_start_col; col <= rect_end_col; col++) {
        vga[rect_start_row * VGA_WIDTH + col] = 0x2F00 | '-';
        vga[rect_end_row * VGA_WIDTH + col] = 0x2F00 | '-';
    }
    for (int row = rect_start_row + 1; row < rect_end_row; row++) {
        vga[row * VGA_WIDTH + rect_start_col] = 0x2F00 | '|';
        vga[row * VGA_WIDTH + rect_end_col] = 0x2F00 | '|';
    }
    vga[rect_start_row * VGA_WIDTH + rect_start_col] = 0x2F00 | '+';
    vga[rect_start_row * VGA_WIDTH + rect_end_col] = 0x2F00 | '+';
    vga[rect_end_row * VGA_WIDTH + rect_start_col] = 0x2F00 | '+';
    vga[rect_end_row * VGA_WIDTH + rect_end_col] = 0x2F00 | '+';

    int text_row = rect_start_row + 1;
    int text_col = rect_start_col + 2;
    print_string_with_attr("Diary Note", text_row++, text_col, 0x2F);
    print_string_with_attr("Press Enter to save, Esc to cancel.", text_row++, text_col, 0x2F);
    print_string_with_attr("----------------------", text_row++, text_col, 0x2F);
    print_string_with_attr("Tell me about your day?", text_row++, text_col, 0x2F);

    for (int i = 0; i < 256; i++) {
        diary_buffer[i] = 0;
    }
    diary_index = 0;
    diary_active = 1;
}

// Virtual Memory Information Display
void display_vm_info() {
    clear_screen();
    print_string_with_attr("Sebria OS Virtual Memory Management", 2, 20, 0x0F);
    print_string("Virtual Memory Status:", 4, 5);
    print_string("Paging: Disabled", 6, 5);
    print_string("Virtual File System (VFS):", 10, 5);
    print_string("Status: Not initialized", 12, 5);
    print_string("Files: ", 13, 5);
    print_number(vfs.files, 13, 21);
    print_string("Inodes Used: ", 14, 5);
    print_number(vfs.inodes_used, 14, 21);
    print_string("User-Space Processes:", 16, 5);
    print_string("PID   Name      State     Priority", 18, 5);
    int row = 19;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (processes[i].pid) {
            char buf[64];
            int pos = 0;
            print_number(processes[i].pid, row, 5);
            buf[pos++] = ' ';
            buf[pos++] = ' ';
            buf[pos++] = ' ';
            buf[pos++] = ' ';
            const char* name = processes[i].privilege == 0 ? "kernel" : "user";
            for (int j = 0; name[j]; j++) {
                buf[pos++] = name[j];
            }
            while (pos < 16) buf[pos++] = ' ';
            const char* state = processes[i].state == 1 ? "Running" : processes[i].state == 0 ? "Ready" : "Terminated";
            for (int j = 0; state[j]; j++) {
                buf[pos++] = state[j];
            }
            while (pos < 24) buf[pos++] = ' ';
            print_number(processes[i].priority, row, 24);
            buf[pos] = 0;
            print_string(buf, row++, 8);
        }
    }
    print_string("Press any key to return to menu...", 22, 20);

    unsigned char status, scancode;
    while (1) {
        asm volatile("inb $0x64, %0" : "=a"(status));
        if (status & 0x01) {
            asm volatile("inb $0x60, %0" : "=a"(scancode));
            if (!(scancode & 0x80)) {
                asm volatile("mov $0x20, %%al\n\tout %%al, $0x20" : : : "eax");
                break;
            }
        }
    }
    clear_screen();
}

void append_to_log(const char* command) {
    if (command[0] == 0) return;
    int i = 0;
    while (command[i] && log_index < 511) {
        command_log[log_index++] = command[i++];
    }
    if (log_index < 511) {
        command_log[log_index++] = '\n';
    }
}

// Process Management
int create_process(void (*task)(), int priority, int privilege) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (!processes[i].pid) {
            processes[i].task = task;
            processes[i].state = 0;
            processes[i].pid = i + 1;
            processes[i].priority = priority;
            processes[i].privilege = privilege;
            processes[i].user_stack = privilege == 3 ? USER_BASE + PAGE_SIZE * 2 : 0;
            processes[i].page_dir = privilege == 3 ? (unsigned int)create_user_page_dir() : (unsigned int)kernel_page_dir;
            return i;
        }
    }
    return -1;
}

void kill_process(int pid) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (processes[i].pid == pid) {
            processes[i].state = 2;
            processes[i].pid = 0;
            if (i == current_process) {
                schedule_flag = 1;
            }
            break;
        }
    }
}

void schedule() {
    int next = -1;
    int max_priority = -1;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (processes[i].state == 0 && processes[i].priority > max_priority) {
            max_priority = processes[i].priority;
            next = i;
        }
    }
    if (next != -1 && next != current_process) {
        processes[current_process].state = 0;
        current_process = next;
        processes[current_process].state = 1;
        // Skip page directory switch since paging is disabled
        // asm volatile("mov %0, %%cr3" : : "r"(processes[current_process].page_dir) : "memory");
    }
}

// System Call Handler
void syscall_handler() {
    unsigned int syscall_num, arg1, arg2, arg3;
    asm volatile(
        "mov %%eax, %0\n\t"
        "mov %%ebx, %1\n\t"
        "mov %%ecx, %2\n\t"
        "mov %%edx, %3"
        : "=r"(syscall_num), "=r"(arg1), "=r"(arg2), "=r"(arg3)
    );
    int result = 0;
    switch (syscall_num) {
        case SYS_WRITE:
            print_string((const char*)arg1, 15, 0);
            break;
        case SYS_OPEN:
            result = vfs_open_file((const char*)arg1);
            break;
        case SYS_READ:
            result = vfs_read_file(arg1, (char*)arg2, arg3);
            break;
        case SYS_CLOSE:
            vfs_close_file(arg1);
            break;
        case SYS_CREATE:
            result = vfs_create_file((const char*)arg1);
            break;
        case SYS_LS:
            vfs_list_files((char*)arg1, (int*)arg2);
            break;
        case SYS_PS:
            result = 0;
            for (int i = 0; i < MAX_PROCESSES; i++) {
                if (processes[i].pid) result++;
            }
            break;
        case SYS_KILL:
            kill_process(arg1);
            break;
        case SYS_EXIT:
            kill_process(processes[current_process].pid);
            break;
        default:
            print_string("Unknown syscall", 15, 0);
    }
    asm volatile("mov %0, %%eax" : : "r"(result) : "eax");
    asm volatile("mov $0x20, %%al\n\tout %%al, $0x20" : : : "eax");
}

// Interrupt Handlers
void default_handler() {
    unsigned short* vga = (unsigned short*)VGA_BUFFER;
    vga[2] = 0x4F44; // 'D'
    while (1);
}

void double_fault_handler() {
    unsigned short* vga = (unsigned short*)VGA_BUFFER;
    vga[4] = 0x4F46; // 'F'
    while (1);
}

void timer_handler() {
    unsigned short* vga = (unsigned short*)VGA_BUFFER;
    vga[6] = 0x4F54; // 'T'
    schedule_flag = 1;
    asm volatile("mov $0x20, %%al\n\tout %%al, $0x20" : : : "eax");
}

void keyboard_handler() {
    static const char scancode_to_ascii[] = {
        0, 0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', 0,
        0, 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
        0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0,
        '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, 0, 0,
        ' ', 0
    };

    unsigned char scancode;
    asm volatile("inb $0x60, %0" : "=a"(scancode));

    unsigned short* vga = (unsigned short*)VGA_BUFFER;
    vga[0] = 0x4F4B; // 'K'

    if (scancode & 0x80) { // Key release
        asm volatile("mov $0x20, %%al\n\tout %%al, $0x20" : : : "eax");
        return;
    }

    print_hex_byte(scancode, 0, 2);

    char c = (scancode < sizeof(scancode_to_ascii) && scancode_to_ascii[scancode]) ? scancode_to_ascii[scancode] : 0;

   if (file_write_active) {
    const int rect_width = 60;
    const int rect_height = 15;
    const int rect_start_row = (VGA_HEIGHT - rect_height) / 2;
    const int rect_start_col = (VGA_WIDTH - rect_width) / 2;
    const int text_row_start = rect_start_row + 5;
    const int text_col = rect_start_col + 2;

    if (scancode == 0x0E && file_write_index > 0) {  // Backspace
        file_write_index--;
        file_write_buffer[file_write_index] = 0;
        redraw_write_buffer(text_row_start, text_col, rect_width, rect_height);
    } else if (scancode == 0x1C) {  // Enter - Save
        file_write_buffer[file_write_index] = 0;
        if (file_write_index > 0 && current_file_fd >= 0) {
            int bytes_written = vfs_write_file(current_file_fd, file_write_buffer, file_write_index);
            print_string("Bytes written: ", 18, 0);
            print_number(bytes_written, 18, 15);
            vfs_close_file(current_file_fd);
            current_file_fd = -1;
        } else {
            print_string("No data or invalid fd", 18, 0);
            if (current_file_fd >= 0) {
                vfs_close_file(current_file_fd);
                current_file_fd = -1;
            }
        }
        file_write_active = 0;
        clear_screen();
        clear_shell();
        display_shell_prompt();
        shell_active = 1;
    } else if (scancode == 0x01) {  // Esc - Cancel
        if (current_file_fd >= 0) {
            vfs_close_file(current_file_fd);
            current_file_fd = -1;
        }
        file_write_active = 0;
        clear_screen();
        clear_shell();
        display_shell_prompt();
        shell_active = 1;
    } else if (c && c >= 32 && c <= 126 && file_write_index < FILE_WRITE_MAX - 1) {
        file_write_buffer[file_write_index++] = c;
        file_write_buffer[file_write_index] = 0;
        redraw_write_buffer(text_row_start, text_col, rect_width, rect_height);
    }

    asm volatile("mov $0x20, %%al\n\tout %%al, $0x20" : : : "eax");
    return;
}

  // Handle diary input
    if (diary_active) {
        const int rect_width = 60;
        const int rect_height = 15;
        const int rect_start_row = (VGA_HEIGHT - rect_height) / 2;
        const int rect_start_col = (VGA_WIDTH - rect_width) / 2;
        const int text_row_start = rect_start_row + 5;
        const int text_col = rect_start_col + 2;
        static int current_row = 0;
        static int current_col = 0;
        int text_row = text_row_start + current_row;

        if (scancode == 0x0E && diary_index > 0) { // Backspace
            diary_index--;
            if (current_col > 0) {
                current_col--;
            } else if (current_row > 0) {
                current_row--;
                current_col = rect_width - 4 - 1;
            }
            vga[(text_row_start + current_row) * VGA_WIDTH + text_col + current_col] = 0x2F00 | ' ';
            diary_buffer[diary_index] = 0;
        } else if (scancode == 0x1C) { // Enter
            diary_buffer[diary_index] = 0;
            if (diary_index > 0) {
                append_to_log(diary_buffer);
                int fd = vfs_open_file("diary.txt");
                if (fd < 0) {
                    fd = vfs_create_file("diary.txt");
                }
                if (fd >= 0) {
                    vfs_write_file(fd, diary_buffer, diary_index);
                    vfs_close_file(fd);
                }
            }
            diary_active = 0;
            current_row = 0;
            current_col = 0;
            clear_screen();
            clear_shell();
            display_shell_prompt();
            shell_active = 1;
        } else if (scancode == 0x01) { // Escape
            diary_active = 0;
            current_row = 0;
            current_col = 0;
            clear_screen();
            clear_shell();
            display_shell_prompt();
            shell_active = 1;
        } else if (c && current_row < rect_height - 6) {
            if (current_col >= rect_width - 4) {
                current_row++;
                current_col = 0;
                text_row = text_row_start + current_row;
            }
            if (current_row < rect_height - 6) {
                diary_buffer[diary_index] = c;
                vga[text_row * VGA_WIDTH + text_col + current_col] = 0x2F00 | c;
                diary_index++;
                current_col++;
                diary_buffer[diary_index] = 0;
            }
        }
        asm volatile("mov $0x20, %%al\n\tout %%al, $0x20" : : : "eax");
        return;
    }



    if (c == '1') {
        if (!menu_active && !shell_active) {
            menu_active = 1;
            display_menu();
        } else if (menu_active && !shell_active) {
            menu_active = 0;
            hide_menu();
        }
        asm volatile("mov $0x20, %%al\n\tout %%al, $0x20" : : : "eax");
        return;
    }

    if (c == '2' && !shell_active) {
        halt_system();
        asm volatile("mov $0x20, %%al\n\tout %%al, $0x20" : : : "eax");
        return;
    }

    if (c == '3' && menu_active && !shell_active) {
        display_bsod();
        asm volatile("mov $0x20, %%al\n\tout %%al, $0x20" : : : "eax");
        return;
    }

    if (menu_active && !shell_active) {
        if (c == 'S' || c == 's') {
            shell_active = 1;
            display_shell_prompt();
        } else if (c == 'V' || c == 'v') {
            display_vm_info();
        }
        asm volatile("mov $0x20, %%al\n\tout %%al, $0x20" : : : "eax");
        return;
    }

    if (shell_active) {
        if (shell_index == 0) {
            clear_shell_input();
            display_shell_prompt();
            for (int i = 0; i < 256; i++) {
                shell_buffer[i] = 0;
            }
        }

        if (scancode == 0x0E && shell_index > 0) {
            shell_index--;
            shell_buffer[shell_index] = 0;
            for (int i = 8; i < VGA_WIDTH; i++) {
                vga[23 * VGA_WIDTH + i] = 0x0700;
            }
            print_string_with_attr("SHELL>> ", 23, 0, 0x2F);
            for (int i = 0; i < shell_index; i++) {
                vga[23 * VGA_WIDTH + 8 + i] = 0x2F00 | shell_buffer[i];
            }
            asm volatile("mov $0x20, %%al\n\tout %%al, $0x20" : : : "eax");
            return;
        }

        if (scancode == 0x1C) {
            shell_buffer[shell_index] = 0;
            clear_shell_command_prompt();
            clear_shell_output();

            for (int i = 0; i < shell_index; i++) {
                if (shell_buffer[i] < 32 || shell_buffer[i] > 126) {
                    shell_buffer[i] = ' ';
                }
            }
            shell_buffer[shell_index] = 0;

            if (strcmp(shell_buffer, "print") == 0) {
                append_to_log(shell_buffer);
                print_string("Print command executed!", 15, 0);
                clear_shell_command_prompt();
                print_string("Command: ", 20, 0);
                print_string(shell_buffer, 20, 9);
                display_shell_prompt();
            } else if (strcmp(shell_buffer, "halt") == 0) {
                append_to_log(shell_buffer);
                halt_system();
            } else if (strcmp(shell_buffer, "dump") == 0) {
                append_to_log(shell_buffer);
                dump_screen();
            } else if (strcmp(shell_buffer, "virtual") == 0) {
                append_to_log(shell_buffer);
                display_vm_info();
            } else if (strcmp(shell_buffer, "ls") == 0) {
                append_to_log(shell_buffer);
                if (!vfs_initialized) {
                    print_string("VFS not initialized. ls command disabled.", 15, 0);
                } else {
                    char buf[256] = {0};
                    int len;
                    vfs_list_files(buf, &len);
                    if (len > 0) {
                        for (int i = 0; i < len && i < 255; i++) {
                            if (buf[i] < 32 || buf[i] > 126) {
                                buf[i] = ' ';
                            }
                        }
                        buf[len] = 0;
                        print_string(buf, 15, 0);
                    } else {
                        print_string("No files found or VFS not initialized.", 15, 0);
                    }
                }
                clear_shell_command_prompt();
                print_string("Command: ", 20, 0);
                print_string(shell_buffer, 20, 9);
                display_shell_prompt();
            } else if (strcmp(shell_buffer, "ps") == 0) {
                append_to_log(shell_buffer);
                char buf[64];
                int row = 15;
                for (int i = 0; i < MAX_PROCESSES; i++) {
                    if (processes[i].pid) {
                        int pos = 0;
                        print_number(processes[i].pid, row, 0);
                        pos += 4;
                        const char* name = processes[i].privilege == 0 ? "kernel" : "user";
                        for (int j = 0; name[j]; j++) {
                            buf[pos++] = name[j];
                        }
                        while (pos < 16) buf[pos++] = ' ';
                        const char* state = processes[i].state == 1 ? "Running" : processes[i].state == 0 ? "Ready" : "Terminated";
                        for (int j = 0; state[j]; j++) {
                            buf[pos++] = state[j];
                        }
                        buf[pos] = 0;
                        print_string(buf, row++, 4);
                    }
                }
                clear_shell_command_prompt();
                print_string("Command: ", 20, 0);
                print_string(shell_buffer, 20, 9);
                display_shell_prompt();
            } else if (strcmp(shell_buffer, "clear") == 0) {
                append_to_log(shell_buffer);
                clear_shell();
                display_shell_prompt();
            } else if (strcmp(shell_buffer, "diary") == 0) {
                append_to_log(shell_buffer);
                DiaryNote();
            } else if (strncmp(shell_buffer, "touch ", 6) == 0) {
                append_to_log(shell_buffer);
                if (!vfs_initialized) {
                    print_string("VFS not initialized. touch command disabled.", 15, 0);
                    clear_shell_command_prompt();
                    print_string("Command: ", 20, 0);
                    print_string(shell_buffer, 20, 9);
                    display_shell_prompt();
                } else {
                    const char* filename = shell_buffer + 6;
                    int inode = vfs_create_file(filename);
                    if (inode >= 0) {
                        current_file_fd = vfs_open_file(filename);
                        if (current_file_fd >= 0) {
                            print_string("File created: ", 15, 0);
                            print_string(filename, 15, 14);
                            FileWrite(filename);
                        } else {
                            print_string("Failed to open file for writing", 15, 0);
                            clear_shell_command_prompt();
                            print_string("Command: ", 20, 0);
                            print_string(shell_buffer, 20, 9);
                            display_shell_prompt();
                        }
                    } else {
                        print_string("Failed to create file", 15, 0);
                        clear_shell_command_prompt();
                        print_string("Command: ", 20, 0);
                        print_string(shell_buffer, 20, 9);
                        display_shell_prompt();
                    }
                }
            } else if (strncmp(shell_buffer, "cat ", 4) == 0) {
    append_to_log(shell_buffer);
    if (!vfs_initialized) {
        print_string("VFS not initialized. cat command disabled.", 1, 0);
    } else {
        const char* filename = shell_buffer + 4;
        int fd = vfs_open_file(filename);
        if (fd >= 0) {
            clear_screen();
            char buf[1024] = {0};
            int total_read = 0;
            int bytes_read = vfs_read_file(fd, buf, sizeof(buf) - 1);

            vfs_close_file(fd);

            if (bytes_read <= 0) {
                print_string("File is empty or read error.", 1, 0);
            } else {
                int row = 1, col = 0;
                for (int i = 0; i < bytes_read; i++) {
                    char c = buf[i];
                    if (c < 32 || c > 126) c = ' ';

                    unsigned short* vga = (unsigned short*)VGA_BUFFER;
                    vga[row * VGA_WIDTH + col] = 0x0700 | c;
                    col++;
                    if (col >= VGA_WIDTH) {
                        col = 0;
                        row++;
                        if (row > 21) {
                            // Show prompt to continue
                            print_string("-- More -- Press Space to continue --", 22, 20);

                            // Wait for keypress
                            unsigned char status, scancode;
                            while (1) {
                                asm volatile("inb $0x64, %0" : "=a"(status));
                                if (status & 0x01) {
                                    asm volatile("inb $0x60, %0" : "=a"(scancode));
                                    if (!(scancode & 0x80)) {
                                        break;
                                    }
                                }
                            }

                            clear_screen();
                            row = 1;
                            col = 0;
                        }
                    }
                }
                // If end of file doesn't end with full screen, just wait for key
                print_string("-- End of File -- Press any key --", 22, 20);
                unsigned char status, scancode;
                while (1) {
                    asm volatile("inb $0x64, %0" : "=a"(status));
                    if (status & 0x01) {
                        asm volatile("inb $0x60, %0" : "=a"(scancode));
                        if (!(scancode & 0x80)) break;
                    }
                }
                clear_screen();
            }
        } else {
            print_string("File not found.", 1, 0);
        }
    }

    // Reset shell
    clear_shell_command_prompt();
    print_string("Command: ", 20, 0);
    print_string(shell_buffer, 20, 9);
    display_shell_prompt();
} else if (strncmp(shell_buffer, "kill ", 5) == 0) {
                append_to_log(shell_buffer);
                int pid = 0;
                for (int i = 5; shell_buffer[i] >= '0' && shell_buffer[i] <= '9'; i++) {
                    pid = pid * 10 + (shell_buffer[i] - '0');
                }
                int found = 0;
                for (int i = 0; i < MAX_PROCESSES; i++) {
                    if (processes[i].pid == pid) {
                        kill_process(pid);
                        print_string("Process killed: ", 15, 0);
                        print_number(pid, 15, 16);
                        found = 1;
                        break;
                    }
                }
                if (!found) {
                    print_string("Process not found", 15, 0);
                }
                clear_shell_command_prompt();
                print_string("Command: ", 20, 0);
                print_string(shell_buffer, 20, 9);
                display_shell_prompt();
            } else {
                if (shell_buffer[0] != 0) {
                    append_to_log(shell_buffer);
                    print_string("Unknown command.", 22, 0);
                    clear_shell_command_prompt();
                    print_string("Command: ", 20, 0);
                    print_string(shell_buffer, 20, 9);
                }
                display_shell_prompt();
            }

            shell_index = 0;
            for (int i = 0; i < 256; i++) {
                shell_buffer[i] = 0;
            }
            asm volatile("mov $0x20, %%al\n\tout %%al, $0x20" : : : "eax");
            return;
        }

        if (c && c >= 32 && c <= 126 && shell_index < VGA_WIDTH - 9) {
            shell_buffer[shell_index] = c;
            for (int i = 8; i < VGA_WIDTH; i++) {
                vga[23 * VGA_WIDTH + i] = 0x0700;
            }
            print_string_with_attr("SHELL>> ", 23, 0, 0x2F);
            for (int i = 0; i < shell_index + 1; i++) {
                vga[23 * VGA_WIDTH + 8 + i] = 0x2F00 | shell_buffer[i];
            }
            shell_index++;
            shell_buffer[shell_index] = 0;
        }
        asm volatile("mov $0x20, %%al\n\tout %%al, $0x20" : : : "eax");
        return;
    }

    if (scancode == 0x0E && buffer_index > 0) {
        buffer_index--;
        vga[15 * VGA_WIDTH + buffer_index] = 0x0700;
        keyboard_buffer[buffer_index] = 0;
        asm volatile("mov $0x20, %%al\n\tout %%al, $0x20" : : : "eax");
        return;
    }

    if (scancode == 0x1C) {
        buffer_index = 0;
        for (int i = 0; i < VGA_WIDTH; i++) {
            vga[15 * VGA_WIDTH + i] = 0x0700;
        }
        for (int i = 0; i < 256; i++) {
            keyboard_buffer[i] = 0;
        }
        asm volatile("mov $0x20, %%al\n\tout %%al, $0x20" : : : "eax");
        return;
    }

    if (c && c >= 32 && c <= 126 && buffer_index < VGA_WIDTH - 1) {
        keyboard_buffer[buffer_index] = c;
        vga[15 * VGA_WIDTH + buffer_index] = 0x0700 | c;
        buffer_index++;
        keyboard_buffer[buffer_index] = 0;
    }
    asm volatile("mov $0x20, %%al\n\tout %%al, $0x20" : : : "eax");
}

// Keyboard Initialization
void wait_kbc_input_buffer() {
    unsigned char status;
    do {
        asm volatile("inb $0x64, %0" : "=a"(status));
    } while (status & 0x02);
}

void wait_kbc_output_buffer() {
    unsigned char status;
    do {
        asm volatile("inb $0x64, %0" : "=a"(status));
    } while (!(status & 0x01));
}

void init_keyboard() {
    unsigned char status;
    wait_kbc_input_buffer();
    asm volatile("mov $0xAD, %%al\n\tout %%al, $0x64" : : : "eax");
    wait_kbc_input_buffer();
    asm volatile("mov $0xA7, %%al\n\tout %%al, $0x64" : : : "eax");
    asm volatile("inb $0x60, %0" : "=a"(status) : : "memory");
    wait_kbc_input_buffer();
    asm volatile("mov $0xAE, %%al\n\tout %%al, $0x64" : : : "eax");
    wait_kbc_input_buffer();
    asm volatile("mov $0x20, %%al\n\tout %%al, $0x64" : : : "eax");
    wait_kbc_output_buffer();
    asm volatile("inb $0x60, %0" : "=a"(status) : : "memory");
    status |= 0x01;
    status &= ~0x02;
    wait_kbc_input_buffer();
    asm volatile("mov $0x60, %%al\n\tout %%al, $0x64" : : : "eax");
    wait_kbc_input_buffer();
    asm volatile("mov %0, %%al\n\tout %%al, $0x60" : : "r"(status) : "eax", "memory");
    wait_kbc_input_buffer();
    asm volatile("mov $0xFF, %%al\n\tout %%al, $0x60" : : : "eax", "memory");
    wait_kbc_output_buffer();
    asm volatile("inb $0x60, %0" : "=a"(status) : : "memory");
    if (status != 0xFA) {
        print_string("KBD RESET FAIL", 1, 0);
    }
}

// Interrupt Descriptor Table Setup
void setup_idt() {
    unsigned int* idt = (unsigned int*)0x10000;
    extern void default_handler_wrapper();
    extern void timer_handler_wrapper();
    extern void keyboard_handler_wrapper();
    extern void double_fault_handler_wrapper();
    extern void syscall_handler_wrapper();
    for (int i = 0; i < 256; i++) {
        unsigned int handler = (unsigned int)default_handler_wrapper;
        idt[i * 2] = (handler & 0xFFFF) | (0x08 << 16);
        idt[i * 2 + 1] = (handler & 0xFFFF0000) | 0x8E00;
    }
    unsigned int df_addr = (unsigned int)double_fault_handler_wrapper;
    idt[0x08 * 2] = (df_addr & 0xFFFF) | (0x08 << 16);
    idt[0x08 * 2 + 1] = (df_addr & 0xFFFF0000) | 0x8E00;
    unsigned int timer_addr = (unsigned int)timer_handler_wrapper;
    idt[0x20 * 2] = (timer_addr & 0xFFFF) | (0x08 << 16);
    idt[0x20 * 2 + 1] = (timer_addr & 0xFFFF0000) | 0x8E00;
    unsigned int kb_addr = (unsigned int)keyboard_handler_wrapper;
    idt[0x21 * 2] = (kb_addr & 0xFFFF) | (0x08 << 16);
    idt[0x21 * 2 + 1] = (kb_addr & 0xFFFF0000) | 0x8E00;
    unsigned int syscall_addr = (unsigned int)syscall_handler_wrapper;
    idt[0x80 * 2] = (syscall_addr & 0xFFFF) | (0x08 << 16);
    idt[0x80 * 2 + 1] = (syscall_addr & 0xFFFF0000) | 0xEE00;
    struct {
        unsigned short limit;
        unsigned int base;
    } __attribute__((packed)) idtr = { 256 * 8 - 1, (unsigned int)idt };
    asm volatile("lidt %0" : : "m"(idtr));
    asm volatile(
        "mov $0x11, %%al\n\t"
        "out %%al, $0x20\n\t"
        "out %%al, $0xA0\n\t"
        "mov $0x20, %%al\n\t"
        "out %%al, $0x21\n\t"
        "mov $0x28, %%al\n\t"
        "out %%al, $0xA1\n\t"
        "mov $0x04, %%al\n\t"
        "out %%al, $0x21\n\t"
        "mov $0x02, %%al\n\t"
        "out %%al, $0xA1\n\t"
        "mov $0x01, %%al\n\t"
        "out %%al, $0x21\n\t"
        "out %%al, $0xA1\n\t"
        "mov $0xFC, %%al\n\t"
        "out %%al, $0x21\n\t"
        "mov $0x7F, %%al\n\t"
        "out %%al, $0xA1\n\t"
        : : : "eax"
    );
    for (volatile int i = 0; i < 10000; i++);
}

// Sample User Process
void user_task() {
    char msg[] = "Hello from user space!";
    asm volatile(
        "mov %0, %%ebx\n\t"
        "mov $1, %%eax\n\t"
        "int $0x80"
        : : "r"(msg) : "eax", "ebx"
    );
    asm volatile("mov $3, %%eax\n\tint $0x80" : : : "eax");
}

// Sample Kernel Tasks
void task1() {
    int counter = 0;
    while (1) {
        char buf[10];
        int i = 0, temp = counter;
        do {
            buf[i++] = (temp % 10) + '0';
            temp /= 10;
        } while (temp);
        buf[i] = 0;
        for (int j = 0; j < i / 2; j++) {
            char t = buf[j];
            buf[j] = buf[i - j - 1];
            buf[i - j - 1] = t;
        }
        print_string(buf, 10, 10);
        counter++;
        for (volatile int j = 0; j < 100000; j++);
        schedule_flag = 1;
    }
}

void task2() {
    while (1) {
        for (volatile int j = 0; j < 100000; j++);
        schedule_flag = 1;
    }
}

// Startup and Animation
void startup_animation() {
    const char* welcome_msg = "Sebria OS!";
    int msg_len = 0;
    while (welcome_msg[msg_len]) msg_len++;
    int msg_row = 12;
    int msg_col = 35;
    int bar_row = 13;
    int bar_col = 30;
    int bar_width = 20;
    char bar[22];
    bar[0] = '[';
    bar[bar_width + 1] = ']';
    bar[bar_width + 2] = 0;
    for (int i = 1; i <= bar_width; i++) {
        bar[i] = ' ';
    }
    print_string(bar, bar_row, bar_col);
    for (int i = 0; i < bar_width; i++) {
        bar[i + 1] = '*';
        print_string_with_attr(bar, bar_row, bar_col, 0x09);
        for (int j = 0; j < 10; j++) {
            while (!schedule_flag);
            schedule_flag = 0;
        }
    }
    for (int i = 0; i < 10; i++) {
        while (!schedule_flag);
        schedule_flag = 0;
    }
    for (int i = 0; i < msg_len; i++) {
        unsigned short* vga = (unsigned short*)VGA_BUFFER;
        vga[msg_row * VGA_WIDTH + msg_col + i] = 0x0700 | welcome_msg[i];
        for (int j = 0; j < 5; j++) {
            while (!schedule_flag);
            schedule_flag = 0;
        }
    }
    for (int i = 0; i < 20; i++) {
        while (!schedule_flag);
        schedule_flag = 0;
    }
    clear_screen();
}

void display_instructions() {
    unsigned short* vga = (unsigned short*)VGA_BUFFER;
    const int rect_width = 60;
    const int rect_height = 15;
    const int rect_start_row = (VGA_HEIGHT - rect_height) / 2;
    const int rect_start_col = (VGA_WIDTH - rect_width) / 2;
    const int rect_end_row = rect_start_row + rect_height - 1;
    const int rect_end_col = rect_start_col + rect_width - 1;
    clear_screen();
    for (int row = rect_start_row; row <= rect_end_row; row++) {
        for (int col = rect_start_col; col <= rect_end_col; col++) {
            vga[row * VGA_WIDTH + col] = 0x2F00 | ' ';
        }
    }
    for (int col = rect_start_col; col <= rect_end_col; col++) {
        vga[rect_start_row * VGA_WIDTH + col] = 0x2F00 | '-';
        vga[rect_end_row * VGA_WIDTH + col] = 0x2F00 | '-';
    }
    for (int row = rect_start_row + 1; row < rect_end_row; row++) {
        vga[row * VGA_WIDTH + rect_start_col] = 0x2F00 | '|';
        vga[row * VGA_WIDTH + rect_end_col] = 0x2F00 | '|';
    }
    vga[rect_start_row * VGA_WIDTH + rect_start_col] = 0x2F00 | '+';
    vga[rect_start_row * VGA_WIDTH + rect_end_col] = 0x2F00 | '+';
    vga[rect_end_row * VGA_WIDTH + rect_start_col] = 0x2F00 | '+';
    vga[rect_end_row * VGA_WIDTH + rect_end_col] = 0x2F00 | '+';
    int text_row = rect_start_row + 1;
    int text_col = rect_start_col + 2;
    print_string_with_attr("Sebria OS Instructions", text_row++, text_col, 0x2F);
    print_string_with_attr("----------------------", text_row++, text_col, 0x2F);
    print_string_with_attr("1. Press '1' to show/hide the menu.", text_row++, text_col, 0x2F);
    print_string_with_attr("2. Press 'S' in menu to enter shell.", text_row++, text_col, 0x2F);
    print_string_with_attr("3. Shell commands: ls, ps, touch, cat, kill, clear, diary", text_row++, text_col, 0x2F);
    print_string_with_attr("4. Use 'touch ' to create and write to a file.", text_row++, text_col, 0x2F);
    print_string_with_attr("5. Use 'cat ' to read a file.", text_row++, text_col, 0x2F);
    print_string_with_attr("6. Press 'V' in menu to view virtual memory info.", text_row++, text_col, 0x2F);
    print_string_with_attr("7. Press '2' to halt the system.", text_row++, text_col, 0x2F);
    print_string_with_attr("8. Press '3' in menu to simulate a crash (BSOD).", text_row++, text_col, 0x2F);
    print_string_with_attr("----------------------", text_row++, text_col, 0x2F);
    print_string_with_attr("Press any key to continue...", text_row, text_col, 0x2F);

while (1) {
    unsigned char status, scancode;
    asm volatile("inb $0x64, %0" : "=a"(status));
if (status & 0x01) {
    asm volatile("inb $0x60, %0" : "=a"(scancode));
if (!(scancode & 0x80)) {
    clear_screen();
break;
}
}
}
}

// Kernel Main Function
void kmain() {
clear_screen();
//print_string("Starting Sebria OS...", 1, 0);

// Initialize subsystems
init_paging();

init_vfs(); // Ensure VFS is initialized

setup_idt();
init_keyboard();
asm volatile("sti");

// Initialize processes
for (int i = 0; i < MAX_PROCESSES; i++) {
processes[i].pid = 0;
processes[i].state = 2;
}

// Create sample processes
create_process(task1, 5, 0);
create_process(task2, 3, 0);
create_process(user_task, 2, 3);
processes[0].state = 1; // Set first process as running



// Run startup animation
startup_animation();

// Display instructions
display_instructions();


// Main kernel loop
while (1) {
if (schedule_flag) {
schedule_flag = 0;
schedule();
}
for (volatile int i = 0; i < 1000; i++);
}
}