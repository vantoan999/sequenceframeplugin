/**
****************************************************************
***********************************
* Copyright (C) 2019 Parrot Faurecia Automotive SAS
*
* @brief file mapping  module process
*
****************************************************************
***********************************
*/
#include "filemapping.h"

#if defined (__QNX__) || defined (__ANDROID__) || defined (__linux__)
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#endif


FileMapping::FileMapping()
    : m_fileBuffer(nullptr)

#if defined (_WIN32)
    , m_fileHandler()
    , m_fileMappingHandler()
#endif

#if defined (__QNX__) || defined (__ANDROID__) || defined (__linux__)
    , m_fileDescriptor()
    , m_statBuffer()
#endif
{
}

FileMapping::~FileMapping()
{
}

void* FileMapping::getFileBuffer()
{
    return m_fileBuffer;
}

int FileMapping::mapFileIntoMemory(const char* fileName)
{
#if defined (_WIN32)
    m_fileHandler = CreateFileA(
        fileName,
        GENERIC_READ,
        0,             /// Prevents other processes from opening a file or device if they request
                       /// delete, read, or write access.
        NULL,          /// The handle returned by CreateFile cannot be inherited by any child
                       /// processes the application may create
                       /// and the file or device associated with the returned handle gets a default
                       /// security descriptor.
        OPEN_EXISTING, /// Opens a file or device, only if it exists.
        FILE_ATTRIBUTE_READONLY | FILE_FLAG_SEQUENTIAL_SCAN,
        NULL);         /// A valid handle to a template file with the GENERIC_READ access right.
                       /// The template file supplies file attributes and extended attributes for
                       /// the file that is being created.

    if (INVALID_HANDLE_VALUE == m_fileHandler) {
        return GetLastError();
    }

    m_fileMappingHandler = CreateFileMappingA(
        m_fileHandler,
        NULL,          /// The handle cannot be inherited and the file mapping object gets a default
                       /// security descriptor.
        PAGE_READONLY, /// Allows views to be mapped for read-only or copy-on-write access.
        0,             /// If dwMaximumSizeHigh and dwMaximumSizeLow are 0 (zero),
        0,             /// the maximum size of the file mapping object is equal to the current size
                       /// of the file that hFile identifies.
        NULL);         /// The name of the file mapping object. If this parameter is NULL, the file
                       /// mapping object is created without a name.

    if (NULL == m_fileMappingHandler) {
        CloseHandle(m_fileHandler);
        return GetLastError();
    }

    m_fileBuffer = MapViewOfFile(m_fileMappingHandler, FILE_MAP_READ, 0, 0, 0);
    if (NULL == m_fileBuffer) {
        CloseHandle(m_fileMappingHandler);
        CloseHandle(m_fileHandler);
        return GetLastError();
    }

    /// Mapped views of a file mapping object maintain internal references to the object, and
    /// a file mapping object does not close until all references to it are released.
    /// Therefore, to fully close a file mapping object, an application must unmap all mapped
    /// views of the file mapping object by calling UnmapViewOfFile
    /// and close the file mapping object handle by calling CloseHandle. These functions can
    /// be called in any order.
    if (0 == CloseHandle(m_fileMappingHandler)) {
        CloseHandle(m_fileHandler);
        return GetLastError();
    }

    if (0 == CloseHandle(m_fileHandler)) {
        return GetLastError();
    }
#endif

#if defined (__QNX__) || defined (__ANDROID__) || defined (__linux__)
    m_fileDescriptor = open(fileName, O_RDONLY);
    if (-1 == m_fileDescriptor) {
        return errno;
    }

    if (-1 == fstat(m_fileDescriptor, &m_statBuffer)) {
        close(m_fileDescriptor);
        return errno;
    }

    /// When you map a file descriptor, the file's reference count is incremented.
    /// Therefore, you can close the file descriptor after mapping the file, and your process will
    /// still have access to it.
    /// The corresponding decrement of the file's reference count will occur when you unmap the
    /// file, or when the process terminates.
    m_fileBuffer = mmap(
        0,                     /// addr   - The addr parameter offers a suggestion to the kernel
                               /// of where best to map the file.
        m_statBuffer.st_size,  /// len
        PROT_READ,             /// prot   - The prot parameter describes the desired memory
                               /// protection of the mapping.
        MAP_SHARED,            /// flags  - The flags argument describes the type of mapping,
                               /// and some elements of its behavior.
        m_fileDescriptor,      /// fd
        0);                    /// offset

    if (MAP_FAILED == m_fileBuffer) {
        close(m_fileDescriptor);
        return errno;
    }

    if (-1 == close(m_fileDescriptor)) {
        return errno;
    }
#endif

    return 0;
}

int FileMapping::closeFileMapping()
{
#if defined (_WIN32)
    if (0 == UnmapViewOfFile(m_fileBuffer)) {
        return GetLastError();
    }
#endif

#if defined (__QNX__) || defined (__ANDROID__) || defined (__linux__)
    if (-1 == munmap(m_fileBuffer, m_statBuffer.st_size)) {
        return errno;
    }
#endif

    return 0;
}
