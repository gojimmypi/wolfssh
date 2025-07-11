/* port.c
 *
 * Copyright (C) 2014-2024 wolfSSL Inc.
 *
 * This file is part of wolfSSH.
 *
 * wolfSSH is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * wolfSSH is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with wolfSSH.  If not, see <http://www.gnu.org/licenses/>.
 */


/*
 * The port module wraps standard C library functions with macros to
 * cover portablility issues when building in environments that rename
 * those functions. This module also provides local versions of some
 * standard C library functions that are missing on some platforms.
 */


#ifdef HAVE_CONFIG_H
    #include <config.h>
#endif


#include <wolfssh/port.h>
#if !defined(USE_WINDOWS_API) && !defined(FREESCALE_MQX)
    #include <stdio.h>
#endif

/*
Flags:
  WOLFSSH_LOCAL_PREAD_PWRITE
    Defined to use local implementations of pread() and pwrite(). Switched
    on automatically if pread() or pwrite() aren't found by configure.
*/


#if !defined(NO_FILESYSTEM) && !defined(WOLFSSH_USER_FILESYSTEM) && \
    !defined(WOLFSSH_ZEPHYR)
#if defined(MICROCHIP_MPLAB_HARMONY)
int wfopen(WFILE* f, const char* filename, SYS_FS_FILE_OPEN_ATTRIBUTES mode)
{
    if (f != NULL) {
        *f = SYS_FS_FileOpen(filename, mode);
        if (*f == WBADFILE) {
            WLOG(WS_LOG_SFTP, "Failed to open file %s", filename);
            return 1;
        }
        else {
            WLOG(WS_LOG_SFTP, "Opened file %s", filename);
            return 0;
        }
    }
    return 1;
}
#else
int wfopen(WFILE** f, const char* filename, const char* mode)
{
#ifdef USE_WINDOWS_API
    return fopen_s(f, filename, mode) != 0;
#elif defined(WOLFSSL_NUCLEUS)
    int m = WOLFSSH_O_CREAT;

    if (WSTRSTR(mode, "r") && WSTRSTR(mode, "w")) {
        m |= WOLFSSH_O_RDWR;
    }
    else {
        if (WSTRSTR(mode, "r")) {
            m |= WOLFSSH_O_RDONLY;
        }
        if (WSTRSTR(mode, "w")) {
            m |= WOLFSSH_O_WRONLY;
        }
    }

    if (filename != NULL && f != NULL) {
        if ((**f = WOPEN(ssh->fs, filename, m, 0)) < 0) {
            return **f;
        }

        /* fopen defaults to normal */
        if (NU_Set_Attributes(filename, 0) != NU_SUCCESS) {
            WCLOSE(ssh->fs, **f);
            return 1;
        }
        return 0;
    }
    else {
        return 1;
    }
#else
    if (f != NULL) {
        *f = fopen(filename, mode);
        return *f == NULL;
    }
    return 1;
#endif
}
#endif

/* If either pread() or pwrite() are missing, use the local versions. */
#if (defined(USE_OSE_API) || \
     !defined(HAVE_DECL_PREAD) || (HAVE_DECL_PREAD == 0) || \
     !defined(HAVE_DECL_PWRITE) || (HAVE_DECL_PWRITE == 0))
    #undef WOLFSSH_LOCAL_PREAD_PWRITE
    #define WOLFSSH_LOCAL_PREAD_PWRITE
#endif


#if (defined(WOLFSSH_SFTP) || defined(WOLFSSH_SCP)) && \
    !defined(NO_WOLFSSH_SERVER)

    #if defined(USE_WINDOWS_API) || defined(WOLFSSL_NUCLEUS) || \
        defined(FREESCALE_MQX) || defined(WOLFSSH_ZEPHYR)

        /* This is current inline in the source. */

    #elif defined(MICROCHIP_MPLAB_HARMONY)
        int wPwrite(WFD fd, unsigned char* buf, unsigned int sz,
                const unsigned int* shortOffset)
        {
            int ret;

            ret = (int)WFSEEK(NULL, &fd, shortOffset[0], SYS_FS_SEEK_SET);
            if (ret != -1) {
                ret = (int)WFWRITE(NULL, buf, 1, sz, &fd);
            }

            return ret;
        }

        int wPread(WFD fd, unsigned char* buf, unsigned int sz,
                const unsigned int* shortOffset)
        {
            int ret;

            ret = (int)WFSEEK(NULL, &fd, shortOffset[0], SYS_FS_SEEK_SET);
            if (ret != -1)
                ret = (int)WFREAD(NULL, buf, 1, sz, &fd);

            return ret;
        }

    #elif defined(WOLFSSH_LOCAL_PREAD_PWRITE)

        int wPwrite(WFD fd, unsigned char* buf, unsigned int sz,
                const unsigned int* shortOffset)
        {
            int ret;

            ret = (int)lseek(fd, shortOffset[0], SEEK_SET);
            if (ret != -1)
                ret = (int)write(fd, buf, sz);

            return ret;
        }

        int wPread(WFD fd, unsigned char* buf, unsigned int sz,
                const unsigned int* shortOffset)
        {
            int ret;

            ret = (int)lseek(fd, shortOffset[0], SEEK_SET);
            if (ret != -1)
                ret = (int)read(fd, buf, sz);

            return ret;
        }

    #else /* USE_WINDOWS_API WOLFSSH_LOCAL_PREAD_PWRITE */

        int wPwrite(WFD fd, unsigned char* buf, unsigned int sz,
                const unsigned int* shortOffset)
        {
            off_t offset = (off_t)shortOffset[0];

        #if SIZEOF_OFF_T == 8
            offset = ((off_t)shortOffset[1] << 32) | offset;
        #endif
            return (int)pwrite(fd, buf, sz, offset);
        }


        int wPread(WFD fd, unsigned char* buf, unsigned int sz,
                const unsigned int* shortOffset)
        {
            off_t offset = (off_t)shortOffset[0];

        #if SIZEOF_OFF_T == 8
            offset = ((off_t)shortOffset[1] << 32) | offset;
        #endif
            return (int)pread(fd, buf, sz, offset);
        }

    #endif /* USE_WINDOWS_API USE_OSE_API */
#endif /* WOLFSSH_SFTP WOLFSSH_SCP NO_WOLFSSH_SERVER */

#endif /* !NO_FILESYSTEM */


#if defined(USE_WINDOWS_API) && (defined(WOLFSSH_SFTP) || defined(WOLFSSH_SCP))

/*
 * SFTP paths all start with a leading root "/". Most Windows file routines
 * expect a drive letter when dealing with an absolute path. If the provided
 * path, f, is of the form "/C:...", adjust the pointer f to point to the "C",
 * and decrement the file path size, fSz, by one.
 *
 * @param f    pointer to a file name
 * @param fSz  size of f in bytes
 * @return     pointer to somewhere in f
 */
static const char* TrimFileName(const char* f, size_t* fSz)
{
    if (f != NULL && fSz != NULL && *fSz >= 3 && f[0] == '/' && f[2] == ':') {
        f++;
        (*fSz)--;
    }
    return f;
}

void* WS_CreateFileA(const char* fileName, unsigned long desiredAccess,
        unsigned long shareMode, unsigned long creationDisposition,
        unsigned long flags, void* heap)
{
    HANDLE fileHandle;
    wchar_t* unicodeFileName;
    size_t unicodeFileNameSz = 0;
    size_t returnSz = 0;
    size_t fileNameSz = 0;
    errno_t error;

    fileNameSz = WSTRLEN(fileName);
    fileName = TrimFileName(fileName, &fileNameSz);

    error = mbstowcs_s(&unicodeFileNameSz, NULL, 0, fileName, 0);
    if (error)
        return INVALID_HANDLE_VALUE;

    unicodeFileName = (wchar_t*)WMALLOC((unicodeFileNameSz+1)*sizeof(wchar_t),
            heap, PORT_DYNTYPE_STRING);
    if (unicodeFileName == NULL)
        return INVALID_HANDLE_VALUE;

    error = mbstowcs_s(&returnSz, unicodeFileName, unicodeFileNameSz,
        fileName, fileNameSz);

    if (!error)
        fileHandle = CreateFileW(unicodeFileName, desiredAccess, shareMode,
                NULL, creationDisposition, flags, NULL);

    WFREE(unicodeFileName, heap, PORT_DYNTYPE_STRING);

    return (void*)(error ? INVALID_HANDLE_VALUE : fileHandle);
}

void* WS_FindFirstFileA(const char* fileName,
        char* realFileName, size_t realFileNameSz, int* isDir, void* heap)
{
    HANDLE findHandle = NULL;
    WIN32_FIND_DATAW findFileData;
    wchar_t* unicodeFileName;
    size_t unicodeFileNameSz = 0;
    size_t returnSz = 0;
    size_t fileNameSz = 0;
    errno_t error;

    fileNameSz = WSTRLEN(fileName);
    fileName = TrimFileName(fileName, &fileNameSz);

    error = mbstowcs_s(&unicodeFileNameSz, NULL, 0, fileName, 0);
    if (error)
        return INVALID_HANDLE_VALUE;

    unicodeFileName = (wchar_t*)WMALLOC((unicodeFileNameSz+1)*sizeof(wchar_t),
            heap, PORT_DYNTYPE_STRING);
    if (unicodeFileName == NULL)
        return INVALID_HANDLE_VALUE;

    error = mbstowcs_s(&returnSz, unicodeFileName, unicodeFileNameSz,
        fileName, fileNameSz);

    if (!error)
        findHandle = FindFirstFileW(unicodeFileName, &findFileData);

    WFREE(unicodeFileName, heap, PORT_DYNTYPE_STRING);

    error = wcstombs_s(NULL, realFileName, realFileNameSz,
        findFileData.cFileName, realFileNameSz);

    if (isDir != NULL) {
        *isDir =
            (findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    }

    return (void*)findHandle;
}


int WS_FindNextFileA(void* findHandle,
        char* realFileName, size_t realFileNameSz)
{
    BOOL success;
    WIN32_FIND_DATAW findFileData;
    errno_t error = 0;

    success = FindNextFileW((HANDLE)findHandle, &findFileData);

    if (success) {
        error = wcstombs_s(NULL, realFileName, realFileNameSz,
            findFileData.cFileName, realFileNameSz);
    }

    return (success != 0) && (error == 0);
}


int WS_GetFileAttributesExA(const char* fileName, void* fileInfo, void* heap)
{
    BOOL success = 0;
    wchar_t* unicodeFileName;
    size_t unicodeFileNameSz = 0;
    size_t returnSz = 0;
    size_t fileNameSz = 0;
    errno_t error;

    fileNameSz = WSTRLEN(fileName);
    fileName = TrimFileName(fileName, &fileNameSz);

    error = mbstowcs_s(&unicodeFileNameSz, NULL, 0, fileName, 0);
    if (error != 0)
        return 0;

    unicodeFileName = (wchar_t*)WMALLOC((unicodeFileNameSz+1)*sizeof(wchar_t),
            heap, PORT_DYNTYPE_STRING);
    if (unicodeFileName == NULL)
        return 0;

    error = mbstowcs_s(&returnSz, unicodeFileName, unicodeFileNameSz,
        fileName, fileNameSz);

    if (error == 0) {
        success = GetFileAttributesExW(unicodeFileName,
                GetFileExInfoStandard, fileInfo);
    }

    WFREE(unicodeFileName, heap, PORT_DYNTYPE_STRING);

    return success != 0;
}


int WS_RemoveDirectoryA(const char* dirName, void* heap)
{
    BOOL success = 0;
    wchar_t* unicodeDirName;
    size_t unicodeDirNameSz = 0;
    size_t returnSz = 0;
    size_t dirNameSz = 0;
    errno_t error;

    dirNameSz = WSTRLEN(dirName);
    dirName = TrimFileName(dirName, &dirNameSz);

    error = mbstowcs_s(&unicodeDirNameSz, NULL, 0, dirName, 0);
    if (error != 0)
        return 0;

    unicodeDirName = (wchar_t*)WMALLOC((unicodeDirNameSz+1)*sizeof(wchar_t),
            heap, PORT_DYNTYPE_STRING);
    if (unicodeDirName == NULL)
        return 0;

    error = mbstowcs_s(&returnSz, unicodeDirName, unicodeDirNameSz,
        dirName, dirNameSz);

    if (error == 0) {
        success = RemoveDirectoryW(unicodeDirName);
    }

    WFREE(unicodeDirName, heap, PORT_DYNTYPE_STRING);

    return success != 0;
}


int WS_CreateDirectoryA(const char* dirName, void* heap)
{
    BOOL success = 0;
    wchar_t* unicodeDirName;
    size_t unicodeDirNameSz = 0;
    size_t returnSz = 0;
    size_t dirNameSz = 0;
    errno_t error;

    dirNameSz = WSTRLEN(dirName);
    dirName = TrimFileName(dirName, &dirNameSz);

    error = mbstowcs_s(&unicodeDirNameSz, NULL, 0, dirName, 0);
    if (error != 0)
        return 0;

    unicodeDirName = (wchar_t*)WMALLOC((unicodeDirNameSz+1)*sizeof(wchar_t),
            heap, PORT_DYNTYPE_STRING);
    if (unicodeDirName == NULL)
        return 0;

    error = mbstowcs_s(&returnSz, unicodeDirName, unicodeDirNameSz,
        dirName, dirNameSz);

    if (error == 0) {
        success = CreateDirectoryW(unicodeDirName, NULL);
    }

    WFREE(unicodeDirName, heap, PORT_DYNTYPE_STRING);

    return success != 0;
}


int WS_MoveFileA(const char* oldName, const char* newName, void* heap)
{
    BOOL success = 0;
    wchar_t* unicodeOldName;
    wchar_t* unicodeNewName;
    size_t unicodeOldNameSz = 0;
    size_t unicodeNewNameSz = 0;
    size_t oldNameSz = 0;
    size_t newNameSz = 0;
    size_t returnSz = 0;
    errno_t error;

    oldNameSz = WSTRLEN(oldName);
    oldName = TrimFileName(oldName, &oldNameSz);

    error = mbstowcs_s(&unicodeOldNameSz, NULL, 0, oldName, 0);
    if (error != 0)
        return 0;

    unicodeOldName = (wchar_t*)WMALLOC((unicodeOldNameSz+1)*sizeof(wchar_t),
            heap, PORT_DYNTYPE_STRING);
    if (unicodeOldName == NULL)
        return 0;

    error = mbstowcs_s(&returnSz, unicodeOldName, unicodeOldNameSz,
        oldName, oldNameSz);

    newNameSz = WSTRLEN(newName);
    newName = TrimFileName(newName, &newNameSz);

    error = mbstowcs_s(&unicodeNewNameSz, NULL, 0, newName, 0);
    if (error != 0)
        return 0;

    unicodeNewName = (wchar_t*)WMALLOC((unicodeNewNameSz+1)*sizeof(wchar_t),
            heap, PORT_DYNTYPE_STRING);
    if (unicodeNewName == NULL) {
        WFREE(unicodeOldName, heap, PORT_DYNTYPE_STRING);
        return 0;
    }

    error = mbstowcs_s(&returnSz, unicodeNewName, unicodeNewNameSz,
        newName, newNameSz);

    if (error == 0) {
        success = MoveFileW(unicodeOldName, unicodeNewName);
    }

    WFREE(unicodeOldName, heap, PORT_DYNTYPE_STRING);
    WFREE(unicodeNewName, heap, PORT_DYNTYPE_STRING);

    return success != 0;
}


int WS_DeleteFileA(const char* fileName, void* heap)
{
    BOOL success = 0;
    wchar_t* unicodeFileName;
    size_t unicodeFileNameSz = 0;
    size_t returnSz = 0;
    size_t fileNameSz = 0;
    errno_t error;

    fileNameSz = WSTRLEN(fileName);
    fileName = TrimFileName(fileName, &fileNameSz);

    error = mbstowcs_s(&unicodeFileNameSz, NULL, 0, fileName, 0);
    if (error != 0)
        return 0;

    unicodeFileName = (wchar_t*)WMALLOC((unicodeFileNameSz+1)*sizeof(wchar_t),
            heap, PORT_DYNTYPE_STRING);
    if (unicodeFileName == NULL)
        return 0;

    error = mbstowcs_s(&returnSz, unicodeFileName, unicodeFileNameSz,
        fileName, fileNameSz);

    if (error == 0) {
        success = DeleteFileW(unicodeFileName);
    }

    WFREE(unicodeFileName, heap, PORT_DYNTYPE_STRING);

    return success != 0;
}


#endif /* USE_WINDOWS_API WOLFSSH_SFTP WOLFSSH_SCP */

#if !defined(NO_FILESYSTEM) && \
    defined(WOLFSSH_ZEPHYR) && (defined(WOLFSSH_SFTP) || defined(WOLFSSH_SCP))

int wssh_z_fstat(const char *p, struct fs_dirent *b)
{
    size_t p_len;

    if (p == NULL || b == NULL)
        return -1;

    p_len = WSTRLEN(p);
    /* Detect if origin directory when it ends in ':' or ':/' */
    if (p_len >= 3 && (p[p_len-1] == ':' ||
            (p[p_len-1] == '/' && p[p_len-2] == ':'))) {
        b->type = FS_DIR_ENTRY_DIR;
        b->size = 0;
        b->name[0] = '/';
        b->name[1] = '\0';
        return 0;
    }
    else
        return fs_stat(p, b);
}

int z_fs_chdir(const char *path)
{
    /* Just make sure that the path exists and is a directory */
    struct fs_dirent dir;
    int ret;

    ret = wssh_z_fstat(path, &dir);
    if (ret != 0 || dir.type != FS_DIR_ENTRY_DIR)
        ret = -1;
    return ret;
}

static struct {
    byte open:1;
    WFILE zfp;
} z_fds[WOLFSSH_MAX_DESCIPRTORS];
static wolfSSL_Mutex z_fds_mutex;
static int z_fds_setup = 0;

int wssh_z_fds_init(void)
{
    int ret = 0;
    if (!z_fds_setup) {
        WMEMSET(z_fds, 0, sizeof(z_fds));
        ret = wc_InitMutex(&z_fds_mutex);
        if (ret == 0)
            z_fds_setup = 1;
    }
    return ret;
}

int wssh_z_fds_cleanup(void)
{
    int ret = 0;
    if (z_fds_setup) {
        WMEMSET(z_fds, 0, sizeof(z_fds));
        ret = wc_FreeMutex(&z_fds_mutex);
        z_fds_setup = 0;
    }
    return ret;
}

WFD wssh_z_open(const char *p, int f)
{
    WFD ret = -1;
    if (p != NULL) {
        if (wc_LockMutex(&z_fds_mutex) == 0) {
            WFD idx = 0;
            /* find a free fd */
            while(idx < WOLFSSH_MAX_DESCIPRTORS && z_fds[idx].open)
                idx++;
            if (idx < WOLFSSH_MAX_DESCIPRTORS) {
                /* found a free fd */
                fs_file_t_init(&z_fds[idx].zfp);
                if (fs_open(&z_fds[idx].zfp, p, f) == 0) {
                    z_fds[idx].open = 1;
                    ret = idx;
                }
            }
            wc_UnLockMutex(&z_fds_mutex);
        }
    }
    return ret;
}

int wssh_z_close(WFD fd)
{
    int ret = -1;
    if (fd >= 0 && fd < WOLFSSH_MAX_DESCIPRTORS) {
        if (wc_LockMutex(&z_fds_mutex) == 0) {
            if (z_fds[fd].open) {
                z_fds[fd].open = 0;
                if (fs_close(&z_fds[fd].zfp) == 0)
                    ret = 0;
            }
            wc_UnLockMutex(&z_fds_mutex);
        }
    }
    return ret;
}

int wPwrite(WFD fd, unsigned char* buf, unsigned int sz,
        const unsigned int* shortOffset)
{
    int ret = -1;
    if (fd >= 0 && fd < WOLFSSH_MAX_DESCIPRTORS) {
        if (wc_LockMutex(&z_fds_mutex) == 0) {
            if (z_fds[fd].open) {
                const word32* offset = (const word32*)shortOffset;
                if (fs_seek(&z_fds[fd].zfp, offset[0], FS_SEEK_SET) == 0)
                    ret = fs_write(&z_fds[fd].zfp, buf, sz);
            }
            wc_UnLockMutex(&z_fds_mutex);
        }
    }
    return ret;
}

int wPread(WFD fd, unsigned char* buf, unsigned int sz,
        const unsigned int* shortOffset)
{
    int ret = -1;
    if (fd >= 0 && fd < WOLFSSH_MAX_DESCIPRTORS) {
        if (wc_LockMutex(&z_fds_mutex) == 0) {
            if (z_fds[fd].open) {
                const word32* offset = (const word32*)shortOffset;
                if (fs_seek(&z_fds[fd].zfp, offset[0], FS_SEEK_SET) == 0)
                    ret = fs_read(&z_fds[fd].zfp, buf, sz);
            }
            wc_UnLockMutex(&z_fds_mutex);
        }
    }
    return ret;
}

#endif

#if !defined(NO_FILESYSTEM) && !defined(WOLFSSH_USER_FILESYSTEM)
#if defined(MICROCHIP_MPLAB_HARMONY)
int wChmod(const char *path, int mode)
{
    SYS_FS_RESULT ret;
    SYS_FS_FILE_DIR_ATTR attr = 0;

    /* mode is the octal value i.e 666 is 0x1B6 */
    if ((mode & 0x180) != 0x180) { /* not octal 6XX read only */
        attr |= SYS_FS_ATTR_RDO;
    }

    /* toggle the read only attribute */
    ret = SYS_FS_FileDirectoryModeSet(path, attr, SYS_FS_ATTR_RDO);
    if (ret != SYS_FS_RES_SUCCESS) {
        WLOG(WS_LOG_SFTP, "Failed to set file/dir mode");
        return -1;
    }
    return 0;
}
#endif
#endif /* NO_FILESYSTEM */
#ifndef WSTRING_USER

char* wstrdup(const char* s1, void* heap, int type)
{
    char* s2 = NULL;

    if (s1 != NULL) {
        unsigned int sz;
        sz = (unsigned int)WSTRLEN(s1) + 1;
        s2 = (char*)WMALLOC(sz, heap, type);
        if (s2 != NULL)
            WSTRNCPY(s2, (const char*)s1, sz);
    }
    return s2;
}


char* wstrnstr(const char* s1, const char* s2, unsigned int n)
{
    unsigned int s2_len = (unsigned int)WSTRLEN(s2);

    if (s2_len == 0)
        return (char*)s1;

    while (n >= s2_len && s1[0]) {
        if (s1[0] == s2[0])
            if (WMEMCMP(s1, s2, s2_len) == 0)
                return (char*)s1;
        s1++;
        n--;
    }

    return NULL;
}


/* Returns s1 if successful. Returns NULL if unsuccessful.
 * Copies the characters string s2 onto the end of s1. n is the size of the
 * buffer s1 is stored in. Returns NULL if s2 is too large to fit onto the
 * end of s1 including a null terminator. */
char* wstrncat(char* s1, const char* s2, size_t n)
{
    size_t freeSpace = n - strlen(s1) - 1;

    if (freeSpace >= strlen(s2)) {
        #ifndef USE_WINDOWS_API
            strncat(s1, s2, freeSpace);
        #else
            strncat_s(s1, n, s2, freeSpace);
        #endif
        return s1;
    }

    return NULL;
}

#endif /* WSTRING_USER */
