#pragma once
#include <windows.h>
#include <algorithm>
#include <string>

// Byte-only commit: callers retain ownership of text encoding and serialization.
class CAtomicFile
{
public:
    static bool Save(const std::wstring& path, const std::string& bytes)
    {
        if (path.empty())
            return false;

        bool destination_exists{};
        if (IsReparsePoint(path, destination_exists))
            return false;
        if (destination_exists)
        {
            const DWORD attributes = ::GetFileAttributesW(path.c_str());
            if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY)
                || !ExistingFileIsWritable(path))
                return false;
        }

        const size_t slash = path.find_last_of(L"\\/");
        const std::wstring directory = slash == std::wstring::npos ? L".\\" : path.substr(0, slash + 1);
        bool directory_exists{};
        if (IsReparsePoint(directory, directory_exists) || !directory_exists)
            return false;
        const DWORD directory_attributes = ::GetFileAttributesW(directory.c_str());
        if (directory_attributes == INVALID_FILE_ATTRIBUTES
            || (directory_attributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
            return false;

        std::wstring temp_path;
        HANDLE temp_file = INVALID_HANDLE_VALUE;
        if (!CreateTempFile(directory, temp_path, temp_file))
            return false;

        bool saved = WriteAll(temp_file, bytes.data(), bytes.size());
        if (saved)
            saved = ::FlushFileBuffers(temp_file) != FALSE;
        const bool close_ok = ::CloseHandle(temp_file) != FALSE;
        temp_file = INVALID_HANDLE_VALUE;
        saved = saved && close_ok;

        if (saved)
        {
            // Recheck before committing, without deleting the original on failure.
            bool current_exists{};
            bool current_directory_exists{};
            if (IsReparsePoint(directory, current_directory_exists)
                || !current_directory_exists
                || IsReparsePoint(path, current_exists))
                saved = false;
            else if (current_exists)
            {
                const DWORD attributes = ::GetFileAttributesW(path.c_str());
                saved = attributes != INVALID_FILE_ATTRIBUTES
                    && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0
                    && ExistingFileIsWritable(path)
                    // The replacement data was explicitly flushed before closing it.
                    && (::ReplaceFileW(path.c_str(), temp_path.c_str(), nullptr,
                        0, nullptr, nullptr) != FALSE);
            }
            else
            {
                saved = ::MoveFileExW(temp_path.c_str(), path.c_str(),
                    MOVEFILE_WRITE_THROUGH) != FALSE;
            }
        }

        if (!saved)
            ::DeleteFileW(temp_path.c_str());
        return saved;
    }

private:
    static bool IsMissingPathError(DWORD error)
    {
        return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND;
    }

    static bool IsReparsePoint(const std::wstring& path, bool& exists)
    {
        HANDLE handle = ::CreateFileW(path.c_str(), 0,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
            OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, nullptr);
        if (handle == INVALID_HANDLE_VALUE)
        {
            const DWORD error = ::GetLastError();
            exists = !IsMissingPathError(error);
            return exists; // Fail closed for access, sharing, and other errors.
        }
        exists = true;
        BY_HANDLE_FILE_INFORMATION information{};
        const bool read_ok = ::GetFileInformationByHandle(handle, &information) != FALSE;
        const bool close_ok = ::CloseHandle(handle) != FALSE;
        return !read_ok || !close_ok || (information.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
    }

    static bool ExistingFileIsWritable(const std::wstring& path)
    {
        HANDLE handle = ::CreateFileW(path.c_str(), GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (handle == INVALID_HANDLE_VALUE)
            return false;
        return ::CloseHandle(handle) != FALSE;
    }

    static bool WriteAll(HANDLE file, const char* data, size_t size)
    {
        while (size != 0)
        {
            const DWORD request = static_cast<DWORD>(std::min<size_t>(size, MAXDWORD));
            DWORD written{};
            if (!::WriteFile(file, data, request, &written, nullptr) || written == 0 || written > request)
                return false;
            data += written;
            size -= written;
        }
        return true;
    }

    static bool CreateTempFile(const std::wstring& directory, std::wstring& temp_path, HANDLE& temp_file)
    {
        static volatile LONG sequence{};
        const std::wstring prefix = directory + L".PulseBar."
            + std::to_wstring(::GetCurrentProcessId()) + L".";
        for (int attempt{}; attempt < 100; ++attempt)
        {
            temp_path = prefix + std::to_wstring(static_cast<DWORD>(::InterlockedIncrement(&sequence))) + L".tmp";
            temp_file = ::CreateFileW(temp_path.c_str(), GENERIC_WRITE, 0, nullptr,
                CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (temp_file != INVALID_HANDLE_VALUE)
                return true;
            const DWORD error = ::GetLastError();
            if (error != ERROR_FILE_EXISTS && error != ERROR_ALREADY_EXISTS)
                return false;
        }
        return false;
    }
};
