#pragma once
#include "gamepool.h"
#include "tetris_ai.h"
#include "tetris_gem.h"

namespace AI {

    struct point {
        float x;
        float y;
        point() : x(0), y(0) {}
        point(float _x, float _y) : x(_x), y(_y) {}
        point(int _x, int _y)
            : x(static_cast<float>(_x))
            , y(static_cast<float>(_y))
        {}
    };

    class Tetris {
    public:
        enum {
            STATE_INIT,  // コンストラクタが実行された状態。一度離れるとこの状態にはならない
            STATE_READY,  // reset()後やミノおいて地形を整理した後の状態
            STATE_MOVING,  // ミノを操作中
            STATE_PASTED,  // ミノを接着後。地形を整理する前。この状態のあと、READYかOVERに繊維
            STATE_OVER,  // ゲーム終了判定
        };

        struct clear_info {
            int gem_num;
            int clears;
            int attack;
            int b2b;
            int combo;
            int pc;  // パフェがあるなら1
            signed char wallkick_spin;
            int total_pc;  // ゲーム中にパフェしたトータルの回数
            int total_b2b;  // ゲーム中にB2Bしたトータルの回数 （B2BはTスピン・テトリスを2回以上続けると+1）
            int total_cb_att;
            int t[4];  // ゲーム中にTスピンしたトータルの回数。添字はTスピン時に消去したライン数。[0]はMiniを表す  // 空Tは含まない
            int normal[5];  // ゲーム中のライン消去のトータルの回数。添字は消去したライン数。Tスピンは含まない。[0]は未使用
            void reset( int _gem_num ) {
                memset(this, 0, sizeof(*this));
                gem_num = _gem_num;
            }
            // データの初期化
            void newgem( int _gem_num ) {
                gem_num = _gem_num;
                clears = 0;
                attack = 0;
                b2b = 0;
                combo = 0;
                pc = 0;
                wallkick_spin = 0;
            }
        };

        Tetris() : m_pool( 10, 20 ) {
            m_state = STATE_INIT;
            reset ( 0, 10, 20 );
        }
        // 状態のリセット
        void reset (unsigned seed, signed char w, signed char h) {
            m_pool.reset( w, h );
            m_pool.combo = 0;
            m_pool.b2b = 0;
            m_pool.m_hold = 0;
            m_next_num = 0;
            //for ( int i = 0; i < 32; ++i ) {
            //    m_next[i] = AI::getGem( m_rand.randint(7) + 1, 0);
            //}
            m_state = STATE_READY;
            m_cur = AI::getGem(0, 0);
            m_cur_x = AI::gem_beg_x;
            m_cur_y = AI::gem_beg_y;
            m_curnum = 0;
            m_clearLines = 0;
            m_attack = 0;
            m_max_combo = 0;
            m_frames = 0;
            m_drop_frame = 0;
            m_clear_info.reset( 0 );
            memset( m_color_pool, 0, sizeof( m_color_pool ) );
        }
        // 横移動。移動出来たらtrueを返す
        // 途中に壁があっても移動できる（ワープできる）
        // @param dx 移動量。+のとき右移動。-のとき左移動
        bool tryXMove(int dx) {
            if ( m_state != STATE_MOVING ) return false;
            if (m_pool.isCollide(m_cur_x + dx, m_cur_y, m_cur))
                return false;
            m_cur_x += dx;
            wallkick_spin = 0;
            return true;
        }
        // 縦移動。移動出来たらtrueを返す
        // 途中に壁があっても移動できる（ワープできる）
        // @param dy 移動量。+のとき下移動。-のとき上移動
        bool tryYMove(int dy) {
            if ( m_state != STATE_MOVING ) return false;
            if (m_pool.isCollide(m_cur_x, m_cur_y + dy, m_cur))
                return false;
            m_cur_y += dy;
            wallkick_spin = 0;
            return true;
        }
        // 回転操作。操作出来たらtrueを返す
        // @param dSpin 回転量。+1のとき左回転。-1のとき右回転
        bool trySpin(int dSpin) {
            if ( m_state != STATE_MOVING ) return false;
            AI::Gem gem = AI::getGem(m_cur.num, (m_cur.spin + dSpin + 4) % 4);
            if (m_pool.isCollide(m_cur_x, m_cur_y, gem)) {
                int spin = 1;
                if ( dSpin == 1 ) spin = 0;
                if ( m_pool.wallkickTest(m_cur_x, m_cur_y, gem, spin) ) {
                    m_cur = gem;
                    wallkick_spin = 2;
                    return true;
                } else {
                    return false;
                }
            }
            m_cur = gem;
            wallkick_spin = 1;
            return true;
        }
        // 180度回転
        bool trySpin180() {
            if ( m_state != STATE_MOVING ) return false;
            AI::Gem gem = AI::getGem(m_cur.num, (m_cur.spin + 2) % 4);
            if (m_pool.isCollide(m_cur_x, m_cur_y, gem)) {
                return false;
            }
            m_cur = gem;
            wallkick_spin = 1;
            return true;
        }
        // ホールドミノの取り出しに成功したら true。そもそも取り出せなかったら false
        // ミノを取り出した結果ゲーム終了になっても戻り値は true で、 m_state が変化する
        // ホールドが空のときはネクストから取り出す
        bool tryHold() {
            if ( m_state != STATE_MOVING ) return false;
            if ( m_hold ) return false;
            m_hold = true;
            int hold = m_pool.m_hold;
            m_pool.m_hold = m_cur.num;

            // wallkick_spinのリセットを忘れている気がする

            if ( hold == 0 ) {
                // ホールドが空のとき
                m_cur_x = AI::gem_beg_x;
                m_cur_y = AI::gem_beg_y;
                m_cur = AI::getGem(m_next[0].num, 0);
                removeNext();
            } else {
                // ホールドがあるとき
                m_cur_x = AI::gem_beg_x;
                m_cur_y = AI::gem_beg_y;
                m_cur = AI::getGem(hold, 0);
            }

            // 取り出した直後、ミノがすでに置けない状態ならゲーム終了
            if ( m_pool.isCollide(m_cur_x, m_cur_y, m_cur)) {
                m_state = STATE_OVER;
                return true;
            }
            return true;
        }
        // ミノを地形（カラー）に反映する
        void paste() {
            for ( int y = 0; y < 4; ++y ) {
                for ( int x = 0; x < 4; ++x ) {
                    if ( m_cur.bitmap[y] & ( 1 << x ) ) {
                        m_color_pool[m_cur_y + y + 32][m_cur_x + x] = m_cur.num;
                    }
                }
            }
        }
        // ハードドロップ操作を行う。paste()も行われる
        bool drop () {
            if ( m_state != STATE_MOVING ) return false;

            // データの初期化
            m_clear_info.newgem( m_cur.num );

            // 接着するまで下に移動
            while ( tryYMove( 1 ) );

            // wallkickを一応最後に更新？wallkickは各操作で更新しているため、あまり意味がない気がする
            wallkick_spin = m_pool.WallKickValue(m_cur.num, m_cur_x, m_cur_y, m_cur.spin, wallkick_spin);

            // ミノを地形（ビット）に反映する
            m_pool.paste( m_cur_x, m_cur_y, m_cur );

            // ミノを地形（カラー）に反映する
            paste();

            m_drop_frame = m_frames;  // m_framesが変更された形跡なし

            // 現在のミノをNULLにする
            m_cur = AI::getGem( 0, 0);

            m_state = STATE_PASTED;
            return true;
        }
        // フィールド（カラー）でそろったラインを削除
        void color_pool_clearLines() {
            int dy = 63;
            for ( int y = dy; y >= 0; --y ) {
                int x = 0;
                for ( ; x < poolw(); ++x ) {
                    if ( m_color_pool[y][x] == 0 ) break;
                }
                if ( x < poolw() ) {
                    if ( dy != y ) {
                        for (x = 0 ; x < poolw(); ++x ) {
                            m_color_pool[dy][x] = m_color_pool[y][x];
                        }
                    }
                    --dy;
                }
            }
            for ( ; dy >= 0; --dy ) {
                for (int x = 0 ; x < poolw(); ++x ) {
                    m_color_pool[dy][x] = 0;
                }
            }
        }
        void clearLines () {
            if ( m_state != STATE_PASTED ) return;

            // フィールド（ビット）でそろったラインを削除
            // m_poolのmemberが更新される
            m_clearLines = m_pool.clearLines( wallkick_spin );

            // フィールド（カラー）でそろったラインを削除
            color_pool_clearLines();

            // 攻撃力を計算
            m_attack = m_pool.getAttack( m_clearLines, wallkick_spin );

            // 最大コンボ数を更新したら、memberを更新
            // `m_pool.combo - 1` は通常のRENのカウントと合わせるためだと思われる（1回目のライン消去で0REN）
            m_max_combo = std::max(m_max_combo, m_pool.combo - 1);

            // ライン消去のときに生ずる情報を記録する
            m_clear_info.clears = m_clearLines;  // 消去されたライン数
            m_clear_info.attack = m_attack;  // 攻撃力
            m_clear_info.b2b = m_pool.b2b;  // 継続中のb2bのカウント数 (1回目のTスピン。テトリスで1)
            m_clear_info.combo = m_pool.combo;  // 継続中のコンボ数（1回目のライン消去で1なので注意）
            m_clear_info.wallkick_spin = wallkick_spin;  // 壁蹴りの状態
            m_clear_info.total_cb_att += getComboAttack( m_pool.combo );  // コンボ数を攻撃力に変換

            if ( m_clear_info.b2b > 1 && m_attack > 0 )
            {
                ++m_clear_info.total_b2b;  // B2Bの回数をカウントアップ
            }

            {
                // パフェチェック
                int i = gem_add_y + m_pool.height();
                for ( ; i >= 0; --i ) {
                    if ( m_pool.m_row[i] ) break;
                }

                if ( i < 0 ) {
                    m_clear_info.pc = 1;
                    ++m_clear_info.total_pc;
                }
            }

            int special = 0;  // Tスピンのとき1
            //if ( m_clear_info.gem_num == 1 && m_clearLines >= 4 )
            //{
            //    special = 1;
            //    m_clear_info.normal[4] += 1;
            //}
            if ( m_attack > 0 )
            {
                // 攻撃が発生したとき  // 空Tは含まない
                if ( m_clear_info.wallkick_spin ) {
                    special = 1;
                    //if ( m_clear_info.gem_num == 2 )
                    {
                        if ( wallkick_spin == 2 && (AI::isEnableAllSpin() || m_clear_info.clears == 1) ) {
                        //if ( m_clear_info.wallkick_spin == 2 ) {
                            ++m_clear_info.t[0];
                        } else {
                            ++m_clear_info.t[m_clear_info.clears];
                        }
                    }
                }
                //if ( m_clear_info.gem_num == 2 )
                //{
                //    int att = m_attack;
                //    att -= m_pool.b2b > 1;
                //    att -= getComboAttack( m_pool.combo );
                //    if ( m_clear_info.pc ) {
                //        att -= 6;
                //    }
                //    if ( att / m_clear_info.clears >= 2 ) { // T1T2T3
                //        special = 1;
                //        ++m_clear_info.t[m_clear_info.clears];
                //    } else if ( m_clear_info.clears == 2 ) { // double
                //        //++m_clear_info.normal[2];
                //    } else if ( m_clear_info.clears == 1 && att == 1 ) { // T0
                //        special = 1;
                //        ++m_clear_info.t[0];
                //    }
                //}
            }


            if ( m_clearLines > 0 && special == 0 )
            {
                ++m_clear_info.normal[m_clearLines];
            }

            m_state = STATE_READY;
        }
        void addRow( int att ) {
            {
                for ( int y = 1; y < 64; ++y ) {
                    for (int x = 0 ; x < poolw(); ++x ) {
                        m_color_pool[y-1][x] = m_color_pool[y][x];
                    }
                }
                for (int x = 0 ; x < poolw(); ++x ) {
                    if ( att & ( 1 << x ) ) {
                        m_color_pool[poolh() + 32][x] = 8;
                    } else {
                        m_color_pool[poolh() + 32][x] = 0;
                    }
                }
            }
            m_pool.addRow( att );
            if ( m_cur_y > 1 ) {
                m_cur_y -= 1;
            }
            if ( m_pool.m_row[0] ) {
                m_state = STATE_OVER;
            }
        }
        void setRow( int y, int att ) {
            {
                for (int x = 0 ; x < poolw(); ++x ) {
                    if ( att & ( 1 << x ) ) {
                        m_color_pool[y + 32][x] = 8;
                    } else {
                        m_color_pool[y + 32][x] = 0;
                    }
                }
            }
            m_pool.row[y] = att;
        }
        // ネクストの先頭のミノを削除する
        // 削除されるミノはmemberなどに記録はされないので、この関数を前に取っておく必要がある
        void removeNext() {
            for (int i = 1; i < m_next_num; ++i) {
                m_next[i - 1] = m_next[i];
            }
            --m_next_num;
            //m_next[15] = AI::getGem( m_rand.randint(7) + 1, 0);
        }
        // 新しいミノの取り出しに成功したら true。そもそも取り出せなかったら false
        // ミノを取り出した結果ゲーム終了になっても戻り値は true で、 m_state が変化する
        bool newpiece() {
            // Ready状態ではなければ離脱。Ready状態には以下の状態になる必要がある
            //     reset()を呼ぶ
            //     clearLines()を呼ぶ
            if ( m_state != STATE_READY ) return false;

            // ミノを初期位置に移動
            m_cur_x = AI::gem_beg_x;
            m_cur_y = AI::gem_beg_y;

            // これまでに使用したミノの個数をカウントアップ
            ++m_curnum;

            // ネクストの先頭からミノを取り出す
            m_cur = m_next[0];

            // ホールドを使用可能にする
            m_hold = false;

            // 壁蹴りの状態をリセット
            wallkick_spin = 0;

            // ネクストからミノを取り出したので、先頭を削除する
            removeNext();

            // 取り出した直後、ミノがすでに置けない状態ならゲーム終了
            //if ( m_pool.row[0] || m_pool.row[1] || m_pool.isCollide(m_cur_x, m_cur_y, m_cur)) {
            if ( m_pool.isCollide(m_cur_x, m_cur_y, m_cur) ) {
                m_state = STATE_OVER;
                return true;
            }

            // Moving状態にする
            m_state = STATE_MOVING;
            return true;
        }
        int poolw() const {
            return m_pool.width();
        }
        int poolh() const {
            return m_pool.height();
        }
        int curx() const {
            return m_cur_x;
        }
        int cury() const {
            return m_cur_y;
        }
        // 未使用
        int getCurGemCell(int x, int y) const {
            if ( m_cur.bitmap[y] & ( 1 << x ) ) return 1;
            return 0;
        }
        // 未使用
        int getNextGemCell(int next, int x, int y) const {
            if ( m_next[next].bitmap[y] & ( 1 << x ) ) return 1;
            return 0;
        }
        // 未使用
        int getPoolCell(int x, int y) const {
            return m_color_pool[y+32][x];
            //if ( m_pool.row[y + 1] & ( 1 << x) ) return 1;
            return 0;
        }
        // ゲームオーバー判定になｔっていないか
        bool alive () const {
            return m_state != STATE_OVER;
        }
    public:
        int m_state;  // 現在のステータス。「ミノを取り出す準備完了」「操作中である」「ミノを地形に反映済み」「ゲーム終了」を表す
    public:
        //  m_pool と m_color_pool のフィールドは、同じになるよう更新されている

        AI::GameField m_pool;  // フィールドの状態。色なしのフィールドは m_color_pool。ホールド中のミノの種類や継続中のコンボ数などの情報も含む
        AI::Gem m_cur;  // 現在操作しているミノ。ステータスがSTATE_MOVING以外では、NULLのGemの可能性もある
        int m_color_pool[64][32];  // 色付きのフィールド。色なしのフィールドは m_pool に含まれる
        int m_hold;  // 現在の操作中ミノですでにホールドしたときtrue。newpiece()でフラグがリセットされる
        int m_cur_x, m_cur_y;  // 操作中のミノの位置
        int m_curnum;  // これまでに使用したミノの個数。操作中のミノを含む
        signed char wallkick_spin;  // 現在の壁蹴りの状態。回転すると1or2になり、移動をするとリセットされる
        AI::Gem m_next[1024];  // ネクストのミノ
        int m_next_num;  // ネクストに保存されているミノの個数
        point m_base, m_size;  // TetrisGameで初期化されている。しかし、どこからも参照されていないため、用途は不明
        int m_clearLines;  // 最後の操作で消去されたライン数
        int m_attack;  // 最後の操作で発生した攻撃力
        int m_max_combo;  // ゲームを通して、最も大きいコンボ数 （1回目のライン消去で0RENとしてカウントする）

        int m_frames;  // 実質的に未使用のため不明
        int m_drop_frame;  // 実質的に未使用のため不明
        clear_info m_clear_info;  // 最後のclearLines()で発生した情報群
    };

}
