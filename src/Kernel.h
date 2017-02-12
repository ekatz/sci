#ifndef KERNEL_H
#define KERNEL_H

#include "List.h"

typedef struct KNode {
    Node     link;
    intptr_t nVal;
} KNode;

typedef uintptr_t *kArgs;
typedef void (*kFunc)(kArgs);

#define arg(n)   (*(args + (n)))
#define kArgc    ((uint)arg(0))
#define argList  kArgs args
#define argCount kArgc

uintptr_t KLoad(argList);
void      KUnLoad(argList);
void      KLock(argList);
uintptr_t KDisposeScript(argList);
uintptr_t KClone(argList);
void      KDisposeClone(argList);
uintptr_t KIsObject(argList);
uintptr_t KRespondsTo(argList);
void      KDrawPic(argList);
void      KShow(argList);
uintptr_t KPicNotValid(argList);

void      KSetNowSeen(argList);
uintptr_t KNumLoops(argList);
uintptr_t KNumCels(argList);
uintptr_t KCelWide(argList);
uintptr_t KCelHigh(argList);
void      KDrawCel(argList);
uintptr_t KIsItSkip(argList);
void      KAnimate(argList);
void      KAddToPic(argList);
void      KShakeScreen(argList);
void      KShiftScreen(argList);
void      KDrawControl(argList);
void      KHiliteControl(argList);
uintptr_t KEditControl(argList);
void      KTextSize(argList);
uintptr_t KDisplay(argList);
uintptr_t KGetEvent(argList);
uintptr_t KNewWindow(argList);
uintptr_t KGetPort(argList);
void      KSetPort(argList);
void      KDisposeWindow(argList);
void      KGlobalToLocal(argList);
void      KLocalToGlobal(argList);
uintptr_t KMapKeyToDir(argList);
void      KParse(argList);
void      KSaid(argList);
void      KSetSynonyms(argList);
uintptr_t KHaveMouse(argList);
void      KSetCursor(argList);
void      KJoystick(argList);
uintptr_t KNewList(argList);
void      KDisposeList(argList);
uintptr_t KNewNode(argList);
uintptr_t KFirstNode(argList);
uintptr_t KLastNode(argList);
uintptr_t KEmptyList(argList);
uintptr_t KNextNode(argList);
uintptr_t KPrevNode(argList);
uintptr_t KNodeValue(argList);
uintptr_t KAddAfter(argList);
uintptr_t KAddToFront(argList);
uintptr_t KAddToEnd(argList);
uintptr_t KFindKey(argList);
uintptr_t KDeleteKey(argList);
uintptr_t KRandom(argList);
uintptr_t KAbs(argList);
uintptr_t KSqrt(argList);
uintptr_t KGetAngle(argList);
uintptr_t KGetDistance(argList);
uintptr_t KWait(argList);
uintptr_t KGetTime(argList);
uintptr_t KStrEnd(argList);
uintptr_t KStrCat(argList);
uintptr_t KStrCmp(argList);
uintptr_t KStrLen(argList);
uintptr_t KStrCpy(argList);
uintptr_t KFormat(argList);
uintptr_t KStrAt(argList);
uintptr_t KStrSplit(argList);
uintptr_t KGetCWD(argList);
uintptr_t KGetFarText(argList);
uintptr_t KReadNumber(argList);
uintptr_t KOnControl(argList);
void      KAvoidPath(argList);
void      KSetDebug(argList);
void      KInspectObj(argList);
void      KShowSends(argList);
void      KShowObjs(argList);
void      KShowFree(argList);
void      KMemoryInfo(argList);
void      KStackUsage(argList);
void      KCheckFreeSpace(argList);
uintptr_t KValidPath(argList);
void      KProfiler(argList);
uintptr_t KDeviceInfo(argList);
void      KFlushResources(argList);
void      KMemorySegment(argList);
void      KIntersections(argList);
void      KMemory(argList);
void      KListOps(argList);
uintptr_t KCoordPri(argList);
uintptr_t KSinMult(argList);
uintptr_t KCosMult(argList);
uintptr_t KSinDiv(argList);
uintptr_t KCosDiv(argList);
uintptr_t KATan(argList);
uintptr_t KFileIO(argList);
void      KSort(argList);
void      KMessage(argList);

#endif // KERNEL_H
