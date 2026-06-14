#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#pragma execution_character_set("utf-8")

#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <shellapi.h>
#include <windowsx.h>
#include <shlwapi.h>
#include <objbase.h>
#include <dwmapi.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <set>
#include <map>
#include <algorithm>
#include <chrono>
#include <thread>
#include <cmath>
#include <iterator>
#include <cstdint>
#include <cwctype>
#include "MiausoftVisualConfig.h"

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "dwmapi.lib")

#ifndef IDI_MIAUSOFT_ICON
#define IDI_MIAUSOFT_ICON 101
#endif

namespace miausoft {
namespace fs = std::filesystem;

inline constexpr double MIAUSOFT_WINDOW_WIDTH_RATIO = miausoft_visual::window_width_ratio;
inline constexpr double MIAUSOFT_WINDOW_HEIGHT_RATIO = miausoft_visual::window_height_ratio;
inline constexpr double MIAUSOFT_ICON_PANEL_RATIO = miausoft_visual::icon_panel_ratio;

inline int clamp_i(int v, int lo, int hi){ return v < lo ? lo : (v > hi ? hi : v); }
inline int max_i(int a, int b){ return a > b ? a : b; }
inline int min_i(int a, int b){ return a < b ? a : b; }
inline size_t max_z(size_t a, size_t b){ return a > b ? a : b; }
inline size_t min_z(size_t a, size_t b){ return a < b ? a : b; }
inline double max_d(double a, double b){ return a > b ? a : b; }
inline std::wstring trim(std::wstring s){
    auto issp=[](wchar_t c){ return c==L' '||c==L'\t'||c==L'\r'||c==L'\n'; };
    while(!s.empty() && issp(s.front())) s.erase(s.begin());
    while(!s.empty() && issp(s.back())) s.pop_back();
    return s;
}
inline std::wstring to_lower(std::wstring s){ std::transform(s.begin(), s.end(), s.begin(), [](wchar_t c){ return (wchar_t)towlower(c); }); return s; }
inline bool ends_with_i(const std::wstring& s, const std::wstring& suf){ if(s.size()<suf.size()) return false; return to_lower(s.substr(s.size()-suf.size())) == to_lower(suf); }

inline std::wstring utf8_to_wide(const std::string& s){
    if(s.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(), (int)s.size(), nullptr, 0);
    if(n <= 0) n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring out((size_t)max_i(0, n), L'\0');
    if(n>0) MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), out.data(), n);
    return out;
}
inline std::string wide_to_utf8(const std::wstring& s){
    if(s.empty()) return std::string();
    int n = WideCharToMultiByte(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0, nullptr, nullptr);
    std::string out((size_t)max_i(0, n), '\0');
    if(n>0) WideCharToMultiByte(CP_UTF8, 0, s.data(), (int)s.size(), out.data(), n, nullptr, nullptr);
    return out;
}
inline std::wstring ansi_to_wide(const std::string& s, UINT cp=1252){
    if(s.empty()) return L"";
    int n = MultiByteToWideChar(cp, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring out((size_t)max_i(0, n), L'\0');
    if(n>0) MultiByteToWideChar(cp, 0, s.data(), (int)s.size(), out.data(), n);
    return out;
}
inline std::string wide_to_ansi(const std::wstring& s, UINT cp=1252){
    if(s.empty()) return std::string();
    int n = WideCharToMultiByte(cp, 0, s.data(), (int)s.size(), nullptr, 0, nullptr, nullptr);
    std::string out((size_t)max_i(0, n), '\0');
    if(n>0) WideCharToMultiByte(cp, 0, s.data(), (int)s.size(), out.data(), n, nullptr, nullptr);
    return out;
}

inline std::vector<std::wstring> command_line_args(){
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    std::vector<std::wstring> out;
    if(argv){ for(int i=1;i<argc;i++) out.emplace_back(argv[i]); LocalFree(argv); }
    return out;
}
inline fs::path module_path(){ wchar_t buf[32768]{}; GetModuleFileNameW(nullptr, buf, (DWORD)std::size(buf)); return fs::path(buf); }
inline fs::path module_dir(){ std::error_code ec; return fs::absolute(module_path(), ec).parent_path(); }
inline std::wstring getenv_w(const wchar_t* name){ DWORD n=GetEnvironmentVariableW(name,nullptr,0); if(!n) return L""; std::wstring s(n,L'\0'); GetEnvironmentVariableW(name,s.data(),n); if(!s.empty() && s.back()==L'\0') s.pop_back(); return s; }
inline fs::path local_appdata(){ auto s=getenv_w(L"LOCALAPPDATA"); if(!s.empty()) return fs::path(s); s=getenv_w(L"APPDATA"); if(!s.empty()) return fs::path(s); return fs::temp_directory_path(); }
inline fs::path log_dir(){ fs::path p=local_appdata()/L"MiausoftSuite"/L"Logs"; std::error_code ec; fs::create_directories(p,ec); return p; }
inline void log_text(const std::wstring& file, const std::wstring& text){ try{ std::ofstream f(log_dir()/file, std::ios::binary|std::ios::app); auto u=wide_to_utf8(text); f.write(u.data(), (std::streamsize)u.size()); f.write("\r\n",2);}catch(...){} }
inline std::wstring last_error_text(DWORD e=GetLastError()){
    LPWSTR msg=nullptr; FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS,nullptr,e,0,(LPWSTR)&msg,0,nullptr);
    std::wstring s=msg?msg:L""; if(msg) LocalFree(msg); return trim(s);
}
inline std::wstring quote_arg(const std::wstring& s){
    if(s.empty()) return L"\"\"";
    bool need=false; for(wchar_t c: s) if(c==L' '||c==L'\t'||c==L'\"'||c==L'\n'||c==L'\r') need=true;
    if(!need) return s;
    std::wstring out=L"\""; size_t bs=0;
    for(wchar_t c: s){ if(c==L'\\'){ bs++; continue; } if(c==L'\"'){ out.append(bs*2+1,L'\\'); out.push_back(c); bs=0; continue; } out.append(bs,L'\\'); bs=0; out.push_back(c); }
    out.append(bs*2,L'\\'); out.push_back(L'\"'); return out;
}
inline bool file_exists(const fs::path& p){ std::error_code ec; return fs::exists(p,ec) && fs::is_regular_file(p,ec); }
inline bool dir_exists(const fs::path& p){ std::error_code ec; return fs::exists(p,ec) && fs::is_directory(p,ec); }
inline std::uintmax_t file_size_safe(const fs::path& p){ std::error_code ec; auto n=fs::file_size(p,ec); return ec?0:n; }

inline fs::path find_exe(const std::wstring& name){
    auto lower = [](std::wstring s){ for(auto& ch:s) ch=(wchar_t)towlower(ch); return s; };
    auto add_unique = [](std::vector<fs::path>& v, const fs::path& p){
        if(p.empty()) return;
        std::wstring key = p.wstring(); for(auto& ch:key) ch=(wchar_t)towlower(ch);
        for(const auto& e:v){ std::wstring k=e.wstring(); for(auto& ch:k) ch=(wchar_t)towlower(ch); if(k==key) return; }
        v.push_back(p);
    };
    auto add_dir = [&](std::vector<fs::path>& dirs, const fs::path& d){ if(!d.empty() && dir_exists(d)) add_unique(dirs, d); };

    std::vector<fs::path> direct;
    std::vector<fs::path> scan_roots;
    fs::path d = module_dir();
    add_unique(direct, d / name);
    add_unique(direct, d.parent_path() / L"common" / name);
    add_unique(direct, d.parent_path() / L"tools" / name);

    auto envp = [&](const wchar_t* n)->fs::path{ auto v=getenv_w(n); return v.empty()?fs::path():fs::path(v); };
    fs::path programFiles = envp(L"ProgramFiles");
    fs::path programFilesX86 = envp(L"ProgramFiles(x86)");
    fs::path programData = envp(L"ProgramData"); if(programData.empty()) programData = L"C:\\ProgramData";
    fs::path local = local_appdata();
    fs::path user = envp(L"USERPROFILE");
    fs::path windir = envp(L"WINDIR"); if(windir.empty()) windir = L"C:\\Windows";
    fs::path choco = envp(L"ChocolateyInstall"); if(choco.empty()) choco = programData / L"chocolatey";
    fs::path miausoftDeps = programData / L"Miausoft" / L"deps";

    // Rutas explícitas de instalación global: la suite ya no requiere external.
    // La reparación Poppler instala pdfinfo.exe/pdftotext.exe en:
    // C:\ProgramData\Miausoft\deps\poppler\<release>\Library\bin
    // Se busca primero por rutas directas frecuentes y después recursivamente.
    add_unique(direct, miausoftDeps / L"poppler" / L"Library" / L"bin" / name);
    add_unique(direct, miausoftDeps / L"poppler" / name);
    add_unique(direct, choco / L"bin" / name);
    add_unique(direct, windir / name);
    add_unique(direct, windir / L"System32" / name);
    if(!programFiles.empty()){
        add_unique(direct, programFiles / L"Tesseract-OCR" / name);
        add_unique(direct, programFiles / L"qpdf" / name);
        add_unique(direct, programFiles / L"qpdf" / L"bin" / name);
        add_unique(direct, programFiles / L"poppler" / L"Library" / L"bin" / name);
        add_unique(direct, programFiles / L"MuPDF" / name);
        add_unique(direct, programFiles / L"Python312" / name);
        add_unique(direct, programFiles / L"Python312" / L"Scripts" / name);
        add_unique(direct, programFiles / L"Python313" / name);
        add_unique(direct, programFiles / L"Python313" / L"Scripts" / name);
        add_unique(direct, programFiles / L"Python314" / name);
        add_unique(direct, programFiles / L"Python314" / L"Scripts" / name);
    }
    if(!programFilesX86.empty()){
        add_unique(direct, programFilesX86 / L"Tesseract-OCR" / name);
        add_unique(direct, programFilesX86 / L"qpdf" / L"bin" / name);
    }
    if(!local.empty()){
        add_unique(direct, local / L"Programs" / L"Python" / L"Python312" / name);
        add_unique(direct, local / L"Programs" / L"Python" / L"Python312" / L"Scripts" / name);
        add_unique(direct, local / L"Programs" / L"Python" / L"Python313" / name);
        add_unique(direct, local / L"Programs" / L"Python" / L"Python313" / L"Scripts" / name);
        add_unique(direct, local / L"Programs" / L"Python" / L"Python314" / name);
        add_unique(direct, local / L"Programs" / L"Python" / L"Python314" / L"Scripts" / name);
    }

    for(auto& p: direct) if(file_exists(p)) return p;

    add_dir(scan_roots, miausoftDeps);
    add_dir(scan_roots, miausoftDeps / L"poppler");
    add_dir(scan_roots, choco / L"bin");
    add_dir(scan_roots, choco / L"lib");
    if(!programFiles.empty()){
        add_dir(scan_roots, programFiles / L"gs");
        add_dir(scan_roots, programFiles / L"Tesseract-OCR");
        add_dir(scan_roots, programFiles / L"Python312");
        add_dir(scan_roots, programFiles / L"Python313");
        add_dir(scan_roots, programFiles / L"Python314");
        add_dir(scan_roots, programFiles / L"poppler");
        add_dir(scan_roots, programFiles / L"qpdf");
        add_dir(scan_roots, programFiles / L"MuPDF");
    }
    if(!programFilesX86.empty()){
        add_dir(scan_roots, programFilesX86 / L"Tesseract-OCR");
        add_dir(scan_roots, programFilesX86 / L"gs");
    }
    if(!local.empty()) add_dir(scan_roots, local / L"Programs" / L"Python");
    if(!user.empty()) add_dir(scan_roots, user / L"AppData" / L"Local" / L"Programs" / L"Python");

    const std::wstring wanted = lower(name);
    for(const auto& root: scan_roots){
        std::error_code ec;
        try{
            fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end;
            for(; !ec && it != end; it.increment(ec)){
                if(ec) break;
                const fs::path p = it->path();
                if(!it->is_regular_file(ec) || ec) continue;
                std::wstring fn = lower(p.filename().wstring());
                if(fn == wanted) return p;
            }
        }catch(...){ }
    }

    wchar_t buf[32768]{};
    if(SearchPathW(nullptr, name.c_str(), nullptr, (DWORD)std::size(buf), buf, nullptr)) return fs::path(buf);
    return {};
}


inline bool extract_rcdata_resource_to_file(int resource_id, const fs::path& out_path, const wchar_t* resource_type = RT_RCDATA){
    HMODULE module = GetModuleHandleW(nullptr);
    if(!module) return false;
    HRSRC rc = FindResourceW(module, MAKEINTRESOURCEW(resource_id), resource_type);
    if(!rc) return false;
    DWORD size = SizeofResource(module, rc);
    if(size == 0) return false;
    HGLOBAL loaded = LoadResource(module, rc);
    if(!loaded) return false;
    void* data = LockResource(loaded);
    if(!data) return false;
    std::error_code ec;
    fs::create_directories(out_path.parent_path(), ec);
    std::ofstream f(out_path, std::ios::binary | std::ios::trunc);
    if(!f) return false;
    f.write(static_cast<const char*>(data), static_cast<std::streamsize>(size));
    f.close();
    if(!f) return false;
    SetFileAttributesW(out_path.c_str(), FILE_ATTRIBUTE_TEMPORARY | FILE_ATTRIBUTE_NOT_CONTENT_INDEXED);
    return file_exists(out_path) && file_size_safe(out_path) == static_cast<std::uintmax_t>(size);
}

inline DWORD resource_size_rcdata(int resource_id, const wchar_t* resource_type = RT_RCDATA){
    HMODULE module = GetModuleHandleW(nullptr);
    if(!module) return 0;
    HRSRC rc = FindResourceW(module, MAKEINTRESOURCEW(resource_id), resource_type);
    if(!rc) return 0;
    return SizeofResource(module, rc);
}

inline void pump_messages(){ MSG m{}; while(PeekMessageW(&m,nullptr,0,0,PM_REMOVE)){ TranslateMessage(&m); DispatchMessageW(&m); } }

struct ProcessResult { DWORD exit_code = 9999; bool started = false; bool timed_out = false; std::wstring error; };
inline ProcessResult run_process(const std::wstring& command_line, const fs::path& cwd = {}, HWND progress_hwnd = nullptr, DWORD timeout_ms = 0){
    ProcessResult r; STARTUPINFOW si{}; PROCESS_INFORMATION pi{}; si.cb=sizeof(si); si.dwFlags=STARTF_USESHOWWINDOW; si.wShowWindow=SW_HIDE;
    std::wstring cmd = command_line;
    std::wstring cwd_s = cwd.empty()?L"":cwd.wstring();
    BOOL ok = CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, cwd_s.empty()?nullptr:cwd_s.c_str(), &si, &pi);
    if(!ok){ r.error=last_error_text(); return r; }
    r.started=true;
    const ULONGLONG started = GetTickCount64();
    for(;;){
        DWORD w = MsgWaitForMultipleObjects(1, &pi.hProcess, FALSE, 80, QS_ALLINPUT);
        if(w == WAIT_OBJECT_0) break;
        if(w == WAIT_OBJECT_0 + 1) pump_messages();
        if(timeout_ms > 0 && GetTickCount64() - started >= timeout_ms){
            r.timed_out = true;
            r.error = L"timeout";
            TerminateProcess(pi.hProcess, 1460);
            WaitForSingleObject(pi.hProcess, 2000);
            break;
        }
        if(progress_hwnd && !IsWindow(progress_hwnd)){ TerminateProcess(pi.hProcess, 125); WaitForSingleObject(pi.hProcess, 2000); break; }
    }
    DWORD ec=0; GetExitCodeProcess(pi.hProcess,&ec); r.exit_code=ec;
    CloseHandle(pi.hThread); CloseHandle(pi.hProcess); return r;
}

inline std::string read_bytes(const fs::path& p){ std::ifstream f(p, std::ios::binary); std::ostringstream ss; ss << f.rdbuf(); return ss.str(); }
inline bool write_bytes(const fs::path& p, const std::string& b){ std::error_code ec; fs::create_directories(p.parent_path(), ec); std::ofstream f(p,std::ios::binary); if(!f) return false; f.write(b.data(), (std::streamsize)b.size()); return true; }
inline bool write_utf8(const fs::path& p, const std::wstring& s, bool bom=false){ std::string b=wide_to_utf8(s); if(bom) b="\xEF\xBB\xBF"+b; return write_bytes(p,b); }
inline std::wstring read_text_auto(const fs::path& p, std::wstring* encoding=nullptr){
    std::string b = read_bytes(p);
    if(b.size()>=3 && (unsigned char)b[0]==0xEF && (unsigned char)b[1]==0xBB && (unsigned char)b[2]==0xBF){ if(encoding)*encoding=L"utf-8-sig"; return utf8_to_wide(b.substr(3)); }
    if(b.size()>=2 && (unsigned char)b[0]==0xFF && (unsigned char)b[1]==0xFE){ if(encoding)*encoding=L"utf-16le"; std::wstring s; for(size_t i=2;i+1<b.size();i+=2) s.push_back((wchar_t)((unsigned char)b[i] | ((unsigned char)b[i+1]<<8))); return s; }
    if(b.size()>=2 && (unsigned char)b[0]==0xFE && (unsigned char)b[1]==0xFF){ if(encoding)*encoding=L"utf-16be"; std::wstring s; for(size_t i=2;i+1<b.size();i+=2) s.push_back((wchar_t)(((unsigned char)b[i]<<8) | (unsigned char)b[i+1])); return s; }
    int n = b.empty()?0:MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, b.data(), (int)b.size(), nullptr, 0);
    if(n>0){ if(encoding)*encoding=L"utf-8"; return utf8_to_wide(b); }
    if(encoding)*encoding=L"cp1252"; return ansi_to_wide(b,1252);
}
inline bool write_text_encoded(const fs::path& p, const std::wstring& s, const std::wstring& enc){
    if(enc==L"utf-16le"){ std::string b="\xFF\xFE"; for(wchar_t ch:s){ b.push_back((char)(ch&0xff)); b.push_back((char)((ch>>8)&0xff)); } return write_bytes(p,b); }
    if(enc==L"utf-16be"){ std::string b="\xFE\xFF"; for(wchar_t ch:s){ b.push_back((char)((ch>>8)&0xff)); b.push_back((char)(ch&0xff)); } return write_bytes(p,b); }
    if(enc==L"cp1252") return write_bytes(p, wide_to_ansi(s,1252));
    return write_utf8(p,s, enc==L"utf-8-sig");
}
inline std::wstring normalize_newlines(std::wstring s){
    std::wstring out; out.reserve(s.size());
    for(size_t i=0;i<s.size();++i){ if(s[i]==L'\r'){ if(i+1<s.size()&&s[i+1]==L'\n') ++i; out.push_back(L'\n'); } else out.push_back(s[i]); }
    return out;
}
inline std::wstring newline_style_from_bytes(const std::string& b){ size_t crlf=0, lf=0, cr=0; for(size_t i=0;i<b.size();++i){ if(b[i]=='\r'){ if(i+1<b.size()&&b[i+1]=='\n'){crlf++;i++;} else cr++; } else if(b[i]=='\n') lf++; } if(crlf >= max_z(size_t(1), lf / 2)) return L"\r\n"; if(cr && !lf) return L"\r"; return L"\n"; }
inline std::wstring apply_newline_style(const std::wstring& s, const std::wstring& nl){ std::wstring n=normalize_newlines(s), out; out.reserve(n.size()+32); for(wchar_t c:n){ if(c==L'\n') out+=nl; else out.push_back(c); } return out; }
inline std::wstring html_decode_basic(std::wstring s){
    auto rep=[&](const wchar_t* a,const wchar_t* b){ size_t p=0; std::wstring A(a); while((p=s.find(A,p))!=std::wstring::npos){ s.replace(p,A.size(),b); p+=wcslen(b);} };
    rep(L"&lt;",L"<"); rep(L"&gt;",L">"); rep(L"&amp;",L"&"); rep(L"&quot;",L"\""); rep(L"&apos;",L"'"); return s;
}
inline std::wstring unique_tmp_name(const std::wstring& stem, const std::wstring& ext){
    auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    return stem + L"_" + std::to_wstring(GetCurrentProcessId()) + L"_" + std::to_wstring((long long)now) + ext;
}

inline std::vector<fs::path> open_file_dialog_multi(HWND owner, const wchar_t* title, const wchar_t* filter){
    std::vector<fs::path> out;
    // Buffer deliberadamente grande: permite cargas masivas por selector nativo sin truncar
    // lotes largos de nombres. 4 Mi WCHARs ~= 8 MiB, seguro para 500 rutas largas.
    std::vector<wchar_t> buf(4u * 1024u * 1024u, L'\0');
    OPENFILENAMEW ofn{};
    ofn.lStructSize=sizeof(ofn);
    ofn.hwndOwner=owner;
    ofn.lpstrTitle=title;
    ofn.lpstrFilter=filter;
    ofn.lpstrFile=buf.data();
    ofn.nMaxFile=(DWORD)buf.size();
    ofn.Flags=OFN_ALLOWMULTISELECT|OFN_EXPLORER|OFN_FILEMUSTEXIST|OFN_HIDEREADONLY;
    if(!GetOpenFileNameW(&ofn)) return out;
    wchar_t* p=buf.data();
    std::wstring first=p;
    p += first.size()+1;
    if(*p==0){ out.emplace_back(first); return out; }
    fs::path dir=first;
    while(*p){ std::wstring n=p; out.push_back(dir/n); p += n.size()+1; }
    return out;
}

inline bool miausoft_is_heavy_document_ext(const std::wstring& ext_in){
    std::wstring e=to_lower(ext_in);
    return e==L".pdf" || e==L".doc" || e==L".docx" || e==L".ppt" || e==L".pptx";
}
inline bool miausoft_is_light_document_ext(const std::wstring& ext_in){
    std::wstring e=to_lower(ext_in);
    return e==L".epub" || e==L".txt" || e==L".md" || e==L".csv" || e==L".tsv";
}
inline bool miausoft_is_text_document_ext(const std::wstring& ext_in, bool include_miau=false){
    std::wstring e=to_lower(ext_in);
    if(e==L".txt" || e==L".md" || e==L".csv" || e==L".tsv") return true;
    return include_miau && e==L".miau";
}
inline bool miausoft_is_processable_ext(const std::wstring& ext_in){
    std::wstring e=to_lower(ext_in);
    return miausoft_is_heavy_document_ext(e) || miausoft_is_light_document_ext(e);
}
inline void miausoft_push_unique(std::vector<fs::path>& out, std::set<std::wstring>& seen, const fs::path& p){
    std::error_code ec;
    fs::path a=fs::absolute(p,ec); if(ec) a=p;
    auto k=to_lower(a.wstring());
    if(!seen.count(k)){ seen.insert(k); out.push_back(a); }
}
inline void miausoft_collect_dir_processable(const fs::path& dir, std::vector<fs::path>& out, std::set<std::wstring>& seen, bool text_only=false, bool include_miau=false){
    std::error_code ec;
    if(!dir_exists(dir)) return;
    size_t scanned=0;
    fs::recursive_directory_iterator it(dir, fs::directory_options::skip_permission_denied, ec), end;
    for(; !ec && it!=end; it.increment(ec)){
        if(scanned++ > 50000) break; // defensa contra árboles enormes o enlaces raros
        std::error_code st_ec;
        if(it->is_directory(st_ec)) continue;
        if(!it->is_regular_file(st_ec)) continue;
        const std::wstring ext=to_lower(it->path().extension().wstring());
        const bool ok = text_only ? miausoft_is_text_document_ext(ext, include_miau) : miausoft_is_processable_ext(ext);
        if(ok) miausoft_push_unique(out, seen, it->path());
    }
}
inline std::vector<fs::path> filter_existing_files(const std::vector<std::wstring>& args){
    std::vector<fs::path> out; std::set<std::wstring> seen;
    for(auto a: args){
        a=trim(a);
        if(a.empty() || (!a.empty()&&a[0]==L'-')) continue;
        fs::path p(a); std::error_code ec; p=fs::absolute(p,ec);
        if(file_exists(p)) miausoft_push_unique(out, seen, p);
        else if(dir_exists(p)) miausoft_collect_dir_processable(p, out, seen, false, false);
    }
    return out;
}
inline std::vector<fs::path> filter_existing_text_files(const std::vector<std::wstring>& args, bool include_miau=false){
    std::vector<fs::path> out; std::set<std::wstring> seen;
    for(auto a: args){
        a=trim(a);
        if(a.empty() || (!a.empty()&&a[0]==L'-')) continue;
        fs::path p(a); std::error_code ec; p=fs::absolute(p,ec);
        if(file_exists(p)){
            if(miausoft_is_text_document_ext(p.extension().wstring(), include_miau)) miausoft_push_unique(out, seen, p);
        } else if(dir_exists(p)) miausoft_collect_dir_processable(p, out, seen, true, include_miau);
    }
    return out;
}
inline std::vector<fs::path> limit_miausoft_file_batch(std::vector<fs::path> files, HWND owner=nullptr, const wchar_t* app_name=L"Miausoft"){
    constexpr size_t kHeavyLimit = 100;  // PDF/DOC/DOCX/PPT/PPTX
    constexpr size_t kLightLimit = 500;  // EPUB/TXT y textos afines
    std::vector<fs::path> out;
    out.reserve(files.size());
    size_t heavy=0, light=0, dropped_heavy=0, dropped_light=0;
    for(const auto& p: files){
        std::wstring ext=to_lower(p.extension().wstring());
        if(miausoft_is_heavy_document_ext(ext)){
            if(heavy < kHeavyLimit){ out.push_back(p); ++heavy; }
            else ++dropped_heavy;
        } else if(miausoft_is_light_document_ext(ext)){
            if(light < kLightLimit){ out.push_back(p); ++light; }
            else ++dropped_light;
        } else {
            out.push_back(p);
        }
    }
    if(dropped_heavy || dropped_light){
        std::wstring msg=L"Se recortó la carga para mantener estabilidad.\n\n";
        msg += L"PDF/DOC/DOCX/PPT/PPTX: máximo 100";
        if(dropped_heavy) msg += L" · omitidos: " + std::to_wstring(dropped_heavy);
        msg += L"\nEPUB/TXT: máximo 500";
        if(dropped_light) msg += L" · omitidos: " + std::to_wstring(dropped_light);
        msg += L"\n\nLos primeros archivos del lote se conservaron en orden.";
        MessageBoxW(owner, msg.c_str(), app_name?app_name:L"Miausoft", MB_ICONINFORMATION|MB_OK);
    }
    return out;
}

struct MultiSelectResult { std::vector<fs::path> files; bool launch = true; HANDLE mutex = nullptr; };
inline void append_spool_locked(const fs::path& spool, const std::vector<fs::path>& files){
    fs::create_directories(spool.parent_path());
    HANDLE h = CreateFileW(spool.c_str(), GENERIC_READ|GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if(h==INVALID_HANDLE_VALUE) return;
    OVERLAPPED ov{}; LockFileEx(h, LOCKFILE_EXCLUSIVE_LOCK, 0, 1, 0, &ov);
    SetFilePointer(h, 0, nullptr, FILE_END);
    for(auto& p: files){ auto u=wide_to_utf8(p.wstring()+L"\n"); DWORD wr=0; WriteFile(h,u.data(),(DWORD)u.size(),&wr,nullptr); }
    UnlockFileEx(h,0,1,0,&ov); CloseHandle(h);
}
inline std::vector<fs::path> drain_spool_locked(const fs::path& spool){
    std::vector<fs::path> out;
    HANDLE h = CreateFileW(spool.c_str(), GENERIC_READ|GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if(h==INVALID_HANDLE_VALUE) return out;
    OVERLAPPED ov{}; LockFileEx(h, LOCKFILE_EXCLUSIVE_LOCK, 0, 1, 0, &ov);
    DWORD sz = GetFileSize(h,nullptr); std::string b(sz,'\0'); DWORD rd=0; SetFilePointer(h,0,nullptr,FILE_BEGIN); if(sz) ReadFile(h,b.data(),sz,&rd,nullptr); SetFilePointer(h,0,nullptr,FILE_BEGIN); SetEndOfFile(h);
    UnlockFileEx(h,0,1,0,&ov); CloseHandle(h);
    std::wistringstream is(utf8_to_wide(b)); std::wstring line;
    while(std::getline(is,line)){ line=trim(line); if(!line.empty() && file_exists(line)) out.emplace_back(line); }
    return out;
}
inline MultiSelectResult merge_multiselect(const std::vector<fs::path>& input, const wchar_t* mutex_name, const wchar_t* spool_name, int settle_ms=12000, int stable_ms=1100, int poll_ms=50){
    MultiSelectResult r; if(input.empty()){ r.files=input; return r; }
    HANDLE h=CreateMutexW(nullptr,FALSE,mutex_name); bool already=(GetLastError()==ERROR_ALREADY_EXISTS);
    fs::path spool=local_appdata()/L"MiausoftSuite"/L"IPC"/spool_name;
    if(already){ append_spool_locked(spool,input); if(h) CloseHandle(h); r.launch=false; return r; }
    r.mutex=h; append_spool_locked(spool,input);
    std::set<std::wstring> seen; int stable=0; auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(settle_ms);
    // No reducir la ventana de agregación: SendTo puede lanzar cientos de procesos separados
    // o grupos parciales. Mantener una ventana estable evita perder archivos de lotes grandes.
    while(std::chrono::steady_clock::now()<deadline && stable<stable_ms){
        int added=0; for(auto& p: drain_spool_locked(spool)){ auto k=to_lower(p.wstring()); if(!seen.count(k)){ seen.insert(k); r.files.push_back(p); added++; } }
        if(added) stable=0; else stable+=poll_ms;
        std::this_thread::sleep_for(std::chrono::milliseconds(max_i(10, poll_ms)));
    }
    for(auto& p: drain_spool_locked(spool)){ auto k=to_lower(p.wstring()); if(!seen.count(k)){ seen.insert(k); r.files.push_back(p); } }
    std::error_code ec; fs::remove(spool,ec); if(r.files.empty()) r.files=input; return r;
}

inline COLORREF mix(COLORREF a, COLORREF b, double t){ auto c=[&](int aa,int bb){return (BYTE)std::lround((1-t)*aa+t*bb);}; return RGB(c(GetRValue(a),GetRValue(b)),c(GetGValue(a),GetGValue(b)),c(GetBValue(a),GetBValue(b))); }
inline COLORREF accent_color(){ return miausoft_visual::accent_color(); }
inline HFONT make_font(int px, int weight=FW_NORMAL, bool italic=false, const wchar_t* fam=L"Segoe UI"){ return CreateFontW(-px,0,0,0,weight,italic,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH|FF_DONTCARE,fam); }


class ProgressWindow {
    static constexpr double PHI_UI = 1.6180339887498948482;
    static constexpr int MAX_UI_LINES = 7;
    HWND hwnd_{};
    HWND btn_cancel_{};
    HICON icon_big_{};
    HICON icon_small_{};
    std::wstring title_ = L"Miausoft";
    std::wstring subtitle_ = L"";
    std::wstring root_text_ = L"";
    std::wstring final_message_ = L"";
    std::vector<std::wstring> slots_;
    double crisp_ = 0.0;
    double ghost_ = 0.0;
    double ghost_intensity_ = 0.25;
    bool cancelled_ = false;
    bool completed_ = false;
    bool config_has_txt_threshold_ = false;
    int config_default_min_txt_kb_ = 9;
    std::wstring config_app_key_ = L"General";

    static int max_i(int a, int b){ return a > b ? a : b; }
    static int min_i(int a, int b){ return a < b ? a : b; }

    static COLORREF windows_accent_color(){ return miausoft_visual::accent_color(); }

    static COLORREF mix_color(COLORREF a, COLORREF b, double t){ return miausoft_visual::mix_color(a, b, t); }

    static void draw_round_rect(HDC hdc, RECT rc, COLORREF fill, COLORREF border, int radius){
        HBRUSH br = CreateSolidBrush(fill);
        HPEN pen = CreatePen(PS_SOLID, 1, border);
        HGDIOBJ oldb = SelectObject(hdc, br);
        HGDIOBJ oldp = SelectObject(hdc, pen);
        RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, radius, radius);
        SelectObject(hdc, oldb);
        SelectObject(hdc, oldp);
        DeleteObject(br);
        DeleteObject(pen);
    }

    static HFONT ui_font(int px, int weight = FW_NORMAL, bool italic = false){
        return CreateFontW(-px, 0, 0, 0, weight, italic, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    }

    static void draw_text_light(HDC hdc, const std::wstring& txt, RECT rc, HFONT font, COLORREF color, UINT flags){
        HFONT old = (HFONT)SelectObject(hdc, font);
        SetTextColor(hdc, color);
        SetBkMode(hdc, TRANSPARENT);
        DrawTextW(hdc, txt.c_str(), -1, &rc, flags | DT_NOPREFIX);
        SelectObject(hdc, old);
    }

    void draw_brand_mark(HDC hdc, const RECT& rc){
        if(icon_big_){
            DrawIconEx(hdc, rc.left, rc.top, icon_big_, rc.right - rc.left, rc.bottom - rc.top, 0, nullptr, DI_NORMAL);
            return;
        }
        HFONT mini = ui_font(max_i(14, static_cast<int>((rc.bottom - rc.top) / 4)), FW_SEMIBOLD);
        RECT rr = rc;
        draw_text_light(hdc, L"M", rr, mini, RGB(28,28,28), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        DeleteObject(mini);
    }

    void draw_owner_button(DRAWITEMSTRUCT* dis){
        if(!dis || !dis->hDC) return;
        HDC hdc = dis->hDC;
        RECT rc = dis->rcItem;
        const bool disabled = (dis->itemState & ODS_DISABLED) != 0;
        const bool pressed = (dis->itemState & ODS_SELECTED) != 0;
        const bool focus = (dis->itemState & ODS_FOCUS) != 0;

        const COLORREF page = miausoft_visual::page_color();
        const COLORREF soft = miausoft_visual::panel_color();
        const COLORREF ink = miausoft_visual::ink_color();
        const COLORREF muted = miausoft_visual::muted_color();
        const COLORREF accent = windows_accent_color();

        COLORREF fill = soft;
        COLORREF text = ink;
        if(pressed){
            fill = mix_color(soft, accent, 0.20);
        }
        if(disabled){
            fill = mix_color(fill, page, 0.55);
            text = mix_color(muted, page, 0.45);
        }
        const int radius = max_i(14, static_cast<int>((rc.bottom - rc.top) * 0.58));
        draw_round_rect(hdc, rc, fill, fill, radius);

        std::wstring caption;
        const int len = GetWindowTextLengthW(dis->hwndItem);
        if(len > 0){
            caption.resize(static_cast<size_t>(len) + 1);
            GetWindowTextW(dis->hwndItem, caption.data(), len + 1);
            caption.resize(static_cast<size_t>(len));
        }
        HFONT f = ui_font(10, FW_NORMAL);
        RECT tr = rc;
        draw_text_light(hdc, caption, tr, f, text, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        DeleteObject(f);

        (void)focus;
    }

    void layout_controls(){
        // El cierre forzado ya vive en la X nativa de la ventana.
        // No se crean botones flotantes encima del panel visual.
        if(btn_cancel_) ShowWindow(btn_cancel_, SW_HIDE);
    }

    void paint(){
        PAINTSTRUCT ps{};
        HDC screen = BeginPaint(hwnd_, &ps);
        RECT client{};
        GetClientRect(hwnd_, &client);
        const int w = max_i(1, static_cast<int>(client.right - client.left));
        const int h = max_i(1, static_cast<int>(client.bottom - client.top));

        HDC hdc = CreateCompatibleDC(screen);
        HBITMAP bmp = CreateCompatibleBitmap(screen, w, h);
        HGDIOBJ oldBmp = SelectObject(hdc, bmp);

        const COLORREF page = miausoft_visual::page_color();
        const COLORREF track = miausoft_visual::progress_track_soft_color();
        const COLORREF accent = windows_accent_color();
        const COLORREF dark = miausoft_visual::ink_color();
        const COLORREF muted = miausoft_visual::muted_color();
        const COLORREF pale = mix_color(muted, page, 0.36);

        HBRUSH bg = CreateSolidBrush(page);
        FillRect(hdc, &client, bg);
        DeleteObject(bg);

        const int leftPanelW = miausoft_visual::icon_panel_width(w);
        const int leftPad = max_i(miausoft_visual::golden_pad_x(w), miausoft_visual::golden_pad_y(h));
        const int iconSide = miausoft_visual::icon_side_for_panel(leftPanelW, h);
        const int iconX = max_i(0, (leftPanelW - iconSide) / 2);
        const int iconY = miausoft_visual::progress_icon_y(h, iconSide);
        const int contentX = leftPanelW;
        const int rightPad = max_i(10, leftPad);
        const int contentW = max_i(220, w - contentX - rightPad);
        const int barY = 0;
        const int barH = miausoft_visual::top_progress_height(h);

        RECT iconRc{iconX, iconY, iconX + iconSide, iconY + iconSide};
        draw_brand_mark(hdc, iconRc);

        RECT bar{0, 0, w, barH};
        HBRUSH brTrack = CreateSolidBrush(track);
        FillRect(hdc, &bar, brTrack);
        DeleteObject(brTrack);

        RECT ghostRc = bar;
        ghostRc.right = bar.left + static_cast<int>((bar.right - bar.left) * std::clamp(ghost_, 0.0, 1.0));
        COLORREF ghostColor = mix_color(track, accent, std::clamp(ghost_intensity_, 0.25, 1.0) * 0.55);
        if(ghostRc.right > ghostRc.left){
            HBRUSH brGhost = CreateSolidBrush(ghostColor);
            FillRect(hdc, &ghostRc, brGhost);
            DeleteObject(brGhost);
        }

        RECT crispRc = bar;
        crispRc.right = bar.left + static_cast<int>((bar.right - bar.left) * std::clamp(crisp_, 0.0, 1.0));
        if(crispRc.right > crispRc.left){
            HBRUSH brCrisp = CreateSolidBrush(accent);
            FillRect(hdc, &crispRc, brCrisp);
            DeleteObject(brCrisp);
        }

        const int titleY = miausoft_visual::title_top_y(h);
        const int titleH = miausoft_visual::title_text_height(h);
        HFONT titleFont = ui_font(miausoft_visual::dialog_title_px(h), FW_SEMIBOLD);
        const int subtitlePx = miausoft_visual::dialog_subtitle_px(h);
        HFONT subFont = ui_font(subtitlePx, FW_NORMAL);
        HFONT slotFont = ui_font(subtitlePx, FW_NORMAL);
        HFONT tinyFont = ui_font(subtitlePx, FW_NORMAL);

        RECT titleRc{contentX, titleY, contentX + contentW, titleY + titleH};
        draw_text_light(hdc, title_, titleRc, titleFont, dark, DT_LEFT | DT_TOP | DT_WORDBREAK | DT_END_ELLIPSIS);

        RECT subRc{contentX, miausoft_visual::subtitle_top_y(h), contentX + contentW, miausoft_visual::subtitle_top_y(h) + 16};
        if(!subtitle_.empty()) draw_text_light(hdc, subtitle_, subRc, subFont, muted, DT_LEFT | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS);

        int y = subRc.bottom + 1;
        const int lineH = clamp_i(h / 31, 10, 14);
        const int maxLines = min_i(static_cast<int>(slots_.size()), MAX_UI_LINES);
        for(int i=0; i<maxLines; ++i){
            const std::wstring& slot = slots_[static_cast<size_t>(i)];
            if(slot.empty() || slot == subtitle_) continue;
            RECT lineRc{contentX, y, contentX + contentW, y + lineH};
            draw_text_light(hdc, slot, lineRc, slotFont, muted, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            y += lineH;
            if(y > h - 58) break;
        }

        if(!final_message_.empty() && completed_){
            RECT fr{contentX, y + 8, contentX + contentW, h - 58};
            draw_text_light(hdc, final_message_, fr, tinyFont, pale, DT_LEFT | DT_TOP | DT_WORDBREAK | DT_END_ELLIPSIS);
        }

        const int footY = h - 45;

        if(!root_text_.empty() && completed_){
            RECT rootRc{8, footY + 5, w - 190, h - 6};
            draw_text_light(hdc, root_text_, rootRc, tinyFont, RGB(142,142,142), DT_LEFT | DT_BOTTOM | DT_SINGLELINE | DT_PATH_ELLIPSIS);
        }

        BitBlt(screen, 0, 0, w, h, hdc, 0, 0, SRCCOPY);

        DeleteObject(titleFont);
        DeleteObject(subFont);
        DeleteObject(slotFont);
        DeleteObject(tinyFont);
        SelectObject(hdc, oldBmp);
        DeleteObject(bmp);
        DeleteDC(hdc);
        EndPaint(hwnd_, &ps);
    }

    static LRESULT CALLBACK proc(HWND h, UINT m, WPARAM w, LPARAM l){
        ProgressWindow* self = reinterpret_cast<ProgressWindow*>(GetWindowLongPtrW(h, GWLP_USERDATA));
        if(m == WM_NCCREATE){
            auto cs = reinterpret_cast<CREATESTRUCTW*>(l);
            self = reinterpret_cast<ProgressWindow*>(cs->lpCreateParams);
            SetWindowLongPtrW(h, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
            self->hwnd_ = h;
        }
        if(!self) return DefWindowProcW(h, m, w, l);
        switch(m){
            case WM_CREATE:
                self->btn_cancel_ = nullptr;
                self->layout_controls();
                return 0;
            case WM_SIZE:
                miausoft_visual::relayout_settings_panel(h);
                self->layout_controls();
                InvalidateRect(h, nullptr, TRUE);
                return 0;
            case WM_LBUTTONUP:
                if (miausoft_visual::handle_settings_invocation(h, m, w, l, self->config_app_key_.c_str(), self->config_has_txt_threshold_, self->config_default_min_txt_kb_)) return 0;
                break;
            case WM_NCRBUTTONUP:
            case WM_CONTEXTMENU:
                if (miausoft_visual::handle_settings_invocation(h, m, w, l, self->config_app_key_.c_str(), self->config_has_txt_threshold_, self->config_default_min_txt_kb_)) return 0;
                break;
            case WM_ERASEBKGND:
                return 1;
            case WM_DRAWITEM:
                self->draw_owner_button(reinterpret_cast<DRAWITEMSTRUCT*>(l));
                return TRUE;
            case WM_COMMAND:
                if(LOWORD(w) == 1003){
                    self->cancelled_ = true;
                    if(self->completed_) DestroyWindow(h);
                    else { TerminateProcess(GetCurrentProcess(), 130); ExitProcess(130); }
                    return 0;
                }
                break;
            case WM_DWMCOLORIZATIONCOLORCHANGED:
                InvalidateRect(h, nullptr, FALSE);
                return 0;
            case WM_PAINT:
                self->paint();
                return 0;
            case WM_CLOSE:
                self->cancelled_ = true;
                if(self->completed_) DestroyWindow(h);
                else { TerminateProcess(GetCurrentProcess(), 130); ExitProcess(130); }
                return 0;
            case WM_DESTROY:
                self->hwnd_ = nullptr;
                self->btn_cancel_ = nullptr;
                return 0;
        }
        return DefWindowProcW(h, m, w, l);
    }

public:
    bool create(HINSTANCE hInst, const std::wstring& caption = L"MiausoftSuite"){
        INITCOMMONCONTROLSEX ic{sizeof(ic), ICC_STANDARD_CLASSES | ICC_PROGRESS_CLASS | ICC_LISTVIEW_CLASSES};
        InitCommonControlsEx(&ic);
        WNDCLASSEXW wc{sizeof(wc)};
        wc.lpfnWndProc = proc;
        wc.hInstance = hInst;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = nullptr;
        wc.lpszClassName = L"MiausoftUnifiedProgressWindow";
        wc.hIcon = LoadIconW(hInst, MAKEINTRESOURCEW(IDI_MIAUSOFT_ICON));
        wc.hIconSm = wc.hIcon;
        wc.style = CS_HREDRAW | CS_VREDRAW;
        RegisterClassExW(&wc);

        icon_big_ = (HICON)LoadImageW(hInst, MAKEINTRESOURCEW(IDI_MIAUSOFT_ICON), IMAGE_ICON, 160, 160, LR_DEFAULTCOLOR);
        const int sx = max_i(16, static_cast<int>(GetSystemMetrics(SM_CXSMICON)));
        const int sy = max_i(16, static_cast<int>(GetSystemMetrics(SM_CYSMICON)));
        icon_small_ = (HICON)LoadImageW(hInst, MAKEINTRESOURCEW(IDI_MIAUSOFT_ICON), IMAGE_ICON, sx, sy, LR_DEFAULTCOLOR);

        const int sw = GetSystemMetrics(SM_CXSCREEN);
        const int sh = GetSystemMetrics(SM_CYSCREEN);
        const int ww = max_i(1, static_cast<int>(std::llround(static_cast<double>(sw) * MIAUSOFT_WINDOW_WIDTH_RATIO)));
        const int wh = max_i(1, static_cast<int>(std::llround(static_cast<double>(sh) * MIAUSOFT_WINDOW_HEIGHT_RATIO)));
        const int x = (sw - ww) / 2;
        const int y = (sh - wh) / 2;

        hwnd_ = CreateWindowExW(WS_EX_APPWINDOW, wc.lpszClassName, caption.c_str(),
            WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_CLIPCHILDREN,
            x, y, ww, wh, nullptr, nullptr, hInst, this);
        if(!hwnd_) return false;
        miausoft_visual::apply_rounded_top_window(hwnd_);
        if(icon_big_) SendMessageW(hwnd_, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(icon_big_));
        if(icon_small_) SendMessageW(hwnd_, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(icon_small_));
        root_text_ = module_dir().wstring();
        ShowWindow(hwnd_, SW_SHOW);
        UpdateWindow(hwnd_);
        return true;
    }

    ~ProgressWindow(){
        if(IsWindow(hwnd_)) DestroyWindow(hwnd_);
        if(icon_small_) DestroyIcon(icon_small_);
        if(icon_big_) DestroyIcon(icon_big_);
    }

    HWND hwnd() const { return hwnd_; }
    bool cancelled() const { return cancelled_; }

    void enable_text_threshold_setting(const std::wstring& app_key, int default_kb = 9){
        config_app_key_ = app_key.empty() ? L"General" : app_key;
        config_has_txt_threshold_ = true;
        config_default_min_txt_kb_ = default_kb;
    }

    void enable_basic_settings(const std::wstring& app_key){
        config_app_key_ = app_key.empty() ? L"General" : app_key;
        config_has_txt_threshold_ = false;
    }

    void set(const std::wstring& title, const std::wstring& subtitle, double progress){
        title_ = title;
        subtitle_ = subtitle;
        const double next = std::clamp(progress, 0.0, 1.0);
        if(next + 0.020 < crisp_){
            crisp_ = next;
            ghost_ = std::clamp(next + 0.035, 0.0, 1.0);
        } else {
            if(next + 0.000001 >= crisp_) crisp_ = next;
            ghost_ = std::clamp(max_d(ghost_, next + 0.035), crisp_, 1.0);
        }
        ghost_intensity_ = 0.28;
        completed_ = (crisp_ >= 0.999);
        if(hwnd_) InvalidateRect(hwnd_, nullptr, FALSE);
        pump_messages();
    }

    void reset_progress(const std::wstring& title = L"", const std::wstring& subtitle = L""){
        crisp_ = 0.0;
        ghost_ = 0.0;
        ghost_intensity_ = 0.25;
        completed_ = false;
        final_message_.clear();
        if(!title.empty()) title_ = title;
        if(!subtitle.empty()) subtitle_ = subtitle;
        if(hwnd_) InvalidateRect(hwnd_, nullptr, FALSE);
        pump_messages();
    }

    void set_slots(const std::vector<std::wstring>& slots){
        slots_ = slots;
        if(hwnd_) InvalidateRect(hwnd_, nullptr, FALSE);
        pump_messages();
    }

    void set_final_message(const std::wstring& message){
        final_message_ = message;
        completed_ = true;
        if(hwnd_) InvalidateRect(hwnd_, nullptr, FALSE);
        pump_messages();
    }
};

inline void show_error(const std::wstring& title, const std::wstring& msg){ MessageBoxW(nullptr,msg.c_str(),title.c_str(),MB_ICONERROR|MB_OK); }
inline void show_info(const std::wstring& title, const std::wstring& msg){ MessageBoxW(nullptr,msg.c_str(),title.c_str(),MB_ICONINFORMATION|MB_OK); }

} // namespace miausoft
