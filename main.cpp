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

// �L�[�������ė����֐�
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

// Unicode�������L�[���͂Ƃ��đ��M����֐�
void TypeUnicodeChar(wchar_t wc) {
    INPUT input = {};
    input.type = INPUT_KEYBOARD;
    input.ki.wScan = wc;
    input.ki.dwFlags = KEYEVENTF_UNICODE;
    SendInput(1, &input, sizeof(INPUT));
}

// 2�̃L�[�𓯎��ɉ����֐��iCtrl+A�Ȃǁj
void PressCombination(WORD key1, WORD key2) {
    INPUT inputs[4] = {};

    // key1 ������ (Ctrl)
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = key1;

    // key2 ������ (A)
    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = key2;

    // key2 �𗣂� (A)
    inputs[2].type = INPUT_KEYBOARD;
    inputs[2].ki.wVk = key2;
    inputs[2].ki.dwFlags = KEYEVENTF_KEYUP;

    // key1 �𗣂� (Ctrl)
    inputs[3].type = INPUT_KEYBOARD;
    inputs[3].ki.wVk = key1;
    inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;

    SendInput(4, inputs, sizeof(INPUT));
}

// ��������L�[���͂Ƃ��đ��M����֐��i�^�C�s���O���x���Č��j
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

        // �^�C�v�~�X���V�~�����[�g
        if (charsTyped >= mistakeThreshold && i < str.length() - 3) {
            Sleep(distrib_thought_pause(gen)); // �Y�ގ���

            int backspaces = distrib_backspace(gen);

            for (int j = 0; j < backspaces; ++j) {
                PressAndReleaseKey(VK_BACK);
                Sleep(distrib_speed(gen));
            }

            // �폜�����������ē���
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
            Sleep(distrib_line_pause(gen)); // ���s��̒Z���ꎞ��~
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

// UTF-8�t�@�C�������C�h������ɕϊ����ēǂݍ��ފ֐�
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

// �t�@�C���I���_�C�A���O��\������֐� (Unicode�Ή�)
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
        std::cout << "�t�@�C����I�����Ă��������B" << std::endl;
        filename = GetFilePathFromDialog();

        if (filename.empty()) {
            std::cout << "�t�@�C�����I������܂���ł����B�v���O�������I�����܂��B" << std::endl;
            return 0;
        }

        code = ReadCodeFromFile(filename);
        if (!code.empty()) {
            break;
        }
    }

    std::cout << "3�b��Ƀ��������N�����A�^�C�s���O���J�n���܂�..." << std::endl;
    Sleep(3000);

    ShellExecuteW(NULL, L"open", L"notepad.exe", NULL, NULL, SW_SHOWNORMAL);
    Sleep(1000);

    HWND notepadHwnd = FindWindowW(NULL, L"�^�C�g���Ȃ� - ������");
    if (notepadHwnd == NULL) {
        std::cerr << "Error: �������̃E�B���h�E��������܂���ł����B" << std::endl;
        return 1;
    }

    SetForegroundWindow(notepadHwnd);
    Sleep(500);
    HIMC hImc = ImmAssociateContext(notepadHwnd, NULL);

    std::cout << "�^�C�s���O���ł��BEsc�L�[�������ƏI�����܂��B" << std::endl;

    while (true) {
        std::cout << "�^�C�s���O���J�n���܂��B" << std::endl;

        PressCombination(VK_CONTROL, 0x41);
        PressAndReleaseKey(VK_DELETE);
        Sleep(500);

        if (!TypeString(code)) {
            break;
        }

        std::cout << "�^�C�s���O�����B3�b��ɍĊJ���܂�..." << std::endl;
        Sleep(3000);
    }

    std::cout << "�v���O�������I�����܂��B" << std::endl;

    ImmAssociateContext(notepadHwnd, hImc);

    return 0;
}