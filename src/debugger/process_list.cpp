#include "process_list.h"
#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>

namespace hype {

std::vector<ProcessEntry> enumerate_processes() {
    std::vector<ProcessEntry> result;

    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return result;

    PROCESSENTRY32 pe{};
    pe.dwSize = sizeof(pe);

    if (Process32First(snap, &pe)) {
        do {
            ProcessEntry entry;
            entry.pid = pe.th32ProcessID;
            entry.ppid = pe.th32ParentProcessID;
            entry.name = pe.szExeFile;
            entry.thread_count = pe.cntThreads;

            HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe.th32ProcessID);
            if (h) {
                char path[MAX_PATH] = {};
                DWORD sz = MAX_PATH;
                if (QueryFullProcessImageNameA(h, 0, path, &sz))
                    entry.path = path;
                CloseHandle(h);
            }

            result.push_back(std::move(entry));
        } while (Process32Next(snap, &pe));
    }

    CloseHandle(snap);
    return result;
}

}
