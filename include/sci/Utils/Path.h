#ifndef PATH_H
#define PATH_H

#include "Types.h"

void InitPath(const char *resDir, const char *saveDir);

void DosToLocalPath(char *localPath, const char *dosPath, bool homeRes);
void LocalToDosPath(char *dosPath, const char *localPath);

const char *GetSaveDosDir();

// Make sure that the last character of the directory is NOT a '/' or '\'.
char *CleanDir(char *dir);

#endif // PATH_H
