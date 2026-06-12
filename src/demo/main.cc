#include <iostream>
#include <client/windows/handler/exception_handler.h>

bool Callback(const wchar_t* dump_path,
    const wchar_t* minidump_id,
    void* context,
    EXCEPTION_POINTERS* exinfo,
    MDRawAssertionInfo* assertion,
    bool succeeded) {
    std::wcerr << L"Crash dump written to " << dump_path << L"\\" << minidump_id << std::endl;
    return succeeded;
}

int main() {
    google_breakpad::ExceptionHandler eh(
        L".\\dumps",
        NULL,
        Callback,
        NULL,
        -1);
    int* p = nullptr;
    *p = 42;
    return 0;
}
