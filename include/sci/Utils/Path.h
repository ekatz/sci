#ifndef SCI_UTILS_PATH_H
#define SCI_UTILS_PATH_H

#include "sci/Utils/Types.h"

void InitPath(const char *resDir, const char *saveDir);

void DosToLocalPath(char *localPath, const char *dosPath, bool homeRes);
void LocalToDosPath(char *dosPath, const char *localPath);

const char *GetSaveDosDir();

// Make sure that the last character of the directory is NOT a '/' or '\'.
char *CleanDir(char *dir);

#endif // SCI_UTILS_PATH_H
