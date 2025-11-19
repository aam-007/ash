/* kernel.c - PhonexOS (Robust ATA Driver) */
/* Features:
 * 1. VGA Text Mode (Colors)
 * 2. Keyboard (Shift)
 * 3. ATA Driver (Split Polling & Status Debugging)
 * 4. PhonexFS
 * 5. Shell
 * 6. File Deletion
 */

#define VGA_ADDRESS 0xB8000
#define BUFSIZE 256

/* --- Colors --- */
#define BLACK 0
#define BLUE 1
#define GREEN 2
#define CYAN 3
#define RED 4
#define MAGENTA 5
#define BROWN 6
#define LIGHT_GREY 7
#define DARK_GREY 8
#define LIGHT_BLUE 9
#define LIGHT_GREEN 10
#define LIGHT_CYAN 11
#define LIGHT_RED 12
#define LIGHT_MAGENTA 13
#define YELLOW 14
#define WHITE 15

/* --- ATA Constants --- */
#define ATA_PRIMARY_DATA    0x1F0
#define ATA_PRIMARY_ERR     0x1F1
#define ATA_PRIMARY_SEC     0x1F2
#define ATA_PRIMARY_LBA_LO  0x1F3
#define ATA_PRIMARY_LBA_MID 0x1F4
#define ATA_PRIMARY_LBA_HI  0x1F5
#define ATA_PRIMARY_DRIVE   0x1F6
#define ATA_PRIMARY_STATUS  0x1F7
#define ATA_PRIMARY_CMD     0x1F7

#define ATA_CMD_READ      0x20
#define ATA_CMD_WRITE     0x30
#define ATA_CMD_FLUSH     0xE7

#define ATA_SR_BSY     0x80    // Busy
#define ATA_SR_DRDY    0x40    // Drive ready
#define ATA_SR_DRQ     0x08    // Data request ready
#define ATA_SR_ERR     0x01    // Error

#define FILES_MAX 16
#define SECTOR_SIZE 512
#define FS_DIR_SECTOR 1
#define FS_DATA_START 2

typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int uint32;

/* --- VGA Driver --- */
uint16* vga_buffer;
int vga_index = 0;
uint8 vga_color = WHITE;

void terminal_init() {
    vga_buffer = (uint16*)VGA_ADDRESS;
    for (int i = 0; i < 80 * 25; i++) {
        vga_buffer[i] = (uint16) ' ' | (uint16) vga_color << 8;
    }
    vga_index = 0;
}

void terminal_scroll() {
    for(int i = 0; i < 24 * 80; i++){
        vga_buffer[i] = vga_buffer[i+80];
    }
    for(int i = 24 * 80; i < 25 * 80; i++){
        vga_buffer[i] = (uint16) ' ' | (uint16) vga_color << 8;
    }
    vga_index = 24 * 80;
}

void print_char(char c) {
    if(c == '\n') {
        vga_index += 80 - (vga_index % 80);
    } else if (c == '\b') {
        if (vga_index > 0) {
            vga_index--;
            vga_buffer[vga_index] = (uint16) ' ' | (uint16) vga_color << 8;
        }
    } else {
        vga_buffer[vga_index] = (uint16) c | (uint16) vga_color << 8;
        vga_index++;
    }
    if (vga_index >= 80 * 25) terminal_scroll();
}

void print(char* str) {
    int i = 0;
    while (str[i] != '\0') { print_char(str[i]); i++; }
}

void set_color(uint8 color) { vga_color = color; }

void print_color(char* str, uint8 color) {
    uint8 old = vga_color;
    vga_color = color;
    print(str);
    vga_color = old;
}

void print_hex(uint8 n) {
    char *digits = "0123456789ABCDEF";
    print_char(digits[(n >> 4) & 0x0F]);
    print_char(digits[n & 0x0F]);
}

void print_int(int n) {
    if (n == 0) { print("0"); return; }
    if (n < 0) { print("-"); n = -n; }
    char buffer[12];
    int i = 0;
    while (n > 0) { buffer[i++] = (n % 10) + '0'; n /= 10; }
    for (int j = i - 1; j >= 0; j--) print_char(buffer[j]);
}

/* --- I/O Ports --- */
static inline uint8 inb(uint16 port) {
    uint8 ret;
    __asm__ volatile("inb %1, %0" : "=a"(ret) : "dN"(port));
    return ret;
}
static inline void outb(uint16 port, uint8 val) {
    __asm__ volatile("outb %0, %1" : : "a"(val), "dN"(port));
}
static inline uint16 inw(uint16 port) {
    uint16 ret;
    __asm__ volatile("inw %1, %0" : "=a"(ret) : "dN"(port));
    return ret;
}
static inline void outw(uint16 port, uint16 val) {
    __asm__ volatile("outw %0, %1" : : "a"(val), "dN"(port));
}

/* --- Utils --- */
int strcmp(char* s1, char* s2) {
    int i = 0;
    while(s1[i] == s2[i]) { if(s1[i] == '\0') return 0; i++; }
    return s1[i] - s2[i];
}
void strcpy(char* dest, char* src) {
    int i = 0;
    while(src[i] != '\0') { dest[i] = src[i]; i++; }
    dest[i] = '\0';
}
void memset(void *dest, char val, int count) {
    char *temp = (char *)dest;
    for(int i=0; i<count; i++) temp[i] = val;
}

/* --- ATA Hard Disk Driver (Robust) --- */
void disk_400ns_delay() {
    inb(ATA_PRIMARY_STATUS);
    inb(ATA_PRIMARY_STATUS);
    inb(ATA_PRIMARY_STATUS);
    inb(ATA_PRIMARY_STATUS);
}

// Wait for BSY to clear and DRQ to set
int disk_wait_poll() {
    disk_400ns_delay();
    int retry = 100000; 
    while(retry--) {
        uint8 status = inb(ATA_PRIMARY_STATUS);
        
        if (status & ATA_SR_ERR) return 1; // Error
        if (status & 0x20) return 1;       // Drive Fault
        
        // If BSY is clear (0) and DRQ is set (1), we are ready
        if (!(status & ATA_SR_BSY) && (status & ATA_SR_DRQ)) return 0;
    }
    return 2; // Timeout
}

// Wait for BSY to clear (used after write/flush)
int disk_wait_busy() {
    disk_400ns_delay();
    int retry = 100000;
    while(retry--) {
        uint8 status = inb(ATA_PRIMARY_STATUS);
        if (!(status & ATA_SR_BSY)) return 0;
    }
    return 2;
}

void disk_read_sector(uint32 lba, uint8* buffer) {
    // 0xE0 = Master Drive in LBA mode
    outb(ATA_PRIMARY_DRIVE, 0xE0 | ((lba >> 24) & 0x0F));
    outb(ATA_PRIMARY_SEC, 1);
    outb(ATA_PRIMARY_LBA_LO, (uint8)lba);
    outb(ATA_PRIMARY_LBA_MID, (uint8)(lba >> 8));
    outb(ATA_PRIMARY_LBA_HI, (uint8)(lba >> 16));
    outb(ATA_PRIMARY_CMD, ATA_CMD_READ);

    int err = disk_wait_poll();
    if (err) { 
        print_color("[Disk Read Err: 0x", RED);
        print_hex(inb(ATA_PRIMARY_ERR)); // Print error code
        print("] ");
        return; 
    }

    uint16* ptr = (uint16*) buffer;
    for (int i = 0; i < 256; i++) ptr[i] = inw(ATA_PRIMARY_DATA);
}

void disk_write_sector(uint32 lba, uint8* buffer) {
    outb(ATA_PRIMARY_DRIVE, 0xE0 | ((lba >> 24) & 0x0F));
    outb(ATA_PRIMARY_SEC, 1);
    outb(ATA_PRIMARY_LBA_LO, (uint8)lba);
    outb(ATA_PRIMARY_LBA_MID, (uint8)(lba >> 8));
    outb(ATA_PRIMARY_LBA_HI, (uint8)(lba >> 16));
    outb(ATA_PRIMARY_CMD, ATA_CMD_WRITE);

    int err = disk_wait_poll();
    if (err) { 
        print_color("[Disk Write Poll Err: 0x", RED);
        print_hex(inb(ATA_PRIMARY_ERR));
        print("] ");
        return; 
    }

    uint16* ptr = (uint16*) buffer;
    for (int i = 0; i < 256; i++) outw(ATA_PRIMARY_DATA, ptr[i]);

    outb(ATA_PRIMARY_CMD, ATA_CMD_FLUSH);
    disk_wait_busy();
}

/* --- PhonexFS --- */
struct FileHeader { char name[31]; uint8 exists; };
struct FileHeader dir_table[FILES_MAX]; 
uint8 data_buffer[SECTOR_SIZE];

void fs_format() {
    print_color("Formatting disk... ", YELLOW);
    for(int i=0; i<FILES_MAX; i++) {
        dir_table[i].exists = 0;
        memset(dir_table[i].name, 0, 31);
    }
    disk_write_sector(FS_DIR_SECTOR, (uint8*)dir_table);
    print_color("Done.\n", GREEN);
}

void fs_load_dir() { disk_read_sector(FS_DIR_SECTOR, (uint8*)dir_table); }

void fs_list() {
    print_color("Reading disk... ", DARK_GREY);
    fs_load_dir();
    print("\n");
    
    print_color("================================\n", BLUE);
    print_color(" ID | FILENAME           | SIZE \n", LIGHT_CYAN);
    print_color("--------------------------------\n", BLUE);
    
    int found = 0;
    for(int i=0; i<FILES_MAX; i++) {
        if(dir_table[i].exists == 1) {
            print(" "); print_int(i); 
            if(i<10) print("  | "); else print(" | ");
            print(dir_table[i].name);
            int len = 0; while(dir_table[i].name[len]) len++;
            for(int k=0; k<(19-len); k++) print(" ");
            print("| 512B\n");
            found = 1;
        }
    }
    if(!found) print_color(" (Disk Empty)                   \n", DARK_GREY);
    print_color("================================\n", BLUE);
}

void fs_save(char* name, char* content) {
    fs_load_dir();
    // Overwrite
    for(int i=0; i<FILES_MAX; i++) {
        if(dir_table[i].exists && strcmp(dir_table[i].name, name) == 0) {
            memset(data_buffer, 0, SECTOR_SIZE);
            strcpy((char*)data_buffer, content);
            disk_write_sector(FS_DATA_START + i, data_buffer);
            print_color("File overwritten.\n", GREEN); return;
        }
    }
    // New
    for(int i=0; i<FILES_MAX; i++) {
        if(dir_table[i].exists == 0) {
            dir_table[i].exists = 1;
            strcpy(dir_table[i].name, name);
            disk_write_sector(FS_DIR_SECTOR, (uint8*)dir_table);
            memset(data_buffer, 0, SECTOR_SIZE);
            strcpy((char*)data_buffer, content);
            disk_write_sector(FS_DATA_START + i, data_buffer);
            print_color("File saved.\n", GREEN); return;
        }
    }
    print_color("Disk Full.\n", RED);
}

void fs_read(char* name) {
    fs_load_dir();
    for(int i=0; i<FILES_MAX; i++) {
        if(dir_table[i].exists && strcmp(dir_table[i].name, name) == 0) {
            disk_read_sector(FS_DATA_START + i, data_buffer);
            print_color("Content: \n", YELLOW);
            print_color((char*)data_buffer, WHITE);
            print("\n");
            return;
        }
    }
    print_color("File not found.\n", RED);
}

/* --- NEW: File Deletion Function --- */
void fs_delete(char* name) {
    fs_load_dir();
    for(int i=0; i<FILES_MAX; i++) {
        if(dir_table[i].exists && strcmp(dir_table[i].name, name) == 0) {
            // Mark file as deleted in directory
            dir_table[i].exists = 0;
            memset(dir_table[i].name, 0, 31);
            
            // Write updated directory to disk
            disk_write_sector(FS_DIR_SECTOR, (uint8*)dir_table);
            
            // Optional: Clear the data sector (write zeros)
            memset(data_buffer, 0, SECTOR_SIZE);
            disk_write_sector(FS_DATA_START + i, data_buffer);
            
            print_color("File '", GREEN);
            print_color(name, YELLOW);
            print_color("' deleted successfully.\n", GREEN);
            return;
        }
    }
    print_color("File '", RED);
    print_color(name, YELLOW);
    print_color("' not found.\n", RED);
}

/* --- Calculator --- */
void simple_calc(char* cmd) {
    int a = 0, b = 0, i = 5;
    char op = 0;
    while(cmd[i] >= '0' && cmd[i] <= '9') { a = a * 10 + (cmd[i] - '0'); i++; }
    while(cmd[i] == ' ') i++;
    op = cmd[i]; i++;
    while(cmd[i] == ' ') i++;
    while(cmd[i] >= '0' && cmd[i] <= '9') { b = b * 10 + (cmd[i] - '0'); i++; }

    print("Result: ");
    if (op == '+') print_int(a + b);
    else if (op == '-') print_int(a - b);
    else if (op == '*') print_int(a * b);
    else if (op == '/') { if(b==0) print("Div0"); else print_int(a / b); }
    else print("Bad Op");
    print("\n");
}

/* --- Help --- */
void help_command() {
    print("\n");
    print_color("========================================\n", LIGHT_BLUE);
    print_color("          PHONEX OS COMMANDS            \n", LIGHT_GREEN);
    print_color("========================================\n", LIGHT_BLUE);
    print("  help      | List available cmnds\n");
    print("  ls        | List files\n");
    print("  write     | Create/Edit file\n");
    print("  read      | Read file\n");
    print("  delete    | Delete file\n");
    print("  calc      | e.g. 1 + 1\n");
    print("  format    | Erase all data\n");
    print("  reboot    | Restart\n");
    print("  poweroff  | Shutdown\n");
    print("  whoisthis | OS info\n");
    print_color("========================================\n", LIGHT_BLUE);
}

/* --- Keyboard --- */
char kbd_map[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',   
  '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',   
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',   
    0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/',   0,   
  '*', 0, ' '
};
char kbd_map_shift[128] = {
    0,  27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',   
  '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',   
    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',   
    0, '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?',   0,   
  '*', 0, ' '
};
int shift_pressed = 0;

char get_input_char() {
    while(1) {
        if(inb(0x64) & 0x1) {
            uint8 scancode = inb(0x60);
            if(scancode == 0x2A || scancode == 0x36) { shift_pressed = 1; continue; }
            if(scancode == 0xAA || scancode == 0xB6) { shift_pressed = 0; continue; }
            if (scancode & 0x80) continue;
            if (shift_pressed) return kbd_map_shift[scancode];
            return kbd_map[scancode];
        }
    }
}

void get_string(char* buffer) {
    int i = 0;
    char c;
    while(1) {
        c = get_input_char();
        if (c == '\n') { print_char('\n'); buffer[i] = '\0'; return; }
        else if (c == '\b') { if (i > 0) { print_char('\b'); i--; } }
        else if (c != 0) { print_char(c); buffer[i] = c; i++; }
    }
}

void kernel_main() {
    terminal_init();
    print_color("Phonex - Ash\n", LIGHT_CYAN);
    print("Type "); print_color("'help'", YELLOW); print(" for commands.\n");
    
    char buffer[BUFSIZE];
    while(1) {
        print_color("phonex> ", LIGHT_GREEN);
        get_string(buffer);
        
        if (strcmp(buffer, "ls") == 0) fs_list();
        else if (strcmp(buffer, "format") == 0) fs_format();
        else if (strcmp(buffer, "list") == 0 || strcmp(buffer, "help") == 0) help_command();
        else if (strcmp(buffer, "whoisthis") == 0) { print_color("PhonexOS ver 1 (Ash) | PhonexLegend ( A M ) | dev build \n", LIGHT_MAGENTA); }
        else if (strcmp(buffer, "reboot") == 0) { uint8 t=2; while(t&2)t=inb(0x64); outb(0x64, 0xFE); }
        else if (strcmp(buffer, "poweroff") == 0) { outw(0x604, 0x2000); outw(0x4004, 0x3400); }
        else if (buffer[0]=='w' && buffer[1]=='r') {
             char f[32]; int j=0, k=6; while(buffer[k]) f[j++]=buffer[k++]; f[j]=0;
             print("Content: "); char c[256]; get_string(c); fs_save(f, c);
        }
        else if (buffer[0]=='r' && buffer[1]=='e') {
             char f[32]; int j=0, k=5; while(buffer[k]) f[j++]=buffer[k++]; f[j]=0;
             fs_read(f);
        }
        else if (buffer[0]=='d' && buffer[1]=='e') {
             char f[32]; int j=0, k=7; while(buffer[k]) f[j++]=buffer[k++]; f[j]=0;
             fs_delete(f);
        }
        else if (buffer[0]=='c' && buffer[1]=='a') simple_calc(buffer);
        else if (strcmp(buffer, "clear") == 0) terminal_init();
        else print("Unknown command. Type 'help' for available commands.\n");
    }
}