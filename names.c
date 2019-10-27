#include <linux/input-event-codes.h>

#include "names.h"

#include "khash.h"
// define hashmap types for name-to-value and value-to-name

KHASH_MAP_INIT_STR(n2v, uint16_t);
KHASH_MAP_INIT_INT(v2n, const char *);

static khash_t(n2v) *n2v = NULL;
static khash_t(v2n) *v2n = NULL;

// #define KEY_MIN_INTERESTING	KEY_MUTE
// #define KEY_MAX			0x2ff
// #define KEY_CNT			(KEY_MAX+1)

#define ADD_NAME(name, value) \
    k = kh_put(n2v, n2v, name, &ret); \
    if(ret == -1){ \
        names_free(); \
        return -1; \
    } \
    kh_val(n2v, k) = value

#define ADD_VALUE(value, name) \
    k = kh_put(v2n, v2n, value, &ret); \
    if(ret == -1){ \
        names_free(); \
        return -1; \
    } \
    kh_val(v2n, k) = name

#define ADD_ALIAS(name, value) \
    ADD_NAME(name, value); \

#define ADD_KEY(name, value) \
    ADD_NAME(name, value); \
    ADD_VALUE(value, name) \

int names_init(void){
    n2v = kh_init(n2v);
    if(!n2v) return -1;

    v2n = kh_init(v2n);
    if(!v2n){
        kh_destroy(n2v, n2v);
        return -1;
    }

    int ret;
    khiter_t k;

    /* don't include "KEY_RESERVED" so that get_input_value() can return 0 for
       "not found" */
    // ADD_KEY("KEY_RESERVED", 0);

    ADD_KEY("KEY_ESC", 1);
    ADD_KEY("KEY_1", 2);
    ADD_KEY("KEY_2", 3);
    ADD_KEY("KEY_3", 4);
    ADD_KEY("KEY_4", 5);
    ADD_KEY("KEY_5", 6);
    ADD_KEY("KEY_6", 7);
    ADD_KEY("KEY_7", 8);
    ADD_KEY("KEY_8", 9);
    ADD_KEY("KEY_9", 10);
    ADD_KEY("KEY_0", 11);
    ADD_KEY("KEY_MINUS", 12);
    ADD_KEY("KEY_EQUAL", 13);
    ADD_KEY("KEY_BACKSPACE", 14);
    ADD_KEY("KEY_TAB", 15);
    ADD_KEY("KEY_Q", 16);
    ADD_KEY("KEY_W", 17);
    ADD_KEY("KEY_E", 18);
    ADD_KEY("KEY_R", 19);
    ADD_KEY("KEY_T", 20);
    ADD_KEY("KEY_Y", 21);
    ADD_KEY("KEY_U", 22);
    ADD_KEY("KEY_I", 23);
    ADD_KEY("KEY_O", 24);
    ADD_KEY("KEY_P", 25);
    ADD_KEY("KEY_LEFTBRACE", 26);
    ADD_KEY("KEY_RIGHTBRACE", 27);
    ADD_KEY("KEY_ENTER", 28);
    ADD_KEY("KEY_LEFTCTRL", 29);
    ADD_KEY("KEY_A", 30);
    ADD_KEY("KEY_S", 31);
    ADD_KEY("KEY_D", 32);
    ADD_KEY("KEY_F", 33);
    ADD_KEY("KEY_G", 34);
    ADD_KEY("KEY_H", 35);
    ADD_KEY("KEY_J", 36);
    ADD_KEY("KEY_K", 37);
    ADD_KEY("KEY_L", 38);
    ADD_KEY("KEY_SEMICOLON", 39);
    ADD_KEY("KEY_APOSTROPHE", 40);
    ADD_KEY("KEY_GRAVE", 41);
    ADD_KEY("KEY_LEFTSHIFT", 42);
    ADD_KEY("KEY_BACKSLASH", 43);
    ADD_KEY("KEY_Z", 44);
    ADD_KEY("KEY_X", 45);
    ADD_KEY("KEY_C", 46);
    ADD_KEY("KEY_V", 47);
    ADD_KEY("KEY_B", 48);
    ADD_KEY("KEY_N", 49);
    ADD_KEY("KEY_M", 50);
    ADD_KEY("KEY_COMMA", 51);
    ADD_KEY("KEY_DOT", 52);
    ADD_KEY("KEY_SLASH", 53);
    ADD_KEY("KEY_RIGHTSHIFT", 54);
    ADD_KEY("KEY_KPASTERISK", 55);
    ADD_KEY("KEY_LEFTALT", 56);
    ADD_KEY("KEY_SPACE", 57);
    ADD_KEY("KEY_CAPSLOCK", 58);
    ADD_KEY("KEY_F1", 59);
    ADD_KEY("KEY_F2", 60);
    ADD_KEY("KEY_F3", 61);
    ADD_KEY("KEY_F4", 62);
    ADD_KEY("KEY_F5", 63);
    ADD_KEY("KEY_F6", 64);
    ADD_KEY("KEY_F7", 65);
    ADD_KEY("KEY_F8", 66);
    ADD_KEY("KEY_F9", 67);
    ADD_KEY("KEY_F10", 68);
    ADD_KEY("KEY_NUMLOCK", 69);
    ADD_KEY("KEY_SCROLLLOCK", 70);
    ADD_KEY("KEY_KP7", 71);
    ADD_KEY("KEY_KP8", 72);
    ADD_KEY("KEY_KP9", 73);
    ADD_KEY("KEY_KPMINUS", 74);
    ADD_KEY("KEY_KP4", 75);
    ADD_KEY("KEY_KP5", 76);
    ADD_KEY("KEY_KP6", 77);
    ADD_KEY("KEY_KPPLUS", 78);
    ADD_KEY("KEY_KP1", 79);
    ADD_KEY("KEY_KP2", 80);
    ADD_KEY("KEY_KP3", 81);
    ADD_KEY("KEY_KP0", 82);
    ADD_KEY("KEY_KPDOT", 83);
    ADD_KEY("KEY_ZENKAKUHANKAKU", 85);
    ADD_KEY("KEY_102ND", 86);
    ADD_KEY("KEY_F11", 87);
    ADD_KEY("KEY_F12", 88);
    ADD_KEY("KEY_RO", 89);
    ADD_KEY("KEY_KATAKANA", 90);
    ADD_KEY("KEY_HIRAGANA", 91);
    ADD_KEY("KEY_HENKAN", 92);
    ADD_KEY("KEY_KATAKANAHIRAGANA", 93);
    ADD_KEY("KEY_MUHENKAN", 94);
    ADD_KEY("KEY_KPJPCOMMA", 95);
    ADD_KEY("KEY_KPENTER", 96);
    ADD_KEY("KEY_RIGHTCTRL", 97);
    ADD_KEY("KEY_KPSLASH", 98);
    ADD_KEY("KEY_SYSRQ", 99);
    ADD_KEY("KEY_RIGHTALT", 100);
    ADD_KEY("KEY_LINEFEED", 101);
    ADD_KEY("KEY_HOME", 102);
    ADD_KEY("KEY_UP", 103);
    ADD_KEY("KEY_PAGEUP", 104);
    ADD_KEY("KEY_LEFT", 105);
    ADD_KEY("KEY_RIGHT", 106);
    ADD_KEY("KEY_END", 107);
    ADD_KEY("KEY_DOWN", 108);
    ADD_KEY("KEY_PAGEDOWN", 109);
    ADD_KEY("KEY_INSERT", 110);
    ADD_KEY("KEY_DELETE", 111);
    ADD_KEY("KEY_MACRO", 112);
    ADD_KEY("KEY_MUTE", 113);
    ADD_KEY("KEY_VOLUMEDOWN", 114);
    ADD_KEY("KEY_VOLUMEUP", 115);
    ADD_KEY("KEY_POWER", 116);	/* SC System Power Down */
    ADD_KEY("KEY_KPEQUAL", 117);
    ADD_KEY("KEY_KPPLUSMINUS", 118);
    ADD_KEY("KEY_PAUSE", 119);
    ADD_KEY("KEY_SCALE", 120);	/* AL Compiz Scale (Expose) */
    ADD_KEY("KEY_KPCOMMA", 121);
    ADD_KEY("KEY_HANGEUL", 122);
    ADD_KEY("KEY_HANJA", 123);
    ADD_KEY("KEY_YEN", 124);
    ADD_KEY("KEY_LEFTMETA", 125);
    ADD_KEY("KEY_RIGHTMETA", 126);
    ADD_KEY("KEY_COMPOSE", 127);
    ADD_KEY("KEY_STOP", 128);	/* AC Stop */
    ADD_KEY("KEY_AGAIN", 129);
    ADD_KEY("KEY_PROPS", 130);	/* AC Properties */
    ADD_KEY("KEY_UNDO", 131);	/* AC Undo */
    ADD_KEY("KEY_FRONT", 132);
    ADD_KEY("KEY_COPY", 133);	/* AC Copy */
    ADD_KEY("KEY_OPEN", 134);	/* AC Open */
    ADD_KEY("KEY_PASTE", 135);	/* AC Paste */
    ADD_KEY("KEY_FIND", 136);	/* AC Search */
    ADD_KEY("KEY_CUT", 137);	/* AC Cut */
    ADD_KEY("KEY_HELP", 138);	/* AL Integrated Help Center */
    ADD_KEY("KEY_MENU", 139);	/* Menu (show menu) */
    ADD_KEY("KEY_CALC", 140);	/* AL Calculator */
    ADD_KEY("KEY_SETUP", 141);
    ADD_KEY("KEY_SLEEP", 142);	/* SC System Sleep */
    ADD_KEY("KEY_WAKEUP", 143);	/* System Wake Up */
    ADD_KEY("KEY_FILE", 144);	/* AL Local Machine Browser */
    ADD_KEY("KEY_SENDFILE", 145);
    ADD_KEY("KEY_DELETEFILE", 146);
    ADD_KEY("KEY_XFER", 147);
    ADD_KEY("KEY_PROG1", 148);
    ADD_KEY("KEY_PROG2", 149);
    ADD_KEY("KEY_WWW", 150);	/* AL Internet Browser */
    ADD_KEY("KEY_MSDOS", 151);
    ADD_KEY("KEY_SCREENLOCK", 152);	/* AL Terminal Lock/Screensaver */
    ADD_KEY("KEY_ROTATE_DISPLAY", 153);	/* Display orientation for e.g. tablets */
    ADD_KEY("KEY_CYCLEWINDOWS", 154);
    ADD_KEY("KEY_MAIL", 155);
    ADD_KEY("KEY_BOOKMARKS", 156);	/* AC Bookmarks */
    ADD_KEY("KEY_COMPUTER", 157);
    ADD_KEY("KEY_BACK", 158);	/* AC Back */
    ADD_KEY("KEY_FORWARD", 159);	/* AC Forward */
    ADD_KEY("KEY_CLOSECD", 160);
    ADD_KEY("KEY_EJECTCD", 161);
    ADD_KEY("KEY_EJECTCLOSECD", 162);
    ADD_KEY("KEY_NEXTSONG", 163);
    ADD_KEY("KEY_PLAYPAUSE", 164);
    ADD_KEY("KEY_PREVIOUSSONG", 165);
    ADD_KEY("KEY_STOPCD", 166);
    ADD_KEY("KEY_RECORD", 167);
    ADD_KEY("KEY_REWIND", 168);
    ADD_KEY("KEY_PHONE", 169);	/* Media Select Telephone */
    ADD_KEY("KEY_ISO", 170);
    ADD_KEY("KEY_CONFIG", 171);	/* AL Consumer Control Configuration */
    ADD_KEY("KEY_HOMEPAGE", 172);	/* AC Home */
    ADD_KEY("KEY_REFRESH", 173);	/* AC Refresh */
    ADD_KEY("KEY_EXIT", 174);	/* AC Exit */
    ADD_KEY("KEY_MOVE", 175);
    ADD_KEY("KEY_EDIT", 176);
    ADD_KEY("KEY_SCROLLUP", 177);
    ADD_KEY("KEY_SCROLLDOWN", 178);
    ADD_KEY("KEY_KPLEFTPAREN", 179);
    ADD_KEY("KEY_KPRIGHTPAREN", 180);
    ADD_KEY("KEY_NEW", 181);	/* AC New */
    ADD_KEY("KEY_REDO", 182);	/* AC Redo/Repeat */
    ADD_KEY("KEY_F13", 183);
    ADD_KEY("KEY_F14", 184);
    ADD_KEY("KEY_F15", 185);
    ADD_KEY("KEY_F16", 186);
    ADD_KEY("KEY_F17", 187);
    ADD_KEY("KEY_F18", 188);
    ADD_KEY("KEY_F19", 189);
    ADD_KEY("KEY_F20", 190);
    ADD_KEY("KEY_F21", 191);
    ADD_KEY("KEY_F22", 192);
    ADD_KEY("KEY_F23", 193);
    ADD_KEY("KEY_F24", 194);
    ADD_KEY("KEY_PLAYCD", 200);
    ADD_KEY("KEY_PAUSECD", 201);
    ADD_KEY("KEY_PROG3", 202);
    ADD_KEY("KEY_PROG4", 203);
    ADD_KEY("KEY_DASHBOARD", 204);	/* AL Dashboard */
    ADD_KEY("KEY_SUSPEND", 205);
    ADD_KEY("KEY_CLOSE", 206);	/* AC Close */
    ADD_KEY("KEY_PLAY", 207);
    ADD_KEY("KEY_FASTFORWARD", 208);
    ADD_KEY("KEY_BASSBOOST", 209);
    ADD_KEY("KEY_PRINT", 210);	/* AC Print */
    ADD_KEY("KEY_HP", 211);
    ADD_KEY("KEY_CAMERA", 212);
    ADD_KEY("KEY_SOUND", 213);
    ADD_KEY("KEY_QUESTION", 214);
    ADD_KEY("KEY_EMAIL", 215);
    ADD_KEY("KEY_CHAT", 216);
    ADD_KEY("KEY_SEARCH", 217);
    ADD_KEY("KEY_CONNECT", 218);
    ADD_KEY("KEY_FINANCE", 219);	/* AL Checkbook/Finance */
    ADD_KEY("KEY_SPORT", 220);
    ADD_KEY("KEY_SHOP", 221);
    ADD_KEY("KEY_ALTERASE", 222);
    ADD_KEY("KEY_CANCEL", 223);	/* AC Cancel */
    ADD_KEY("KEY_BRIGHTNESSDOWN", 224);
    ADD_KEY("KEY_BRIGHTNESSUP", 225);
    ADD_KEY("KEY_MEDIA", 226);
    ADD_KEY("KEY_SWITCHVIDEOMODE", 227);	/* Cycle between available video outputs (Monitor/LCD/TV-out/etc) */
    ADD_KEY("KEY_KBDILLUMTOGGLE", 228);
    ADD_KEY("KEY_KBDILLUMDOWN", 229);
    ADD_KEY("KEY_KBDILLUMUP", 230);
    ADD_KEY("KEY_SEND", 231);	/* AC Send */
    ADD_KEY("KEY_REPLY", 232);	/* AC Reply */
    ADD_KEY("KEY_FORWARDMAIL", 233);	/* AC Forward Msg */
    ADD_KEY("KEY_SAVE", 234);	/* AC Save */
    ADD_KEY("KEY_DOCUMENTS", 235);
    ADD_KEY("KEY_BATTERY", 236);
    ADD_KEY("KEY_BLUETOOTH", 237);
    ADD_KEY("KEY_WLAN", 238);
    ADD_KEY("KEY_UWB", 239);
    ADD_KEY("KEY_UNKNOWN", 240);
    ADD_KEY("KEY_VIDEO_NEXT", 241);	/* drive next video source */
    ADD_KEY("KEY_VIDEO_PREV", 242);	/* drive previous video source */
    ADD_KEY("KEY_BRIGHTNESS_CYCLE", 243);	/* brightness up, after max is min */
    ADD_KEY("KEY_BRIGHTNESS_AUTO", 244);	/* Set Auto Brightness: manual brightness control is off, rely on ambient */
    ADD_KEY("KEY_DISPLAY_OFF", 245);	/* display device to off state */
    ADD_KEY("KEY_WWAN", 246);	/* Wireless WAN (LTE, UMTS, GSM, etc.) */
    ADD_KEY("KEY_RFKILL", 247);	/* Key that controls all radios */
    ADD_KEY("KEY_MICMUTE", 248);	/* Mute / unmute the microphone */
    ADD_KEY("BTN_MISC", 0x100);
    ADD_KEY("BTN_0", 0x100);
    ADD_KEY("BTN_1", 0x101);
    ADD_KEY("BTN_2", 0x102);
    ADD_KEY("BTN_3", 0x103);
    ADD_KEY("BTN_4", 0x104);
    ADD_KEY("BTN_5", 0x105);
    ADD_KEY("BTN_6", 0x106);
    ADD_KEY("BTN_7", 0x107);
    ADD_KEY("BTN_8", 0x108);
    ADD_KEY("BTN_9", 0x109);
    ADD_KEY("BTN_MOUSE", 0x110);
    ADD_KEY("BTN_LEFT", 0x110);
    ADD_KEY("BTN_RIGHT", 0x111);
    ADD_KEY("BTN_MIDDLE", 0x112);
    ADD_KEY("BTN_SIDE", 0x113);
    ADD_KEY("BTN_EXTRA", 0x114);
    ADD_KEY("BTN_FORWARD", 0x115);
    ADD_KEY("BTN_BACK", 0x116);
    ADD_KEY("BTN_TASK", 0x117);
    ADD_KEY("BTN_JOYSTICK", 0x120);
    ADD_KEY("BTN_TRIGGER", 0x120);
    ADD_KEY("BTN_THUMB", 0x121);
    ADD_KEY("BTN_THUMB2", 0x122);
    ADD_KEY("BTN_TOP", 0x123);
    ADD_KEY("BTN_TOP2", 0x124);
    ADD_KEY("BTN_PINKIE", 0x125);
    ADD_KEY("BTN_BASE", 0x126);
    ADD_KEY("BTN_BASE2", 0x127);
    ADD_KEY("BTN_BASE3", 0x128);
    ADD_KEY("BTN_BASE4", 0x129);
    ADD_KEY("BTN_BASE5", 0x12a);
    ADD_KEY("BTN_BASE6", 0x12b);
    ADD_KEY("BTN_DEAD", 0x12f);
    ADD_KEY("BTN_GAMEPAD", 0x130);
    ADD_KEY("BTN_SOUTH", 0x130);
    ADD_KEY("BTN_EAST", 0x131);
    ADD_KEY("BTN_C", 0x132);
    ADD_KEY("BTN_NORTH", 0x133);
    ADD_KEY("BTN_WEST", 0x134);
    ADD_KEY("BTN_Z", 0x135);
    ADD_KEY("BTN_TL", 0x136);
    ADD_KEY("BTN_TR", 0x137);
    ADD_KEY("BTN_TL2", 0x138);
    ADD_KEY("BTN_TR2", 0x139);
    ADD_KEY("BTN_SELECT", 0x13a);
    ADD_KEY("BTN_START", 0x13b);
    ADD_KEY("BTN_MODE", 0x13c);
    ADD_KEY("BTN_THUMBL", 0x13d);
    ADD_KEY("BTN_THUMBR", 0x13e);
    ADD_KEY("BTN_DIGI", 0x140);
    ADD_KEY("BTN_TOOL_PEN", 0x140);
    ADD_KEY("BTN_TOOL_RUBBER", 0x141);
    ADD_KEY("BTN_TOOL_BRUSH", 0x142);
    ADD_KEY("BTN_TOOL_PENCIL", 0x143);
    ADD_KEY("BTN_TOOL_AIRBRUSH", 0x144);
    ADD_KEY("BTN_TOOL_FINGER", 0x145);
    ADD_KEY("BTN_TOOL_MOUSE", 0x146);
    ADD_KEY("BTN_TOOL_LENS", 0x147);
    ADD_KEY("BTN_TOOL_QUINTTAP", 0x148);	/* Five fingers on trackpad */
    ADD_KEY("BTN_STYLUS3", 0x149);
    ADD_KEY("BTN_TOUCH", 0x14a);
    ADD_KEY("BTN_STYLUS", 0x14b);
    ADD_KEY("BTN_STYLUS2", 0x14c);
    ADD_KEY("BTN_TOOL_DOUBLETAP", 0x14d);
    ADD_KEY("BTN_TOOL_TRIPLETAP", 0x14e);
    ADD_KEY("BTN_TOOL_QUADTAP", 0x14f);	/* Four fingers on trackpad */
    ADD_KEY("BTN_WHEEL", 0x150);
    ADD_KEY("BTN_GEAR_DOWN", 0x150);
    ADD_KEY("BTN_GEAR_UP", 0x151);
    ADD_KEY("KEY_OK", 0x160);
    ADD_KEY("KEY_SELECT", 0x161);
    ADD_KEY("KEY_GOTO", 0x162);
    ADD_KEY("KEY_CLEAR", 0x163);
    ADD_KEY("KEY_POWER2", 0x164);
    ADD_KEY("KEY_OPTION", 0x165);
    ADD_KEY("KEY_INFO", 0x166);	/* AL OEM Features/Tips/Tutorial */
    ADD_KEY("KEY_TIME", 0x167);
    ADD_KEY("KEY_VENDOR", 0x168);
    ADD_KEY("KEY_ARCHIVE", 0x169);
    ADD_KEY("KEY_PROGRAM", 0x16a);	/* Media Select Program Guide */
    ADD_KEY("KEY_CHANNEL", 0x16b);
    ADD_KEY("KEY_FAVORITES", 0x16c);
    ADD_KEY("KEY_EPG", 0x16d);
    ADD_KEY("KEY_PVR", 0x16e);	/* Media Select Home */
    ADD_KEY("KEY_MHP", 0x16f);
    ADD_KEY("KEY_LANGUAGE", 0x170);
    ADD_KEY("KEY_TITLE", 0x171);
    ADD_KEY("KEY_SUBTITLE", 0x172);
    ADD_KEY("KEY_ANGLE", 0x173);
    ADD_KEY("KEY_ZOOM", 0x174);
    ADD_KEY("KEY_MODE", 0x175);
    ADD_KEY("KEY_KEYBOARD", 0x176);
    ADD_KEY("KEY_SCREEN", 0x177);
    ADD_KEY("KEY_PC", 0x178);	/* Media Select Computer */
    ADD_KEY("KEY_TV", 0x179);	/* Media Select TV */
    ADD_KEY("KEY_TV2", 0x17a);	/* Media Select Cable */
    ADD_KEY("KEY_VCR", 0x17b);	/* Media Select VCR */
    ADD_KEY("KEY_VCR2", 0x17c);	/* VCR Plus */
    ADD_KEY("KEY_SAT", 0x17d);	/* Media Select Satellite */
    ADD_KEY("KEY_SAT2", 0x17e);
    ADD_KEY("KEY_CD", 0x17f);	/* Media Select CD */
    ADD_KEY("KEY_TAPE", 0x180);	/* Media Select Tape */
    ADD_KEY("KEY_RADIO", 0x181);
    ADD_KEY("KEY_TUNER", 0x182);	/* Media Select Tuner */
    ADD_KEY("KEY_PLAYER", 0x183);
    ADD_KEY("KEY_TEXT", 0x184);
    ADD_KEY("KEY_DVD", 0x185);	/* Media Select DVD */
    ADD_KEY("KEY_AUX", 0x186);
    ADD_KEY("KEY_MP3", 0x187);
    ADD_KEY("KEY_AUDIO", 0x188);	/* AL Audio Browser */
    ADD_KEY("KEY_VIDEO", 0x189);	/* AL Movie Browser */
    ADD_KEY("KEY_DIRECTORY", 0x18a);
    ADD_KEY("KEY_LIST", 0x18b);
    ADD_KEY("KEY_MEMO", 0x18c);	/* Media Select Messages */
    ADD_KEY("KEY_CALENDAR", 0x18d);
    ADD_KEY("KEY_RED", 0x18e);
    ADD_KEY("KEY_GREEN", 0x18f);
    ADD_KEY("KEY_YELLOW", 0x190);
    ADD_KEY("KEY_BLUE", 0x191);
    ADD_KEY("KEY_CHANNELUP", 0x192);	/* Channel Increment */
    ADD_KEY("KEY_CHANNELDOWN", 0x193);	/* Channel Decrement */
    ADD_KEY("KEY_FIRST", 0x194);
    ADD_KEY("KEY_LAST", 0x195);	/* Recall Last */
    ADD_KEY("KEY_AB", 0x196);
    ADD_KEY("KEY_NEXT", 0x197);
    ADD_KEY("KEY_RESTART", 0x198);
    ADD_KEY("KEY_SLOW", 0x199);
    ADD_KEY("KEY_SHUFFLE", 0x19a);
    ADD_KEY("KEY_BREAK", 0x19b);
    ADD_KEY("KEY_PREVIOUS", 0x19c);
    ADD_KEY("KEY_DIGITS", 0x19d);
    ADD_KEY("KEY_TEEN", 0x19e);
    ADD_KEY("KEY_TWEN", 0x19f);
    ADD_KEY("KEY_VIDEOPHONE", 0x1a0);	/* Media Select Video Phone */
    ADD_KEY("KEY_GAMES", 0x1a1);	/* Media Select Games */
    ADD_KEY("KEY_ZOOMIN", 0x1a2);	/* AC Zoom In */
    ADD_KEY("KEY_ZOOMOUT", 0x1a3);	/* AC Zoom Out */
    ADD_KEY("KEY_ZOOMRESET", 0x1a4);	/* AC Zoom */
    ADD_KEY("KEY_WORDPROCESSOR", 0x1a5);	/* AL Word Processor */
    ADD_KEY("KEY_EDITOR", 0x1a6);	/* AL Text Editor */
    ADD_KEY("KEY_SPREADSHEET", 0x1a7);	/* AL Spreadsheet */
    ADD_KEY("KEY_GRAPHICSEDITOR", 0x1a8);	/* AL Graphics Editor */
    ADD_KEY("KEY_PRESENTATION", 0x1a9);	/* AL Presentation App */
    ADD_KEY("KEY_DATABASE", 0x1aa);	/* AL Database App */
    ADD_KEY("KEY_NEWS", 0x1ab);	/* AL Newsreader */
    ADD_KEY("KEY_VOICEMAIL", 0x1ac);	/* AL Voicemail */
    ADD_KEY("KEY_ADDRESSBOOK", 0x1ad);	/* AL Contacts/Address Book */
    ADD_KEY("KEY_MESSENGER", 0x1ae);	/* AL Instant Messaging */
    ADD_KEY("KEY_DISPLAYTOGGLE", 0x1af);	/* Turn display (LCD) on and off */
    ADD_KEY("KEY_BRIGHTNESS_TOGGLE", KEY_DISPLAYTOGGLE);
    ADD_KEY("KEY_SPELLCHECK", 0x1b0);   /* AL Spell Check */
    ADD_KEY("KEY_LOGOFF", 0x1b1);   /* AL Logoff */
    ADD_KEY("KEY_DOLLAR", 0x1b2);
    ADD_KEY("KEY_EURO", 0x1b3);
    ADD_KEY("KEY_FRAMEBACK", 0x1b4);	/* Consumer - transport controls */
    ADD_KEY("KEY_FRAMEFORWARD", 0x1b5);
    ADD_KEY("KEY_CONTEXT_MENU", 0x1b6);	/* GenDesc - system context menu */
    ADD_KEY("KEY_MEDIA_REPEAT", 0x1b7);	/* Consumer - transport control */
    ADD_KEY("KEY_10CHANNELSUP", 0x1b8);	/* 10 channels up (10+) */
    ADD_KEY("KEY_10CHANNELSDOWN", 0x1b9);	/* 10 channels down (10-) */
    ADD_KEY("KEY_IMAGES", 0x1ba);	/* AL Image Browser */
    ADD_KEY("KEY_DEL_EOL", 0x1c0);
    ADD_KEY("KEY_DEL_EOS", 0x1c1);
    ADD_KEY("KEY_INS_LINE", 0x1c2);
    ADD_KEY("KEY_DEL_LINE", 0x1c3);
    ADD_KEY("KEY_FN", 0x1d0);
    ADD_KEY("KEY_FN_ESC", 0x1d1);
    ADD_KEY("KEY_FN_F1", 0x1d2);
    ADD_KEY("KEY_FN_F2", 0x1d3);
    ADD_KEY("KEY_FN_F3", 0x1d4);
    ADD_KEY("KEY_FN_F4", 0x1d5);
    ADD_KEY("KEY_FN_F5", 0x1d6);
    ADD_KEY("KEY_FN_F6", 0x1d7);
    ADD_KEY("KEY_FN_F7", 0x1d8);
    ADD_KEY("KEY_FN_F8", 0x1d9);
    ADD_KEY("KEY_FN_F9", 0x1da);
    ADD_KEY("KEY_FN_F10", 0x1db);
    ADD_KEY("KEY_FN_F11", 0x1dc);
    ADD_KEY("KEY_FN_F12", 0x1dd);
    ADD_KEY("KEY_FN_1", 0x1de);
    ADD_KEY("KEY_FN_2", 0x1df);
    ADD_KEY("KEY_FN_D", 0x1e0);
    ADD_KEY("KEY_FN_E", 0x1e1);
    ADD_KEY("KEY_FN_F", 0x1e2);
    ADD_KEY("KEY_FN_S", 0x1e3);
    ADD_KEY("KEY_FN_B", 0x1e4);
    ADD_KEY("KEY_BRL_DOT1", 0x1f1);
    ADD_KEY("KEY_BRL_DOT2", 0x1f2);
    ADD_KEY("KEY_BRL_DOT3", 0x1f3);
    ADD_KEY("KEY_BRL_DOT4", 0x1f4);
    ADD_KEY("KEY_BRL_DOT5", 0x1f5);
    ADD_KEY("KEY_BRL_DOT6", 0x1f6);
    ADD_KEY("KEY_BRL_DOT7", 0x1f7);
    ADD_KEY("KEY_BRL_DOT8", 0x1f8);
    ADD_KEY("KEY_BRL_DOT9", 0x1f9);
    ADD_KEY("KEY_BRL_DOT10", 0x1fa);
    ADD_KEY("KEY_NUMERIC_0", 0x200);	/* used by phones, remote controls, */
    ADD_KEY("KEY_NUMERIC_1", 0x201);	/* and other keypads */
    ADD_KEY("KEY_NUMERIC_2", 0x202);
    ADD_KEY("KEY_NUMERIC_3", 0x203);
    ADD_KEY("KEY_NUMERIC_4", 0x204);
    ADD_KEY("KEY_NUMERIC_5", 0x205);
    ADD_KEY("KEY_NUMERIC_6", 0x206);
    ADD_KEY("KEY_NUMERIC_7", 0x207);
    ADD_KEY("KEY_NUMERIC_8", 0x208);
    ADD_KEY("KEY_NUMERIC_9", 0x209);
    ADD_KEY("KEY_NUMERIC_STAR", 0x20a);
    ADD_KEY("KEY_NUMERIC_POUND", 0x20b);
    ADD_KEY("KEY_NUMERIC_A", 0x20c);	/* Phone key A - HUT Telephony 0xb9 */
    ADD_KEY("KEY_NUMERIC_B", 0x20d);
    ADD_KEY("KEY_NUMERIC_C", 0x20e);
    ADD_KEY("KEY_NUMERIC_D", 0x20f);
    ADD_KEY("KEY_CAMERA_FOCUS", 0x210);
    ADD_KEY("KEY_WPS_BUTTON", 0x211);	/* WiFi Protected Setup key */
    ADD_KEY("KEY_TOUCHPAD_TOGGLE", 0x212);	/* Request switch touchpad on or off */
    ADD_KEY("KEY_TOUCHPAD_ON", 0x213);
    ADD_KEY("KEY_TOUCHPAD_OFF", 0x214);
    ADD_KEY("KEY_CAMERA_ZOOMIN", 0x215);
    ADD_KEY("KEY_CAMERA_ZOOMOUT", 0x216);
    ADD_KEY("KEY_CAMERA_UP", 0x217);
    ADD_KEY("KEY_CAMERA_DOWN", 0x218);
    ADD_KEY("KEY_CAMERA_LEFT", 0x219);
    ADD_KEY("KEY_CAMERA_RIGHT", 0x21a);
    ADD_KEY("KEY_ATTENDANT_ON", 0x21b);
    ADD_KEY("KEY_ATTENDANT_OFF", 0x21c);
    ADD_KEY("KEY_ATTENDANT_TOGGLE", 0x21d);	/* Attendant call on or off */
    ADD_KEY("KEY_LIGHTS_TOGGLE", 0x21e);	/* Reading light on or off */
    ADD_KEY("BTN_DPAD_UP", 0x220);
    ADD_KEY("BTN_DPAD_DOWN", 0x221);
    ADD_KEY("BTN_DPAD_LEFT", 0x222);
    ADD_KEY("BTN_DPAD_RIGHT", 0x223);
    ADD_KEY("KEY_ALS_TOGGLE", 0x230);	/* Ambient light sensor */
    ADD_KEY("KEY_ROTATE_LOCK_TOGGLE", 0x231);	/* Display rotation lock */
    ADD_KEY("KEY_BUTTONCONFIG", 0x240);	/* AL Button Configuration */
    ADD_KEY("KEY_TASKMANAGER", 0x241);	/* AL Task/Project Manager */
    ADD_KEY("KEY_JOURNAL", 0x242);	/* AL Log/Journal/Timecard */
    ADD_KEY("KEY_CONTROLPANEL", 0x243);	/* AL Control Panel */
    ADD_KEY("KEY_APPSELECT", 0x244);	/* AL Select Task/Application */
    ADD_KEY("KEY_SCREENSAVER", 0x245);	/* AL Screen Saver */
    ADD_KEY("KEY_VOICECOMMAND", 0x246);	/* Listening Voice Command */
    ADD_KEY("KEY_ASSISTANT", 0x247);	/* AL Context-aware desktop assistant */
    ADD_KEY("KEY_BRIGHTNESS_MIN", 0x250);	/* Set Brightness to Minimum */
    ADD_KEY("KEY_BRIGHTNESS_MAX", 0x251);	/* Set Brightness to Maximum */
    ADD_KEY("KEY_KBDINPUTASSIST_PREV", 0x260);
    ADD_KEY("KEY_KBDINPUTASSIST_NEXT", 0x261);
    ADD_KEY("KEY_KBDINPUTASSIST_PREVGROUP", 0x262);
    ADD_KEY("KEY_KBDINPUTASSIST_NEXTGROUP", 0x263);
    ADD_KEY("KEY_KBDINPUTASSIST_ACCEPT", 0x264);
    ADD_KEY("KEY_KBDINPUTASSIST_CANCEL", 0x265);
    ADD_KEY("KEY_RIGHT_UP", 0x266);
    ADD_KEY("KEY_RIGHT_DOWN", 0x267);
    ADD_KEY("KEY_LEFT_UP", 0x268);
    ADD_KEY("KEY_LEFT_DOWN", 0x269);
    ADD_KEY("KEY_ROOT_MENU", 0x26a); /* Show Device's Root Menu */
    ADD_KEY("KEY_MEDIA_TOP_MENU", 0x26b); /* Show Top Menu of the Media (e.g. DVD) */
    ADD_KEY("KEY_NUMERIC_11", 0x26c);
    ADD_KEY("KEY_NUMERIC_12", 0x26d);
    ADD_KEY("KEY_AUDIO_DESC", 0x26e); /* Toggle Audio Description: refers to an audio service that helps blind and visually impaired consumers understand the action in a program. Note: in some countries this is referred to as "Video Description".  */
    ADD_KEY("KEY_3D_MODE", 0x26f);
    ADD_KEY("KEY_NEXT_FAVORITE", 0x270);
    ADD_KEY("KEY_STOP_RECORD", 0x271);
    ADD_KEY("KEY_PAUSE_RECORD", 0x272);
    ADD_KEY("KEY_VOD", 0x273); /* Video on Demand */
    ADD_KEY("KEY_UNMUTE", 0x274);
    ADD_KEY("KEY_FASTREVERSE", 0x275);
    ADD_KEY("KEY_SLOWREVERSE", 0x276);
    ADD_KEY("KEY_DATA", 0x277); /* Control a data application associated with the currently viewed channel, e.g. teletext or data broadcast application (MHEG, MHP, HbbTV, etc.) */
    ADD_KEY("KEY_ONSCREEN_KEYBOARD", 0x278);
    ADD_KEY("BTN_TRIGGER_HAPPY", 0x2c0);
    ADD_KEY("BTN_TRIGGER_HAPPY1", 0x2c0);
    ADD_KEY("BTN_TRIGGER_HAPPY2", 0x2c1);
    ADD_KEY("BTN_TRIGGER_HAPPY3", 0x2c2);
    ADD_KEY("BTN_TRIGGER_HAPPY4", 0x2c3);
    ADD_KEY("BTN_TRIGGER_HAPPY5", 0x2c4);
    ADD_KEY("BTN_TRIGGER_HAPPY6", 0x2c5);
    ADD_KEY("BTN_TRIGGER_HAPPY7", 0x2c6);
    ADD_KEY("BTN_TRIGGER_HAPPY8", 0x2c7);
    ADD_KEY("BTN_TRIGGER_HAPPY9", 0x2c8);
    ADD_KEY("BTN_TRIGGER_HAPPY10", 0x2c9);
    ADD_KEY("BTN_TRIGGER_HAPPY11", 0x2ca);
    ADD_KEY("BTN_TRIGGER_HAPPY12", 0x2cb);
    ADD_KEY("BTN_TRIGGER_HAPPY13", 0x2cc);
    ADD_KEY("BTN_TRIGGER_HAPPY14", 0x2cd);
    ADD_KEY("BTN_TRIGGER_HAPPY15", 0x2ce);
    ADD_KEY("BTN_TRIGGER_HAPPY16", 0x2cf);
    ADD_KEY("BTN_TRIGGER_HAPPY17", 0x2d0);
    ADD_KEY("BTN_TRIGGER_HAPPY18", 0x2d1);
    ADD_KEY("BTN_TRIGGER_HAPPY19", 0x2d2);
    ADD_KEY("BTN_TRIGGER_HAPPY20", 0x2d3);
    ADD_KEY("BTN_TRIGGER_HAPPY21", 0x2d4);
    ADD_KEY("BTN_TRIGGER_HAPPY22", 0x2d5);
    ADD_KEY("BTN_TRIGGER_HAPPY23", 0x2d6);
    ADD_KEY("BTN_TRIGGER_HAPPY24", 0x2d7);
    ADD_KEY("BTN_TRIGGER_HAPPY25", 0x2d8);
    ADD_KEY("BTN_TRIGGER_HAPPY26", 0x2d9);
    ADD_KEY("BTN_TRIGGER_HAPPY27", 0x2da);
    ADD_KEY("BTN_TRIGGER_HAPPY28", 0x2db);
    ADD_KEY("BTN_TRIGGER_HAPPY29", 0x2dc);
    ADD_KEY("BTN_TRIGGER_HAPPY30", 0x2dd);
    ADD_KEY("BTN_TRIGGER_HAPPY31", 0x2de);
    ADD_KEY("BTN_TRIGGER_HAPPY32", 0x2df);
    ADD_KEY("BTN_TRIGGER_HAPPY33", 0x2e0);
    ADD_KEY("BTN_TRIGGER_HAPPY34", 0x2e1);
    ADD_KEY("BTN_TRIGGER_HAPPY35", 0x2e2);
    ADD_KEY("BTN_TRIGGER_HAPPY36", 0x2e3);
    ADD_KEY("BTN_TRIGGER_HAPPY37", 0x2e4);
    ADD_KEY("BTN_TRIGGER_HAPPY38", 0x2e5);
    ADD_KEY("BTN_TRIGGER_HAPPY39", 0x2e6);
    ADD_KEY("BTN_TRIGGER_HAPPY40", 0x2e7);

    ADD_ALIAS("KEY_RESERVED", 0);
    ADD_ALIAS("KEY_HANGUEL", KEY_HANGEUL);
    ADD_ALIAS("KEY_COFFEE", KEY_SCREENLOCK);
    ADD_ALIAS("KEY_DIRECTION", KEY_ROTATE_DISPLAY);
    ADD_ALIAS("KEY_BRIGHTNESS_ZERO", KEY_BRIGHTNESS_AUTO);
    ADD_ALIAS("KEY_WIMAX", KEY_WWAN);
    ADD_ALIAS("BTN_A", BTN_SOUTH);
    ADD_ALIAS("BTN_B", BTN_EAST);
    ADD_ALIAS("BTN_X", BTN_NORTH);
    ADD_ALIAS("BTN_Y", BTN_WEST);

    return 0;
}

void names_free(void){
    kh_destroy(n2v, n2v);
    kh_destroy(v2n, v2n);
}

const char *get_input_name(uint16_t value){
    khiter_t k = kh_get(v2n, v2n, value);
    if(k == kh_end(v2n)){
        return "unknown_key_value";
    }

    return kh_val(v2n, k);
}

// returns 0 if name not found
uint16_t get_input_value(const char *name){
    khiter_t k = kh_get(n2v, n2v, name);
    if(k == kh_end(n2v)){
        return 0;
    }

    return kh_val(n2v, k);
}

void for_each_name(name_hook_t hook, void* arg){
    khiter_t k;
    for(k = kh_begin(n2v); k != kh_end(n2v); k++){
        if(!(kh_exist(n2v, k))) continue;

        hook(arg, kh_key(n2v, k), kh_val(n2v, k));
    }
}

void for_each_value(name_hook_t hook, void* arg){
    khiter_t k;
    for(k = kh_begin(v2n); k != kh_end(v2n); k++){
        if(!(kh_exist(v2n, k))) continue;

        hook(arg, kh_val(v2n, k), kh_key(v2n, k));
    }
}
