#pragma once
#define _ALLOW_ITERATOR_DEBUG_LEVEL_MISMATCH

namespace AI {
    enum GemType {
        GEMTYPE_NULL,
        GEMTYPE_I,
        GEMTYPE_T,
        GEMTYPE_L,
        GEMTYPE_J,
        GEMTYPE_Z,
        GEMTYPE_S,
        GEMTYPE_O,
    };

    enum Rotation {
        Spawn = 0,
        Left = 1,
        Reverse = 2,
        Right = 3,
    };

    // ミノの定義
    // おもにブロックの位置を表す
    struct Gem {
        unsigned long bitmap[4];  // ブロックの位置
        int num;  // ミノ番号
        int spin;  // 回転方向
        int mod;  // ブロックの状態数  // 基本 4  // Oミノのみ 1
        int geth() const { return 4; }  // ブロック領域の高さ  // 4x4の領域に定義されているため固定
        // ミノを表現する文字
        char getLetter() const {
            char map[]="NITLJZSO";
            return map[num];
        }
    };
    inline Gem& getGem( int number, int spin ) {
        extern Gem gems[8][4];
        return gems[number][spin];
    }
    // 指定したx列で、最も低い位置にあるブロックの高さ
    // 添字0がフィールドの上端を表すため、最も数字が大きいもの=最も低い位置となる
    inline int getGemColH( int number, int spin, int x) {
        extern int GEM_COL_H[8][4][4];
        return GEM_COL_H[number][spin][x];
    }

    // 各x の `getGemColH()` の中で最も大きい数字（ミノ全体で最も低い位置にあるブロックの高さ）
    inline int getGemMaxH( int number, int spin) {
        extern int GEM_MAXH[8][4];
        return GEM_MAXH[number][spin];
    }
}
