// WinMain cakismasini kokten engellemek icin en ustte tanimliyoruz
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define MAX_LINES 2000
#define MAX_LINE_LEN 512
#define MAX_HISTORY 50

// -----------------------------------------------------------------------------
// CORE DATA STRUCTURES

typedef struct {
    char lines[MAX_LINES][MAX_LINE_LEN];
    int line_count;
    int cursor_x;
    int cursor_y;
    char filename[256];
} EditorState;

EditorState undo_stack[MAX_HISTORY];
int undo_count = 0;

EditorState redo_stack[MAX_HISTORY];
int redo_count = 0;

HANDLE hInput, hOutput;
DWORD originalConsoleMode;

// -----------------------------------------------------------------------------
// CONSOLE & TERMINAL MANAGEMENT

void enableRawMode() {
    hInput = GetStdHandle(STD_INPUT_HANDLE);
    hOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    GetConsoleMode(hInput, &originalConsoleMode);
    DWORD rawMode = originalConsoleMode & ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT);
    SetConsoleMode(hInput, rawMode);
}

void disableRawMode() {
    SetConsoleMode(hInput, originalConsoleMode);
}

void setCursorPosition(int x, int y) {
    COORD coord = { (SHORT)x, (SHORT)y };
    SetConsoleCursorPosition(hOutput, coord);
}

void clearScreen() {
    COORD topLeft = { 0, 0 };
    CONSOLE_SCREEN_BUFFER_INFO screen;
    DWORD written;
    GetConsoleScreenBufferInfo(hOutput, &screen);
    FillConsoleOutputCharacterA(hOutput, ' ', screen.dwSize.X * screen.dwSize.Y, topLeft, &written);
    FillConsoleOutputAttribute(hOutput, FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_BLUE, screen.dwSize.X * screen.dwSize.Y, topLeft, &written);
    SetConsoleCursorPosition(hOutput, topLeft);
}

// -----------------------------------------------------------------------------
// HISTORY MANAGEMENT (UNDO / REDO)

void saveSnapshot(EditorState *ed) {
    if (undo_count < MAX_HISTORY) {
        undo_stack[undo_count++] = *ed;
    } else {
        for (int i = 0; i < MAX_HISTORY - 1; i++) undo_stack[i] = undo_stack[i + 1];
        undo_stack[MAX_HISTORY - 1] = *ed;
    }
    redo_count = 0;
}

void undo(EditorState *ed) {
    if (undo_count > 0) {
        if (redo_count < MAX_HISTORY) redo_stack[redo_count++] = *ed;
        *ed = undo_stack[--undo_count];
    }
}

void redo(EditorState *ed) {
    if (redo_count > 0) {
        if (undo_count < MAX_HISTORY) undo_stack[undo_count++] = *ed;
        *ed = redo_stack[--redo_count];
    }
}

// -----------------------------------------------------------------------------
// FILE I/O & BUILD SYSTEM 

void saveFile(EditorState *ed) {
    FILE *fp = fopen(ed->filename, "w");
    if (!fp) return;
    for (int i = 0; i < ed->line_count; i++) {
        fprintf(fp, "%s\n", ed->lines[i]);
    }
    fclose(fp);
}

void loadFile(EditorState *ed, const char *filename) {
    strcpy(ed->filename, filename);
    FILE *fp = fopen(filename, "r");
    
    if (fp == NULL) {
        // Dosya yoksa otomatik yaratmak uzere 1 bos satir acilir
        ed->line_count = 1;
        ed->lines[0][0] = '\0';
        return;
    }

    ed->line_count = 0;
    while (fgets(ed->lines[ed->line_count], MAX_LINE_LEN, fp)) {
        ed->lines[ed->line_count][strcspn(ed->lines[ed->line_count], "\r\n")] = 0;
        ed->line_count++;
        if (ed->line_count >= MAX_LINES) break;
    }
    fclose(fp);

    if (ed->line_count == 0) {
        ed->line_count = 1;
        ed->lines[0][0] = '\0';
    }
}

// DERLEME VE CALISTIRMA MOTORU
void buildAndRun(EditorState *ed) {
    saveFile(ed); 
    char *ext = strrchr(ed->filename, '.');
    if (!ext) return;

    char command[512] = {0};
    char nameNoExt[256] = {0};
    strncpy(nameNoExt, ed->filename, ext - ed->filename);

    // C & C++ & Assembly
    if (strcmp(ext, ".c") == 0)           snprintf(command, sizeof(command), "gcc %s -o %s.exe && %s.exe", ed->filename, nameNoExt, nameNoExt);
    else if (strcmp(ext, ".cpp") == 0 || strcmp(ext, ".cxx") == 0 || strcmp(ext, ".cc") == 0) 
                                          snprintf(command, sizeof(command), "g++ %s -o %s.exe && %s.exe", ed->filename, nameNoExt, nameNoExt);
    else if (strcmp(ext, ".asm") == 0 || strcmp(ext, ".s") == 0 || strcmp(ext, ".S") == 0) 
                                          snprintf(command, sizeof(command), "gcc %s -o %s.exe && %s.exe", ed->filename, nameNoExt, nameNoExt);

    // Modern Systems Languages
    else if (strcmp(ext, ".rs") == 0)     snprintf(command, sizeof(command), "rustc %s && %s.exe", ed->filename, nameNoExt);
    else if (strcmp(ext, ".go") == 0)     snprintf(command, sizeof(command), "go run %s", ed->filename);
    else if (strcmp(ext, ".zig") == 0)    snprintf(command, sizeof(command), "zig run %s", ed->filename);
    else if (strcmp(ext, ".d") == 0)      snprintf(command, sizeof(command), "rdmd %s", ed->filename);
    else if (strcmp(ext, ".nim") == 0)    snprintf(command, sizeof(command), "nim r %s", ed->filename);
    else if (strcmp(ext, ".v") == 0)      snprintf(command, sizeof(command), "v run %s", ed->filename);
    else if (strcmp(ext, ".odin") == 0)   snprintf(command, sizeof(command), "odin run %s -file", ed->filename);
    else if (strcmp(ext, ".swift") == 0)  snprintf(command, sizeof(command), "swift %s", ed->filename);

    // JVM & Managed Languages
    else if (strcmp(ext, ".java") == 0)   snprintf(command, sizeof(command), "java %s", ed->filename);
    else if (strcmp(ext, ".kt") == 0 || strcmp(ext, ".kts") == 0) 
                                          snprintf(command, sizeof(command), "kotlinc %s -include-runtime -d %s.jar && java -jar %s.jar", ed->filename, nameNoExt, nameNoExt);
    else if (strcmp(ext, ".scala") == 0)  snprintf(command, sizeof(command), "scala %s", ed->filename);
    else if (strcmp(ext, ".cs") == 0)     snprintf(command, sizeof(command), "dotnet run", ed->filename);
    else if (strcmp(ext, ".fs") == 0)     snprintf(command, sizeof(command), "dotnet run", ed->filename);

    // Scripting & Interpreted
    else if (strcmp(ext, ".py") == 0 || strcmp(ext, ".pyw") == 0) 
                                          snprintf(command, sizeof(command), "python %s", ed->filename);
    else if (strcmp(ext, ".js") == 0)     snprintf(command, sizeof(command), "node %s", ed->filename);
    else if (strcmp(ext, ".ts") == 0)     snprintf(command, sizeof(command), "ts-node %s", ed->filename);
    else if (strcmp(ext, ".rb") == 0)     snprintf(command, sizeof(command), "ruby %s", ed->filename);
    else if (strcmp(ext, ".php") == 0)    snprintf(command, sizeof(command), "php %s", ed->filename);
    else if (strcmp(ext, ".lua") == 0)    snprintf(command, sizeof(command), "lua %s", ed->filename);
    else if (strcmp(ext, ".pl") == 0)     snprintf(command, sizeof(command), "perl %s", ed->filename);
    else if (strcmp(ext, ".r") == 0 || strcmp(ext, ".R") == 0) 
                                          snprintf(command, sizeof(command), "Rscript %s", ed->filename);
    else if (strcmp(ext, ".jl") == 0)     snprintf(command, sizeof(command), "julia %s", ed->filename);
    else if (strcmp(ext, ".dart") == 0)   snprintf(command, sizeof(command), "dart run %s", ed->filename);

    // Shell Scripts & Windows Batch
    else if (strcmp(ext, ".bat") == 0 || strcmp(ext, ".cmd") == 0) 
                                          snprintf(command, sizeof(command), "%s", ed->filename);
    else if (strcmp(ext, ".ps1") == 0)    snprintf(command, sizeof(command), "powershell -ExecutionPolicy Bypass -File %s", ed->filename);
    else if (strcmp(ext, ".sh") == 0)     snprintf(command, sizeof(command), "bash %s", ed->filename);

    // Functional Languages
    else if (strcmp(ext, ".hs") == 0)     snprintf(command, sizeof(command), "runhaskell %s", ed->filename);
    else if (strcmp(ext, ".ml") == 0)     snprintf(command, sizeof(command), "ocaml %s", ed->filename);
    else if (strcmp(ext, ".ex") == 0 || strcmp(ext, ".exs") == 0) 
                                          snprintf(command, sizeof(command), "elixir %s", ed->filename);
    else if (strcmp(ext, ".erl") == 0)    snprintf(command, sizeof(command), "escript %s", ed->filename);
    else if (strcmp(ext, ".clj") == 0)    snprintf(command, sizeof(command), "clojure %s", ed->filename);
    else if (strcmp(ext, ".scm") == 0 || strcmp(ext, ".ss") == 0 || strcmp(ext, ".rkt") == 0) 
                                          snprintf(command, sizeof(command), "racket %s", ed->filename);
    else if (strcmp(ext, ".lisp") == 0 || strcmp(ext, ".lsp") == 0) 
                                          snprintf(command, sizeof(command), "sbcl --script %s", ed->filename);

    // Web & Data Formats
    else if (strcmp(ext, ".html") == 0)   snprintf(command, sizeof(command), "start %s", ed->filename);
    else if (strcmp(ext, ".sql") == 0)    snprintf(command, sizeof(command), "sqlite3 < %s", ed->filename);

    // Additional Specialized Languages
    else if (strcmp(ext, ".pas") == 0)    snprintf(command, sizeof(command), "fpc %s && %s.exe", ed->filename, nameNoExt);
    else if (strcmp(ext, ".fortran") == 0 || strcmp(ext, ".f90") == 0 || strcmp(ext, ".f") == 0) 
                                          snprintf(command, sizeof(command), "gfortran %s -o %s.exe && %s.exe", ed->filename, nameNoExt, nameNoExt);
    else if (strcmp(ext, ".tcl") == 0)    snprintf(command, sizeof(command), "tclsh %s", ed->filename);
    else if (strcmp(ext, ".awk") == 0)    snprintf(command, sizeof(command), "gawk -f %s", ed->filename);
    else if (strcmp(ext, ".vbs") == 0)    snprintf(command, sizeof(command), "cscript //Nologo %s", ed->filename);

    if (strlen(command) > 0) {
        disableRawMode();
        system("cls");
        printf("--- AC EDITOR BUILD SYSTEM ---\n");
        printf(">> Executing: %s\n\n", command);
        
        system(command);
        
        printf("\n\n[Process finished. Press ENTER to return to aceditor...]");
        getchar();
        enableRawMode();
    }
}

// -----------------------------------------------------------------------------
// UI RENDERING 

void renderUI(EditorState *ed) {
    clearScreen();

    // 1. UST BASLIK (Invert Colors)
    SetConsoleTextAttribute(hOutput, BACKGROUND_GREEN | BACKGROUND_RED | BACKGROUND_BLUE);
    printf("  aceditor %-30s File: %-30s \n", " ", ed->filename);
    
    // Rengi normale dondur
    SetConsoleTextAttribute(hOutput, FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_BLUE);

    // 2. METIN ALANI
    for (int i = 0; i < ed->line_count; i++) {
        printf("%s\n", ed->lines[i]);
    }

    // 3. ALT MENU (Nano Style + Imza)
    CONSOLE_SCREEN_BUFFER_INFO screen;
    GetConsoleScreenBufferInfo(hOutput, &screen);
    
    int menu_y = screen.srWindow.Bottom - 2; 
    setCursorPosition(0, menu_y);
    
    SetConsoleTextAttribute(hOutput, BACKGROUND_GREEN | BACKGROUND_RED | BACKGROUND_BLUE);
    printf(" ^X Save    ^E Exit    ^D Clear All    ^Z Undo \n");
    printf(" ^R Redo    ^L Build & Run       [ by-Emrre_Drn ] \n");
    SetConsoleTextAttribute(hOutput, FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_BLUE);

    // 4. IMLECI KONUMLANDIR
    setCursorPosition(ed->cursor_x, ed->cursor_y + 1);
}

// -----------------------------------------------------------------------------
// INPUT HANDLING
bool processKeyPress(EditorState *ed) {
    DWORD bytesRead;
    INPUT_RECORD record;
    ReadConsoleInput(hInput, &record, 1, &bytesRead);

    if (record.EventType == KEY_EVENT && record.Event.KeyEvent.bKeyDown) {
        char ch = record.Event.KeyEvent.uChar.AsciiChar;
        WORD vk = record.Event.KeyEvent.wVirtualKeyCode;
        DWORD ctrlState = record.Event.KeyEvent.dwControlKeyState;

        // CTRL KISAYOLLARI
        if (ctrlState & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)) {
            if (vk == 'X' || vk == 'x') { saveFile(ed); return true; }
            if (vk == 'E' || vk == 'e') { return false; }
            if (vk == 'D' || vk == 'd') {
                saveSnapshot(ed);
                memset(ed->lines, 0, sizeof(ed->lines));
                ed->line_count = 1;
                ed->cursor_x = 0;
                ed->cursor_y = 0;
                return true;
            }
            if (vk == 'Z' || vk == 'z') { undo(ed); return true; }
            if (vk == 'R' || vk == 'r') { redo(ed); return true; }
            if (vk == 'L' || vk == 'l') { buildAndRun(ed); return true; }
        }

        // YON TUSLARI
        if (vk == VK_LEFT) { if (ed->cursor_x > 0) ed->cursor_x--; }
        else if (vk == VK_RIGHT) { if (ed->cursor_x < strlen(ed->lines[ed->cursor_y])) ed->cursor_x++; }
        else if (vk == VK_UP) { 
            if (ed->cursor_y > 0) {
                ed->cursor_y--;
                int upper_len = strlen(ed->lines[ed->cursor_y]);
                if (ed->cursor_x > upper_len) ed->cursor_x = upper_len;
            }
        }
        else if (vk == VK_DOWN) { 
            if (ed->cursor_y < ed->line_count - 1) {
                ed->cursor_y++;
                int lower_len = strlen(ed->lines[ed->cursor_y]);
                if (ed->cursor_x > lower_len) ed->cursor_x = lower_len;
            }
        }

        // BACKSPACE (SILME)
        else if (vk == VK_BACK) {
            saveSnapshot(ed);
            if (ed->cursor_x > 0) {
                int len = strlen(ed->lines[ed->cursor_y]);
                for (int i = ed->cursor_x - 1; i < len; i++) {
                    ed->lines[ed->cursor_y][i] = ed->lines[ed->cursor_y][i + 1];
                }
                ed->cursor_x--;
            } else if (ed->cursor_y > 0) {
                int prev_len = strlen(ed->lines[ed->cursor_y - 1]);
                int curr_len = strlen(ed->lines[ed->cursor_y]);
                if (prev_len + curr_len < MAX_LINE_LEN) {
                    strcat(ed->lines[ed->cursor_y - 1], ed->lines[ed->cursor_y]);
                    for (int i = ed->cursor_y; i < ed->line_count - 1; i++) {
                        strcpy(ed->lines[i], ed->lines[i + 1]);
                    }
                    ed->line_count--;
                    ed->cursor_y--;
                    ed->cursor_x = prev_len;
                }
            }
        }
        
        // ENTER (YENI SATIR)
        else if (vk == VK_RETURN) {
            saveSnapshot(ed);
            if (ed->line_count < MAX_LINES) {
                char *rest = &ed->lines[ed->cursor_y][ed->cursor_x];
                for (int i = ed->line_count; i > ed->cursor_y + 1; i--) {
                    strcpy(ed->lines[i], ed->lines[i - 1]);
                }
                strcpy(ed->lines[ed->cursor_y + 1], rest);
                ed->lines[ed->cursor_y][ed->cursor_x] = '\0';
                ed->line_count++;
                ed->cursor_y++;
                ed->cursor_x = 0;
            }
        }
        
        // NORMAL KARAKTER GIRISI
        else if (ch >= 32 && ch <= 126) {
            saveSnapshot(ed);
            int len = strlen(ed->lines[ed->cursor_y]);
            if (len < MAX_LINE_LEN - 1) {
                for (int i = len; i >= ed->cursor_x; i--) {
                    ed->lines[ed->cursor_y][i + 1] = ed->lines[ed->cursor_y][i];
                }
                ed->lines[ed->cursor_y][ed->cursor_x] = ch;
                ed->cursor_x++;
            }
        }
    }
    return true;
}

// -----------------------------------------------------------------------------
// MAIN ENTRY POINT
// -----------------------------------------------------------------------------
#undef main
int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: ac <filename>\n");
        return 1;
    }

    EditorState ed = {0};
    loadFile(&ed, argv[1]);

    enableRawMode();

    bool running = true;
    while (running) {
        renderUI(&ed);
        running = processKeyPress(&ed);
    }

    disableRawMode();
    clearScreen();
    printf("aceditor closed successfully.\n");
    return 0;
}