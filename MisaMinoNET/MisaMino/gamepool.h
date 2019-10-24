#pragma once
#define _ALLOW_ITERATOR_DEBUG_LEVEL_MISMATCH

#include <string.h>
#include <cstdint>
#include <iostream>
#include "tetris_gem.h"
#define AI_POOL_MAX_H 50

namespace AI {
    struct GameField;
    Gem& getGem( int number, int spin );
    int getComboAttack( int combo );
    void setAllSpin(bool allSpin);
    bool isEnableAllSpin();
    void setSoftdrop( bool softdrop );
    bool softdropEnable();
    typedef uint64_t uint64;
    void InitHashTable();
    uint64 hash(const GameField & pool);
    const int gem_add_y = 20;
    const int gem_beg_x = 3;
    const int gem_beg_y = 1;

    struct GameField {
        signed char m_w, m_h;  // フィールドの幅、高さ
        signed short combo;  // 現在続いているコンボ数 // 1回目のライン消去で1。つまり通常のRENより1大きい
        // B2Bは 壁蹴り+ライン消去 で発生する
        int b2b;  // 現在続いているb2b数 // 1回目のTスピン・テトリスで1  // つまりB2Bが続いている状態を表すため、実際にB2Bが発生した回数ではない
        unsigned long m_w_mask;  // 1段すべてがうまっているときを表すマスク
        unsigned long m_row[AI_POOL_MAX_H];  // フィールドデータ  // 表示部分以外の状態も持っている
        int m_hold;  // ホールドしているミノの種類
        int m_pc_att;  // パフェしたときの火力
        uint64 hashval;  // 現在のフィールドの状態のハッシュ値
        unsigned long *row;  // 表示部分を先頭にした配列  // row[0] = m_row[gem_add_y] は表示されないので注意  // もしh=5ならrow[0]は6段目を表す // つまりrow[h]がフィールドの一番下

        GameField () {
            row = &m_row[gem_add_y];
        }
        GameField ( const GameField& field ) {
            row = &m_row[gem_add_y];
            *this = field;
        }
        GameField (signed char w, signed char h) {
            row = &m_row[gem_add_y];
            reset(w, h);
        }

        // `0b00000_00001` という数値があるとき、フィールドはみため通り、右端にブロックがあるように表示される
        // （つまり、フィールドが反転している）
        friend std::ostream& operator<<(std::ostream &os, const GameField &obj){
            os<<"\n";
            for (int i = AI::gem_add_y + 1; i < AI_POOL_MAX_H-5; ++i) {
            //for (int i = 0; i < AI_POOL_MAX_H; ++i) {
                os<<i<<"|";
                for (int k = obj.width()-1; k >=0; --k) {
                    if((obj.m_row[i] & (1<<k)) == 1<<k){
                        os<<"1";
                    }else{
                        os<<"0";
                    }
                }
                os<<"\n";
            }
        }
        int width() const { return m_w; }
        int height() const { return m_h; }

        // 初期化
        void reset (signed char w, signed char h) {
            m_w = w;
            m_h = h;
            m_pc_att = 10;
            m_w_mask = ( 1 << w ) - 1;
            for (int i = 0; i < AI_POOL_MAX_H; ++i) {
                m_row[i] = 0;
            }
            for (int i = gem_add_y + m_h + 1; i < AI_POOL_MAX_H; ++i) {
                m_row[i] = (unsigned)-1;
            }
        }
        GameField& operator = (const GameField& field) {
            memcpy( this, &field, (size_t)&row - (size_t)this );
            row = m_row + ( field.row - field.m_row );
            return *this;
        }
        bool operator == (const GameField& field) const {
            if ( m_w != field.m_w || m_h != field.m_h ) return false;
            if ( m_hold != field.m_hold ) return false;
            if ( combo != field.combo ) return false;
            if ( b2b != field.b2b ) return false;
            if ( row - m_row != field.row - field.m_row ) return false;
            for ( int i = 0; i <= m_h + gem_add_y; ++i ) {
                if ( m_row[i] != field.m_row[i] ) return false;
            }
            return true;
        }

        inline
        bool isCollide(int y, const Gem & gem) const {
            for ( int h = 3; h >= 0; --h ) {
                if ( y + h > m_h && gem.bitmap[h] ) return true;
                if ( row[y + h] & gem.bitmap[h] ) return true;
            }
            return false;
        }

        // すでにあるブロックとぶつかったらtrue
        inline
        bool isCollide(int x, int y, const Gem & gem) const {
            Gem _gem = gem;
            for ( int h = 0; h < 4; ++h ) {
                if ( x < 0 ) {
                    if (gem.bitmap[h] & ( ( 1 << (-x) ) - 1 ) ) return true;
                    _gem.bitmap[h] >>= -x;
                } else {
                    if ( (gem.bitmap[h] << x) & ~m_w_mask ) return true;
                    _gem.bitmap[h] <<= x;
                }
                if ( y + h > m_h && _gem.bitmap[h] ) return true;
                if ( row[y + h] & _gem.bitmap[h] ) return true;
            }
            return false; //isCollide(y, _gem);
        }

        // 壁蹴りによって置けるようになるとき true。最後まで置けないときは false。回転後の移動なし0,0はチェックに含まれない
        // @param gem 回転後のミノ
        // @param spinclockwise 左回転0 or 右回転1で指定
        bool wallkickTest(int& x, int& y, const Gem & gem, int spinclockwise) const {
            // Iミノ用回転後の移動先
            static int Iwallkickdata[4][2][4][2] = {
                { // O
                    { // R  // 出現方向 -> 右回転状態
                        { 2, 0},{-1, 0},{ 2,-1},{-1, 2},
                    },
                    { // L  // 出現方向 -> 左回転状態
                        { 1, 0},{-2, 0},{ 1, 2},{-2,-1},
                    },
                },
                { // L
                    { // O  // 左回転状態 -> 出現方向
                        {-1, 0},{ 2, 0},{-1,-2},{ 2, 1},
                    },
                    { // 2  // 左回転状態 -> 裏返し方向
                        { 2, 0},{-1, 0},{ 2,-1},{-1, 2},
                    },
                },
                { // 2
                    { // L  // 裏返し方向 -> 左回転状態
                        {-2, 0},{ 1, 0},{-2, 1},{ 1,-2},
                    },
                    { // R  // 裏返し方向 -> 右回転状態
                        {-1, 0},{ 2, 0},{-1,-2},{ 2, 1},
                    },
                },
                { // R
                    { // 2  // 右回転状態 -> 裏返し方向
                        { 1, 0},{-2, 0},{ 1, 2},{-2,-1},
                    },
                    { // O  // 右回転状態 -> 出現方向
                        {-2, 0},{ 1, 0},{-2, 1},{ 1,-2},
                    },
                },
            };
            // Iミノ以外用回転後の移動先
            static int wallkickdata[4][2][4][2] = {
                { // O
                    { // R
                        { 1, 0},{ 1, 1},{ 0,-2},{ 1,-2},
                    },
                    { // L
                        {-1, 0},{-1, 1},{ 0,-2},{-1,-2},
                    },
                },
                { // L
                    { // O
                        { 1, 0},{ 1,-1},{ 0, 2},{ 1, 2},
                    },
                    { // 2
                        { 1, 0},{ 1,-1},{ 0, 2},{ 1, 2},
                    },
                },
                { // 2
                    { // L
                        {-1, 0},{-1, 1},{ 0,-2},{-1,-2},
                    },
                    { // R
                        { 1, 0},{ 1, 1},{ 0,-2},{ 1,-2},
                    },
                },
                { // R
                    { // 2
                        {-1, 0},{-1,-1},{ 0, 2},{-1, 2},
                    },
                    { // O
                        {-1, 0},{-1,-1},{ 0, 2},{-1, 2},
                    },
                },
            };
            int (*pdata)[2][4][2] = wallkickdata;
            if ( gem.num == 1 ) pdata = Iwallkickdata;
            for ( int itest = 0; itest < 4; ++itest) {
                int dx = x + pdata[gem.spin][spinclockwise][itest][0];
                int dy = y + pdata[gem.spin][spinclockwise][itest][1];
                if ( ! isCollide(dx, dy, gem) ) {
                    x = dx; y = dy;
                    return true;
                }
            }
            return false;
        }
        // @param x gemの4x4の最も右（xが小さい）の位置で指定
        // @param y gemの4x4の最も高い位置で指定。フィールド上端が1なので注意
        void paste(int x, int y, const Gem & gem) {
            for ( int h = 0; h < gem.geth(); ++h ) {
                if (x >= 0)
                    row[y + h] |= gem.bitmap[h] << x;
                else
                    row[y + h] |= gem.bitmap[h] >> (-x);
            }
        }
        // T-Spinの判定
        signed char isWallKickSpin(int x, int y, const Gem & gem) const {
            if ( isEnableAllSpin() ) {  // KOS
                if ( isCollide( x - 1, y, gem )
                    && isCollide( x + 1, y, gem )
                    && isCollide( x, y - 1, gem )) {
                        return 1;
                }
            } else {
                if ( gem.num == 2 ) { // T
                    // x=[0,2], y=[0,2]の組み合わせ(Tの4隅)の位置にブロックがあるか確認
                    // ブロックが3つ以上あればTスピン
                    // TのGemの定義により、どの回転方向でも x=[0,2], y=[0,2] に隅がくる
                    int cnt = 0;
                    if ( x < 0 || (row[y] & (1 << x))) ++cnt;
                    if ( x < 0 || y+2 > m_h || (row[y+2] & (1 << x))) ++cnt;
                    if ( x+2 >= m_w || (row[y] & (1 << (x+2)))) ++cnt;
                    if ( x+2 >= m_w || y+2 > m_h || (row[y+2] & (1 << (x+2)))) ++cnt;
                    if ( cnt >= 3 ) return 1;
                }
            }
            return 0;
        }
        // @param wallkick_spin いまチェック中のスピンの結果
        // この関数を呼び出す前に GenMoving() が呼ばれる。そこでwallkickの状態は追跡されていると思われる
        //                      この関数内では isEnableAllSpin() == false のとき、wallkick_spin=2にならない
        //  0: Tスピンなし
        //  1: その場での回転。壁蹴りなしの回転。確実にRegular T-Spin
        //  2: 壁蹴りありの移動。T-Spin Miniの可能性あり
        signed char WallKickValue(int gem_num, int x, int y, int spin, signed char wallkick_spin) const {
            if ( ! isWallKickSpin( x, y, getGem(gem_num, spin) ) ) {
                // T-Spinの形でない  // 4隅が埋まっていない
                return wallkick_spin = 0;
            }
            if ( isEnableAllSpin() ) {  // KOSの回転設定
                if ( wallkick_spin == 2) {
                    wallkick_spin = 1;
                    Gem g = getGem(gem_num, spin);
                    for ( int dy = 0; dy < 4; ++dy ) { //KOS mini test
                        if ( g.bitmap[dy] == 0 ) continue;
                        if ( ((g.bitmap[dy] << x) | row[y+dy]) == m_w_mask ) continue;
                        wallkick_spin = 2;
                        break;
                    }
                }
            } else {  // 通常のSRS
                if ( wallkick_spin == 2 ) {  // 壁蹴りありのT-Spinより、その場での回転のT-Spinがあるか確認
                    // spin^2: 180度回転したときのスピン: 0<->2, L<->R
                    if ( ! isCollide( x, y, getGem(gem_num, spin^2) ) ) {
                        wallkick_spin = 1; // not t-mini
                    }
                }
            }
            return wallkick_spin;
        }
        // 揃っているラインを消去する
        // フィールドとclearnum・b2b・comboを更新する
        // @param _wallkick_spin 最後のwallkickの状態
        int clearLines( signed char _wallkick_spin ) {
            int clearnum = 0;
            int h2 = m_h;
            for (int h = m_h; h >= -gem_add_y; --h) {
                if ( row[h] != m_w_mask) {
                    row[h2--] = row[h];
                } else {
                    ++ clearnum;
                }
            }
            for (int h = h2; h >= -gem_add_y; --h) {
                row[h] = 0;
            }
            if ( clearnum > 0 ) {
                ++combo;
                if ( clearnum == 4 ) {
                    ++b2b;
                } else if ( _wallkick_spin > 0 ) {
                    ++b2b;
                } else {
                    b2b = 0;
                }
            } else {
                combo = 0;
            }
            hashval = hash(*this);
            return clearnum;
        }
        // パフェの攻撃量
        int getPCAttack() const {
            return m_pc_att;
        }
        // clearLines() -> getAttack() の順に呼ばれる。combo,b2bの値に注意
        // @param clearfull 消去ライン数
        // @param wallkick Tスピン判定。詳細は `WallKickValue()` 参照
        int getAttack( int clearfull, signed char wallkick ) {
            int attack = 0;
            if ( clearfull > 1 ) {
                if ( clearfull < 4 ) {
                    // シングルライン・ダブルライン・トリプルライン
                    attack = clearfull - 1;
                } else {
                    // テトリス
                    attack = clearfull;
                    if ( b2b > 0 ) attack += 1;
                }
            }
            if ( clearfull > 0 ) {
                if ( wallkick ) {
                    if ( b2b > 0 ) attack += 1;
                    if ( clearfull == 1 ) {
                        if ( wallkick != 2 ) {
                            attack += 2;
                        }
                    } else {
                        // 上の通常ライン消去分とあわせて調整する
                        // (clearfull-1) + (clearfull+1) = 2 * clearfull
                        attack += clearfull + 1;
                    }
                }
                attack += getComboAttack( combo );
                {
                    int i = gem_add_y + m_h;
                    for ( ; i >= 0; --i ) {
                        if ( m_row[i] ) break;
                    }
                    if ( i < 0 ) {
                        attack = m_pc_att; // pc
                    }
                }
            }
            return attack;
        }
        // @param y 表示部分の上端が0。下端がh-1
        void setBlock(int x, int y) {
            row[y + 1] |= 1U << (unsigned) x;
        }
        void setBlockDirect(int x, int y) {
            row[y] |= 1U << (unsigned) x;
        }
        void addRow( int rowdata ) {
            for ( int h = -gem_add_y + 1; h <= m_h; ++h ) {
                row[h-1] = row[h];
            }
            row[m_h] = rowdata;
        }
        void minusRow( int lines ) {
            //row += lines;
            //m_h -= lines;
        }
    };
}
