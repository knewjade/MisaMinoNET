#include "main.h"
#include "tetris_ai.h"
#include <assert.h>
#define USING_MOV_D     0
#define GENMOV_W_MASK   15   // 0b1111
#define SWITCH_USING_HEIGHT_OPT

#define _MACRO_CREATE_MOVINGSIMPLE(arg_action_name,arg_wkspin,arg_sd) \
    MovingSimple nm = m; \
    nm.x = nx; \
    nm.y = ny; \
    nm.spin = ns; \
    nm.lastmove = Moving::arg_action_name; \
    nm.wallkick_spin = arg_wkspin; \
	nm.softdrop = arg_sd;

#define _MACRO_CREATE_MOVING(arg_action_name,arg_wkspin) \
    Moving nm = m; \
    nm.x = nx; \
    nm.y = ny; \
    nm.spin = ns; \
    nm.movs.push_back(Moving::arg_action_name); \
    nm.wallkick_spin = arg_wkspin

// arg_hash_table[ny][ns][nx & GENMOV_W_MASK]みたいになる
#define _MACRO_HASH_POS(arg_hash_table,arg_prefix) \
    arg_hash_table[arg_prefix##y][arg_prefix##s][arg_prefix##x & GENMOV_W_MASK]

namespace AI {
    bool g_spin180 = false;
    std::vector<int> g_combo_attack;
    bool g_allSpin = false;  // KOSの設定
    bool g_softdrop = true;
    //bool g_softdrop = false;

    void setSpin180( bool enable ) {
        g_spin180 = enable;
    }
    bool spin180Enable() {
        return g_spin180;
    }

    void setComboList( std::vector<int> combolist ) {
        g_combo_attack = combolist;
    }
    int getComboAttack( int combo ) {
		if (g_combo_attack.empty()) return 0;
		return (int) ((double)((combo >= (int)g_combo_attack.size()) ? g_combo_attack.back() : g_combo_attack[combo]) * MisaBot.tetris.m_ai_param.combo / 30);
    }
    void setAllSpin(bool allSpin) {
        g_allSpin = allSpin;
    }
    bool isEnableAllSpin() {
        return g_allSpin;
    }
    void setSoftdrop( bool softdrop ) {
        g_softdrop = softdrop;
    }
    bool softdropEnable() {
        return g_softdrop;
    }
    //enum {
    //    MOV_SCORE_DROP = 1,
    //    MOV_SCORE_LR = 10,
    //    MOV_SCORE_LR2 = 19,
    //    MOV_SCORE_LLRR = 100,
    //    MOV_SCORE_D = 1000,
    //    MOV_SCORE_DD = 3000,
    //    MOV_SCORE_SPIN = 20,
    //};
    enum {
        MOV_SCORE_DROP = 1,  // ハードドロップ  // のはずが未使用  // 結局すべての最後の動作はDROPなので、実質的に無意味
        MOV_SCORE_LR = 80,  // 1回目の左右移動・1回目の回転操作
        MOV_SCORE_LR2 = 200,  // 2連続同じ方向への左右移動  // いわゆるコンコン  // 左右移動2回目以降、1回ずつ加算される
        MOV_SCORE_LLRR = 100,  // 左右に移動できるだけ移動する
        MOV_SCORE_D = 1000,  // 下移動  // USING_MOV_D がオンであること
        MOV_SCORE_DD = 3000,  // 下にいけるところまでソフトドロップ
        MOV_SCORE_SPIN = 150,  // 回転操作（2回目以降。1回目の回転は MOV_SCORE_LR）
    };

    // TODO: @param x 探索開始時のx座標?
    // TODO: @param y 探索開始時のy座標?
    // @param hold ホールド開始のときtrue。（prefix的に）最初の操作を表現する用で、探索には使われない
    void GenMoving(const GameField& field, std::vector<MovingSimple> & movs, Gem cur, int x, int y, bool hold) {
        movs.clear();
		if (cur.num == 0) { // rare race condition, we're dead already if this happens
			assert(true); // debug break
			cur = AI::getGem(AI::GEMTYPE_I, 0);
		}
        if ( field.isCollide(x, y, getGem(cur.num, cur.spin) ) ) {
            return ;
        }
        //if ( field.isCollide(x, y + 1, getGem(cur.num, cur.spin) ) ) {
        //    return ;
        //}
        // 確認したことがある状態ならフラグをたてる
        // wallkickの状態で桁を分けて保存する
        //    0b001: wallkick=0で到達できる
        //    0b010: wallkick=1で到達できる  // その場での回転
        //    0b100  wallkick=2で到達できる  // 壁蹴りありでの回転
        char _hash[64][4][GENMOV_W_MASK+1] = {0};
        char _hash_drop[64][4][GENMOV_W_MASK+1] = {0};
        char (*hash)[4][GENMOV_W_MASK+1] = &_hash[gem_add_y];
        char (*hash_drop)[4][GENMOV_W_MASK+1] = &_hash_drop[gem_add_y];
        MovList<MovingSimple> q(1024);

#ifdef SWITCH_USING_HEIGHT_OPT
        // height of every column
        // 各xについて、最も高い位置にあるブロックのyを取得
        // ブロックがないときh+1
        // 下端にブロックがあるときh
        int field_w = field.width(), field_h = field.height();
        int min_y[32] = {0};  // wまで使用  // 使っていないx列は0
        {
            int beg_y = -5;
            while ( field.row[beg_y] == 0 ) ++beg_y;
            for ( int x = 0; x < field_w; ++x ) {
                for ( int y = beg_y, ey = field_h + 1; y <= ey; ++y ) {
                    if ( field.row[y] & ( 1 << x ) ) {
                        min_y[x] = y;
                        break;
                    }
                }
            }
        }
#endif
        // 初期状態の作成
        {
            MovingSimple m;
            m.x = x;
            m.y = y;
            m.spin = cur.spin;
            m.wallkick_spin = 0;
            if ( hold ) {
                m.lastmove = MovingSimple::MOV_HOLD;
                m.hold = true;
            } else {
                m.lastmove = MovingSimple::MOV_NULL;
                m.hold = false;
            }
            q.push(m);
            hash[m.y][m.spin][m.x & GENMOV_W_MASK] = 1;
        }

        // 最後の操作がドロップになるまで、pop->1操作進める->pushを繰り返す
        //   - `q` から操作をpopする
        //   - ドロップなら `movs` に記録
        //   - そこから、一つの操作（Movingの操作用enumを参照）を行って `q` にpushする
        while ( ! q.empty() ) {
            MovingSimple m;
            q.pop(m);
            //if ( m.y < -1 ) continue;

            // 最後の操作がドロップなら終了
            if ( m.lastmove == MovingSimple::MOV_DROP ) {
                // フィールド内であることを確認する
                if ( getGemMaxH(cur.num, m.spin) + m.y <= 2 ) // lockout
                    continue;
                movs.push_back(m);
                continue;
            }

            // 下移動
            // 最後の操作がD,DD以外  // 同じ処理を繰り返すのを防ぐ
            if ( m.lastmove != MovingSimple::MOV_DD && m.lastmove != MovingSimple::MOV_D )
            {
                // D,DDの処理
                int nx = m.x, ny = m.y, ns = m.spin;
                int wallkick_spin = m.wallkick_spin;
#ifndef SWITCH_USING_HEIGHT_OPT
                while ( field.row[ny + cur.geth()] == 0 && ny + cur.geth() <= field.height() ) { // �ǿ����в���ʹ�õ��Ż�
                    ++ny; wallkick_spin = 0;
                }
                while ( ! field.isCollide(nx, ny + 1, getGem(cur.num, ns) ) ) {
                    if ( !USING_MOV_D && ( _MACRO_HASH_POS(hash, n) & 1 ) == 0) {
                        _MACRO_HASH_POS(hash, n) |= 1;
                    }
                    ++ny; wallkick_spin = 0;
                }
#endif
#ifdef SWITCH_USING_HEIGHT_OPT
                {
                    int dist_min = 0x7fffffff;  // 地形に一番近いミノブロックとの距離  // 接着していたら0のはず
                    for ( int x = 0; x < 4; ++x ) {
                        if ( getGemColH(cur.num, ns, x) ) { // 0 = empty column
                            // ミノの各xについて一番下のブロックと、地形の一番上のブロックの距離を算出
                            int dist_cur_col = min_y[nx + x] - (ny + getGemColH(cur.num, ns, x));

                            if ( dist_cur_col < dist_min ) {
                                dist_min = dist_cur_col;
                            }
                        }
                    }

                    // 地形の表面より下にミノがあるとき
                    // たとえば http://fumen.zui.jp/?v115@fgA8AeI8AeI8AeI8EeI8AeI8AeI8OexRJ とか
                    if ( dist_min < 0 ) { // underground
                        // フィールドをぶつかるまで下に落とす
                        while ( ! field.isCollide(nx, ny + 1, getGem(cur.num, ns) ) ) {
                            if ( !USING_MOV_D && (_MACRO_HASH_POS(hash,n) & 1) == 0) {
                                _MACRO_HASH_POS(hash,n) |= 1;  // ドロップ中に通った場所を記録
                            }
                            ++ny;
                            wallkick_spin = 0;  // kick終わりじゃない
                        }
                    } else { // under the sun
                        ny = ny + dist_min;
                        if ( dist_min > 0 ) wallkick_spin = 0;
                        for ( int y = m.y + 1; y < ny; ++y ) {
                            if ( !USING_MOV_D && (hash[y][ns][nx & GENMOV_W_MASK] & 1) == 0) {
                                hash[y][ns][nx & GENMOV_W_MASK] |= 1;  // ドロップ中に通った場所を記録
                            }
                        }
                    }

                    // この時点でnyはフィールドと接着している状態
                }
#endif
                {
                    int v_spin = (isEnableAllSpin() || cur.num == GEMTYPE_T) ? wallkick_spin : 0;

                    if ( (_MACRO_HASH_POS(hash_drop, n) & ( 1 << v_spin)) == 0 )
                    {
                        // wallkickがv_spinの状態で、到達したことがない

                        int _nx = nx, _ny = ny, _ns = ns;

                        //if ( cur.num == GEMTYPE_I || cur.num == GEMTYPE_Z || cur.num == GEMTYPE_S ) {
                        //    if ( ns == 2 ) {
                        //        _ny = ny + 1;
                        //        _ns = 0;
                        //    }
                        //    if ( ns == 3 ) {
                        //        _nx = nx + 1;
                        //        _ns = 1;
                        //    }
                        //}

                        //if ( (_MACRO_HASH_POS(hash_drop, _n) & ( 1 << v_spin)) == 0 )
                        {
                                _MACRO_CREATE_MOVINGSIMPLE(MOV_DROP, v_spin, m.softdrop);
                                _MACRO_HASH_POS(hash_drop, _n) |= 1 << v_spin;
                                q.push(nm);  // 候補追加
                        }
                    }

                    if ( softdropEnable() ) // DD
                    {
                        if ( ny != y ) {
                            // ハードドロップした結果、開始時点から移動した場合

                            if ( ( _MACRO_HASH_POS(hash, n) & 1 ) == 0) {
                                // wallkick==0でまだ通ってない
                                _MACRO_CREATE_MOVINGSIMPLE(MOV_DD, 0, m.softdrop);
                                _MACRO_HASH_POS(hash, n) |= 1;
								nm.softdrop = true;
                                q.push(nm);  // 候補追加
                            }
                        }
                    }
                }
            }

            // 左移動
            {
                int nx = m.x, ny = m.y, ns = m.spin;
                --nx;
                if ( ( _MACRO_HASH_POS(hash, n) & 1 ) == 0) {
                    // wallkick==0でまだ通ってない

                    if ( ! field.isCollide(nx, ny, getGem(cur.num, ns) ) ) {
                        // フィールドと重ならない

                        _MACRO_CREATE_MOVINGSIMPLE(MOV_L, 0, m.softdrop);
                        _MACRO_HASH_POS(hash, n) |= 1;
                        q.push(nm);  // 候補追加

                        if ( m.lastmove != MovingSimple::MOV_L && m.lastmove != MovingSimple::MOV_R
                            && m.lastmove != MovingSimple::MOV_LL && m.lastmove != MovingSimple::MOV_RR )
                        {
                            // 最後の移動が左右の移動以外のとき

                            int nx = m.x - 1, ny = m.y, ns = m.spin;
                            while ( ! field.isCollide(nx - 1, ny, getGem(cur.num, ns) ) ) {
                                --nx;
                            }

                            // 左にいけるところまで移動

                            if ( nx != x && ( _MACRO_HASH_POS(hash, n) & 1 ) == 0) {
                                _MACRO_CREATE_MOVINGSIMPLE(MOV_LL, 0, m.softdrop);
                                _MACRO_HASH_POS(hash, n) |= 1;
                                q.push(nm); // 候補追加
                            }
                        }
                    }
                }
            }

            // 右移動  // 左移動と同じ
            {
                int nx = m.x, ny = m.y, ns = m.spin;
                ++nx;
                if ( ( _MACRO_HASH_POS(hash, n) & 1 ) == 0) {
                    if ( ! field.isCollide(nx, ny, getGem(cur.num, ns) ) ) {
                        _MACRO_CREATE_MOVINGSIMPLE(MOV_R, 0, m.softdrop);
                        _MACRO_HASH_POS(hash, n) |= 1;
                        q.push(nm);
                        if ( m.lastmove != MovingSimple::MOV_L && m.lastmove != MovingSimple::MOV_R
                            && m.lastmove != MovingSimple::MOV_LL && m.lastmove != MovingSimple::MOV_RR )
                        {
                            int nx = m.x + 1, ny = m.y, ns = m.spin;
                            while ( ! field.isCollide(nx + 1, ny, getGem(cur.num, ns) ) ) {
                                ++nx;
                            }
                            if ( nx != x && ( _MACRO_HASH_POS(hash, n) & 1 ) == 0) {
                                _MACRO_CREATE_MOVINGSIMPLE(MOV_RR, 0, m.softdrop);
                                _MACRO_HASH_POS(hash, n) |= 1;
                                q.push(nm);
                            }
                        }
                    }
                }
            }
#if USING_MOV_D > 0
            if ( m.lastmove != MovingSimple::MOV_DD )
            {
                int nx = m.x, ny = m.y, ns = m.spin;
                ++ny;
                MovingSimple nm = m;
                while ( field.row[ny + cur.geth()] == 0 && ny + cur.geth() <= field.height() ) { // �ǿ����в���ʹ�õ��Ż�
                    ++ny;
                    nm.lastmove = MovingSimple::MOV_D;
                }
                if ( ( _MACRO_HASH_POS(hash, n) & 1 ) == 0) {
                    if ( ! field.isCollide(nx, ny, getGem(cur.num, ns) ) ) {
                        _MACRO_CREATE_MOVINGSIMPLE(MOV_D, 0);
                        _MACRO_HASH_POS(hash, n) |= 1;
                        q.push(nm);
                    }
                }
            }
#endif
            // 左回転
            {
                int nx = m.x, ny = m.y, ns = (m.spin + 1) % cur.mod;
                if ( ns != m.spin ) {
                    // ミノの回転方向がかわったとき

                    if ( (isEnableAllSpin() || cur.num == GEMTYPE_T) ) {
                        // スピン

                        if ( ! field.isCollide(nx, ny, getGem(cur.num, ns) ) ) {
                            // フィールドと重ならないとき  // 壁蹴りは発生しない

                            if ( ( _MACRO_HASH_POS(hash, n) & ( 1 << 1 ) ) == 0 ) {
                                // wallkick==1 (その場での回転)でまだ通っていない

                                _MACRO_CREATE_MOVINGSIMPLE(MOV_LSPIN, 1, m.softdrop);
                                _MACRO_HASH_POS(hash, n) |= 1 << 1;
                                q.push(nm);
                            }
                        } else if ( field.wallkickTest(nx, ny, getGem(cur.num, ns), 0 ) ) {
                            // 壁蹴りで移動できるとき

                            if ( ( _MACRO_HASH_POS(hash, n) & ( 1 << 2 ) ) == 0 ) {
                                // wallkick==2 (壁蹴りあり)でまだ通っていない

                                _MACRO_CREATE_MOVINGSIMPLE(MOV_LSPIN, 2, m.softdrop);
                                _MACRO_HASH_POS(hash, n) |= 1 << 2;
                                q.push(nm);
                            }
                        }
                    } else {
                        if ( ! field.isCollide(nx, ny, getGem(cur.num, ns) ) 
                            || field.wallkickTest(nx, ny, getGem(cur.num, ns), 0 ) ) {
                            // フィールドと重ならない || 壁蹴りで移動できる とき

                            if ( ( _MACRO_HASH_POS(hash, n) & 1 ) == 0 ) {
                                // wallkick==0でまだ通ってない

                                _MACRO_CREATE_MOVINGSIMPLE(MOV_LSPIN, 0, m.softdrop);
                                _MACRO_HASH_POS(hash, n) |= 1;
                                q.push(nm);
                            }
                        }
                    }
                }
            }

            // 右回転
            {
                int nx = m.x, ny = m.y, ns = (m.spin + 3) % cur.mod;
                if ( ns != m.spin ) {
                    if ( (isEnableAllSpin() || cur.num == GEMTYPE_T) ) {
                        if ( ! field.isCollide(nx, ny, getGem(cur.num, ns) ) ) {
                            if ( ( _MACRO_HASH_POS(hash, n) & ( 1 << 1 ) ) == 0 ) {
                                _MACRO_CREATE_MOVINGSIMPLE(MOV_RSPIN, 1, m.softdrop);
                                _MACRO_HASH_POS(hash, n) |= 1 << 1;
                                q.push(nm);
                            }
                        } else if ( field.wallkickTest(nx, ny, getGem(cur.num, ns), 1 ) ) {
                            if ( ( _MACRO_HASH_POS(hash, n) & ( 1 << 2 ) ) == 0 ) {
                                _MACRO_CREATE_MOVINGSIMPLE(MOV_RSPIN, 2, m.softdrop);
                                _MACRO_HASH_POS(hash, n) |= 1 << 2;
                                q.push(nm);
                            }
                        }
                    } else {
                        if ( ! field.isCollide(nx, ny, getGem(cur.num, ns) )
                            || field.wallkickTest(nx, ny, getGem(cur.num, ns), 1 ) ) {
                            if ( ( _MACRO_HASH_POS(hash, n) & 1 ) == 0 ) {
                                _MACRO_CREATE_MOVINGSIMPLE(MOV_RSPIN, 0, m.softdrop);
                                _MACRO_HASH_POS(hash, n) |= 1;
                                q.push(nm);
                            }
                        }
                    }
                }
            }

            // 180度回転
            if ( spin180Enable() && m.lastmove != MovingSimple::MOV_SPIN2 ) // no 180 wallkick only
            {
                int nx = m.x, ny = m.y, ns = (m.spin + 2) % cur.mod;
                if ( ns != m.spin ) {
                    if ( ( _MACRO_HASH_POS(hash, n) & 1 ) == 0) {
                        if ( ! field.isCollide(nx, ny, getGem(cur.num, ns) ) ) {
                            _MACRO_CREATE_MOVINGSIMPLE(MOV_SPIN2, 1, m.softdrop);
                            _MACRO_HASH_POS(hash, n) |= 1;
                            q.push(nm);
                        }
                    }
                }
            }
        }
    }

    // 移動スコア付きですべての移動先を列挙する
    // ダイクストラ法で実装されている。
    // ところどころ `_MACRO_HASH_POS(hash, n) |= 1;` 的なコードがコメントアウトされているのはそのため。
    // もし、見つけた瞬間に訪問したことを記録すると、
    // 後からみつかるコストがより少ない操作をスルーしてしまうので、pop()した後にフラグを立てている
    // 全体的に `GenMoving()` と同じ構造をしているので、コメントは気持ち少なめになっています
    void FindPathMoving(const GameField& field, std::vector<Moving> & movs, Gem cur, int x, int y, bool hold) {
        movs.clear();

        if ( field.isCollide(x, y, getGem(cur.num, cur.spin) ) ) {
            return ;
        }
        char _hash[64][4][GENMOV_W_MASK+1] = {0};
        char _hash_drop[64][4][GENMOV_W_MASK+1] = {0};
        char (*hash)[4][GENMOV_W_MASK+1] = &_hash[gem_add_y];
        char (*hash_drop)[4][GENMOV_W_MASK+1] = &_hash_drop[gem_add_y];
        MovQueue<Moving> q(1024);  // 内部的にスコア順にソートしている

        {
            Moving m;
            m.x = x;
            m.y = y;
            m.spin = cur.spin;
            m.wallkick_spin = 0;
            if ( hold ) {
                m.movs.push_back(Moving::MOV_HOLD);
            } else {
                m.movs.push_back(Moving::MOV_NULL);
            }
            m.score = 0;
            q.push(m);
            //hash[m.y][m.spin][m.x & GENMOV_W_MASK] = 1;
        }
        while ( ! q.empty() ) {
            Moving m;
            q.pop(m);  // スコアが低い順に取り出される
            if ( m.movs.back() == Moving::MOV_DROP) {
                movs.push_back(m);
                continue;
            }

            // 同じところを訪れたらスキップする
            // pop()でスコアが小さい順に取り出されるので、スキップされるのはほかに良いスコアがあるケース
            {
                if ( (isEnableAllSpin() || cur.num == GEMTYPE_T) ) {
                    if ( hash[m.y][m.spin][m.x & GENMOV_W_MASK] & ( 1 << m.wallkick_spin ) )
                        continue;
                    hash[m.y][m.spin][m.x & GENMOV_W_MASK] |= 1 << m.wallkick_spin;
                } else {
                    if ( hash[m.y][m.spin][m.x & GENMOV_W_MASK] & 1 )
                        continue;
                    hash[m.y][m.spin][m.x & GENMOV_W_MASK] |= 1;
                }
            }

            // 下移動
            if ( m.movs.back() != Moving::MOV_DD && m.movs.back() != Moving::MOV_D)
            {
                int nx = m.x, ny = m.y, ns = m.spin;
                int wallkick_spin = m.wallkick_spin;
                //while ( field.row[ny + cur.geth()] == 0 && ny + cur.geth() <= field.height() ) { // �ǿ����в���ʹ�õ��Ż�
                //    ++ny; wallkick_spin = 0;
                //}
                while ( ! field.isCollide(nx, ny + 1, getGem(cur.num, ns) ) ) {
                    //if ( !USING_MOV_D && ( _MACRO_HASH_POS(hash, n) & 1 ) == 0) {
                    //    _MACRO_HASH_POS(hash, n) |= 1;
                    //}
                    ++ny; wallkick_spin = 0;
                }
                {
                    int v_spin = (isEnableAllSpin() || cur.num == GEMTYPE_T) ? wallkick_spin : 0;
                    if ( (_MACRO_HASH_POS(hash_drop, n) & ( 1 << v_spin )) == 0 )
                    {
                        int _nx = nx, _ny = ny, _ns = ns;
                        //if ( (_MACRO_HASH_POS(hash_drop, _n) & ( 1 << v_spin)) == 0 )
                        {
                                _MACRO_CREATE_MOVING(MOV_DROP, v_spin);

                                // 唯一、q.push()するときに訪れたことを記録している
                                // つまり、スコアより先にみつかったものが優先される。
                                // おそらくドロップする直前までは最良の操作となるため、結果に影響ないと判断したと思われる
                                _MACRO_HASH_POS(hash_drop, _n) |= 1 << v_spin;
                                q.push(nm);

                                // DROPのスコアは使われていない
                                // 最終的にすべての動作はDROPで終わるためだと思われる
                        }
                    }
                    if ( softdropEnable() ) {
                        if ( ny != y ) {
                            if ( ( _MACRO_HASH_POS(hash, n) & 1 ) == 0) {
                                _MACRO_CREATE_MOVING(MOV_DD, 0);
                                //_MACRO_HASH_POS(hash, n) |= 1;
                                nm.score += MOV_SCORE_DD - nm.movs.size();  // TODO: nm.movs.size()?
                                q.push(nm);
                            }
                        }
                    }
                }
            }

            // 左移動
            {
                int nx = m.x, ny = m.y, ns = m.spin;
                --nx;
                if ( ( _MACRO_HASH_POS(hash, n) & 1 ) == 0) {
                    if ( ! field.isCollide(nx, ny, getGem(cur.num, ns) ) ) {
                        _MACRO_CREATE_MOVING(MOV_L, 0);
                        //_MACRO_HASH_POS(hash, n) = 1;
                        if ( m.movs.back() != Moving::MOV_L )
                            nm.score += MOV_SCORE_LR;
                        else
                            nm.score += MOV_SCORE_LR2;
                        q.push(nm);
                        if ( m.movs.back() != Moving::MOV_L && m.movs.back() != Moving::MOV_R
                            && m.movs.back() != Moving::MOV_LL && m.movs.back() != Moving::MOV_RR )
                        {
                            int nx = m.x - 1, ny = m.y, ns = m.spin;
                            while ( ! field.isCollide(nx - 1, ny, getGem(cur.num, ns) ) ) {
                                --nx;
                            }
                            if ( nx != x && ( _MACRO_HASH_POS(hash, n) & 1 ) == 0) {
                                _MACRO_CREATE_MOVING(MOV_LL, 0);
                                //_MACRO_HASH_POS(hash, n) |= 1;
                                nm.score += MOV_SCORE_LLRR;
                                q.push(nm);
                            }
                        }
                    }
                }
            }

            // 右移動
            {
                int nx = m.x, ny = m.y, ns = m.spin;
                ++nx;
                if ( ( _MACRO_HASH_POS(hash, n) & 1 ) == 0) {
                    if ( ! field.isCollide(nx, ny, getGem(cur.num, ns) ) ) {
                        _MACRO_CREATE_MOVING(MOV_R, 0);
                        //_MACRO_HASH_POS(hash, n) |= 1;
                        if ( m.movs.back() != Moving::MOV_R )
                            nm.score += MOV_SCORE_LR;
                        else
                            nm.score += MOV_SCORE_LR2;  // ひとつ前の操作もMOV_Rのとき
                        q.push(nm);
                        if ( m.movs.back() != Moving::MOV_L && m.movs.back() != Moving::MOV_R
                            && m.movs.back() != Moving::MOV_LL && m.movs.back() != Moving::MOV_RR )
                        {
                            int nx = m.x + 1, ny = m.y, ns = m.spin;
                            while ( ! field.isCollide(nx + 1, ny, getGem(cur.num, ns) ) ) {
                                ++nx;
                            }
                            if ( nx != x && ( _MACRO_HASH_POS(hash, n) & 1 ) == 0) {
                                _MACRO_CREATE_MOVING(MOV_RR, 0);
                                //_MACRO_HASH_POS(hash, n) |= 1;
                                nm.score += MOV_SCORE_LLRR;
                                q.push(nm);
                            }
                        }
                    }
                }
            }

            // 1段下移動
            //if (USING_MOV_D)
            if ( m.movs.back() != Moving::MOV_DD )
            {
                int nx = m.x, ny = m.y, ns = m.spin;
                ++ny;
                if ( ( _MACRO_HASH_POS(hash, n) & 1 ) == 0) {
                    if ( ! field.isCollide(nx, ny, getGem(cur.num, ns) ) ) {
                        _MACRO_CREATE_MOVING(MOV_D, 0);
                        //_MACRO_HASH_POS(hash, n) |= 1;
                        nm.score += MOV_SCORE_D;
                        q.push(nm);
                    }
                }
            }

            // 左回転
            {
                int nx = m.x, ny = m.y, ns = (m.spin + 1) % cur.mod;
                if ( ns != m.spin ) {
                    if ( (isEnableAllSpin() || cur.num == GEMTYPE_T) ) {
                        if ( ! field.isCollide(nx, ny, getGem(cur.num, ns) ) ) {
                            // フィールドと重ならないとき  // 壁蹴りは発生しない

                            if ( ( _MACRO_HASH_POS(hash, n) & ( 1 << 1 ) ) == 0 ) {
                                _MACRO_CREATE_MOVING(MOV_LSPIN, 1);
                                //_MACRO_HASH_POS(hash, n) |= 1 << 1;
                                if ( m.movs.back() != Moving::MOV_LSPIN )
                                    nm.score += MOV_SCORE_LR;  // 最後の操作が左回転ではない（左回転1回目） → 左右移動と同じスコアらしい
                                else
                                    nm.score += MOV_SCORE_SPIN;  // 最後の操作が左回転のとき
                                q.push(nm);
                            }
                        } else if ( field.wallkickTest(nx, ny, getGem(cur.num, ns), 0 ) ) {
                            // 壁蹴りで移動できるとき

                            if ( ( _MACRO_HASH_POS(hash, n) & ( 1 << 2 ) ) == 0 ) {
                                _MACRO_CREATE_MOVING(MOV_LSPIN, 2);
                                //_MACRO_HASH_POS(hash, n) |= 1 << 2;
                                // 上と同じ
                                if ( m.movs.back() != Moving::MOV_LSPIN )
                                    nm.score += MOV_SCORE_LR;
                                else
                                    nm.score += MOV_SCORE_SPIN;
                                q.push(nm);
                            }
                        }
                    } else {
                        if ( ! field.isCollide(nx, ny, getGem(cur.num, ns) )
                            || field.wallkickTest(nx, ny, getGem(cur.num, ns), 0 ) ) {
                            // フィールドと重ならない || 壁蹴りで移動できる とき

                            if ( ( _MACRO_HASH_POS(hash, n) & 1 ) == 0 ) {
                                _MACRO_CREATE_MOVING(MOV_LSPIN, 0);
                                //_MACRO_HASH_POS(hash, n) |= 1;
                                // 上と同じ
                                if ( m.movs.back() != Moving::MOV_LSPIN )
                                    nm.score += MOV_SCORE_LR;
                                else
                                    nm.score += MOV_SCORE_SPIN;
                                q.push(nm);
                            }
                        }
                    }
                }
            }

            // 右回転
            {
                int nx = m.x, ny = m.y, ns = (m.spin + 3) % cur.mod;
                if ( ns != m.spin ) {
                    if ( (isEnableAllSpin() || cur.num == GEMTYPE_T) ) {
                        if ( ! field.isCollide(nx, ny, getGem(cur.num, ns) ) ) {
                            if ( ( _MACRO_HASH_POS(hash, n) & ( 1 << 1 ) ) == 0 ) {
                                _MACRO_CREATE_MOVING(MOV_RSPIN, 1);
                                //_MACRO_HASH_POS(hash, n) |= 1 << 1;
                                if ( m.movs.back() != Moving::MOV_RSPIN )
                                    nm.score += MOV_SCORE_LR;
                                else
                                    nm.score += MOV_SCORE_SPIN;
                                q.push(nm);
                            }
                        } else if ( field.wallkickTest(nx, ny, getGem(cur.num, ns), 1 ) ) {
                            if ( ( _MACRO_HASH_POS(hash, n) & ( 1 << 2 ) ) == 0 ) {
                                _MACRO_CREATE_MOVING(MOV_RSPIN, 2);
                                //_MACRO_HASH_POS(hash, n) |= 1 << 2;
                                if ( m.movs.back() != Moving::MOV_RSPIN )
                                    nm.score += MOV_SCORE_LR;
                                else
                                    nm.score += MOV_SCORE_SPIN;
                                q.push(nm);
                            }
                        }
                    } else {
                        if ( ! field.isCollide(nx, ny, getGem(cur.num, ns) )
                            || field.wallkickTest(nx, ny, getGem(cur.num, ns), 1 ) ) {
                            if ( ( _MACRO_HASH_POS(hash, n) & 1 ) == 0 ) {
                                _MACRO_CREATE_MOVING(MOV_RSPIN, 0);
                                //_MACRO_HASH_POS(hash, n) |= 1;
                                if ( m.movs.back() != Moving::MOV_RSPIN )
                                    nm.score += MOV_SCORE_LR;
                                else
                                    nm.score += MOV_SCORE_SPIN;
                                q.push(nm);
                            }
                        }
                    }
                }
            }

            // 180度回転
            if ( spin180Enable() ) //&& m.movs.back() != Moving::MOV_SPIN2 ) // no 180 wallkick only
            {
                int nx = m.x, ny = m.y, ns = (m.spin + 2) % cur.mod;
                if ( ns != m.spin ) {
                    if ( ( _MACRO_HASH_POS(hash, n) & 1 ) == 0) {
                        if ( ! field.isCollide(nx, ny, getGem(cur.num, ns) ) ) {
                            _MACRO_CREATE_MOVING(MOV_SPIN2, 1);
                            //_MACRO_HASH_POS(hash, n) |= 1;
                                if ( m.movs.back() != Moving::MOV_SPIN2 )
                                    nm.score += MOV_SCORE_LR;
                                else
                                    nm.score += MOV_SCORE_SPIN;
                            q.push(nm);
                        }
                    }
                }
            }
        }
    }
}
