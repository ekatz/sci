#ifndef SELECTOR_H
#define SELECTOR_H

enum objOffsets {
    actX,
    actY,
    actSignal,
    actView,
    actLoop,
    actCel,
    actPri,
    actLS,
    actNS,
    actBR,
    actBRBottom,
    actUB,
    actXStep,
    actYStep,
    actLooper,
    actHeading,
    actMover,
    actMoveSpeed,
    actIllegalBits,
    motClient,
    motX,
    motY,
    motDX,
    motDY,
    motI1,
    motI2,
    motDI,
    motIncr,
    motXAxis,
    motMoveCnt,
    motXLast,
    motYLast,
    avClient,
    avHeading,
    jmpXStep,
    jmpYStep,
    evX,
    evY,
    evType,
    evMsg,
    evMod,
    evClaimed,
    sndBLANK1,
    sndBLANK2,
    sndBLANK3,
    sndBLANK4,
    sndBLANK5,
    actXLast,
    actYLast,
    actZ,
    syncTime,
    syncCue,
    sndBLANK6,
    sndBLANK7,
    sndBLANK8,
    OBJOFSSIZE
};

#define s_y              3
#define s_x              4
#define s_view           5
#define s_loop           6
#define s_cel            7
#define s_underBits      8
#define s_nowSeen        9
#define s_nowSeenT       s_nowSeen
#define s_nowSeenL       10
#define s_nowSeenB       11
#define s_nowSeenBottom  s_nowSeenB
#define s_nowSeenR       12
#define s_nsLeft         s_nowSeenL
#define s_nsRight        s_nowSeenR
#define s_nsTop          s_nowSeenT
#define s_nsBottom       s_nowSeenB
#define s_lastSeen       13
#define s_lastSeenL      14
#define s_lastSeenBottom 15
#define s_lastSeenR      16
#define s_signal         17
#define s_illegalBits    18
#define s_baseRect       19
#define s_baseRectT      s_baseRect
#define s_baseRectL      20
#define s_baseRectB      21
#define s_baseRectR      22
#define s_brLeft         s_baseRectL
#define s_brRight        s_baseRectR
#define s_brTop          s_baseRectT
#define s_brBottom       s_baseRectB
#define s_name           23
#define s_key            24
#define s_time           25
#define s_text           26
#define s_elements       27

#define s_play           42

#define s_doit           60
#define s_isBlocked      61
#define s_looper         62
#define s_priority       63
#define s_modifiers      64
#define s_replay         65

#define s_said           75
#define s_claimed        76
#define s_value          77
#define s_save           78
#define s_restore        79
#define s_title          80
#define s_button         81
#define s_icon           82
#define s_draw           83
#define s_delete         84
#define s_z              85
#define s_parseLang      86
#define s_printLang      87
#define s_subtitleLang   88
#define s_size           89
#define s_points         90
#define s_palette        91
#define s_dataInc        92
#define s_handle         93
#define s_min            94
#define s_sec            95
#define s_frame          96
#define s_vol            97
#define s_BLANK          98
#define s_perform        99

#endif // SELECTOR_H
