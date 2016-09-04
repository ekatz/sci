#ifndef KERNEL_H
#define KERNEL_H

#include "Types.h"

typedef uintptr_t *kArgs;
typedef void (*kFunc)(kArgs);

#define arg(n)   (*(args + (n)))
#define kArgc    ((uint)arg(0))
#define argList  kArgs args
#define argCount kArgc

void KLoad(argList);
void KUnLoad(argList);
void KLock(argList);
void KScriptID(argList);
void KDisposeScript(argList);
void KClone(argList);
void KDisposeClone(argList);
void KIsObject(argList);
void KRespondsTo(argList);
void KDrawPic(argList);
void KShow(argList);
void KPicNotValid(argList);

void KSetNowSeen(argList);
void KNumLoops(argList);
void KNumCels(argList);
void KCelWide(argList);
void KCelHigh(argList);
void KDrawCel(argList);
void KIsItSkip(argList);
void KAnimate(argList);
void KAddToPic(argList);
void KShakeScreen(argList);
void KShiftScreen(argList);
void KDrawControl(argList);
void KHiliteControl(argList);
void KEditControl(argList);
void KTextSize(argList);
void KDisplay(argList);
void KGetEvent(argList);
void KNewWindow(argList);
void KGetPort(argList);
void KSetPort(argList);
void KDisposeWindow(argList);
void KGlobalToLocal(argList);
void KLocalToGlobal(argList);
void KMapKeyToDir(argList);
void KParse(argList);
void KSaid(argList);
void KSetSynonyms(argList);
void KHaveMouse(argList);
void KSetCursor(argList);
void KJoystick(argList);
void KGetSaveFiles(argList);
void KSaveGame(argList);
void KRestoreGame(argList);
void KRestartGame(argList);
void KGameIsRestarting(argList);
void KDoSync(argList);
void KNewList(argList);
void KDisposeList(argList);
void KNewNode(argList);
void KFirstNode(argList);
void KLastNode(argList);
void KEmptyList(argList);
void KNextNode(argList);
void KPrevNode(argList);
void KNodeValue(argList);
void KAddAfter(argList);
void KAddToFront(argList);
void KAddToEnd(argList);
void KFindKey(argList);
void KDeleteKey(argList);
void KRandom(argList);
void KAbs(argList);
void KSqrt(argList);
void KGetAngle(argList);
void KGetDistance(argList);
void KWait(argList);
void KGetTime(argList);
void KStrEnd(argList);
void KStrCat(argList);
void KStrCmp(argList);
void KStrLen(argList);
void KStrCpy(argList);
void KFormat(argList);
void KStrAt(argList);
void KStrSplit(argList);
void KGetCWD(argList);
void KGetFarText(argList);
void KReadNumber(argList);
void KBaseSetter(argList);
void KDirLoop(argList);
void KCantBeHere(argList);
void KOnControl(argList);
void KInitBresen(argList);
void KDoBresen(argList);
void KDoAvoider(argList);
void KAvoidPath(argList);
void KSetJump(argList);
void KSetDebug(argList);
void KInspectObj(argList);
void KShowSends(argList);
void KShowObjs(argList);
void KShowFree(argList);
void KMemoryInfo(argList);
void KStackUsage(argList);
void KCheckFreeSpace(argList);
void KValidPath(argList);
void KProfiler(argList);
void KDeviceInfo(argList);
void KGetSaveDir(argList);
void KCheckSaveGame(argList);
void KFlushResources(argList);
void KMemorySegment(argList);
void KIntersections(argList);
void KMemory(argList);
void KListOps(argList);
void KCoordPri(argList);
void KSinMult(argList);
void KCosMult(argList);
void KSinDiv(argList);
void KCosDiv(argList);
void KATan(argList);
void KFileIO(argList);
void KSort(argList);
void KMessage(argList);

#endif // KERNEL_H
