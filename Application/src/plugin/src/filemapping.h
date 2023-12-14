/**
****************************************************************
***********************************
* Copyright (C) 2019 Parrot Faurecia Automotive SAS
*
* @brief file mapping module header file
*
****************************************************************
***********************************
*/
#ifndef PLUGIN_SRC_FILEMAPPING_H_
#define PLUGIN_SRC_FILEMAPPING_H_

#if defined (_WIN32)
#include <windows.h>
#endif

#if defined (__QNX__) || defined (__ANDROID__) || defined (__linux__)
#include <sys/stat.h>
#include <sys/types.h>
#endif


class FileMapping
{
public:

    FileMapping();
    ~FileMapping();
    // get the buffer of file
    void* getFileBuffer();

    // map file into memort
    int mapFileIntoMemory(const char* fileName);
    // close file mapping
    int closeFileMapping();

private:

    void* m_fileBuffer;

#if defined (_WIN32)
    HANDLE m_fileHandler;
    HANDLE m_fileMappingHandler;
#endif

#if defined (__QNX__) || defined (__ANDROID__) || defined (__linux__)
    int         m_fileDescriptor;
    struct stat m_statBuffer;
#endif
};

#endif // PLUGIN_SRC_FILEMAPPING_H_
