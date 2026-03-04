#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <stdlib.h>
#include <time.h>
#include <shellapi.h>
#include <tchar.h>
#include <strsafe.h>
#include <stdio.h>
#include <fstream>
#include <iostream>
#include <profileapi.h>

// Переменные по умолчанию
#define DEFAULT_N           4
#define DEFAULT_WIDTH       320
#define DEFAULT_HEIGHT      240
#define DEFAULT_BG_COLOR    RGB(0, 0, 255)
#define DEFAULT_GRID_COLOR  RGB(255, 0, 0)
#define CIRCLE_COLOR        RGB(0, 255, 0)
#define CROSS_COLOR         RGB(255, 255, 0)

// Константа для имени файла конфигурации
const TCHAR* CONFIG_FILE = _T("config.txt");

const TCHAR* TEST_FILE = _T("test_1mb.bin");
const DWORD TEST_SIZE = 1024 * 1024;

// Структуры 
enum CellType { EMPTY, CIRCLE, CROSS };
struct Cell {
    CellType type = EMPTY;
};

// Глобальные переменные
int N = DEFAULT_N;
int winWidth = DEFAULT_WIDTH;
int winHeight = DEFAULT_HEIGHT;
COLORREF bgColor = DEFAULT_BG_COLOR;
COLORREF gridColor = DEFAULT_GRID_COLOR;
Cell* cells = nullptr;
HBRUSH hBgBrush = nullptr;
int io_method = 4; // По умолчанию WinAPI
bool n_from_cmdline = false; // Флаг, указывающий, был ли N задан через командную строку
bool test_mode = false;

// Прототипы функций
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
void DrawGrid(HDC hdc, int width, int height);
void DrawCells(HDC hdc, int width, int height);
void LoadConfig();
void SaveConfig();
void ChangeBgColor(HWND hwnd);
void ChangeGridColor(int delta);

void LoadConfig_MMap();
void LoadConfig_Stdio();
void LoadConfig_FStream();
void LoadConfig_WinAPI();

void SaveConfig_MMap();
void SaveConfig_Stdio();
void SaveConfig_FStream();
void SaveConfig_WinAPI();

void CheckFileString(wchar_t* line, wchar_t* context);

bool ReadFile_MMap(const TCHAR* filename, void** buffer, DWORD* out_size);
bool ReadFile_Stdio(const TCHAR* filename, void** buffer, DWORD* out_size);
bool ReadFile_FStream(const TCHAR* filename, void** buffer, DWORD* out_size);
bool ReadFile_WinAPI(const TCHAR* filename, void** buffer, DWORD* out_size);
bool CreateTestFile(const TCHAR* filename);
void PerformTest();
void ShowHelp();

// Парсинг командной строки для получения параметра N
void ParseCmdLine() {
    int argc;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) {
        MessageBoxW(NULL, L"Ошибка парсинга командной строки.", L"Ошибка", MB_ICONERROR);
        return;
    }

    bool has_n_parameter = false;
    bool has_io_parameter = false;

    for (int i = 1; i < argc; ++i) {
        wchar_t* arg = argv[i];

        // Пропускаем пустые аргументы
        if (arg == NULL || *arg == L'\0') continue;

        // Обработка параметра N (числовой аргумент)
        if (iswdigit(arg[0]) || (arg[0] == L'-' && iswdigit(arg[1]))) {
            if (has_n_parameter) {
                MessageBoxW(NULL,
                    L"Предупреждение: параметр N указан несколько раз.\nБудет использовано последнее значение.",
                    L"Предупреждение", MB_ICONWARNING | MB_OK);
            }

            wchar_t* end;
            long val = wcstol(arg, &end, 10);

            // Проверка на корректное число
            if (end == arg || *end != L'\0') {
                MessageBoxW(NULL,
                    L"Ошибка: параметр не является целым числом.\n"
                    L"Ожидается целое число от 1 до 20 включительно.",
                    L"Некорректный аргумент", MB_ICONWARNING | MB_OK);
                continue;
            }

            // Проверка диапазона
            if (val < 1 || val > 20) {
                wchar_t msg[256];
                StringCchPrintf(msg, ARRAYSIZE(msg),
                    L"Значение %ld выходит за допустимый диапазон (1-20).\n"
                    L"Будет использовано значение по умолчанию (%d).",
                    val, DEFAULT_N);
                MessageBoxW(NULL, msg, L"Недопустимое значение", MB_ICONWARNING | MB_OK);
                continue;
            }

            N = (int)val;
            n_from_cmdline = true;
            has_n_parameter = true;
        }
        // Обработка параметра --io=
        else if (wcsncmp(arg, L"--io=", 5) == 0) {
            if (has_io_parameter) {
                MessageBoxW(NULL,
                    L"Предупреждение: параметр --io указан несколько раз.\nБудет использовано последнее значение.",
                    L"Предупреждение", MB_ICONWARNING | MB_OK);
            }

            wchar_t* mstr = arg + 5;

            // Проверка, что после = есть значение
            if (*mstr == L'\0') {
                MessageBoxW(NULL,
                    L"Ошибка: не указано значение для --io.\n"
                    L"Формат: --io=<номер метода (1-4)>",
                    L"Некорректный аргумент", MB_ICONWARNING | MB_OK);
                continue;
            }

            wchar_t* end;
            long m = wcstol(mstr, &end, 10);

            // Проверка на корректное число
            if (end == mstr || *end != L'\0') {
                MessageBoxW(NULL,
                    L"Ошибка: значение --io должно быть целым числом.\n"
                    L"Допустимые значения: 1, 2, 3, 4",
                    L"Некорректный аргумент", MB_ICONWARNING | MB_OK);
                continue;
            }

            // Проверка диапазона
            if (m < 1 || m > 4) {
                wchar_t msg[256];
                StringCchPrintf(msg, ARRAYSIZE(msg),
                    L"Значение %ld выходит за допустимый диапазон (1-4).\n"
                    L"Будет использовано значение по умолчанию (%d).",
                    m, io_method);
                MessageBoxW(NULL, msg, L"Недопустимое значение", MB_ICONWARNING | MB_OK);
                continue;
            }

            io_method = (int)m;
            has_io_parameter = true;
        }
        // Обработка флага --test
        else if (wcscmp(arg, L"--test") == 0) {
            test_mode = true;
        }
        // Обработка флага --help
        else if (wcscmp(arg, L"--help") == 0 || wcscmp(arg, L"/?") == 0 || wcscmp(arg, L"-h") == 0) {
            ShowHelp();
        }
        // Неизвестный аргумент
        else {
            wchar_t msg[512];
            StringCchPrintf(msg, ARRAYSIZE(msg),
                L"Неизвестный аргумент: %s\n\n"
                L"Используйте --help для просмотра справки.",
                arg);
            MessageBoxW(NULL, msg, L"Неизвестный аргумент", MB_ICONWARNING | MB_OK);
        }
    }

    LocalFree(argv);
}

// Функция для отображения справки
void ShowHelp() {
    const wchar_t* helpText =
        L"Использование: Lab2.exe [параметры]\n\n"
        L"Параметры:\n"
        L"  <число>           Размер поля N (1-20)\n"
        L"  --io=<номер>      Метод ввода-вывода (1-4)\n"
        L"                    1 - Memory Mapping\n"
        L"                    2 - Stdio (fopen/fread)\n"
        L"                    3 - FStream\n"
        L"                    4 - WinAPI (по умолчанию)\n"
        L"  --test            Запустить тест производительности\n"
        L"  --help, -h, /?    Показать эту справку\n\n"
        L"Примеры:\n"
        L"  LAB.exe 8 --io=2\n"
        L"  LAB.exe --test\n"
        L"  LAB.exe 5 --io=1 --test\n\n"
        L"Управление в программе:\n"
        L"  ЛКМ - поставить кружок\n"
        L"  ПКМ - поставить крестик\n"
        L"  Enter - случайный цвет фона\n"
        L"  Колесо мыши - изменение цвета сетки\n"
        L"  Ctrl+Q - выход\n"
        L"  Shift+C - открыть блокнот";

    MessageBoxW(NULL, helpText, L"Справка", MB_ICONINFORMATION | MB_OK);
}

// Главная функция
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) {
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    srand((unsigned int)time(NULL));

    // Загружаем конфиг
    LoadConfig();

    hBgBrush = CreateSolidBrush(bgColor);

    // Парсим командную строку (параметр N)
    ParseCmdLine();

    if (test_mode) {
        PerformTest();
    }

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
    wc.hbrBackground = hBgBrush;

    hBgBrush = CreateSolidBrush(bgColor);
    wc.hbrBackground = hBgBrush;

    // Проверка результата регистрации окна
    if (!RegisterClassEx(&wc)) {
        MessageBox(NULL,
            TEXT("Ошибка регистрации класса окна!"),
            TEXT("Ошибка"),
            MB_ICONERROR | MB_OK);
        return 0;
    }

    // Создание окна
    HWND hwnd = CreateWindowEx(
        0,
        TEXT("MyWindowClass"),
        TEXT("Лабораторная 2"),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        winWidth, winHeight,
        NULL, NULL, hInstance, NULL);

    if (!hwnd) {
        UnregisterClass(TEXT("MyWindowClass"), hInstance);
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
    UnregisterClass(TEXT("MyWindowClass"), hInstance);

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
        RECT rect;
        GetWindowRect(hwnd, &rect);

        winWidth = rect.right - rect.left;
        winHeight = rect.bottom - rect.top;
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
            Cell& cell = cells[row * N + col];

            if (cell.type == EMPTY) {
                cell.type = CIRCLE;
                InvalidateRect(hwnd, NULL, FALSE);
            }
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
            Cell& cell = cells[row * N + col];

            if (cell.type == EMPTY) {
                cell.type = CROSS;
                InvalidateRect(hwnd, NULL, FALSE);
            }
        }
        return 0;
    }

    case WM_KEYDOWN: {
        if (wParam == VK_ESCAPE) {
            DestroyWindow(hwnd);
        }
        else if (wParam == 'Q' && (GetAsyncKeyState(VK_CONTROL) & 0x8000)) {
            DestroyWindow(hwnd);
        }
        else if (wParam == 'C' && (GetAsyncKeyState(VK_SHIFT) & 0x8000)) {
            HINSTANCE result = ShellExecute(NULL, TEXT("open"), TEXT("notepad.exe"), NULL, NULL, SW_SHOW);

            if ((INT_PTR)result <= 32) {
                TCHAR szError[256];
                if ((INT_PTR)result == SE_ERR_FNF) {
                    _tcscpy_s(szError, TEXT("Файл notepad.exe не найден в системных путях."));
                }
                else if ((INT_PTR)result == SE_ERR_ACCESSDENIED) {
                    _tcscpy_s(szError, TEXT("Доступ к запуску Блокнота заблокирован системой."));
                }
                else {
                    _stprintf_s(szError, TEXT("Произошла ошибка ShellExecute. Код: %d"), (int)(INT_PTR)result);
                }

                MessageBox(hwnd, szError, TEXT("Ошибка запуска"), MB_ICONERROR | MB_OK);
            }

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

// Функция для рисования сетки
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

// Функция для рисования клеток
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

// Выбор метода загрузки
void LoadConfig() {
    switch (io_method) {
    case 1:
        LoadConfig_MMap();
        break;
    case 2:
        LoadConfig_Stdio();
        break;
    case 3:
        LoadConfig_FStream();
        break;
    case 4:
        LoadConfig_WinAPI();
        break;
    default:
        LoadConfig_WinAPI();
        break;
    }
}
// Выбор метода сохранения
void SaveConfig() {
    switch (io_method) {
    case 1:
        SaveConfig_MMap();
        break;
    case 2:
        SaveConfig_Stdio();
        break;
    case 3:
        SaveConfig_FStream();
        break;
    case 4:
        SaveConfig_WinAPI();
        break;
    default:
        SaveConfig_WinAPI();
        break;
    }
}

// Метод 1: при помощи отображения файлов на память
void LoadConfig_MMap() {
    HANDLE hFile = CreateFile(CONFIG_FILE, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return;

    DWORD fileSize = GetFileSize(hFile, NULL);
    if (fileSize == INVALID_FILE_SIZE) {
        CloseHandle(hFile);
        return;
    }

    HANDLE hMapping = CreateFileMapping(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
    if (hMapping == NULL) {
        CloseHandle(hFile);
        return;
    }

    wchar_t* buffer = (wchar_t*)MapViewOfFile(hMapping, FILE_MAP_READ, 0, 0, 0);
    if (buffer == nullptr) {
        CloseHandle(hMapping);
        CloseHandle(hFile);
        return;
    }

    wchar_t* data = buffer;
    size_t len = fileSize / sizeof(wchar_t);
    if (buffer[0] == 0xFEFF) {
        data++;
        len--;
    }

    wchar_t* copy = new wchar_t[len + 1];
    wcsncpy_s(copy, len + 1, data, len);
    copy[len] = L'\0';

    UnmapViewOfFile(buffer);
    CloseHandle(hMapping);
    CloseHandle(hFile);

    wchar_t* context = nullptr;
    wchar_t* line = wcstok(copy, L"\n", &context);
    CheckFileString(line, context);
    delete[] copy;
}

void SaveConfig_MMap() {
    TCHAR buf[256];
    StringCchPrintf(buf, ARRAYSIZE(buf),
        _T("N=%d\nWidth=%d\nHeight=%d\nBgColor=%lu\nGridColor=%lu\n"),
        N, winWidth, winHeight, bgColor, gridColor);

    size_t len = _tcslen(buf);
    DWORD size = (DWORD)((len + 1) * sizeof(TCHAR));

    HANDLE hFile = CreateFile(CONFIG_FILE, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        MessageBoxW(NULL, L"Ошибка создания config.txt", L"Ошибка", MB_ICONERROR);
        return;
    }

    SetFilePointer(hFile, size, NULL, FILE_BEGIN);
    SetEndOfFile(hFile);

    HANDLE hMapping = CreateFileMapping(hFile, NULL, PAGE_READWRITE, 0, size, NULL);
    if (hMapping == NULL) {
        MessageBoxW(NULL, L"Ошибка mapping config.txt", L"Ошибка", MB_ICONERROR);
        CloseHandle(hFile);
        return;
    }

    wchar_t* pBuf = (wchar_t*)MapViewOfFile(hMapping, FILE_MAP_WRITE, 0, 0, 0);
    if (pBuf == nullptr) {
        MessageBoxW(NULL, L"Ошибка view config.txt", L"Ошибка", MB_ICONERROR);
        CloseHandle(hMapping);
        CloseHandle(hFile);
        return;
    }

    pBuf[0] = 0xFEFF;
    memcpy(pBuf + 1, buf, len * sizeof(TCHAR));

    UnmapViewOfFile(pBuf);
    CloseHandle(hMapping);
    CloseHandle(hFile);
}

// Метод 2: при помощи файловых переменных
void LoadConfig_Stdio() {
    FILE* fp = nullptr;
    if (_tfopen_s(&fp, CONFIG_FILE, _T("rb")) != 0) return;

    fseek(fp, 0, SEEK_END);
    long fileSize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    wchar_t* buffer = new wchar_t[fileSize / sizeof(wchar_t) + 1];
    fread(buffer, sizeof(wchar_t), fileSize / sizeof(wchar_t), fp);
    buffer[fileSize / sizeof(wchar_t)] = L'\0';
    fclose(fp);

    wchar_t* data = buffer;
    if (buffer[0] == 0xFEFF) data++;

    wchar_t* copy = _wcsdup(data);
    delete[] buffer;

    wchar_t* context = nullptr;
    wchar_t* line = wcstok(copy, L"\n", &context);
    CheckFileString(line, context);
    free(copy);
}

void SaveConfig_Stdio() {
    FILE* fp = nullptr;
    if (_tfopen_s(&fp, CONFIG_FILE, _T("wb")) != 0) return;

    WORD bom = 0xFEFF;
    fwrite(&bom, sizeof(bom), 1, fp);

    TCHAR buf[256];
    StringCchPrintf(buf, ARRAYSIZE(buf),
        _T("N=%d\nWidth=%d\nHeight=%d\nBgColor=%lu\nGridColor=%lu\n"),
        N, winWidth, winHeight, bgColor, gridColor);

    fwrite(buf, sizeof(TCHAR), _tcslen(buf), fp);
    fclose(fp);
}

// Метод 3: при помощи потоков ввода-вывода
void LoadConfig_FStream() {
    std::basic_ifstream<TCHAR> ifs(CONFIG_FILE, std::ios::binary);
    if (!ifs.is_open()) return;

    ifs.seekg(0, std::ios::end);
    std::streampos fileSize = ifs.tellg();
    ifs.seekg(0, std::ios::beg);

    wchar_t* buffer = new wchar_t[(size_t)fileSize / sizeof(wchar_t) + 1];
    ifs.read(buffer, fileSize / sizeof(wchar_t));
    buffer[fileSize / sizeof(wchar_t)] = L'\0';
    ifs.close();

    wchar_t* data = buffer;
    if (buffer[0] == 0xFEFF) data++;

    wchar_t* copy = _wcsdup(data);
    delete[] buffer;

    wchar_t* context = nullptr;
    wchar_t* line = wcstok(copy, L"\n", &context);    
    CheckFileString(line, context);
    free(copy);
}

void SaveConfig_FStream() {
    std::basic_ofstream<TCHAR> ofs(CONFIG_FILE, std::ios::binary);
    if (!ofs.is_open()) return;

    WORD bom = 0xFEFF;
    ofs.write((TCHAR*)&bom, sizeof(bom) / sizeof(TCHAR));

    TCHAR buf[256];
    StringCchPrintf(buf, ARRAYSIZE(buf),
        _T("N=%d\nWidth=%d\nHeight=%d\nBgColor=%lu\nGridColor=%lu\n"),
        N, winWidth, winHeight, bgColor, gridColor);

    ofs.write(buf, _tcslen(buf));
    ofs.close();
}

// Метод 4: при помощи файловых функций WinAPI
void LoadConfig_WinAPI() {
    if (GetFileAttributes(CONFIG_FILE) == INVALID_FILE_ATTRIBUTES)
        return;
    HANDLE hFile = CreateFile(CONFIG_FILE, GENERIC_READ, FILE_SHARE_READ, NULL,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
        return;
    wchar_t buffer[512];
    DWORD bytesRead = 0;
    if (!ReadFile(hFile, buffer, sizeof(buffer) - sizeof(wchar_t), &bytesRead, NULL)) {
        CloseHandle(hFile);
        MessageBoxW(NULL, L"Ошибка чтения config.txt", L"Ошибка", MB_ICONERROR);
        return;
    }
    buffer[bytesRead / sizeof(wchar_t)] = L'\0';
    CloseHandle(hFile);
    if (buffer[0] == 0xFEFF) {
        memmove(buffer, buffer + 1, (wcslen(buffer)) * sizeof(wchar_t));
    }
    wchar_t* context = nullptr;
    wchar_t* line = wcstok(buffer, L"\n", &context);
    CheckFileString(line, context);
}

void SaveConfig_WinAPI() {
    HANDLE hFile = CreateFile(CONFIG_FILE,
        GENERIC_WRITE, 0, NULL,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return;
    WORD bom = 0xFEFF;
    DWORD written;
    WriteFile(hFile, &bom, sizeof(bom), &written, NULL);
    TCHAR buf[256];
    StringCchPrintf(buf, ARRAYSIZE(buf),
        _T("N=%d\nWidth=%d\nHeight=%d\nBgColor=%lu\nGridColor=%lu\n"),
        N, winWidth, winHeight, bgColor, gridColor);
    WriteFile(hFile, buf, (DWORD)(_tcslen(buf) * sizeof(TCHAR)), &written, NULL);
    CloseHandle(hFile);
}

// Функция для изменения цвета фона
void ChangeBgColor(HWND hwnd) {
    bgColor = RGB(rand() % 256, rand() % 256, rand() % 256);
    while (bgColor == CIRCLE_COLOR || bgColor == CROSS_COLOR || bgColor == gridColor) {
        bgColor = RGB(rand() % 256, rand() % 256, rand() % 256);
    }    
    DeleteObject(hBgBrush);
    hBgBrush = CreateSolidBrush(bgColor);
    SetClassLongPtr(hwnd, GCLP_HBRBACKGROUND, (LONG_PTR)hBgBrush);
    InvalidateRect(hwnd, NULL, TRUE);
}

// Функция для изменения цвета сетки в зависимости от прокрутки колеса мыши
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
// Проверка формата каждой строки и парсинг ключей и значений
void CheckFileString(wchar_t* line, wchar_t* context) {
    while (line) {
        wchar_t* eq = wcschr(line, L'=');
        if (!eq) {
            MessageBoxW(NULL,
                L"Ошибка формата строки в config.txt (отсутствует '=')",
                L"Ошибка формата",
                MB_ICONWARNING);
            line = wcstok(nullptr, L"\n", &context);
            continue;
        }
        *eq = L'\0';
        wchar_t* key = line;
        wchar_t* val = eq + 1;
        if (_wcsicmp(key, L"N") == 0) {
            wchar_t* end;
            long temp = wcstol(val, &end, 10);
            if (*end != L'\0' || temp < 1 || temp > 20) {
                MessageBoxW(NULL,
                    L"Значение N не входит в разрешенный диапазон значений, оно должно быть от 1 до 20.\nУстановлено значение по умолчанию равное 4.",
                    L"Ошибка загрузки параметра N из файла config.txt",
                    MB_ICONWARNING);
                N = DEFAULT_N;
            }
            else if (!n_from_cmdline) {
                N = (int)temp;
            }
        }
        else if (_wcsicmp(key, L"Width") == 0) {
            wchar_t* end;
            long temp = wcstol(val, &end, 10);
            if (*end != L'\0' || temp < 1 || temp > 10000) {
                MessageBoxW(NULL,
                    L"Значение Width не входит в разрешенный диапазон значений, оно должно быть от 1 до 10000.\nУстановлено значение по умолчанию равное 320.",
                    L"Ошибка загрузки параметра Width из файла config.txt",
                    MB_ICONWARNING);
                winWidth = DEFAULT_WIDTH;
            }
            else {
                winWidth = (int)temp;
            }
        }
        else if (_wcsicmp(key, L"Height") == 0) {
            wchar_t* end;
            long temp = wcstol(val, &end, 10);
            if (*end != L'\0' || temp < 1 || temp > 10000) {
                MessageBoxW(NULL,
                    L"Значение Height не входит в разрешенный диапазон значений, оно должно быть от 1 до 10000.\nУстановлено значение по умолчанию равное 240.",
                    L"Ошибка загрузки параметра Height из файла config.txt",
                    MB_ICONWARNING);
                winHeight = DEFAULT_HEIGHT;
            }
            else {
                winHeight = (int)temp;
            }
        }
        else if (_wcsicmp(key, L"BgColor") == 0) {
            wchar_t* end;
            unsigned long temp = wcstoul(val, &end, 10);
            if (*end != L'\0' || temp > 0xFFFFFF) {
                MessageBoxW(NULL,
                    L"Значение BgColor не входит в разрешенный диапазон значений, оно должно быть от 0 до 16777215.\nУстановлен цвет по умолчанию.",
                    L"Ошибка загрузки параметра BgColor из файла config.txt",
                    MB_ICONWARNING);
                bgColor = DEFAULT_BG_COLOR;
            }
            else {
                bgColor = (COLORREF)temp;
            }
        }
        else if (_wcsicmp(key, L"GridColor") == 0) {
            wchar_t* end;
            unsigned long temp = wcstoul(val, &end, 10);
            if (*end != L'\0' || temp > 0xFFFFFF) {
                MessageBoxW(NULL,
                    L"Значение GridColor не входит в разрешенный диапазон значений, оно должно быть от 0 до 16777215.\nУстановлен цвет по умолчанию.",
                    L"Ошибка загрузки параметра GridColor из файла config.txt",
                    MB_ICONWARNING);
                gridColor = DEFAULT_GRID_COLOR;
            }
            else {
                gridColor = (COLORREF)temp;
            }
        }
        line = wcstok(nullptr, L"\n", &context);
    }
}

// Функция для чтения файла методом 1 (MMap)
bool ReadFile_MMap(const TCHAR* filename, void** buffer, DWORD* out_size) {
    *buffer = nullptr;
    *out_size = 0;
    HANDLE hFile = CreateFile(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return false;

    DWORD fileSize = GetFileSize(hFile, NULL);
    if (fileSize == INVALID_FILE_SIZE) {
        CloseHandle(hFile);
        return false;
    }

    HANDLE hMapping = CreateFileMapping(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
    if (hMapping == NULL) {
        CloseHandle(hFile);
        return false;
    }

    void* mapped = MapViewOfFile(hMapping, FILE_MAP_READ, 0, 0, 0);
    if (mapped == NULL) {
        CloseHandle(hMapping);
        CloseHandle(hFile);
        return false;
    }

    // Копируем в новый буфер (чтобы unmap не повлиял)
    *buffer = malloc(fileSize);
    if (*buffer) {
        memcpy(*buffer, mapped, fileSize);
        *out_size = fileSize;
    }

    UnmapViewOfFile(mapped);
    CloseHandle(hMapping);
    CloseHandle(hFile);
    return *buffer != nullptr;
}

// Метод 2: Stdio
bool ReadFile_Stdio(const TCHAR* filename, void** buffer, DWORD* out_size) {
    *buffer = nullptr;
    *out_size = 0;
    FILE* fp = nullptr;
    if (_tfopen_s(&fp, filename, _T("rb")) != 0) return false;

    fseek(fp, 0, SEEK_END);
    long fileSize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    *buffer = malloc(fileSize);
    if (*buffer) {
        size_t read = fread(*buffer, 1, fileSize, fp);
        if (read == fileSize) *out_size = (DWORD)fileSize;
        else {
            free(*buffer);
            *buffer = nullptr;
        }
    }
    fclose(fp);
    return *buffer != nullptr;
}

// Метод 3: FStream
bool ReadFile_FStream(const TCHAR* filename, void** buffer, DWORD* out_size) {
    *buffer = nullptr;
    *out_size = 0;
    std::basic_ifstream<char> ifs(filename, std::ios::binary);
    if (!ifs.is_open()) return false;

    ifs.seekg(0, std::ios::end);
    std::streampos fileSize = ifs.tellg();
    ifs.seekg(0, std::ios::beg);

    *buffer = malloc((size_t)fileSize);
    if (*buffer) {
        ifs.read((char*)*buffer, fileSize);
        if (ifs.gcount() == fileSize) *out_size = (DWORD)fileSize;
        else {
            free(*buffer);
            *buffer = nullptr;
        }
    }
    ifs.close();
    return *buffer != nullptr;
}

// Метод 4: WinAPI
bool ReadFile_WinAPI(const TCHAR* filename, void** buffer, DWORD* out_size) {
    *buffer = nullptr;
    *out_size = 0;
    HANDLE hFile = CreateFile(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return false;

    DWORD fileSize = GetFileSize(hFile, NULL);
    if (fileSize == INVALID_FILE_SIZE) {
        CloseHandle(hFile);
        return false;
    }

    *buffer = malloc(fileSize);
    if (*buffer) {
        DWORD bytesRead;
        if (!ReadFile(hFile, *buffer, fileSize, &bytesRead, NULL) || bytesRead != fileSize) {
            free(*buffer);
            *buffer = nullptr;
        }
        else {
            *out_size = fileSize;
        }
    }
    CloseHandle(hFile);
    return *buffer != nullptr;
}

bool CreateTestFile(const TCHAR* filename) {
    HANDLE hFile = CreateFile(filename, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return false;

    // Заполним случайными байтами
    BYTE* data = new BYTE[TEST_SIZE];
    if (!data) {
        CloseHandle(hFile);
        return false;
    }
    for (DWORD i = 0; i < TEST_SIZE; ++i) {
        data[i] = (BYTE)(rand() % 256);
    }

    DWORD written;
    bool success = WriteFile(hFile, data, TEST_SIZE, &written, NULL) && written == TEST_SIZE;
    delete[] data;
    CloseHandle(hFile);
    return success;
}

void PerformTest() {
    // Сначала пытаемся присоединиться к родительской консоли
    BOOL attached = AttachConsole(ATTACH_PARENT_PROCESS);

    if (!attached) {
        // Если не удалось — создаём новую
        attached = AllocConsole();
    }

    if (attached) {
        // Перенаправляем stdout и stderr в консоль
        freopen("CONOUT$", "w", stdout);
        freopen("CONOUT$", "w", stderr);
        setvbuf(stdout, NULL, _IONBF, 0);
        setvbuf(stderr, NULL, _IONBF, 0);
        SetConsoleOutputCP(1251);
        SetConsoleCP(1251);
    }
    else {
        // Крайне редкий случай — не удалось ни attach, ни alloc
        MessageBoxW(NULL, L"Не удалось создать/присоединиться к консоли", L"Ошибка теста", MB_ICONERROR);
        return;
    }

    if (!CreateTestFile(TEST_FILE)) {
        printf("Ошибка создания тестового файла %S\n", TEST_FILE);
        return;
    }

    const int ITERATIONS = 10;

    typedef bool (*ReadFunc)(const TCHAR*, void**, DWORD*);
    ReadFunc methods[4] = { ReadFile_MMap, ReadFile_Stdio, ReadFile_FStream, ReadFile_WinAPI };
    const char* method_names[4] = { "Memory Mapping (1)", "Stdio (2)", "FStream (3)", "WinAPI (4)" };

    printf("\nТестирование чтения файла 1 МБ (%u байт), %d итераций на метод\n\n", TEST_SIZE, ITERATIONS);

    for (int m = 0; m < 4; ++m) {
        double total_time_ms = 0.0;

        for (int i = 0; i < ITERATIONS; ++i) {
            LARGE_INTEGER start, end, freq;
            QueryPerformanceFrequency(&freq);
            QueryPerformanceCounter(&start);

            void* buffer = nullptr;
            DWORD size = 0;
            bool success = methods[m](TEST_FILE, &buffer, &size);

            if (success) {
                // Имитация использования данных (чтобы компилятор не выкинул чтение)
                volatile BYTE checksum = 0;
                for (DWORD j = 0; j < size; j += 4096) {
                    checksum += ((BYTE*)buffer)[j];
                }
                (void)checksum;  // убираем warning unused

                free(buffer);
            }

            QueryPerformanceCounter(&end);

            double elapsed_ms = (double)(end.QuadPart - start.QuadPart) * 1000.0 / (double)freq.QuadPart;

            printf("Метод %-18s | Итерация %2d | Время: %8.3f мс\n",
                method_names[m], i + 1, elapsed_ms);

            total_time_ms += elapsed_ms;
        }

        double avg_ms = total_time_ms / (double)ITERATIONS;
        printf("Метод %s, Итераций: %d, Среднее время: %.3f ms\n\n", method_names[m], ITERATIONS, avg_ms);
    }

    // Удаляем тестовый файл
    DeleteFile(TEST_FILE);

    printf("Тестирование завершено.");
}