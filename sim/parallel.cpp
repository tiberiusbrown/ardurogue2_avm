#include "parallel.hpp"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <vector>
#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
extern char** environ;
#endif

namespace sim {
namespace {
namespace fs=std::filesystem;
struct Scratch {
    fs::path path;
    Scratch() {
        auto stamp=std::chrono::steady_clock::now().time_since_epoch().count();
        for(unsigned attempt=0; attempt<100; ++attempt) {
            path=fs::temp_directory_path()/("ardurogue2-workers-"+std::to_string(stamp)+"-"+std::to_string(attempt));
            if(fs::create_directory(path)) return;
        }
        throw std::runtime_error("cannot create worker scratch directory");
    }
    ~Scratch() { std::error_code error; fs::remove_all(path,error); }
};
#if defined(_WIN32)
std::wstring quote(const std::string& argument) {
    // CRT argv quoting, including spaces, quotes and trailing backslashes.
    std::wstring result=L"\""; unsigned slashes=0;
    for(wchar_t c : fs::path(argument).wstring()) {
        if(c==L'\\') { ++slashes; continue; }
        result.append(c==L'"' ? 2*slashes+1 : slashes,L'\\');
        result+=c; slashes=0;
    }
    result.append(2*slashes,L'\\'); return result+L'"';
}
#endif
struct Process {
#if defined(_WIN32)
    HANDLE process=nullptr;
#else
    pid_t process=-1;
#endif
    Process(const std::vector<std::string>& arguments, const fs::path& log) {
#if defined(_WIN32)
        std::wstring command;
        for(const auto& arg:arguments) { if(!command.empty()) command+=L' '; command+=quote(arg); }
        SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES),nullptr,TRUE};
        HANDLE output=CreateFileW(log.c_str(),GENERIC_WRITE,FILE_SHARE_READ,&security,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
        HANDLE input=CreateFileW(L"NUL",GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,&security,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(output==INVALID_HANDLE_VALUE || input==INVALID_HANDLE_VALUE) {
            if(output!=INVALID_HANDLE_VALUE) CloseHandle(output);
            if(input!=INVALID_HANDLE_VALUE) CloseHandle(input);
            throw std::runtime_error("cannot open worker log/input");
        }
        STARTUPINFOW startup{}; startup.cb=sizeof(startup); startup.dwFlags=STARTF_USESTDHANDLES;
        startup.hStdInput=input; startup.hStdOutput=startup.hStdError=output;
        PROCESS_INFORMATION info{};
        bool ok=CreateProcessW(fs::path(arguments.front()).c_str(),command.data(),nullptr,nullptr,TRUE,
            CREATE_NO_WINDOW,nullptr,nullptr,&startup,&info)!=FALSE;
        auto error=GetLastError(); CloseHandle(output); CloseHandle(input);
        if(!ok) throw std::runtime_error("cannot start simulator worker: Windows error "+std::to_string(error));
        process=info.hProcess; CloseHandle(info.hThread);
#else
        std::vector<char*> argv;
        for(const auto& arg:arguments) argv.push_back(const_cast<char*>(arg.c_str()));
        argv.push_back(nullptr);
        posix_spawn_file_actions_t actions;
        int error=posix_spawn_file_actions_init(&actions);
        if(error) throw std::runtime_error("cannot initialize simulator worker");
        error=posix_spawn_file_actions_addopen(&actions,STDIN_FILENO,"/dev/null",O_RDONLY,0);
        if(!error) error=posix_spawn_file_actions_addopen(&actions,STDERR_FILENO,log.c_str(),O_WRONLY|O_CREAT|O_TRUNC,0600);
        if(!error) error=posix_spawn_file_actions_adddup2(&actions,STDERR_FILENO,STDOUT_FILENO);
        if(!error) error=posix_spawnp(&process,arguments.front().c_str(),&actions,nullptr,argv.data(),environ);
        posix_spawn_file_actions_destroy(&actions);
        if(error) { process=-1; throw std::runtime_error("cannot start simulator worker: error "+std::to_string(error)); }
#endif
    }
    int wait() {
#if defined(_WIN32)
        if(!process) return 0;
        DWORD code=1;
        if(WaitForSingleObject(process,INFINITE)==WAIT_OBJECT_0) GetExitCodeProcess(process,&code);
        CloseHandle(process); process=nullptr; return static_cast<int>(code);
#else
        if(process<0) return 0;
        int status=0; pid_t result;
        do { result=waitpid(process,&status,0); } while(result<0 && errno==EINTR);
        process=-1;
        return result>=0 && WIFEXITED(status) ? WEXITSTATUS(status) : 1;
#endif
    }
    ~Process() { wait(); }
};
std::string header(void (*write)(std::ostream&)) {
    std::ostringstream out; write(out); auto text=out.str(); text.pop_back(); return text;
}
void merge(const fs::path& file, std::ostream& output, const std::string& expected) {
    std::ifstream input(file); std::string first;
    if(!input || !std::getline(input,first) || first!=expected) throw std::runtime_error("invalid worker CSV: "+file.string());
    // A sparse stream may contain only its header. Inserting an empty streambuf
    // sets failbit on the destination, although there is no I/O error.
    if(input.peek()!=std::char_traits<char>::eof()) output<<input.rdbuf();
    if(input.bad() || !output) throw std::runtime_error("worker CSV merge failed: "+file.string());
}
}
BatchCounts parallel_batch(const std::string& executable, uint64_t start,
    uint64_t count, unsigned jobs, const Options& options, const std::array<std::ostream*,7>& outputs,
    std::ostream* entry_state) {
    if(options.trace) throw std::runtime_error("parallel batches cannot trace multiple seeds");
    jobs=static_cast<unsigned>(std::min<uint64_t>(jobs,count));
    Scratch scratch;
    std::vector<fs::path> directories;
    std::vector<std::unique_ptr<Process>> workers;
    // Launch every worker before waiting. Only the parent writes final outputs.
    for(unsigned j=0; j<jobs; ++j) {
        uint64_t first=start+count*j/jobs, last=start+count*(j+1)/jobs-1;
        auto directory=scratch.path/std::to_string(j); fs::create_directory(directory);
        std::vector<std::string> args{executable,"--seeds",std::to_string(first)+":"+std::to_string(last),
            "--output",directory.string(),"--max-actions",std::to_string(options.max_actions)};
        if(!options.telemetry) args.push_back("--no-telemetry");
        if(entry_state) args.push_back("--entry-state");
        args.insert(args.end(),{"--experiment",options.experiment_id,"--variant",options.variant});
        for(const auto& rule:options.intervention_rules) args.insert(args.end(),{"--intervention",rule});
        directories.push_back(directory);
        workers.push_back(std::make_unique<Process>(args,directory/"worker.log"));
    }
    for(size_t j=0; j<workers.size(); ++j) if(workers[j]->wait()!=0) {
        std::ifstream log(directories[j]/"worker.log"); std::ostringstream text; text<<log.rdbuf();
        throw std::runtime_error("simulator worker "+std::to_string(j)+" failed: "+text.str());
    }
    BatchCounts result; uint64_t next=start;
    for(const auto& directory:directories) {
        std::ifstream input(directory/"runs.csv"); std::string line;
        if(!input || !std::getline(input,line) || line!=header(write_runs_header)) throw std::runtime_error("invalid worker runs CSV");
        while(std::getline(input,line)) {
            size_t comma=line.find(',');
            if(comma==std::string::npos || line.substr(0,comma)!=std::to_string(next++)) throw std::runtime_error("worker seed ordering/count mismatch");
            size_t first=comma+1;
            for(int field=0; field<2; ++field) { comma=line.find(',',first); if(comma==std::string::npos) throw std::runtime_error("invalid worker result"); first=comma+1; }
            auto outcome=line.substr(first,line.find(',',first)-first);
            result.escaped+=outcome=="escaped"; result.deaths+=outcome=="death";
            result.stuck+=outcome=="SIM_STUCK" || outcome=="SIM_ERROR";
            *outputs[0]<<line<<'\n';
        }
        if(input.bad()) throw std::runtime_error("worker runs read failed");
        for(size_t s=1;s<csv_streams.size();++s) if(outputs[s])
            merge(directory/csv_streams[s].name,*outputs[s],header(csv_streams[s].header));
        if(entry_state) merge(directory/"entry_state.csv",*entry_state,header(write_entry_state_header));
    }
    if(next!=start+count) throw std::runtime_error("worker seed count mismatch");
    return result;
}
}
