#include <iostream>
#include <fstream>
#include <string>
#include <thread>
#include <chrono>
#include <random>
#include <vector>
#include <locale>
#include <codecvt>

#include <Windows.h>
#include <shellapi.h>
#include <commdlg.h>
#include <winuser.h>
#include <imm.h>

#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "imm32.lib")

// キーを押して離す関数
void PressAndReleaseKey(WORD vk) {
    INPUT inputPress = {};
    inputPress.type = INPUT_KEYBOARD;
    inputPress.ki.wVk = vk;
    SendInput(1, &inputPress, sizeof(INPUT));

    Sleep(20);

    INPUT inputRelease = {};
    inputRelease.type = INPUT_KEYBOARD;
    inputRelease.ki.wVk = vk;
    inputRelease.ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(1, &inputRelease, sizeof(INPUT));
}

// Unicode文字をキー入力として送信する関数
void TypeUnicodeChar(wchar_t wc) {
    INPUT input = {};
    input.type = INPUT_KEYBOARD;
    input.ki.wScan = wc;
    input.ki.dwFlags = KEYEVENTF_UNICODE;
    SendInput(1, &input, sizeof(INPUT));
}

// 2つのキーを同時に押す関数（Ctrl+Aなど）
void PressCombination(WORD key1, WORD key2) {
    INPUT inputs[4] = {};

    // key1 を押す (Ctrl)
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = key1;

    // key2 を押す (A)
    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = key2;

    // key2 を離す (A)
    inputs[2].type = INPUT_KEYBOARD;
    inputs[2].ki.wVk = key2;
    inputs[2].ki.dwFlags = KEYEVENTF_KEYUP;

    // key1 を離す (Ctrl)
    inputs[3].type = INPUT_KEYBOARD;
    inputs[3].ki.wVk = key1;
    inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;

    SendInput(4, inputs, sizeof(INPUT));
}

// 文字列をキー入力として送信する関数（タイピング速度を再現）
bool TypeString(const std::wstring& str) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> distrib_speed(50, 200);
    std::uniform_int_distribution<> distrib_mistake(50, 150);
    std::uniform_int_distribution<> distrib_backspace(1, 3);
    std::uniform_int_distribution<> distrib_line_pause(2000, 5000);
    std::uniform_int_distribution<> distrib_thought_pause(5000, 10000);

    int charsTyped = 0;
    int mistakeThreshold = distrib_mistake(gen);

    for (size_t i = 0; i < str.length(); ++i) {
        if (GetAsyncKeyState(VK_ESCAPE)) {
            return false;
        }

        // タイプミスをシミュレート
        if (charsTyped >= mistakeThreshold && i < str.length() - 3) {
            Sleep(distrib_thought_pause(gen)); // 悩む時間

            int backspaces = distrib_backspace(gen);

            for (int j = 0; j < backspaces; ++j) {
                PressAndReleaseKey(VK_BACK);
                Sleep(distrib_speed(gen));
            }

            // 削除した文字を再入力
            for (int j = 0; j < backspaces; ++j) {
                wchar_t wc_to_retype = str[i - backspaces + j];
                short vk_retype = VkKeyScanW(wc_to_retype);
                if (vk_retype == -1) {
                    TypeUnicodeChar(wc_to_retype);
                }
                else {
                    WORD vk_code = LOBYTE(vk_retype);
                    WORD shift_state = HIBYTE(vk_retype);
                    if (shift_state & 1) {
                        PressCombination(VK_LSHIFT, vk_code);
                    }
                    else {
                        PressAndReleaseKey(vk_code);
                    }
                }
                Sleep(distrib_speed(gen));
            }

            charsTyped = 0;
            mistakeThreshold = distrib_mistake(gen);
        }

        wchar_t wc = str[i];
        short vk = VkKeyScanW(wc);

        if (wc == L'\n' || wc == L'\r') {
            PressAndReleaseKey(VK_RETURN);
            Sleep(distrib_line_pause(gen)); // 改行後の短い一時停止
        }
        else if (vk == -1) {
            TypeUnicodeChar(wc);
        }
        else {
            WORD virtualKeyCode = LOBYTE(vk);
            WORD shiftState = HIBYTE(vk);

            if (shiftState & 1) {
                PressCombination(VK_LSHIFT, virtualKeyCode);
            }
            else {
                PressAndReleaseKey(virtualKeyCode);
            }
        }

        charsTyped++;
        std::this_thread::sleep_for(std::chrono::milliseconds(distrib_speed(gen)));
    }
    return true;
}

// UTF-8ファイルをワイド文字列に変換して読み込む関数
std::wstring ReadCodeFromFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open file '" << filename << "'" << std::endl;
        return L"";
    }

    std::string utf8_content((std::istreambuf_iterator<char>(file)),
        (std::istreambuf_iterator<char>()));

    try {
        std::string cleaned_content;
        for (size_t i = 0; i < utf8_content.length(); ++i) {
            if (utf8_content[i] == '\r' && i + 1 < utf8_content.length() && utf8_content[i + 1] == '\n') {
                cleaned_content += '\n';
                i++;
            }
            else {
                cleaned_content += utf8_content[i];
            }
        }

        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
        return converter.from_bytes(cleaned_content);
    }
    catch (const std::range_error& e) {
        std::cerr << "Error: Failed to convert UTF-8 to UTF-16. " << e.what() << std::endl;
        return L"";
    }
}

// ファイル選択ダイアログを表示する関数 (Unicode対応)
std::string GetFilePathFromDialog() {
    char szFile[MAX_PATH] = { 0 };

    OPENFILENAMEA ofn = { 0 };
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = "C++ Source Files (*.cpp)\0*.cpp\0Text Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    if (GetOpenFileNameA(&ofn) == TRUE) {
        return szFile;
    }
    return "";
}

int main() {
    std::wstring code;
    std::string filename;

    while (true) {
        std::cout << "ファイルを選択してください。" << std::endl;
        filename = GetFilePathFromDialog();

        if (filename.empty()) {
            std::cout << "ファイルが選択されませんでした。プログラムを終了します。" << std::endl;
            return 0;
        }

        code = ReadCodeFromFile(filename);
        if (!code.empty()) {
            break;
        }
    }

    std::cout << "3秒後にメモ帳を起動し、タイピングを開始します..." << std::endl;
    Sleep(3000);

    ShellExecuteW(NULL, L"open", L"notepad.exe", NULL, NULL, SW_SHOWNORMAL);
    Sleep(1000);

    HWND notepadHwnd = FindWindowW(NULL, L"タイトルなし - メモ帳");
    if (notepadHwnd == NULL) {
        std::cerr << "Error: メモ帳のウィンドウが見つかりませんでした。" << std::endl;
        return 1;
    }

    SetForegroundWindow(notepadHwnd);
    Sleep(500);
    HIMC hImc = ImmAssociateContext(notepadHwnd, NULL);

    std::cout << "タイピング中です。Escキーを押すと終了します。" << std::endl;

    while (true) {
        std::cout << "タイピングを開始します。" << std::endl;

        PressCombination(VK_CONTROL, 0x41);
        PressAndReleaseKey(VK_DELETE);
        Sleep(500);

        if (!TypeString(code)) {
            break;
        }

        std::cout << "タイピング完了。3秒後に再開します..." << std::endl;
        Sleep(3000);
    }

    std::cout << "プログラムを終了します。" << std::endl;

    ImmAssociateContext(notepadHwnd, hImc);

    return 0;
}