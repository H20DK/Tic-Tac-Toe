#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <stdlib.h>
#include <time.h>
#include <shellapi.h>
#include <tchar.h>      // Для _T, TEXT, _tcscmp, _ttoi и т.д.
#include <strsafe.h>    // Для StringCchCopy и безопасной работы со строками

// Default values
#define DEFAULT_N           4
#define DEFAULT_WIDTH       320
#define DEFAULT_HEIGHT      240
#define DEFAULT_BG_COLOR    RGB(0, 0, 255)   // Blue
#define DEFAULT_GRID_COLOR  RGB(255, 0, 0)   // Red
#define CIRCLE_COLOR        RGB(0, 255, 0)   // Green
#define CROSS_COLOR         RGB(255, 255, 0) // Yellow

// Config file
const TCHAR* CONFIG_FILE = _T("config.txt");

// Struct for cell states
enum CellType { EMPTY, CIRCLE, CROSS };
struct Cell {
    CellType type = EMPTY;
};

// Global variables
int N = DEFAULT_N;
int winWidth = DEFAULT_WIDTH;
int winHeight = DEFAULT_HEIGHT;
COLORREF bgColor = DEFAULT_BG_COLOR;
COLORREF gridColor = DEFAULT_GRID_COLOR;
Cell* cells = nullptr;
HBRUSH hBgBrush = nullptr;

// Function prototypes
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
void DrawGrid(HDC hdc, int width, int height);
void DrawCells(HDC hdc, int width, int height);
void LoadConfig();
void SaveConfig();
void ChangeBgColor(HWND hwnd);
void ChangeGridColor(int delta);

// Parse command line for N (Unicode version)
void ParseCmdLine() {
    LPWSTR cmdLine = GetCommandLineW();
    if (cmdLine) {
        // Пропускаем имя программы
        if (*cmdLine == L'"') {
            cmdLine = wcschr(cmdLine + 1, L'"');
            if (cmdLine) cmdLine++;
        }
        else {
            cmdLine = wcschr(cmdLine, L' ');
        }
        if (cmdLine && *cmdLine == L' ') {
            cmdLine++;
            N = _wtoi(cmdLine);
            if (N <= 0) N = DEFAULT_N;
        }
    }
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) {
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    srand((unsigned int)time(NULL));

    // Загружаем конфиг
    LoadConfig();

    // Аргумент командной строки имеет приоритет
    ParseCmdLine();

    // Выделяем память под клетки
    cells = new Cell[N * N];

    // Регистрация класса окна
    WNDCLASSEX wc = { 0 };
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = TEXT("MyWindowClass");

    hBgBrush = CreateSolidBrush(bgColor);
    wc.hbrBackground = hBgBrush;

    RegisterClassEx(&wc);

    // Создание окна
    HWND hwnd = CreateWindowEx(
        0,
        TEXT("MyWindowClass"),
        TEXT("Лабораторная 2 — WinAPI"),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        winWidth, winHeight,
        NULL, NULL, hInstance, NULL);

    if (!hwnd) {
        return 0;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    // Цикл сообщений
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Очистка
    delete[] cells;
    DeleteObject(hBgBrush);

    return (int)msg.wParam;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        RECT clientRect;
        GetClientRect(hwnd, &clientRect);
        DrawGrid(hdc, clientRect.right, clientRect.bottom);
        DrawCells(hdc, clientRect.right, clientRect.bottom);

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_SIZE: {
        winWidth = LOWORD(lParam);
        winHeight = HIWORD(lParam);
        InvalidateRect(hwnd, NULL, TRUE);
        return 0;
    }

    case WM_LBUTTONDOWN: {
        int x = LOWORD(lParam);
        int y = HIWORD(lParam);
        RECT clientRect;
        GetClientRect(hwnd, &clientRect);
        int cellW = clientRect.right / N;
        int cellH = clientRect.bottom / N;
        int col = x / cellW;
        int row = y / cellH;
        if (col >= 0 && col < N && row >= 0 && row < N) {
            cells[row * N + col].type = CIRCLE;
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    }

    case WM_RBUTTONDOWN: {
        int x = LOWORD(lParam);
        int y = HIWORD(lParam);
        RECT clientRect;
        GetClientRect(hwnd, &clientRect);
        int cellW = clientRect.right / N;
        int cellH = clientRect.bottom / N;
        int col = x / cellW;
        int row = y / cellH;
        if (col >= 0 && col < N && row >= 0 && row < N) {
            cells[row * N + col].type = CROSS;
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    }

    case WM_KEYDOWN: {
        if (wParam == VK_ESCAPE) {
            PostQuitMessage(0);
        }
        else if (wParam == 'Q' && (GetAsyncKeyState(VK_CONTROL) & 0x8000)) {
            PostQuitMessage(0);
        }
        else if (wParam == 'C' && (GetAsyncKeyState(VK_SHIFT) & 0x8000)) {
            ShellExecute(NULL, TEXT("open"), TEXT("notepad.exe"), NULL, NULL, SW_SHOW);
        }
        else if (wParam == VK_RETURN) {
            ChangeBgColor(hwnd);
            InvalidateRect(hwnd, NULL, TRUE);
        }
        return 0;
    }

    case WM_MOUSEWHEEL: {
        short delta = GET_WHEEL_DELTA_WPARAM(wParam);
        ChangeGridColor(delta);
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    }

    case WM_DESTROY: {
        SaveConfig();
        PostQuitMessage(0);
        return 0;
    }
    }
    return DefWindowProc(hwnd, message, wParam, lParam);
}

void DrawGrid(HDC hdc, int width, int height) {
    HPEN hPen = CreatePen(PS_SOLID, 1, gridColor);
    HGDIOBJ oldPen = SelectObject(hdc, hPen);

    int cellW = width / N;
    int cellH = height / N;

    for (int i = 1; i < N; ++i) {
        MoveToEx(hdc, i * cellW, 0, NULL);
        LineTo(hdc, i * cellW, height);
    }
    for (int i = 1; i < N; ++i) {
        MoveToEx(hdc, 0, i * cellH, NULL);
        LineTo(hdc, width, i * cellH);
    }

    SelectObject(hdc, oldPen);
    DeleteObject(hPen);
}

void DrawCells(HDC hdc, int width, int height) {
    int cellW = width / N;
    int cellH = height / N;

    for (int row = 0; row < N; ++row) {
        for (int col = 0; col < N; ++col) {
            Cell cell = cells[row * N + col];
            int x = col * cellW;
            int y = row * cellH;

            if (cell.type == CIRCLE) {
                HPEN hPen = CreatePen(PS_SOLID, 2, CIRCLE_COLOR);
                HBRUSH hBrush = CreateSolidBrush(CIRCLE_COLOR);
                HGDIOBJ oldPen = SelectObject(hdc, hPen);
                HGDIOBJ oldBrush = SelectObject(hdc, hBrush);

                Ellipse(hdc, x + 5, y + 5, x + cellW - 5, y + cellH - 5);

                SelectObject(hdc, oldPen);
                SelectObject(hdc, oldBrush);
                DeleteObject(hPen);
                DeleteObject(hBrush);
            }
            else if (cell.type == CROSS) {
                HPEN hPen = CreatePen(PS_SOLID, 2, CROSS_COLOR);
                HGDIOBJ oldPen = SelectObject(hdc, hPen);

                MoveToEx(hdc, x + 5, y + 5, NULL);
                LineTo(hdc, x + cellW - 5, y + cellH - 5);
                MoveToEx(hdc, x + cellW - 5, y + 5, NULL);
                LineTo(hdc, x + 5, y + cellH - 5);

                SelectObject(hdc, oldPen);
                DeleteObject(hPen);
            }
        }
    }
}

void LoadConfig() {
    if (GetFileAttributes(CONFIG_FILE) == INVALID_FILE_ATTRIBUTES) {
        return;
    }

    HANDLE hFile = CreateFile(CONFIG_FILE, GENERIC_READ, FILE_SHARE_READ, NULL,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return;

    char buffer[512];
    DWORD bytesRead;
    ReadFile(hFile, buffer, sizeof(buffer) - 1, &bytesRead, NULL);
    buffer[bytesRead] = '\0';
    CloseHandle(hFile);

    // Очень простой парсинг (можно улучшить)
    char* line = strtok(buffer, "\n");
    while (line) {
        char* eq = strchr(line, '=');
        if (eq) {
            *eq = '\0';
            char* key = line;
            char* val = eq + 1;

            if (_stricmp(key, "N") == 0)          N = atoi(val);
            else if (_stricmp(key, "Width") == 0)  winWidth = atoi(val);
            else if (_stricmp(key, "Height") == 0) winHeight = atoi(val);
            else if (_stricmp(key, "BgColor") == 0)   bgColor = strtoul(val, NULL, 10);
            else if (_stricmp(key, "GridColor") == 0) gridColor = strtoul(val, NULL, 10);
        }
        line = strtok(NULL, "\n");
    }
}

void SaveConfig() {
    HANDLE hFile = CreateFile(CONFIG_FILE, GENERIC_WRITE, 0, NULL,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return;

    TCHAR buffer[512];
    StringCchPrintf(buffer, ARRAYSIZE(buffer),
        _T("N=%d\nWidth=%d\nHeight=%d\nBgColor=%lu\nGridColor=%lu\n"),
        N, winWidth, winHeight, bgColor, gridColor);

    DWORD bytesWritten;
    WriteFile(hFile, buffer, (DWORD)(_tcslen(buffer) * sizeof(TCHAR)), &bytesWritten, NULL);
    CloseHandle(hFile);
}

void ChangeBgColor(HWND hwnd) {
    bgColor = RGB(rand() % 256, rand() % 256, rand() % 256);

    DeleteObject(hBgBrush);
    hBgBrush = CreateSolidBrush(bgColor);

    SetClassLongPtr(hwnd, GCLP_HBRBACKGROUND, (LONG_PTR)hBgBrush);
    InvalidateRect(hwnd, NULL, TRUE);
}

void ChangeGridColor(int delta) {
    int step = delta / 120;
    int r = GetRValue(gridColor) + step * 10;
    int g = GetGValue(gridColor) + step * 5;
    int b = GetBValue(gridColor) + step * 2;

    r = max(0, min(255, r));
    g = max(0, min(255, g));
    b = max(0, min(255, b));

    gridColor = RGB(r, g, b);
}