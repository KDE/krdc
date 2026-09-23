/*
    SPDX-FileCopyrightText: 2024 KRDC Contributors

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <array>
#include <cstdint>

/*
 * Mapping from Linux evdev keycodes to PC XT set-1 scancodes.
 * For E0-extended keys the value has bit 8 set (i.e. value >= 0x100),
 * matching the encoding expected by spice_inputs_channel_key_press():
 *   "For scancodes with an 0xe0 prefix, drop the prefix and OR with 0x100."
 * Unmapped entries are 0.
 */

static constexpr auto buildEvdevToXtKbdMap()
{
    std::array<uint16_t, 525> map{};

    // 0x01-0x53: evdev == XT scancode (original PC/XT set, identity mapping)
    for (uint16_t i = 0x01; i <= 0x53; ++i)
        map[i] = i;

    // 0x54+: exceptions — either remapped or E0-extended (bit 8 set)
    map[0x55] = 0x076; // KEY_ZENKAKUHANKAKU
    map[0x56] = 0x056; // KEY_102ND
    map[0x57] = 0x057; // KEY_F11
    map[0x58] = 0x058; // KEY_F12
    map[0x59] = 0x073; // KEY_RO
    map[0x5a] = 0x078; // KEY_KATAKANA
    map[0x5b] = 0x077; // KEY_HIRAGANA
    map[0x5c] = 0x079; // KEY_HENKAN
    map[0x5d] = 0x070; // KEY_KATAKANAHIRAGANA
    map[0x5e] = 0x07b; // KEY_MUHENKAN
    map[0x5f] = 0x05c; // KEY_KPJPCOMMA
    map[0x60] = 0x11c; // KEY_KPENTER       (E0-extended)
    map[0x61] = 0x11d; // KEY_RIGHTCTRL     (E0-extended)
    map[0x62] = 0x135; // KEY_KPSLASH       (E0-extended)
    map[0x63] = 0x054; // KEY_SYSRQ
    map[0x64] = 0x138; // KEY_RIGHTALT      (E0-extended)
    map[0x65] = 0x05b; // KEY_LINEFEED
    map[0x66] = 0x147; // KEY_HOME          (E0-extended)
    map[0x67] = 0x148; // KEY_UP            (E0-extended)
    map[0x68] = 0x149; // KEY_PAGEUP        (E0-extended)
    map[0x69] = 0x14b; // KEY_LEFT          (E0-extended)
    map[0x6a] = 0x14d; // KEY_RIGHT         (E0-extended)
    map[0x6b] = 0x14f; // KEY_END           (E0-extended)
    map[0x6c] = 0x150; // KEY_DOWN          (E0-extended)
    map[0x6d] = 0x151; // KEY_PAGEDOWN      (E0-extended)
    map[0x6e] = 0x152; // KEY_INSERT        (E0-extended)
    map[0x6f] = 0x153; // KEY_DELETE        (E0-extended)
    map[0x70] = 0x16f; // KEY_MACRO
    map[0x71] = 0x120; // KEY_MUTE          (E0-extended)
    map[0x72] = 0x12e; // KEY_VOLUMEDOWN    (E0-extended)
    map[0x73] = 0x130; // KEY_VOLUMEUP      (E0-extended)
    map[0x74] = 0x15e; // KEY_POWER         (E0-extended)
    map[0x75] = 0x059; // KEY_KPEQUAL
    map[0x76] = 0x14e; // KEY_KPPLUSMINUS   (E0-extended)
    map[0x77] = 0x146; // KEY_PAUSE
    map[0x78] = 0x10b; // KEY_SCALE         (E0-extended)
    map[0x79] = 0x07e; // KEY_KPCOMMA
    map[0x7a] = 0x0f2; // KEY_HANGEUL
    map[0x7b] = 0x0f1; // KEY_HANJA
    map[0x7c] = 0x07d; // KEY_YEN
    map[0x7d] = 0x15b; // KEY_LEFTMETA      (E0-extended)
    map[0x7e] = 0x15c; // KEY_RIGHTMETA     (E0-extended)
    map[0x7f] = 0x15d; // KEY_COMPOSE       (E0-extended)
    map[0x80] = 0x168; // KEY_STOP
    map[0x81] = 0x105; // KEY_AGAIN
    map[0x82] = 0x106; // KEY_PROPS
    map[0x83] = 0x107; // KEY_UNDO
    map[0x84] = 0x10c; // KEY_FRONT
    map[0x85] = 0x178; // KEY_COPY
    map[0x86] = 0x064; // KEY_OPEN
    map[0x87] = 0x065; // KEY_PASTE
    map[0x88] = 0x141; // KEY_FIND
    map[0x89] = 0x13c; // KEY_CUT
    map[0x8a] = 0x175; // KEY_HELP
    map[0x8b] = 0x11e; // KEY_MENU          (E0-extended)
    map[0x8c] = 0x121; // KEY_CALC          (E0-extended)
    map[0x8d] = 0x066; // KEY_SETUP
    map[0x8e] = 0x15f; // KEY_SLEEP         (E0-extended)
    map[0x8f] = 0x163; // KEY_WAKEUP        (E0-extended)
    map[0x90] = 0x067; // KEY_FILE
    map[0x91] = 0x068; // KEY_SENDFILE
    map[0x92] = 0x069; // KEY_DELETEFILE
    map[0x93] = 0x113; // KEY_XFER
    map[0x94] = 0x11f; // KEY_PROG1         (E0-extended)
    map[0x95] = 0x117; // KEY_PROG2
    map[0x96] = 0x102; // KEY_WWW
    map[0x97] = 0x06a; // KEY_MSDOS
    map[0x98] = 0x112; // KEY_SCREENLOCK
    map[0x99] = 0x06b; // KEY_DIRECTION
    map[0x9a] = 0x126; // KEY_CYCLEWINDOWS
    map[0x9b] = 0x16c; // KEY_MAIL
    map[0x9c] = 0x166; // KEY_BOOKMARKS
    map[0x9d] = 0x16b; // KEY_COMPUTER
    map[0x9e] = 0x16a; // KEY_BACK
    map[0x9f] = 0x169; // KEY_FORWARD
    map[0xa0] = 0x123; // KEY_CLOSECD
    map[0xa1] = 0x06c; // KEY_EJECTCD
    map[0xa2] = 0x17d; // KEY_EJECTCLOSECD
    map[0xa3] = 0x119; // KEY_NEXTSONG
    map[0xa4] = 0x122; // KEY_PLAYPAUSE
    map[0xa5] = 0x110; // KEY_PREVIOUSSONG
    map[0xa6] = 0x124; // KEY_STOPCD
    map[0xa7] = 0x131; // KEY_RECORD
    map[0xa8] = 0x118; // KEY_REWIND
    map[0xa9] = 0x063; // KEY_PHONE
    map[0xab] = 0x101; // KEY_CONFIG
    map[0xac] = 0x132; // KEY_HOMEPAGE
    map[0xad] = 0x167; // KEY_REFRESH
    map[0xb0] = 0x108; // KEY_EDIT
    map[0xb1] = 0x075; // KEY_SCROLLUP
    map[0xb2] = 0x10f; // KEY_SCROLLDOWN
    map[0xb3] = 0x176; // KEY_KPLEFTPAREN
    map[0xb4] = 0x17b; // KEY_KPRIGHTPAREN
    map[0xb5] = 0x109; // KEY_NEW
    map[0xb6] = 0x10a; // KEY_REDO
    map[0xb7] = 0x05d; // KEY_F13
    map[0xb8] = 0x05e; // KEY_F14
    map[0xb9] = 0x05f; // KEY_F15
    map[0xba] = 0x055; // KEY_F16
    map[0xbb] = 0x103; // KEY_F17
    map[0xbc] = 0x177; // KEY_F18
    map[0xbd] = 0x104; // KEY_F19
    map[0xbe] = 0x05a; // KEY_F20
    map[0xbf] = 0x074; // KEY_F21
    map[0xc0] = 0x179; // KEY_F22
    map[0xc1] = 0x06d; // KEY_F23
    map[0xc2] = 0x06f; // KEY_F24
    map[0xc3] = 0x115;
    map[0xc4] = 0x116;
    map[0xc5] = 0x11a;
    map[0xc6] = 0x11b;
    map[0xc7] = 0x127;
    map[0xc8] = 0x128; // KEY_PLAYCD
    map[0xc9] = 0x129; // KEY_PAUSECD
    map[0xca] = 0x12b; // KEY_PROG3
    map[0xcb] = 0x12c; // KEY_PROG4
    map[0xcc] = 0x12d; // KEY_DASHBOARD
    map[0xcd] = 0x125; // KEY_SUSPEND
    map[0xce] = 0x12f; // KEY_CLOSE
    map[0xcf] = 0x133; // KEY_PLAY
    map[0xd0] = 0x134; // KEY_FASTFORWARD
    map[0xd1] = 0x136; // KEY_BASSBOOST
    map[0xd2] = 0x139; // KEY_PRINT
    map[0xd3] = 0x13a; // KEY_HP
    map[0xd4] = 0x13b; // KEY_CAMERA
    map[0xd5] = 0x13d; // KEY_SOUND
    map[0xd6] = 0x13e; // KEY_QUESTION
    map[0xd7] = 0x13f; // KEY_EMAIL
    map[0xd8] = 0x140; // KEY_CHAT
    map[0xd9] = 0x165; // KEY_SEARCH
    map[0xda] = 0x142; // KEY_CONNECT
    map[0xdb] = 0x143; // KEY_FINANCE
    map[0xdc] = 0x144; // KEY_SPORT
    map[0xdd] = 0x145; // KEY_SHOP
    map[0xde] = 0x114; // KEY_ALTERASE
    map[0xdf] = 0x14a; // KEY_CANCEL
    map[0xe0] = 0x14c; // KEY_BRIGHTNESSDOWN
    map[0xe1] = 0x154; // KEY_BRIGHTNESSUP
    map[0xe2] = 0x16d; // KEY_MEDIA
    map[0xe3] = 0x156; // KEY_SWITCHVIDEOMODE
    map[0xe4] = 0x157; // KEY_KBDILLUMTOGGLE
    map[0xe5] = 0x158; // KEY_KBDILLUMDOWN
    map[0xe6] = 0x159; // KEY_KBDILLUMUP
    map[0xe7] = 0x15a; // KEY_SEND
    map[0xe8] = 0x164; // KEY_REPLY
    map[0xe9] = 0x10e; // KEY_FORWARDMAIL
    map[0xea] = 0x155; // KEY_SAVE
    map[0xeb] = 0x170; // KEY_DOCUMENTS
    map[0xec] = 0x171; // KEY_BATTERY
    map[0xed] = 0x172; // KEY_BLUETOOTH
    map[0xee] = 0x173; // KEY_WLAN
    map[0xef] = 0x174; // KEY_UWB

    return map;
}

static constexpr auto evdevToXtKbdMap = buildEvdevToXtKbdMap();
