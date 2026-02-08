#include "common.h"
#include <vector>
#include <string>

void showLoadingScreen();
void showMainMenu();

uint8_t showDialogPrompt(const wchar_t* message, const wchar_t* button1, const wchar_t* button2, const wchar_t* button3 = nullptr, const wchar_t* button4 = nullptr, uint8_t defaultOption = 0);
uint8_t showDialogPrompt(const wchar_t* message, const wchar_t* button1, const wchar_t* button2, const wchar_t* button3, const wchar_t* button4, const wchar_t* button5, const wchar_t* button6, uint8_t defaultOption = 0);
uint8_t showDialogPrompt(const wchar_t* message, const std::vector<std::wstring>& buttons, uint8_t defaultOption = 0);
void showDialogPrompt(const wchar_t* message, const wchar_t* button);
bool showOptionMenu(dumpingConfig& config, bool showAccountOption);
void setErrorPrompt(const wchar_t* message);
void setErrorPrompt(std::wstring message);
void showErrorPrompt(const wchar_t* button);