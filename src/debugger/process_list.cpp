#include "process_list.h"

#ifdef _WIN32
#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#elif defined(__linux__)
#include <filesystem>
#include <fstream>
#include <unistd.h>
#elif defined(__APPLE__)
#include <libproc.h>
#include <sys/proc_info.h>
#endif

namespace hype {

std::vector<ProcessEntry> enumerate_processes() {
    std::vector<ProcessEntry> result;

#ifdef _WIN32
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
#elif defined(__linux__)
    for (const auto& entry : std::filesystem::directory_iterator("/proc")) {
        if (!entry.is_directory()) continue;
        std::string dirname = entry.path().filename().string();
        if (!std::all_of(dirname.begin(), dirname.end(), ::isdigit)) continue;

        ProcessEntry pe;
        pe.pid = std::stoull(dirname);

        std::ifstream comm(entry.path() / "comm");
        if (comm >> pe.name) {
            std::error_code ec;
            auto exe_path = std::filesystem::read_symlink(entry.path() / "exe", ec);
            if (!ec) pe.path = exe_path.string();
            
            // Basic stat parsing for ppid
            std::ifstream stat(entry.path() / "stat");
            std::string tmp;
            stat >> tmp >> tmp >> tmp >> pe.ppid;
            
            result.push_back(std::move(pe));
        }
    }
#elif defined(__APPLE__)
    int count = proc_listpids(PROC_ALL_PIDS, 0, nullptr, 0);
    if (count > 0) {
        std::vector<pid_t> pids(count);
        count = proc_listpids(PROC_ALL_PIDS, 0, pids.data(), count * sizeof(pid_t));
        for (int i = 0; i < count; ++i) {
            if (pids[i] == 0) continue;
            struct proc_bsdinfo bsdinfo;
            if (proc_pidinfo(pids[i], PROC_PIDTBSDINFO, 0, &bsdinfo, sizeof(bsdinfo)) > 0) {
                ProcessEntry pe;
                pe.pid = pids[i];
                pe.ppid = bsdinfo.pbi_ppid;
                pe.name = bsdinfo.pbi_comm;
                
                char pathbuf[PROC_PIDPATHINFO_MAXSIZE];
                if (proc_pidpath(pids[i], pathbuf, sizeof(pathbuf)) > 0)
                    pe.path = pathbuf;
                
                result.push_back(std::move(pe));
            }
        }
    }
#endif
    
    return result;
}

}
