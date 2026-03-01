#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <stdlib.h>
#include <time.h>
#include <shellapi.h>
#include <tchar.h>
#include <strsafe.h>

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

// Прототипы функций
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
void DrawGrid(HDC hdc, int width, int height);
void DrawCells(HDC hdc, int width, int height);
void LoadConfig();
void SaveConfig();
void ChangeBgColor(HWND hwnd);
void ChangeGridColor(int delta);

// Парсинг командной строки для получения параметра N
void ParseCmdLine() {
    LPWSTR cmdLine = GetCommandLineW();
    if (!cmdLine) return;

    LPWSTR args = cmdLine;  //  Указатель на начало строки

    // Пропускаем имя исполняемого файла
    if (*args == L'"') {
        // Имя в кавычках: ищем вторую кавычку
        args = wcschr(args + 1, L'"');
        if (args) args++;  // Если нашли, двигаем указатель за кавычку
        else return;       // Если кавычек нет, считаем, что аргументов нет
    }
    else {
        // Имя без кавычек: ищем первый пробел
        args = wcschr(args, L' ');
    }

    // Если пробела нет, значит аргументов нет
    if (!args) {
        return;
    }

    // Пропускаем пробелы после имени
    while (*args == L' ') args++;

    // Если строка закончилась — параметров нет
    if (*args == L'\0') {
        return;
    }

    // Пытаемся преобразовать аргумент в число
    wchar_t* end;
    long val = wcstol(args, &end, 10);

    if (end == args) {
        MessageBoxW(NULL,
            L"Ошибка: параметр не является целым числом.\n"
            L"Ожидается одно целое число от 1 до 20 включительно.\nЗначение размера поля установленно значением по умолчанию равным 4.",
            L"Некорректный аргумент", MB_ICONWARNING | MB_OK);
        N = DEFAULT_N;
        return;
    }

    while (*end == L' ') end++;
    if (*end != L'\0') {
        MessageBoxW(NULL,
            L"Ошибка: обнаружены лишние символы после числа.\n"
            L"Должен быть только один параметр (целое число от 1 до 20 включительно).\nЗначение размера поля установленно значением по умолчанию равным 4.",
            L"Некорректный аргумент", MB_ICONWARNING | MB_OK);
        N = DEFAULT_N;
        return;
    }

    if (val < 1) {
        MessageBoxW(NULL,
            L"Размер поля не может быть меньше 1.\n"
            L"Число должно быть целым от 1 до 20 включительно.\nЗначение размера поля установленно значением по умолчанию равным 4.",
            L"Недопустимое значение", MB_ICONWARNING | MB_OK);
        N = DEFAULT_N;
        return;
    }

    if (val > 20) {
        MessageBoxW(NULL,
            L"Размер поля не может быть больше 20.\n"
            L"Число должно быть целым от 1 до 20 включительно.\nЗначение размера поля установленно значением по умолчанию равным 4.",
            L"Недопустимое значение", MB_ICONWARNING | MB_OK);
        N = DEFAULT_N;
        return;
    }

    N = (int)val;
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

// Функция для загрузки конфигурации из файла
void LoadConfig() {
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
            else {
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

// Функция для сохранения конфигурации в файл
void SaveConfig() {
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