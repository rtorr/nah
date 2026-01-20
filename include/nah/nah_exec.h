/*
 * NAH Exec - Contract Execution for NAH
 * 
 * This file provides process spawning to execute a LaunchContract.
 * Platform-specific implementations for Unix and Windows.
 * 
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef NAH_EXEC_H
#define NAH_EXEC_H

#ifdef __cplusplus

#include "nah_core.h"

#include <cstdlib>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <process.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#include <cstring>
#endif

namespace nah {
namespace exec {

// ============================================================================
// EXECUTION RESULT
// ============================================================================

struct ExecResult {
    bool ok = false;
    int exit_code = -1;
    std::string error;
};

// ============================================================================
// ENVIRONMENT BUILDING
// ============================================================================

/**
 * Build environment array for execve/CreateProcess.
 * 
 * Merges contract environment with library path.
 */
inline std::vector<std::string> build_environment(const core::LaunchContract& contract) {
    std::vector<std::string> env;
    
    // Add all contract environment variables
    for (const auto& [key, value] : contract.environment) {
        env.push_back(key + "=" + value);
    }
    
    // Build library path
    if (!contract.execution.library_paths.empty()) {
        std::string lib_path;
        char sep = core::get_path_separator();
        
        for (size_t i = 0; i < contract.execution.library_paths.size(); i++) {
            if (i > 0) lib_path += sep;
            lib_path += contract.execution.library_paths[i];
        }
        
        // Check if key already exists in environment
        std::string lib_key = contract.execution.library_path_env_key;
        bool found = false;
        for (auto& e : env) {
            if (e.find(lib_key + "=") == 0) {
                // Prepend to existing value
                std::string existing = e.substr(lib_key.size() + 1);
                e = lib_key + "=" + lib_path + sep + existing;
                found = true;
                break;
            }
        }
        
        if (!found) {
            env.push_back(lib_key + "=" + lib_path);
        }
    }
    
    return env;
}

// ============================================================================
// COMMAND LINE BUILDING
// ============================================================================

/**
 * Build argv array for exec.
 */
inline std::vector<std::string> build_argv(const core::LaunchContract& contract) {
    std::vector<std::string> argv;
    argv.push_back(contract.execution.binary);
    for (const auto& arg : contract.execution.arguments) {
        argv.push_back(arg);
    }
    return argv;
}

// ============================================================================
// UNIX EXECUTION
// ============================================================================

#ifndef _WIN32

/**
 * Execute contract using fork/exec (Unix).
 * 
 * If wait_for_exit is true, waits for process to complete and returns exit code.
 * If false, returns immediately after spawning (exit_code will be 0).
 */
inline ExecResult execute_unix(const core::LaunchContract& contract, bool wait_for_exit = true) {
    ExecResult result;
    
    auto argv_strings = build_argv(contract);
    auto env_strings = build_environment(contract);
    
    // Build C-style arrays
    std::vector<char*> argv;
    for (auto& s : argv_strings) {
        argv.push_back(const_cast<char*>(s.c_str()));
    }
    argv.push_back(nullptr);
    
    std::vector<char*> envp;
    for (auto& s : env_strings) {
        envp.push_back(const_cast<char*>(s.c_str()));
    }
    envp.push_back(nullptr);
    
    pid_t pid = fork();
    
    if (pid == -1) {
        result.error = "fork failed: " + std::string(strerror(errno));
        return result;
    }
    
    if (pid == 0) {
        // Child process
        
        // Change directory
        if (!contract.execution.cwd.empty()) {
            if (chdir(contract.execution.cwd.c_str()) != 0) {
                _exit(127);
            }
        }
        
        // Execute
        execve(contract.execution.binary.c_str(), argv.data(), envp.data());
        
        // If execve returns, it failed
        _exit(127);
    }
    
    // Parent process
    if (wait_for_exit) {
        int status;
        if (waitpid(pid, &status, 0) == -1) {
            result.error = "waitpid failed: " + std::string(strerror(errno));
            return result;
        }
        
        if (WIFEXITED(status)) {
            result.exit_code = WEXITSTATUS(status);
            result.ok = true;
        } else if (WIFSIGNALED(status)) {
            result.exit_code = 128 + WTERMSIG(status);
            result.ok = true;
        } else {
            result.error = "process terminated abnormally";
        }
    } else {
        result.ok = true;
        result.exit_code = 0;
    }
    
    return result;
}

/**
 * Replace current process with contract (Unix).
 * 
 * This function does not return on success.
 */
inline ExecResult exec_replace_unix(const core::LaunchContract& contract) {
    ExecResult result;
    
    auto argv_strings = build_argv(contract);
    auto env_strings = build_environment(contract);
    
    std::vector<char*> argv;
    for (auto& s : argv_strings) {
        argv.push_back(const_cast<char*>(s.c_str()));
    }
    argv.push_back(nullptr);
    
    std::vector<char*> envp;
    for (auto& s : env_strings) {
        envp.push_back(const_cast<char*>(s.c_str()));
    }
    envp.push_back(nullptr);
    
    // Change directory
    if (!contract.execution.cwd.empty()) {
        if (chdir(contract.execution.cwd.c_str()) != 0) {
            result.error = "chdir failed: " + std::string(strerror(errno));
            return result;
        }
    }
    
    // Replace process
    execve(contract.execution.binary.c_str(), argv.data(), envp.data());
    
    // If we get here, execve failed
    result.error = "execve failed: " + std::string(strerror(errno));
    return result;
}

#endif // !_WIN32

// ============================================================================
// WINDOWS EXECUTION
// ============================================================================

#ifdef _WIN32

/**
 * Build command line string for CreateProcess (Windows).
 */
inline std::string build_command_line(const std::vector<std::string>& argv) {
    std::string cmd;
    for (size_t i = 0; i < argv.size(); i++) {
        if (i > 0) cmd += " ";
        
        // Quote arguments containing spaces
        bool needs_quotes = argv[i].find(' ') != std::string::npos ||
                           argv[i].find('\t') != std::string::npos;
        
        if (needs_quotes) cmd += "\"";
        cmd += argv[i];
        if (needs_quotes) cmd += "\"";
    }
    return cmd;
}

/**
 * Build environment block for CreateProcess (Windows).
 */
inline std::string build_environment_block(const std::vector<std::string>& env) {
    std::string block;
    for (const auto& e : env) {
        block += e;
        block += '\0';
    }
    block += '\0';  // Double null terminator
    return block;
}

/**
 * Execute contract using CreateProcess (Windows).
 */
inline ExecResult execute_windows(const core::LaunchContract& contract, bool wait_for_exit = true) {
    ExecResult result;
    
    auto argv = build_argv(contract);
    auto env = build_environment(contract);
    
    std::string cmd_line = build_command_line(argv);
    std::string env_block = build_environment_block(env);
    
    STARTUPINFOA si = {0};
    si.cb = sizeof(si);
    
    PROCESS_INFORMATION pi = {0};
    
    BOOL success = CreateProcessA(
        contract.execution.binary.c_str(),
        const_cast<char*>(cmd_line.c_str()),
        nullptr,  // Process security attributes
        nullptr,  // Thread security attributes
        FALSE,    // Inherit handles
        0,        // Creation flags
        const_cast<char*>(env_block.c_str()),
        contract.execution.cwd.empty() ? nullptr : contract.execution.cwd.c_str(),
        &si,
        &pi
    );
    
    if (!success) {
        result.error = "CreateProcess failed: " + std::to_string(GetLastError());
        return result;
    }
    
    if (wait_for_exit) {
        WaitForSingleObject(pi.hProcess, INFINITE);
        
        DWORD exit_code;
        if (GetExitCodeProcess(pi.hProcess, &exit_code)) {
            result.exit_code = static_cast<int>(exit_code);
            result.ok = true;
        } else {
            result.error = "GetExitCodeProcess failed";
        }
    } else {
        result.ok = true;
        result.exit_code = 0;
    }
    
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    
    return result;
}

#endif // _WIN32

// ============================================================================
// CROSS-PLATFORM API
// ============================================================================

/**
 * Execute a launch contract.
 * 
 * Spawns a new process according to the contract's execution specification.
 * 
 * @param contract The launch contract to execute
 * @param wait_for_exit If true, wait for process to complete
 * @return ExecResult with success status and exit code
 */
inline ExecResult execute(const core::LaunchContract& contract, bool wait_for_exit = true) {
#ifdef _WIN32
    return execute_windows(contract, wait_for_exit);
#else
    return execute_unix(contract, wait_for_exit);
#endif
}

/**
 * Replace current process with contract (Unix only).
 * 
 * On Windows, this spawns a new process and exits the current one.
 * 
 * @param contract The launch contract to execute
 * @return ExecResult (only returns on failure)
 */
inline ExecResult exec_replace(const core::LaunchContract& contract) {
#ifdef _WIN32
    auto result = execute_windows(contract, false);
    if (result.ok) {
        ExitProcess(0);
    }
    return result;
#else
    return exec_replace_unix(contract);
#endif
}

} // namespace exec
} // namespace nah

#endif // __cplusplus

#endif // NAH_EXEC_H
