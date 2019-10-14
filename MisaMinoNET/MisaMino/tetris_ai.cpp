#include "tetris_ai.h"
#include <cmath>
#include <deque>
#include <map>
#include <set>

#include "tetris_setting.h"
#include <assert.h>

#define GENMOV_W_MASK   15
#define GEN_MOV_NO_PUSH 0

namespace AI {
    struct _ai_settings {
        bool hash;
        bool combo;
        bool strategy_4w;
        _ai_settings() {
            hash = true;
            combo = true;
            strategy_4w = false;
        }
    } ai_settings[2];

    void setAIsettings(int player, const char* key, int val) {
        if ( strcmp(key, "hash") == 0 ) {
            ai_settings[player].hash = val;
        } else if ( strcmp(key, "combo") == 0 ) {
            ai_settings[player].combo = val;
        } else if ( strcmp(key, "4w") == 0 ) {
            ai_settings[player].strategy_4w = val;
        }
    }

    int Evaluate( long long &clearScore, double &avg_height, const AI_Param& ai_param, const GameField& last_pool, const GameField& pool, int cur_num,
        int curdepth,
        int total_clear_att, int total_clears, int clear_att, int clears, signed char wallkick_spin,
        int lastCombo, int t_dis, int upcomeAtt
        ) {
        int score = 0;
        // ��߶�
        //int last_min_y[32] = {0};
        int min_y[32] = {0};
        int emptys[32] = {0};
        int maxy_index = 31, maxy_cnt = 0;
        int maxy_flat_cnt = 0; // �ƽ̨
        int miny_val = 31;
        int total_hole = 0;
        int beg_y = -5;
        const int pool_w = pool.width(), pool_h = pool.height();
        //last_min_y[31] = -1;
        min_y[31] = -1;
        {
            //while ( last_pool.row[beg_y] == 0 ) ++beg_y;
            //for ( int x = 0; x < pool_w; ++x) {
            //    for ( int y = beg_y, ey = pool_h + 1; y <= ey; ++y) { // Ҫ�е��б�����pool.h����������
            //        if ( last_pool.row[y] & ( 1 << x ) ) {
            //            last_min_y[x] = y;
            //            break;
            //        }
            //    }
            //}
            beg_y = -5;
            while ( pool.row[beg_y] == 0 ) ++beg_y;
            for ( int x = 0; x < pool_w; ++x) {
                for ( int y = beg_y, ey = pool_h + 1; y <= ey; ++y) { // Ҫ�е��б�����pool.h����������
                    if ( pool.row[y] & ( 1 << x ) ) {
                        min_y[x] = y;
                        miny_val = std::min(miny_val, y);
                        if ( y > min_y[maxy_index]) {
                            maxy_index = x;
                            maxy_cnt = 0;
                        } else if ( y == min_y[maxy_index]) {
                            ++maxy_cnt;
                        }
                        break;
                    }
                }
            }
            int transitions = 0;
            for ( int y = beg_y; y <= pool_h; ++y) {
                int last = 1; //pool.row[y] & 1;
                if ( pool.row[y] > 0 ) {
                    for ( int x = 0; x < pool_w; ++x) {
                        if ( pool.row[y] & ( 1 << x ) ) {
                            if ( last == 0 ) ++transitions;
                            last = 1;
                        } else {
                            if ( last == 1 ) ++transitions;
                            last = 0;
                            ++emptys[y];
                        }
                    }
                } else {
                    emptys[y] = pool_w;
                }
                transitions += !last;
            }
            score += ai_param.v_transitions * transitions / 10;
        }
        min_y[pool_w] = min_y[pool_w-2];
        //last_min_y[pool_w] = last_min_y[pool_w-2];

        if ( pool.m_hold == GEMTYPE_I ) {
            score -= ai_param.hold_I;
        }
        if ( pool.m_hold == GEMTYPE_T ) {
            score -= ai_param.hold_T;
        }
        if ( maxy_cnt > 0 ) {
            int ybeg = min_y[maxy_index];
            unsigned rowdata = pool.row[ybeg-1];
            int empty = ~rowdata & pool.m_w_mask;
            for ( int b = maxy_index; b < pool_w; ++b ) {
                if ( ybeg != min_y[b] ) continue;
                int cnt = 1;
                for ( int b1 = b + 1; empty & ( 1 << b1); ++b1) ++cnt;
                if ( maxy_flat_cnt < cnt ) {
                    maxy_flat_cnt = cnt;
                    maxy_index = b;
                }
            }
        }
        // ��������
        int x_holes[32] = {0}; // ˮƽ���򶴵�����
        int y_holes[32] = {0}; // ��ֱ���򶴵�����
        int x_op_holes[32] = {0}; // ˮƽ���򶴵�����
        //int last_pool_hole_score;
        int pool_hole_score;
        int pool_total_cell = 0;
        //{   // last_pool
        //    int x_holes[32] = {0}; // ˮƽ���򶴵�����
        //    int x_op_holes[32] = {0}; // ˮƽ���򶴵�����
        //    int first_hole_y[32] = {0}; // ��ֱ��������Ķ���y
        //    int hole_score = 0;
        //    const GameField& _pool = last_pool;
        //    for ( int x = 0; x < pool_w; ++x) {
        //        int last = 0, next;
        //        first_hole_y[x] = pool_h + 1;
        //        for ( int y = last_min_y[x] + 1; y <= pool_h; ++y, last = next) {
        //            if ( ( _pool.row[y] & ( 1 << x ) ) == 0) {
        //                next = 1;
        //                if ( x > 1 ) {
        //                    if (last_min_y[x-1] > y && last_min_y[x-2] > y) {
        //                        if ( last == 0) {
        //                            hole_score += ai_param.open_hole;
        //                            if ( y >= 0 ) ++x_op_holes[y];
        //                            continue;
        //                        }
        //                    }
        //                }
        //                if ( x < pool_w - 2 ) {
        //                    if (last_min_y[x+1] > y && last_min_y[x+2] > y) {
        //                        if ( last == 0) {
        //                            hole_score += ai_param.open_hole;
        //                            if ( y >= 0 ) ++x_op_holes[y];
        //                            continue;
        //                        }
        //                    }
        //                }
        //                if ( y >= 0 ) ++x_holes[y];
        //                if ( first_hole_y[x] > pool_h ) {
        //                    first_hole_y[x] = y;
        //                }
        //                if ( last ) {
        //                    hole_score += ai_param.hole / 4;
        //                } else {
        //                    //score += (y - min_y[x]) * ai_param.hole_dis;
        //                    //if ( x_holes[y] > 2 ) {
        //                    //    hole_score += ai_param.hole / 4;
        //                    //} else
        //                    if ( x_holes[y] >= 2 ) {
        //                        hole_score += ai_param.hole * x_holes[y];
        //                    } else {
        //                        hole_score += ai_param.hole * 2;
        //                    }
        //                }
        //            } else {
        //                next = 0;
        //            }
        //        }
        //    }
        //    //if(1)
        //    for ( int y = 0; y <= pool_h; ++y) {
        //        if ( x_holes[y] > 0 ) {
        //            hole_score += ai_param.hole_dis * (pool_h - y + 1);
        //            //int min_dis = pool_h;
        //            //for ( int x = 0; x < pool_w; ++x) {
        //            //    if ( first_hole_y[x] == y ) {
        //            //        min_dis = std::min(min_dis, y - min_y[x]);
        //            //    }
        //            //}
        //            //if ( min_dis == 1 ) {
        //            //    bool fill = true;
        //            //    for ( int i = y - min_dis; i < y ; ++i ) {
        //            //        int empty = ~pool.row[i] & pool.m_w_mask;
        //            //        if ( empty & ( empty - 1) ) {
        //            //            fill = false;
        //            //            break;
        //            //        }
        //            //    }
        //            //    if ( fill ) {
        //            //        score -= ai_param.hole_dis;
        //            //    }
        //            //}
        //            break;
        //        }
        //    }
        //    //for ( int y = 0; y <= pool_h; ++y) {
        //    //    if ( x_holes[y] ) score += ai_param.has_hole_row;
        //    //}
        //    last_pool_hole_score = hole_score;
        //}
        {   // pool
            int first_hole_y[32] = {0}; // ��ֱ��������Ķ���y
            int x_renholes[32] = {0}; // ��ֱ������������
            double hole_score = 0;
            const GameField& _pool = pool;
            for ( int x = 0; x < pool_w; ++x) {
                for ( int y = min_y[x]; y <= pool_h; ++y ) {
                    if ( ( _pool.row[y] & ( 1 << x ) ) == 0 ) {
                        pool_total_cell++;
                    }
                }
            }
            for ( int x = 0; x < pool_w; ++x) {
                int last = 0, next;
                first_hole_y[x] = pool_h + 1;
                int y = (x>0) ? std::min(min_y[x] + 1, std::max(min_y[x-1] + 6, min_y[x+1] + 6)) : min_y[x] + 1;
                for ( ; y <= pool_h; ++y, last = next) {
                    if ( ( _pool.row[y] & ( 1 << x ) ) == 0 ) { //_pool.row[y] && 
                        double factor = ( y < 20 ? ( 1 + (20 - y) / 20.0 * 2 ) : 1.0);
                        y_holes[x]++;
                        next = 1;
                        if ( softdropEnable() ) {
                            if ( x > 1 ) {
                                if (min_y[x-1] > y && min_y[x-2] > y) {
                                    //if ( last == 0) {
                                    hole_score += ai_param.open_hole * factor;
                                    if ( y >= 0 ) ++x_op_holes[y];
                                    continue;
                                    //}
                                }
                            }
                            if ( x < pool_w - 2 ) {
                                if (min_y[x+1] > y && min_y[x+2] > y) {
                                    //if ( last == 0) {
                                    hole_score += ai_param.open_hole * factor;
                                    if ( y >= 0 ) ++x_op_holes[y];
                                    continue;
                                    //}
                                }
                            }
                        }
                        if ( y >= 0 ) ++x_holes[y];
                        if ( first_hole_y[x] > pool_h ) {
                            first_hole_y[x] = y;
                        }
                        int hs = 0;
                        if ( last ) {
                            hs += ai_param.hole / 2;
                            if ( y >= 0 ) ++x_renholes[y];
                        } else {
                            hs += ai_param.hole * 2;
                        }
                        {
                            //if ( x_holes[y] == 2 ) {
                            //    hs -= ai_param.hole;
                            //} else if ( x_holes[y] >= 3 ){
                            //    hs -= ai_param.hole * 2;
                            //}
                            ++total_hole;
                        }
                        hole_score += hs * factor;
                    } else {
                        next = 0;
                    }
                }
            }
            //for ( int y = 0; y <= pool_h; ++y) {
            //    if ( x_holes[y] > 1 ) {
            //        int n = x_holes[y] - x_renholes[y];
            //        int hs = 0;
            //        if ( n == 2 )
            //            hs = ai_param.hole + x_renholes[y] * ai_param.hole / 2;
            //        else if ( n > 2 )
            //            hs = (n - 2) * ai_param.hole * 2 + x_renholes[y] * ai_param.hole / 2;
            //        hole_score -= hs * ( y < 10 ? ( 1 + (10 - y) / 10.0 * 2 ) : 1.0);
            //        score -= ai_param.v_transitions * x_holes[y] / 10;
            //    }
            //}
            for ( int y = 0; y <= pool_h; ++y) {
                if ( x_holes[y] > 0 ) {
                    score += ai_param.hole_dis * (pool_h - y + 1);
                    break;
                }
            }
            if(1)
            if ( ai_param.hole_dis_factor ) {
                for ( int y = 0, cnt = 5, index = -1; y <= pool_h; ++y) {
                    if ( x_holes[y] > 0 ) {
                        if ( cnt > 0 ) --cnt, ++index;
                        else break;
                        for ( int x = 0; x <= pool_w; ++x) {
                            if ( ( _pool.row[y] & ( 1 << x ) ) == 0) {
                                int h = y - min_y[x];
                                if ( h > 4 - index ) h = 4 + (h - (4 - index)) * cnt / 4;
                                //if ( h > 4 ) h = 4;
                                if ( h > 0 ) {
                                    if ( ( _pool.row[y - 1] & ( 1 << x ) ) != 0) {
                                        score += ai_param.hole_dis_factor * h * cnt / 5 / 2;
                                    }
                                }
                            }
                        }
                    }
                }
            }
            if(1)
            if ( ai_param.hole_dis_factor2 ) {
                int miny = pool_h;
                for ( int y = 0, cnt = 0; y <= pool_h; ++y) {
                    if ( x_holes[y] > 0 ) {
                        if ( cnt < 4 ) ++cnt;
                        else break;
                        for ( int x = 0; x <= pool_w; ++x) {
                            if ( ( _pool.row[y] & ( 1 << x ) ) == 0) {
                                int vy = min_y[x] + cnt * 1;
                                if ( vy < miny ) miny = vy;
                            }
                        }
                    }
                }
                for ( int x = 0; x <= pool_w; ++x) {
                    int vy = min_y[x] + 6;
                    if ( vy < miny ) miny = vy;
                }
                double total_emptys = 0;
                for ( int y = miny; y <= pool_h; ++y ) {
                    total_emptys += emptys[y] * ( y < 10 ? (10 + 10 - y ) / 10.0 : 1);
                }
                score += ai_param.hole_dis_factor2 * total_emptys / 4;
                //score += ai_param.hole_dis_factor2 * (pool_h - miny);
                //int h = 0;
                //h = min_y[maxy_index] - miny - 1;
                //if ( h > 0 )
                //    score += ai_param.hole_dis_factor2 * h;
                //for ( int x = 0; x <= pool_w; ++x) {
                //    h += min_y[x] - miny;
                //}
                //if ( h > 0 )
                //    score += ai_param.hole_dis_factor2 * h * 2 / pool_w;
            }
            //for ( int y = 0; y <= pool_h; ++y) {
            //    if ( x_holes[y] ) score += ai_param.has_hole_row;
            //}
            pool_hole_score = hole_score;
        }
        score += pool_hole_score;

        // �߶Ȳ�
        {
            //int n_maxy_index = maxy_index;
            //if ( maxy_cnt != 0 ) n_maxy_index = -9;

            int last = min_y[1];
            for ( int x = 0; x <= pool_w; last = min_y[x], ++x) {
                int v = min_y[x] - last;
                if ( x == maxy_index ) {
                    x += maxy_flat_cnt;
                    continue;
                }
                int absv = abs(v);
                if ( x + 1 == maxy_index && v > 0 || x - 2 == maxy_index && v < 0 ) ; //
                else score += absv * ai_param.h_factor;
            }
        }
        // ƽ��
        /*
        {
            int last = -1, len = 0;
            for ( int x = 0; x <= pool_w; ++x) {
                if ( last == min_y[x] ) {
                    ++len;
                } else {
                    if ( len > 0 && len < 4) {
                        score -= ((len - 1) / 3 + 1) * ai_param.flat_factor;
                    }
                    len = 0;
                    last = min_y[x];
                }
            }
            if ( len > 0 && len < 4) {
                score -= ((len - 1) / 3 + 1) * ai_param.flat_factor;
            }
        }
        */
        int center = 10; // ��¥������
        double warning_factor = 1;
        int h_variance_score = 0;
        // �㷽��
        {
            int avg = 0;
            {
                int sum = 0;
                int sample_cnt = 0;
                for ( int x = 0; x < pool_w; ++x) {
                    avg += min_y[x];
                }
                if (0)
                {
                    double h = pool_h - (double)avg / pool_w;
                    score += int(ai_param.miny_factor * h * h / pool_h);
                }
                else
                {
                    int h = std::min(std::min(min_y[gem_beg_x], min_y[gem_beg_x+1]), std::min(min_y[gem_beg_x+2], min_y[gem_beg_x+3]));
                    if ( h < 8 )
                    {
                        score += int(ai_param.miny_factor * ( 8 - h ) * 2);
                    }
                }
                if (1)
                {
                    if ( avg < pool_w * center ) {
                        warning_factor = 0.0 + (double)avg / pool_w / center / 1;
                    }
                }
                // ƫ��ֵ
                {
                    int dif_sum = 0;
                    for ( int x = 0; x < pool_w; ++x) {
                        dif_sum += abs( min_y[x] * pool_w - avg );
                    }
                    score += ai_param.dif_factor * dif_sum / pool_w / pool_w;
                }
            }
            // ��������
            {
                int s = 0;
                int t_att = total_clear_att;
                double t_clear = total_clears; //+ total_clears / 4.0;
				double h = pool_h - (double)avg / pool_w;
                if ( pool.b2b ) s -= (int) ((double)(ai_param.b2b * 5) / (1 + (TSD_only? 0 : pow(5, h - 6.5)))) + 2; // b2b score
                if ( t_clear > 0 ) {
                    s -= int( ((ai_param.clear_efficient) * ( t_att ) ) );
                }
                {
                    //if ( t_clear > t_att ) {
                        //int warning_factor = 0.5 + (double)avg / pool_w / center / 2;
                        s += int( warning_factor * t_clear * ai_param.clear_useless_factor);
                    //}
                }
                int cs = 0;
                if ( cur_num == GEMTYPE_T && wallkick_spin && clears > 0 && ai_param.tspin > 0 ) { // T�����ӷ֣�Ҫ��T1/T2��״�����ִ�һ
                    s -= ai_param.hold_T;
                    if ( clears >= 3 ) {
                        if ( clear_att >= clears * 2 ) { // T3
                            cs -= int( warning_factor * (ai_param.tspin3 * 8 ) + ai_param.hole * 2 );
                        }
                    } else if ( clears >= 2 ) {
                        if ( clear_att >= clears * 2 ) { // T2
                            cs -= int( warning_factor * (ai_param.tspin * 5 + ai_param.open_hole / 2) );
                        }
                    } else if ( wallkick_spin == 1 ) { // T1
                        cs -= int( warning_factor * (ai_param.tspin * 1 + ai_param.open_hole / 2) );
                    } else if ( wallkick_spin == 2 ) { // Tmini
						cs -= int(warning_factor * (ai_param.tmini / 2));
						if (ai_param.tmini == 0) cs += 100000000;
                    }
                }
                clearScore += cs;

                if (1)
                if ( clears > 0 && upcomeAtt >= 4 && ai_param.upcomeAtt > 0 ) {
                    int cur_s = 0;
                    cur_s -= int( ((ai_param.clear_efficient) * ( clear_att ) ) )
                        - int( warning_factor * clears * ai_param.clear_useless_factor);
                    if ( avg - (12 + upcomeAtt) * pool_w > 0 && cur_s + cs < 0 )
                        s -= (cur_s + cs) * ( avg - (12 + upcomeAtt) * pool_w ) * ai_param.upcomeAtt / pool_w / 10 / 20;
                    //if ( upcomeAtt >= 4 ) {
                    //    if ( total_hole < 4 && avg - upcomeAtt * pool_w >= 8 * pool_w ) {
                    //        s = s - s * ( 4 - total_hole ) * ai_param.upcomeAtt / 40;
                    //    }
                    //}
                }

                score += s;
            }
            //if ( clears ) {
            //    int center = 10; // ��¥������
            //    double factor = 1;
            //    if ( avg < pool_w * center ) {
            //        factor = (double)avg / pool_w / center;
            //    }
            //    int s = 0;
            //    if ( pool_hole_score < last_pool_hole_score ) {
            //        s -= int( factor * (ai_param.clear_efficient * ( clear_att ) * ( clear_att ) / clears) );
            //        //s -= ai_param.open_hole;
            //        if ( clear_att >= 4 ) {
            //            if ( clear_att >= clears * 2 ) { // T2/T3
            //                clearScore -= int( factor * (ai_param.tspin * 4 + ai_param.open_hole + ai_param.clear_efficient * ( clear_att ) ) );
            //                s -= ai_param.hold_T;
            //            }
            //        }
            //        if ( clears > clear_att ) {
            //            s += int( factor * (ai_param.clear_efficient * ( clears - clear_att ) / 2 ) );
            //        }
            //    } else if ( pool_hole_score == last_pool_hole_score ) {
            //        s -= int( factor * (ai_param.clear_efficient * ( clear_att ) * ( clear_att ) / clears) );
            //        if ( clear_att >= 4 ) {
            //            if ( clear_att >= clears * 2 ) { // T2/T3
            //                clearScore -= int( factor * (ai_param.tspin * 4 + ai_param.open_hole + ai_param.clear_efficient * ( clear_att ) ) );
            //                s -= ai_param.hold_T;
            //            }
            //        } else if ( clear_att >= clears ) {
            //            if ( clear_att >= clears * 2 ) {
            //                if ( clears == 1 ) { // T1
            //                    //s += int( factor * (ai_param.clear_efficient * ( clear_att ) / clears) );
            //                }
            //            }
            //        } else if ( avg < 8 * pool_w ) {
            //            //s += int(ai_param.hole * ( clears - clear_att ) * factor / 2 );
            //            if ( clears > clear_att ) {
            //                s += int( factor * (ai_param.clear_efficient * ( clears - clear_att ) / 2 ) );
            //            }
            //        } else if ( total_hole >= 1 || min_y[maxy_index] < pool_h - 4 ) {
            //            if ( clears > clear_att ) {
            //                s += int( factor * (ai_param.clear_efficient * ( clears - clear_att ) * 2 ) );
            //            }
            //            //if ( clear_att == 0 ) {
            //            //    s += int( factor * (ai_param.hole * ( clears - clear_att ) ) / 3 );
            //            //}
            //        } else {
            //            if ( clears > clear_att ) {
            //                s += int( factor * (ai_param.clear_efficient * ( clears - clear_att ) * 4) );
            //            }
            //            //if ( clear_att == 0 ) {
            //            //    s += int( factor * (ai_param.hole * ( clears - clear_att ) ) / 3 );
            //            //}
            //        }
            //    } else {
            //        s -= int( factor * (ai_param.clear_efficient * ( clear_att ) / clears) );
            //        if ( clears > clear_att ) {
            //            s += int( factor * (ai_param.clear_efficient * ( clears - clear_att ) * 4 ) );
            //        }
            //    }
            //    if ( pool.combo > 2 )
            //    {
            //        int combo = pool.combo - 2;
            //        //clearScore -= combo * combo * ai_param.combo_factor;
            //    }
            //    score += s;
            //}
        }

        // ������״�ж�

        // ����ɹ�����Tetris��T2��
        //int t2_x[32] = {0};
        if ( maxy_cnt == 0 )
        {
            //if ( maxy_index == 0 || maxy_index == pool_w - 1 ) {
            //    score += ai_param.att_col_sel_side;
            //}
            int ybeg = 0;
            if ( softdropEnable() && maxy_index > 0 && maxy_index < pool_w - 1 && ai_param.tspin > 0 ) { // T1/T2������״��
                ybeg = std::max( min_y[maxy_index - 1], min_y[maxy_index + 1] );
                if ( min_y[maxy_index - 1] == min_y[maxy_index + 1]
                    && x_holes[ybeg] == 0 && (!ybeg || x_holes[ybeg-1] == 0)
                    && x_op_holes[ybeg] == 0 && (!ybeg || x_op_holes[ybeg-1] == 0)
                    )
                { // T׼��
                    int cnt = 0;
                    if ( maxy_index > 1 && min_y[maxy_index - 2] >= min_y[maxy_index - 1] - 2 ) ++cnt;
                    if ( maxy_index < pool_w - 2 && min_y[maxy_index + 2] >= min_y[maxy_index + 1] - 2 ) ++cnt;
                    if ( cnt > 0 )
                    {
                        score -= int(warning_factor * ai_param.tspin);
                        if ( (~pool.row[ybeg] & pool.m_w_mask) == (1 << maxy_index) ) { // T1����
                            score -= int(warning_factor * ai_param.tspin);
                            if ( (~pool.row[ybeg - 1] & pool.m_w_mask) == (7 << (maxy_index-1) ) ) { // ��T2������
                                score -= int( warning_factor * (ai_param.tspin * cnt) );
                            }
                        }
                    }
                } else if ( ybeg <= 6 && ybeg - t_dis > 1 || ybeg > 6 ) {
                    int row_data = pool.row[ybeg - 1];
                    if ( (row_data & ( 1 << (maxy_index-1) ) ) == 0 && (row_data & ( 1 << (maxy_index+1) ) ) == 0 // �ӵ�����Ϊ��
                         && x_holes[ybeg] == 0 && (!ybeg || x_holes[ybeg-1] == 0) // ����λ���޶�
                         && x_op_holes[ybeg] == 0 && (!ybeg || x_op_holes[ybeg-1] <= 1)
                         )
                    {
                        // T����״
                        if ( ( pool.row[ybeg] & (1 << (maxy_index-1)) ) && ( pool.row[ybeg] & (1 << (maxy_index+1)) ) ) { // �ӵ������������
                            if ( !!( pool.row[ybeg-2] & (1 << (maxy_index-1)) ) + !!( pool.row[ybeg-2] & (1 << (maxy_index+1)) ) == 1 ) { // �ӵ�����Ŀ����
                                double s = 0;
                                //t2_x[maxy_index] = ybeg;
                                double factor = ybeg > 6 ? 0.5 : 1 - t_dis / 6.0 * 0.5;
                                if ( warning_factor < 1 )
                                    factor = ybeg > 6 ? 1.0 / 5 : 1 / (1 + t_dis / 3.0);
                                s += ai_param.open_hole;
                                if ( (~pool.row[ybeg] & pool.m_w_mask) == (1 << maxy_index) ) { // ��T1
                                    s += ai_param.tspin + ai_param.tspin * 1 * factor;
                                    if ( (~row_data & pool.m_w_mask) == (7 << (maxy_index-1) ) ) { // ��T2������
                                        s += ai_param.tspin * 3 * factor;
                                        // s -= ai_param.tspin * 3 / factor / 1;
                                    }
                                } else {
                                    s += ai_param.tspin * 1 + ai_param.tspin * 2 * factor / 2 ;
                                }
                                score -= int( warning_factor * s );
                            }
                        }
                    }
                }
            } else {
                if ( maxy_index == 0 ) {
                    ybeg = min_y[maxy_index + 1];
                } else {
                    ybeg = min_y[maxy_index - 1];
                }
            }
            int readatt = 0;
            int last = pool.row[ybeg];
            for ( int y = ybeg; y <= pool_h; ++y ) {
                if ( last != pool.row[y] ) break;
                int row_data = ~pool.row[y] & pool.m_w_mask;
                if ( (row_data & (row_data - 1)) != 0 ) break;
                ++readatt;
            }
            if ( readatt > 4 ) readatt = 4;
            //score -= readatt * ai_param.readyatt;

        }
        // T3 形状判定 (Shape jugdement/ how to determine tst shape?)
        //3001	
        //2000	
        // 1101	
        // 1x01	
        // 1101	
        // Fumen visualization: https://tinyurl.com/y2ygrqtw 
        // 1003
        // 0002
        //1011 
        //10x1
        //1011
        if ( softdropEnable() && ai_param.tspin3 > 0 )
        {
            for ( int y = 3; y < pool_h; ++y ) {
                if ( x_holes[y] == 0 ) continue;
                for ( int x = 1; x < pool_w - 1; ++x ) {
                    if ( ( pool.row[y + 1] & ( 1 << x ) ) == 0 || ( pool.row[y + 1] & ( 1 << x ) ) == 0  ) {
                        continue; // 上下无洞 (No holes top or bottom)
                    }
                    int row_y[5];
                    for ( int i = 0; i < 5; ++i ) {
                        row_y[i] = ( (pool.row[y - 3 + i] | (3 << pool_w)) << 2 ) | 3;
                    }
                    if ( ( (row_y[3] >> (x + 1)) & ( 7 ) ) == 1 /*100*/ ) { // 上图情况 (See above diagram)
                        if ( x == pool_w - 2 ) continue;
                        //if ( t2_x[x+1] == y ) continue; // 排除T2坑 (ignore tsd hole)
                        // 所有空的地方先匹配 (Match all empty space first)
                        if (   ( (row_y[2] >> (x + 1)) & ( 7 ) ) != 3 /*110*/
                            //|| ( (row_y[4] >> (x + 1)) & ( 15 ) ) != 11 /*1101*/
                            || ( (row_y[4] >> (x + 1)) & ( 13 ) ) != 9 /*1011mask=1001*/
                            || ( (row_y[1] >> (x + 1)) & ( 7 ) ) != 0 /*000*/
                            //|| ( (row_y[0] >> (x + 1)) & ( 3 ) ) != 0 /*00*/
                            ) {
                            continue;
                        }
                        if ( min_y[x] != y - 1 || min_y[x-1] != y - 1 ) {
                            continue;
                        }
                        if ( ( row_y[0] & ( 1 << (x) ) ) == 0 && ( row_y[1] & ( 1 << (x) ) ) ) {
                            continue; // 高处转角 (High turning corner -> Overhang for kick?)
                        }
                        if ( min_y[x + 1] > y ) { // 洞判定 (find holes)
                            if ( x_holes[y - 1] > 0 || x_holes[y + 1] > 0 || x_holes[y] > 1
                                || x_op_holes[y - 1] > 0 || x_op_holes[y + 1] > 0 || x_op_holes[y] > 0)
                            {
                                continue;
                            }
                        } else {
                            if ( x_holes[y - 1] > 1 || x_holes[y + 1] > 1 || x_holes[y] > 2
                                || x_op_holes[y - 1] > 0 || x_op_holes[y + 1] > 0 || x_op_holes[y] > 0)
                            {
                                continue;
                            }
                        }
                        if ( ( (row_y[0] >> (x + 3)) & ( 1 ) ) == 0 && y - min_y[x + 2] > 3 ) continue;
                        int s = 0;
                        //tp3 * 1
                        s -= int( warning_factor * ai_param.tspin3 );// + int( warning_factor * ( ai_param.tspin * 4 + ai_param.open_hole ) );
                        score += s;
                        if ( y <= pool_h - 3 && ( pool.row[y + 3] & ( 1 << x ) ) == 0 ) {
                            int r = ~pool.row[y + 3] & pool.m_w_mask;
                            if ( ( r & ( r - 1 ) ) == 0 ) {
                                score -= int( warning_factor * (ai_param.tspin * 4 + ai_param.open_hole) );
                            }
                        }
                        //int full = 0;
                        {
                            int e = ~(pool.row[y + 1] | (1<<x) ) & pool.m_w_mask;
                            e &= ( e-1); 
                            if ( e == 0 ) { // 最底只剩一空 (Bottom only has one space)
                                //++full;
                            } else {
                                score -= s;
                                continue;
                            }
                        }
                        {
                            int e = ~(pool.row[y] | (1<<(x+2))) & pool.m_w_mask;
                            e &= ( e-1 );
                            if ( (e & ( e-1)) == 0 ) { // 底二只剩两空 (2nd row only has 2 spaces left)
                                //++full;
                            } else {
                                if ( (pool.row[y] & (1<<(x+2))) == 0 ) {
                                    score -= int( warning_factor * ai_param.tspin3 * 3 );
                                }
                                score -= s;
                                score -= int( warning_factor * ai_param.tspin3 / 3 );
                                continue;
                            }
                        }
                        {
                            int e = ~pool.row[y - 1] & pool.m_w_mask;
                            e &= ( e-1 );
                            if ( e == 0 ) { // 底三只剩一空 (3rd row only has 1 space left)
                                //++full;
                            } else {
                                score -= s;
                                score -= int( warning_factor * ai_param.tspin3 );
                                continue;
                            }
                        }
                        score -= int( warning_factor * ai_param.tspin3 * 3 );
                        if ( pool.row[y-3] & ( 1 << (x + 1)) ) {
                            if ( t_dis < 7 ) {
                                score -= int( warning_factor * ( ai_param.tspin3 * 1 ) + ai_param.hole * 2);
                                score -= int( warning_factor * ai_param.tspin3 * 3 / ( t_dis + 1 ) );
                            }
                        }
                    } else if ( ( (row_y[3] >> (x+1) ) & ( 7 ) ) == 4 /*001*/ ) { // 镜像情况 (Mirrored)
                        if ( x == 1 ) continue;
                        //if ( t2_x[x-1] == y ) continue; // �ų�T2��
                        // ���пյĵط���ƥ��
                        if (   ( (row_y[2] >> (x+1)) & ( 7 ) ) != 6 /*011*/
                            //|| ( (row_y[4] >> (x)) & ( 15 ) ) != 13 /*1011*/
                            || ( (row_y[4] >> (x)) & ( 11 ) ) != 9 /*1101mask=1001*/
                            || ( (row_y[1] >> (x + 1)) & ( 7 ) ) != 0 /*000*/
                            //|| ( (row_y[0] >> (x + 1)) & ( 3 ) ) != 0 /*00*/
                            ) {
                            continue;
                        }
                        if ( min_y[x] != y - 1 || min_y[x+1] != y - 1 ) {
                            continue;
                        }
                        if ( ( row_y[0] & ( 1 << (x + 4) ) ) == 0 && ( row_y[1] & ( 1 << (x + 4) ) ) ) {
                            continue; // �ߴ�ת��
                        }
                        if ( min_y[x - 1] > y ) { // ���ж�
                            if ( x_holes[y - 1] > 0 || x_holes[y + 1] > 0 || x_holes[y] > 1
                                || x_op_holes[y - 1] > 0 || x_op_holes[y + 1] > 0 || x_op_holes[y] > 0)
                            {
                                continue;
                            }
                        } else {
                            if ( x_holes[y - 1] > 1 || x_holes[y + 1] > 1 || x_holes[y] > 2
                                || x_op_holes[y - 1] > 0 || x_op_holes[y + 1] > 0 || x_op_holes[y] > 0)
                            {
                                continue;
                            }
                        }
                        if ( ( (row_y[0] >> (x + 1)) & ( 1 ) ) == 0 && y - min_y[x - 2] > 3 ) continue;
                        int s = 0;
                        // tp3 * 1
                        s -= int( warning_factor * ai_param.tspin3 );// + int( warning_factor * ( ai_param.tspin * 4 + ai_param.open_hole ) );
                        score += s;
                        if ( y <= pool_h - 3 && ( pool.row[y + 3] & ( 1 << x ) ) == 0 ) {
                            int r = ~pool.row[y + 3] & pool.m_w_mask;
                            if ( ( r & ( r - 1 ) ) == 0 ) {
                                score -= int( warning_factor * (ai_param.tspin * 4 + ai_param.open_hole) );
                            }
                        }
                        //int full = 0;
                        {
                            int e = ~(pool.row[y + 1] | (1<<x) ) & pool.m_w_mask;
                            e &= ( e-1);
                            if ( e == 0 ) { // ���ֻʣһ��
                                //++full;
                            } else {
                                score -= s;
                                continue;
                            }
                        }
                        {
                            int e = ~(pool.row[y] | (1<<x-2)) & pool.m_w_mask;
                            e &= ( e-1);
                            if ( (e & ( e-1)) == 0 ) { // �׶�ֻʣ����
                                //++full;
                            } else {
                                if ( (pool.row[y] & (1<<(x-2))) == 0 ) {
                                    score -= int( warning_factor * ai_param.tspin3 * 3 );
                                }
                                score -= s;
                                score -= int( warning_factor * ai_param.tspin3 / 3 );
                                continue;
                            }
                        }
                        {
                            int e = ~pool.row[y - 1] & pool.m_w_mask;
                            e &= ( e-1);
                            if ( e == 0 ) { // ����ֻʣһ��
                                //++full;
                            } else {
                                score -= s;
                                score -= int( warning_factor * ai_param.tspin3 );
                                continue;
                            }
                        }
                        score -= int( warning_factor * ai_param.tspin3 * 3 );
                        if ( pool.row[y-3] & ( 1 << (x - 1)) ) {
                            if ( t_dis < 7 ) {
                                score -= int( warning_factor * ( ai_param.tspin3 * 1 ) + ai_param.hole * 2);
                                score -= int( warning_factor * ai_param.tspin3 * 3 / ( t_dis + 1 ) );
                            }
                        }
                    }
                }
            }
        }
        // 4W��״�ж�
        if ( USE4W )
        if ( ai_param.strategy_4w > 0 && total_clears < 1 ) //&& lastCombo < 1 && pool.combo < 1 )
        {
            int maxy_4w = min_y[3];
            maxy_4w = std::max(maxy_4w, min_y[4] );
            maxy_4w = std::max(maxy_4w, min_y[5] );
            maxy_4w = std::max(maxy_4w, min_y[6] );
            int maxy_4w_combo = min_y[0];
            maxy_4w_combo = std::max(maxy_4w_combo, min_y[1] );
            maxy_4w_combo = std::max(maxy_4w_combo, min_y[2] );
            maxy_4w_combo = std::max(maxy_4w_combo, min_y[pool_w-3] );
            maxy_4w_combo = std::max(maxy_4w_combo, min_y[pool_w-2] );
            maxy_4w_combo = std::max(maxy_4w_combo, min_y[pool_w-1] );
            if ( (min_y[4] < min_y[3] && min_y[4] <= min_y[5])
                || (min_y[5] < min_y[6] && min_y[5] <= min_y[4]) )
            {
                maxy_4w = -10;
            } else
            for ( int x = 0; x < pool_w; ++x ) {
                if ( min_y[x] > maxy_4w ) {
                    maxy_4w = -10;
                    break;
                }
            }
            while ( maxy_4w > 0 ) {
                //if ( abs( min_y[0] - min_y[1] ) > 4 ) { maxy_4w = -10; break; }
                //if ( abs( min_y[1] - min_y[2] ) > 4 ) { maxy_4w = -10; break; }
                //if ( abs( min_y[pool_w-1] - min_y[pool_w-2] ) > 4 ) { maxy_4w = -10; break; }
                //if ( abs( min_y[pool_w-2] - min_y[pool_w-3] ) > 4 ) { maxy_4w = -10; break; }
                //if ( abs( min_y[2] - min_y[pool_w-3] ) > 7 ) { maxy_4w = -10; break; }
                //int avg = (min_y[0] + min_y[1] + min_y[2] + min_y[pool_w-1] + min_y[pool_w-2] + min_y[pool_w-3]) / 6;
                if ( (pool_h - maxy_4w) * 2 >= maxy_4w - maxy_4w_combo ) {
                    maxy_4w = -10;
                    break;
                }
                break;
            }
            if ( maxy_4w <= pool_h - 4 ) { // ����г���4�����оͲ���
                maxy_4w = -10;
            }
            //if ( maxy_4w - maxy_4w_combo > 15 ) { // ����г���10Ԥ���оͲ���
            //    maxy_4w = -10;
            //}
            if ( maxy_4w - maxy_4w_combo < 9 && pool_hole_score > ai_param.hole * (maxy_4w - maxy_4w_combo) / 2 ) {
                maxy_4w = -10;
            }

            if ( maxy_4w > 8 ) {
                bool has_hole = false;
                for ( int y = maxy_4w - 1; y >= 0; --y ) {
                    if ( x_holes[y] || x_op_holes[y] ) {
                        has_hole = true;
                        break;
                    }
                }
                if ( ! has_hole && maxy_4w < pool_h ) {
                    if ( x_holes[maxy_4w]>1 || x_op_holes[maxy_4w]>1 ) {
                        has_hole = true;
                    }
                }

                if ( ! has_hole )
                {
                    int sum = maxy_4w - min_y[3];
                    sum += maxy_4w - min_y[4];
                    sum += maxy_4w - min_y[5];
                    sum += maxy_4w - min_y[6];
                    int s = 0;
                    if ( sum == 3 || sum == 0 || sum == 4 ) //{ // - (pool_h - maxy_4w) - clears * lastCombo * 2
                    {
                        int hv = (maxy_4w - maxy_4w_combo + 1) * 1 + pool.combo;
                        s += ai_param.strategy_4w * ( hv ) + (ai_param.hole * 2 + ai_param.tspin * 4);
                        if ( sum > 0 ) {
                            s -= ai_param.strategy_4w / 3;
                        }
                    }
                    if ( s > 0 ) {
                        score -= s;
                    }
                    //if ( pool_h * 4 + 4 + x_holes[pool_h] + x_op_holes[pool_h] - min_y[0] - min_y[1] - min_y[2] - min_y[3] <= 4 ) {
                    //    score -= 800 + (ai_param.hole * 2 + ai_param.tspin * 4);
                    //} else if ( pool_h * 4 + 4 + x_holes[pool_h] + x_op_holes[pool_h] - min_y[pool_w - 4] - min_y[pool_w - 3] - min_y[pool_w - 2] - min_y[pool_w - 1] <= 4 ) {
                    //    score -= 800 + (ai_param.hole * 2 + ai_param.tspin * 4);
                    //}
                }
            }
        }
        // �ۻ���

		if (TSD_only) {
			if (cur_num == AI::GEMTYPE_T) {
				if (wallkick_spin != 0 && clears == 2) clearScore -= 100000000;
				else clearScore += 100000000;

			} else if (clears != 0) clearScore += 100000000;
		}

        score += clearScore;

        return score;
    }
    struct MovsState {
        MovingSimple first;  // この操作の前に行われた操作で、探索開始直後の最初のもの  // スコア値はこの中に保存されている
        GameField pool_last;
        int att, clear;
        signed short max_combo, max_att, combo;  // max_combo, comboは純粋なコンボ数ではなく、コンボ数に応じたスコア値なので注意
        signed short player, upcomeAtt;  // upcomeAtt=受け取る火力。マイナスのときは、確定した火力でabs(upcomeAtt)だけすでに受け取ったことを表す。プラスの時は相殺できるが、マイナスの時はできない。
        MovsState() { upcomeAtt = max_combo = combo = att = clear = 0; }
        bool operator < (const MovsState& m) const {
#if 0
            {
                if ( max_combo > (combo - 1) * 32 && m.max_combo > (m.combo - 1) * 32 ) {
                    if ( att > 8 || m.att > 8 ) {
                        if ( abs(first.score - m.first.score) < 400 ) {
                            if ( att != m.att )
                                return att < m.att;
                        } else {
                            return first < m.first;
                        }
                    }
                }
                if ( ( max_combo > 6 * 32 || m.max_combo > 6 * 32 ) ) {
                    if ( max_combo != m.max_combo ) {
                        return max_combo < m.max_combo;
                    }
                }
                if ( ai_settings[player].strategy_4w )
                    if ( ( combo > 3 * 32 || m.combo > 3 * 32 ) ) {
                        if ( combo != m.combo ) {
                            return combo < m.combo;
                        }
                    }
            }
            //if (0)
            if ( (pool_last.combo > 3 * 32 || m.pool_last.combo > 3 * 32) && pool_last.combo != m.pool_last.combo) {
                return pool_last.combo < m.pool_last.combo;
            }
#else
            //if ( abs(first.score - m.first.score) >= 900 ) {
            //    return first < m.first;
            //}
            //if ( (max_att >= 6 || m.max_att >= 6) && abs(max_att - m.max_att) >= 2 ) {
            //    return max_att < m.max_att;
            //}
            //else
            // 戦略が4列RENかで順序の定義が変わる
			if ( ai_settings[player].strategy_4w )
			{
			    // 全体のスコア値よりもコンボのスコア値を優先する

				if ( ( max_combo > 6 * 32 || m.max_combo > 6 * 32 ) ) {
					if ( max_combo != m.max_combo ) {
						return max_combo < m.max_combo;
					}
				}
				if ( (combo >= 32 * 3 || m.combo >= 32 * 3) && combo != m.combo) {
					return combo < m.combo;
				}

				// この先は return first < m.first; のみ
			}
			else
			{
                // 戦略がRENかで順序の定義が変わる
                if ( ai_settings[player].combo ) {
                    // 全体のスコア値よりもコンボのスコア値を優先する

                    if ( ( max_combo > 6 * 32 || m.max_combo > 6 * 32 ) ) {
						if ( max_combo != m.max_combo ) {
							return max_combo < m.max_combo;
						}
					}

                    // 意味のない分岐な気がする  // 詳細は中のコメント参照
					if ( max_combo > combo && m.max_combo > m.combo && (m.max_combo > 4 * 32 || max_combo > 4 * 32) ) {
                        // 比較対象どちらかのcomboが 4 * 32 より大きいとき

                        if ( (combo <= 2 * 32 && m.combo <= 2 * 32) ) {
						    // 比較対象両方のcomboが 2 * 32以下のとき

						    // TODO Unreachableでは？

							long long diff = first.score - m.first.score;
							if ( -1000 < diff && diff < 1000 ) {
								if ( att != m.att )
									return att < m.att;
							} else {
								return first < m.first;
							}
						}
					}
					////if ( ai_settings[player].strategy_4w ) {
					//    if ( ( combo > 3 * 32 || m.combo > 3 * 32 ) ) {
					//        if ( combo != m.combo ) {
					//            return combo < m.combo;
					//        }
					//    }
					//}
				}
				//if (0)
				//if ( (pool_last.combo > 32 || m.pool_last.combo > 32 ) )
				//{
				//    int m1 = (max_combo!=pool_last.combo ? std::max(max_combo - 32 * 2, 0) * 2 : 0 ) + pool_last.combo;
				//    int m2 = (m.max_combo!=m.pool_last.combo ? std::max(m.max_combo - 32 * 2, 0) * 2 : 0 ) + m.pool_last.combo;
				//    if ( m1 != m2 ) {
				//        return m1 < m2;
				//    }
				//}

                // 戦略にかかわらずcomboの評価はする
                if ( (combo > 32 * 2 || m.combo > 32 * 2) && combo != m.combo) {
					return combo < m.combo;
				}
			}
            //if ( (pool_last.combo > 1 || m.pool_last.combo > 1) && pool_last.combo != m.pool_last.combo) {
            //    return pool_last.combo < m.pool_last.combo;
            //}
#endif
            return first < m.first;
        }
    };
    struct GameState{
        uint64 hash;
        signed short hold, att, clear, combo, b2b;
        GameState(uint64 _hash
            ,signed short _hold
            ,signed short _att
            ,signed short _clear
            ,signed short _combo
            ,signed short _b2b
            )
            :hash(_hash)
            ,hold(_hold)
            ,att(_att)
            ,combo(_combo)
            ,b2b(_b2b)
        {
        }
        bool operator < ( const GameState& gs) const {
            if ( hash != gs.hash ) return hash < gs.hash;
            if ( hold != gs.hold ) return hold < gs.hold;
            if ( att != gs.att ) return att < gs.att;
            if ( clear != gs.clear ) return clear < gs.clear;
            if ( combo != gs.combo ) return combo < gs.combo;
            if ( b2b != gs.b2b ) return b2b < gs.b2b;
            return false;
        }
        bool operator == ( const GameState& gs) const {
            if ( hash != gs.hash ) return false;
            if ( hold != gs.hold ) return false;
            if ( att != gs.att ) return false;
            if ( clear != gs.clear ) return false;
            if ( combo != gs.combo ) return false;
            if ( b2b != gs.b2b ) return false;
            return true;
        };
    };
    #define BEG_ADD_Y 1
    // @param canhold ホールドの使用を許可するか
    // @param hold poolの中のホールドミノと同じ値。ほとんど使われていない
    MovingSimple AISearch(AI_Param ai_param, const GameField& pool, int hold, Gem cur, int x, int y, const std::vector<Gem>& next, bool canhold, int upcomeAtt, int maxDeep, int & searchDeep) {
		// GEMTYPE_NULLになったとき
        if (cur.num == 0) { // rare race condition, we're dead already if this happens
			assert(true); // debug break
			cur = AI::getGem(AI::GEMTYPE_I, 0);
		}

		int player = 0;
		int level = 10;
        typedef std::vector<MovingSimple> MovingList;
        // 次における場所を入れるためのベクター
        MovingList movs;
        // TODO 次に探索する操作をいれておくキュー?
        MovQueue<MovsState> que(16384);
        MovQueue<MovsState> que2(16384);
        movs.reserve(128);
        // 探索したノード数
        int search_nodes = 0;
        const int combo_step_max = 32;
        searchDeep = 0;
        upcomeAtt = std::min(upcomeAtt, pool.height() - gem_beg_y - 1);

        // 4列RENをやめる
        // 1 < pool.combo なら true
        // pool.comnbo == 0 で pool.row[10]にブロックがあるなら true
        if ( pool.combo > 0 && (pool.row[10] || pool.combo > 1) ) ai_param.strategy_4w = 0;

        //
        if ( ai_param.hole < 0 ) ai_param.hole = 0;
        ai_param.hole += ai_param.open_hole;

        //if ( AI_SHOW && GAMEMODE_4W ) max_search_nodes *= 2;
        //if ( level <= 0 ) maxDeep = 0;
        //else if ( level <= 6 ) maxDeep = std::min(level, 6); // TODO: max deep
        //else maxDeep = level;

        // ホールドが空のとき 1
        // ホールド済みのときと比べて、探索にネクストがひとつ多く必要になるため
        int next_add = 0;
        if ( pool.m_hold == 0 ) {
            next_add = 1;
            if ( next.empty() ) {
                //return MovingSimple();
            }
        }

        // 現在のミノを置くケース
        {
            const GameField& _pool = pool;

            // 次のTまで何ミノあるか
            // 現在のミノの種類は無視
            // ネクストの先頭にあれば0
            // ホールドに持っていれば0
            int t_dis = 14;
            if ( _pool.m_hold != GEMTYPE_T ) {
                for ( size_t i = 0; i < next.size(); ++i ) {
                    if ( next[i].num == GEMTYPE_T ) {
                        t_dis = i;
                        break;
                    }
                }
            } else {
                t_dis = 0;
            }

            // 次における場所をmovsにいれる
            GenMoving(_pool, movs, cur, x, y, 0);

            for (MovingList::iterator it = movs.begin(); it != movs.end(); ++it) {
                ++search_nodes;
                // キューに新しい要素を追加して、書き換え用の参照を得る
                MovsState &ms = que.append();
                // フィールドをコピー
                ms.pool_last = _pool;
                // 次に置く位置でのwallkickを計算
                signed char wallkick_spin = it->wallkick_spin;
                wallkick_spin = ms.pool_last.WallKickValue(cur.num, (*it).x, (*it).y, (*it).spin, wallkick_spin);
                // フィールドにミノを反映
                ms.pool_last.paste((*it).x, (*it).y, getGem(cur.num, (*it).spin));
                // フィールドのライン消去
                int clear = ms.pool_last.clearLines( wallkick_spin );
                // 攻撃力を計算
                int att = ms.pool_last.getAttack( clear, wallkick_spin );
                // プレイヤー設定
                ms.player = player;
                ms.clear = clear;
                ms.att = att;
                // ライン消去が発生したとき
                if ( clear > 0 ) {
                    // コピー元のフィールドのコピーを参照している
                    // Moveで置いたものが反映されていないので、 combo+1 している
                    // pool_lastを参照したほうがよさそう？
                    ms.combo = (_pool.combo + 1) * combo_step_max * ai_param.combo / 30;
                    // 相殺分を差し引いて、受け取る攻撃力を計算
                    ms.upcomeAtt = std::max(0, upcomeAtt - att);
                } else {
                    ms.combo = 0;
                    // マイナスでいれている理由: 火力を受け取るとミノを出現位置がさがる（地形に近づく）ため、
                    // それを区別できるように、せりあがるライン数にマイナスをつけて記録しておく
                    ms.upcomeAtt = -upcomeAtt;
                    // 中身が空で何もしない
                    ms.pool_last.minusRow(upcomeAtt);
                }
                // 最初なのでmaxにattをそのまま保存
                ms.max_att = att;
                // 最初なのでmaxにcomboをそのまま保存  // TODO comboはREN+1?
                ms.max_combo = ms.combo; //ms_last.max_combo + getComboAttack( ms.pool_last.combo );
                // 最初の要素を記録
                ms.first = *it;
                // スコアを計算
                ms.first.score2 = 0;  // Evaluate()で同時に更新される
				double h = 0;
                ms.first.score = Evaluate(ms.first.score2, h, ai_param, pool, ms.pool_last, cur.num, 0, ms.att, ms.clear, att, clear, wallkick_spin, _pool.combo, t_dis, upcomeAtt);
                // TODO なりたつ条件がある？
                if ( wallkick_spin == 0 && it->wallkick_spin ) ms.first.score += 1;  // 少し悪い評価値にする
                // ソフトドロップを避ける
				ms.first.score += score_avoid_softdrop(ai_param.avoid_softdrop, it->softdrop, cur.num, it->wallkick_spin, h);
				// heapの作り直し  // MovsStateの評価に従う
                que.push_back();
            }
        }

        // ホールドが使えるなら、ホールドを置いたケースも探す
        if ( canhold && ! hold &&
            (
                pool.m_hold == 0
                && !next.empty() && ! pool.isCollide(gem_beg_x, gem_beg_y, getGem( next[0].num, 0 ) )
                || ! pool.isCollide(gem_beg_x, gem_beg_y, getGem( pool.m_hold, 0 ) )
            )
            )
        if (next.size() > 0){
            // 現在のミノをホールドに置き換え
            int cur_num;
            if ( pool.m_hold ) {
                cur_num = pool.m_hold;
            } else {
                cur_num = next[0].num;
            }

            // ホールド前と同じミノだったらスキップ
            if ( cur_num != cur.num ) {
                GameField _pool = pool;

                // ホールドミノを更新
                _pool.m_hold = cur.num;

                // 基本的に上の繰り返し

                // 次のTまで何ミノあるか
                int t_dis = 14;
                if ( _pool.m_hold != GEMTYPE_T ) {
                    for ( size_t i = 0; i + next_add < next.size(); ++i ) {
                        if ( next[i + next_add].num == GEMTYPE_T ) {
                            t_dis = i;
                            break;
                        }
                    }
                } else {
                    t_dis = 0;
                }

                // ミノの開始位置をスポーン場所にする
                int x = gem_beg_x, y = gem_beg_y;
                Gem cur = getGem( cur_num, 0 );

                // 次における場所をmovsにいれる
                GenMoving(_pool, movs, cur, x, y, 1);

                for (MovingList::iterator it = movs.begin(); it != movs.end(); ++it) {
                    // 上を参照
                    ++search_nodes;
                    MovsState &ms = que.append();
                    ms.pool_last = _pool;
                    signed char wallkick_spin = it->wallkick_spin;
                    wallkick_spin = ms.pool_last.WallKickValue(cur_num, (*it).x, (*it).y, (*it).spin, wallkick_spin);
                    ms.pool_last.paste((*it).x, (*it).y, getGem(cur_num, (*it).spin));
                    int clear = ms.pool_last.clearLines( wallkick_spin );
                    int att = ms.pool_last.getAttack( clear, wallkick_spin );
                    ms.player = player;
                    ms.clear = clear;
                    ms.att = att;
                    if ( clear > 0 ) {
						ms.combo = (_pool.combo + 1) * combo_step_max * ai_param.combo / 30;
                        ms.upcomeAtt = std::max(0, upcomeAtt - att);
                    } else {
                        ms.combo = 0;
                        ms.upcomeAtt = -upcomeAtt;
                        ms.pool_last.minusRow(upcomeAtt);
                    }
                    ms.max_att = att;
                    ms.max_combo = ms.combo; //ms_last.max_combo + getComboAttack( ms.pool_last.combo );
                    ms.first = *it;
                    ms.first.score2 = 0;
					double h;
                    ms.first.score = Evaluate(ms.first.score2, h, ai_param, pool, ms.pool_last, cur.num, 0, ms.att, ms.clear, att, clear, wallkick_spin, _pool.combo, t_dis, upcomeAtt);
                    if ( wallkick_spin == 0 && it->wallkick_spin ) ms.first.score += 1;

					ms.first.score += score_avoid_softdrop(ai_param.avoid_softdrop, it->softdrop, cur.num, it->wallkick_spin, h);
                    que.push_back();
                }
            }
        }

        // 動かせる場所がない
        if ( que.empty() ) {
            return MovingSimple();
        }

        // mapのひとつめの[]はレベル。ふたつめの[]は探索の深さ
        // 7 < depthのときは、7の値を使用
        // [level][0]はベースの値らしい
        int sw_map1[16][8] = {
            {999,   4,   2,   2,   2,   2,   2,   2},
            {999,   4,   4,   2,   2,   2,   2,   2},
            { 50, 999,   4,   2,   2,   2,   2,   2},
            { 20,  40, 999,   4,   2,   2,   2,   2},
            { 15,  30,  20, 999,   2,   2,   2,   2}, // 4
            { 13,  25,  15,  12, 999,   2,   2,   2},
            { 14,  27,  17,  14,  20, 999,   3,   2},
            //{ 15,  27,  17,  15,  20, 999,   3,   2},
            //{ 20,  30,  20,  20,  20, 100, 999, 999},
            { 20,  30,  25,  20,  20, 100, 999, 999}, // 7
            { 25,  60,  50,  40,  40,  40, 500, 999},
            //{ 30,  50,  40,  30,  30,  25,  25,  20},
            //{ 30, 150, 130, 130, 110, 100,  100, 80},
            { 30,  90,  75,  60,  60,  60,  60, 9999}, // 9
            //{ 50, 720, 720, 480, 480, 480, 480, 480}, // 9 PC
            //{ 30,  90,  80,  60,  60,  60,  60,  60},
            { 30, 240, 200, 200, 180, 160, 160, 9999}, // 10
        };
        int sw_map2[16][8] = {
            {999, 999, 999, 999, 999, 999, 999, 999},
            { 60,  60, 999, 999, 999, 999, 999, 999},
            { 40,  40,  40, 999, 999, 999, 999, 999},
            { 30,  60,  60,  60, 999, 999, 999, 999},
            { 25,  45,  30,  30,  30, 999, 999, 999}, // 4
            { 25,  35,  35,  30,  30,  30, 999, 999},
            { 25,  35,  35,  35,  30,  25,  25, 999},
            { 25,  45,  40,  30,  30,  30,  30,  30}, // 7
            { 25,  90,  80,  60,  50,  50,  50,  50},
            //{ 30, 220, 200, 200, 160, 150, 150, 120},
            { 30, 150, 130, 100,  80,  80,  50,  50}, // 9
            //{ 30, 150, 130, 130, 130, 130, 130, 130}, // 9 PC
            { 30, 300, 200, 180, 120, 100,  80,  80}, // 10
            //{ 30, 400, 400, 300, 300, 300, 300, 200}, // 10
        };
        int sw_map3[16][8] = {
            {999, 999, 999, 999, 999, 999, 999, 999},
            { 60,  60, 999, 999, 999, 999, 999, 999},
            { 40,  40,  40, 999, 999, 999, 999, 999},
            { 30,  60,  60,  60, 999, 999, 999, 999},
            { 25,  45,  30,  30,  30, 999, 999, 999}, // 4
            { 25,  35,  35,  30,  30,  30, 999, 999},
            { 25,  35,  35,  35,  30,  25,  25, 999},
            { 25,  45,  40,  30,  30,  30,  30,  30}, // 7
            { 25,  90,  80,  60,  50,  40,  30,  30},
            //{ 30, 220, 200, 200, 160, 150, 150, 120},
            { 30, 120, 100,  80,  70,  60,  50,  40}, // 9
            //{ 30, 150, 130, 130, 130, 130, 130, 130}, // 9 PC
            { 30, 240, 200, 160, 120,  90,  70,  60}, // 10
        };
        MovQueue<MovsState> * pq_last = &que2, * pq = &que;
        searchDeep = 1;
		int final_depth = 65535;

		// 各深さごとに探索する
		// 幅優先探索
		int depth = 0;
        for (; /*search_nodes < max_search_nodes &&*/ depth < maxDeep; searchDeep = ++depth ) { //d < maxDeep
            // 探索用と結果出力用を交換
            std::swap(pq_last, pq);

            // 基本的にmap1, 戦略がhashのときはmap2を使用
            int (*sw_map)[8] = sw_map1;
            if ( ai_settings[player].hash ) {
                sw_map = sw_map2;
                //if ( ai_param.strategy_4w > 0 ) {
                //    sw_map = sw_map3;
                //}
            }

            // search_wide, seach_select_bestを決める
            // search_wide = 今の探索の深さで探索する状態の数
            // seach_select_best = ある盤面でミノをおとすとき、上位何個の状態を次の探索に進めるか。現在のミノ、ホールドミノそれぞれでseach_select_best個ずつ選択される
            int search_base_width = sw_map[level][0];// - sw_map[level][0] / 6;
            int search_wide = 1000;
            if ( depth > 7 ) search_wide = sw_map[level][7];
            else search_wide = sw_map[level][depth];
            
            //int seach_select_best = (level <= 3 ? 1000 : (std::min(search_wide, 30) ) );
            // だいたい search_wideの3/4 と search_base_width のより小さい値を選択  // baseが小さいほうが多そう
            int seach_select_best = std::min(search_wide - search_wide / 4, search_base_width);
            if ( level <= 3 ) {
                seach_select_best = search_wide - search_wide / 4;
            }

            // baseではないほうが選択されたとき
            if ( seach_select_best < search_base_width ) {
                seach_select_best = std::min(search_base_width, std::max(15, search_wide) );
            }

            // 結果出力用キューのクリア
            pq->clear();

            int max_combo = 3;  // 実質的に未使用
            // 前の結果で最も悪いスコアを取りだす  // 未使用
            long long max_search_score = pq_last->back().first.score;  // 配列の一番後ろの値を取り出す  // だいたい悪い
            {
                // 厳密に最も悪いものを取り出す  // 配列の真ん中からくらいから探す  // heap（2分木）の特性上、半分より後ろに葉（悪い値）が入っている
                for ( int s = pq_last->size(), i = s / 2; i < s; ++i ) {
                    max_search_score = std::max((long long)max_search_score, pq_last->queue[i].first.score );
                }
                max_search_score = (max_search_score * 2 + pq_last->front().first.score) / 3;
            }

            // 最大search_wide個まで探索する
            std::set<GameState> gsSet;
            for ( int pqmax_size = (int)pq_last->size(),
                pq_size = (int)pq_last->size(),
                stop_size = std::max(0, (int)pq_size - search_wide);
                pq_size > stop_size;

                --pq_size, pq_last->dec_size())
            {
                // forの条件によって、pq_size <= 0にはならないはず
                if ( pq_size > 1 ) pq_last->pop_back();  // 最も一番良い要素を取り出す  // 実際には back() で要素が取り出される

                // 後ろの要素を取り出す
                const MovsState &ms_last = pq_last->back();
                if ( pq_size != pqmax_size && ms_last.first.score > 50000000 ) { // スコアが高すぎるときはスキップ
                    break;
                }

                // 中止するときはとりあえず要素をそのままコピーしているっぽい
                if (Abort()) {
					if (final_depth > depth) final_depth = depth;

                    //MovsState ms_last = pq_last->back();
                    pq->push(ms_last);
                    continue;
                }

                // 実質的に未使
                max_combo = std::max( max_combo, (int)ms_last.pool_last.combo );
                if (0)
                if ( pq_size != pqmax_size ) { // ����combo�����combo��֦
                    if ( ms_last.pool_last.combo > 0 && max_combo > 5 && ms_last.pool_last.combo < max_combo - 1 ) {
                        break;
                    }
                    //if ( ms_last.pool_last.combo > 0 && max_combo > 3 ) {
                    //    if ( max_combo - ms_last.pool_last.combo != 0 && max_combo - ms_last.pool_last.combo <= 1 ) {
                    //        break;
                    //    }
                    //}
                }
                if (0)
                if ( depth > 0 && maxDeep > 2 && ms_last.first.score > max_search_score ) {
                    if ( pq_size + 2 < pqmax_size ) {
                        break;
                    }
                }

                // 戦略がhashのとき
                if ( ai_settings[player].hash )
                {
                    // 同じ盤面が過去に発生していないか確認する
                    // gsSetのスコープ的に、同じ探索深さ内の盤面をチェックしている
                    GameState gs(ms_last.pool_last.hashval, ms_last.pool_last.m_hold, ms_last.att, ms_last.clear, ms_last.combo, ms_last.pool_last.b2b);
                    if ( gsSet.find(gs) == gsSet.end() ) {
                        gsSet.insert(gs);
                    } else {
                        // スキップ
                        continue;
                    }
                }

                // 未使用
                int hold = 0;
                //if ( !ms_last.first.movs.empty() && ms_last.first.movs[0] == Moving::MOV_HOLD ) hold = 1;
                if ( !ms_last.first.hold ) hold = 1;

                // 次のTまで何ミノあるか
                // 現在のミノの種類は無視
                // ネクストの先頭にあれば0
                // ホールドに持っていれば0
                int t_dis = 14;
                int d_pos = depth;
                // 探索開始時にホールドが空で、いまホールドが空ではないとき、チェックするミノの深さをひとつずらす
                if ( next_add && ms_last.pool_last.m_hold ) d_pos = depth + 1;
                // みたい深さのネクストにミノがない
                if ( d_pos >= next.size() ) {
                    pq->push(ms_last);
                    continue;
                }

                int cur_num = next[d_pos].num;
                if ( ms_last.pool_last.m_hold != GEMTYPE_T ) {
                    for ( size_t i = 0; d_pos + 1 + i < next.size(); ++i ) {
                        if ( next[d_pos + 1 + i].num == GEMTYPE_T ) {
                            t_dis = i;
                            break;
                        }
                    }
                } else {
                    t_dis = 0;
                }

                // 次における場所をmovsにいれる
                // ms_last.upcomeAtt < 0は、相手からの火力が確定しているケース（相殺できない）。受け取った火力だけマイナスで入っている
                if ( BEG_ADD_Y && ms_last.upcomeAtt < 0 )
                    // 火力を受け取っているため、探索開始位置をさげる
                    GenMoving(ms_last.pool_last, movs, getGem( cur_num, 0 ), AI::gem_beg_x, AI::gem_beg_y - ms_last.upcomeAtt, 0 );
                else
                    GenMoving(ms_last.pool_last, movs, getGem( cur_num, 0 ), AI::gem_beg_x, AI::gem_beg_y, 0 );

                if ( movs.empty() ) {
                    // 動けるところがない
                    MovsState ms = ms_last;
                    ms.first.score += 100000000;  // 最悪の評価値
                    pq->push(ms);

                    // 現在のミノを置くことができないなら、ホールドでの探索は実行しない
                    continue;
                } else {
                    // 初手とほぼ同じ処理
                    // 現在の盤面での結果を一度 p に保存する。そのあと、スコア上位seach_select_best個だけ選択してpqに戻す
                    MovQueue<MovsState> p(movs.size());
                    for (size_t i = 0; i < movs.size() ; ++i) {
                        ++search_nodes;
                        MovsState &ms = p.append();
                        {
                            ms.first = ms_last.first;
                            // フィールドをコピー
                            ms.pool_last = ms_last.pool_last;
                            // 次に置く位置でのwallkickを計算
                            signed char wallkick_spin = movs[i].wallkick_spin;
                            wallkick_spin = ms.pool_last.WallKickValue(cur_num, movs[i].x, movs[i].y, movs[i].spin, wallkick_spin);
                            // フィールドにミノを反映
                            ms.pool_last.paste(movs[i].x, movs[i].y, getGem(cur_num, movs[i].spin));
                            // フィールドのライン消去
                            int clear = ms.pool_last.clearLines( wallkick_spin );
                            // 攻撃力を計算
                            int att = ms.pool_last.getAttack( clear, wallkick_spin );
                            // プレイヤー設定
                            ms.player = player;
                            ms.clear = clear + ms_last.clear;
                            ms.att = att + ms_last.att;
                            ms.upcomeAtt = ms_last.upcomeAtt;
                            if ( clear > 0 ) {
                                // 相殺を当て続ける限りせり上がりは発生しないと考えているように見える

                                ms.combo = ms_last.combo + (combo_step_max + 1 - clear) * ai_param.combo / 30;
                                // 火力がまだ確定していない
                                if ( ms_last.upcomeAtt > 0 )
                                    ms.upcomeAtt = std::max(0, ms_last.upcomeAtt - att);  // 相殺する
                            } else {
                                ms.combo = 0;
                                if ( ms_last.upcomeAtt > 0 ) {
                                    ms.upcomeAtt = -ms_last.upcomeAtt;  // 火力を受け取った
                                    ms.pool_last.minusRow(ms_last.upcomeAtt);
                                }
                            }
                            // 最大値の更新
                            ms.max_att = std::max((int)ms_last.max_att, ms_last.att + att);
                            ms.max_combo = std::max(ms_last.max_combo, ms.combo); //ms_last.max_combo + getComboAttack( ms.pool_last.combo );
                            // スコアを計算
                            ms.first.score2 = ms_last.first.score2;  // Evaluate()で同時に更新される
                            double h;
                            ms.first.score = Evaluate(ms.first.score2, h, ai_param,
                                pool,
                                ms.pool_last, cur_num, depth + 1, ms.att, ms.clear, att, clear, wallkick_spin, ms_last.pool_last.combo, t_dis, ms_last.upcomeAtt);

                            if ( wallkick_spin == 0 && movs[i].wallkick_spin ) ms.first.score += 1;
                            // ソフトドロップを避ける
							ms.first.score += score_avoid_softdrop(ai_param.avoid_softdrop, movs[i].softdrop, cur.num, movs[i].wallkick_spin, h);
                        }
                        //
                        p.push_back();
                    }
                    for ( int i = 0; i < seach_select_best && ! p.empty(); ++i) {
                        pq->push(p.front());
                        p.pop_back();
                        p.dec_size();
                    }
                }

                // ホールドが使えるとき
                if ( canhold && depth + next_add < next.size())
                {
                    MovsState ms_last = pq_last->back();  // 再取得？取得しなくても変わらないはず
                    //int cur_num = ms_last.pool_last.m_hold;

                    int cur_num;
                    int d_pos = depth + next_add;
                    if ( ms_last.pool_last.m_hold != next[d_pos].num ) {
                        // 現在のミノとホールドミノが違うとき

                        // ホールドミノを更新
                        if ( ms_last.pool_last.m_hold ) {
                            // ホールドミノを現在のミノとして扱う
                            cur_num = ms_last.pool_last.m_hold;
                        } else {
                            // ネクストのミノを現在のミノとして扱う
                            cur_num = next[d_pos].num;
                        }
                        // TODO: たぶんバグ。上で現在のミノとして扱っていたミノをホールドしなければならないはず。再定義される前のcur_numが取れれば、それを使用したほうが良い
                        // 問題が表面化しないのは、発生するのは2手目以降であり、初手を置くたびに置く場所を探索している限り、不正な手順は発生しない
                        ms_last.pool_last.m_hold = next[d_pos].num;

                       // 次のTまで何ミノあるか
                        if ( ms_last.pool_last.m_hold != GEMTYPE_T ) {
                            t_dis -= next_add;
                            if ( t_dis < 0 ) {
                                for ( size_t i = 0; d_pos + 1 + i < next.size(); ++i ) {
                                    if ( next[i + 1 + d_pos].num == GEMTYPE_T ) {
                                        t_dis = i;
                                        break;
                                    }
                                }
                            }
                        } else {
                            t_dis = 0;
                        }

                        // 次における場所をmovsにいれる
                        // ms_last.upcomeAtt < 0は、相手からの火力が確定しているケース（相殺できない）。受け取った火力だけマイナスで入っている
                        if ( BEG_ADD_Y && ms_last.upcomeAtt < 0 )
                            GenMoving(ms_last.pool_last, movs, getGem( cur_num, 0 ), AI::gem_beg_x, AI::gem_beg_y - ms_last.upcomeAtt, 1 );
                        else
                            GenMoving(ms_last.pool_last, movs, getGem( cur_num, 0 ), AI::gem_beg_x, AI::gem_beg_y, 1 );

                        if ( movs.empty() ) {
                            // 動けるところがない
                            MovsState ms = ms_last;
                            ms.first.score += 100000000;
                            pq->push(ms);
                        } else {
                            // 初手とほぼ同じ処理
                            // 現在の盤面での結果を一度 p に保存する。そのあと、スコア上位seach_select_best個だけ選択してpqに戻す
                            MovQueue<MovsState> p(movs.size());
                            for (size_t i = 0; i < movs.size() ; ++i) {
                                ++search_nodes;
                                MovsState &ms = p.append();
                                {
                                    ms.first = ms_last.first;
                                    ms.pool_last = ms_last.pool_last;
                                    signed char wallkick_spin = movs[i].wallkick_spin;
                                    wallkick_spin = ms.pool_last.WallKickValue(cur_num, movs[i].x, movs[i].y, movs[i].spin, wallkick_spin);
                                    ms.pool_last.paste(movs[i].x, movs[i].y, getGem(cur_num, movs[i].spin));
                                    int clear = ms.pool_last.clearLines( wallkick_spin );
                                    int att = ms.pool_last.getAttack( clear, wallkick_spin );
                                    ms.player = player;
                                    ms.clear = clear + ms_last.clear;
                                    ms.att = att + ms_last.att;
                                    ms.upcomeAtt = ms_last.upcomeAtt;
                                    if ( clear > 0 ) {
										ms.combo = ms_last.combo + (combo_step_max + 1 - clear) * ai_param.combo / 30;
                                        if ( ms_last.upcomeAtt > 0 )
                                            ms.upcomeAtt = std::max(0, ms_last.upcomeAtt - att);
                                    } else {
                                        ms.combo = 0;
                                        if ( ms_last.upcomeAtt > 0 ) {
                                            ms.upcomeAtt = -ms_last.upcomeAtt;
                                            ms.pool_last.minusRow(ms_last.upcomeAtt);
                                        }
                                    }
                                    ms.max_att = std::max((int)ms_last.max_att, ms_last.att + att);
                                    ms.max_combo = std::max(ms_last.max_combo, ms.combo); //ms_last.max_combo + getComboAttack( ms.pool_last.combo );
                                    ms.first.score2 = ms_last.first.score2;
									double h;
                                    ms.first.score = Evaluate(ms.first.score2, h, ai_param,
                                        pool,
                                        ms.pool_last, cur_num, depth + 1, ms.att, ms.clear, att, clear, wallkick_spin, ms_last.pool_last.combo, t_dis, ms_last.upcomeAtt);

									ms.first.score += score_avoid_softdrop(ai_param.avoid_softdrop, movs[i].softdrop, cur.num, movs[i].wallkick_spin, h);
                                    if ( wallkick_spin == 0 && movs[i].wallkick_spin ) ms.first.score += 1;
                                }
                                p.push_back();
                            }

                            for ( int i = 0; i < seach_select_best && ! p.empty(); ++i) {
                                pq->push(p.front());
                                p.pop_back();
                                p.dec_size();
                            }
                        }
                    }
                }
            }

            // どこにも動けないとき
            // ネクストを辿って探索した結果、動けないと判断した場合でも、手なしと判断されることを意味している
            // おけるところにおいてとりあえず延命はしようとしない
            if ( pq->empty() ) {
                return MovingSimple();
            }
        }

        // この時点で、maxDeepまで探索ができている

		if (final_depth > depth) final_depth = depth;

		// 現時点で不明なネクストをホールドして、判明しているホールドを使ったケースの探索
		// 処理はこれまでとほぼ同じ

        //if (0)
        if ( ai_settings[player].hash && canhold && !Abort() ) { // extra search
            std::swap(pq_last, pq);
            pq->clear();
            int depth = searchDeep - 1;
            
            int (*sw_map)[8] = sw_map1;
            if ( ai_settings[player].hash )
                sw_map = sw_map2;
            int search_base_width = sw_map[level][0];// - sw_map[level][0] / 6;
            int search_wide = 1000;
            if ( depth > 7 ) search_wide = sw_map[level][7];
            else search_wide = sw_map[level][depth];
            
            //int seach_select_best = (level <= 3 ? 1000 : (std::min(search_wide, 30) ) );
            int seach_select_best = std::min(search_wide - search_wide / 4, search_base_width);
            if ( level <= 3 ) {
                seach_select_best = search_wide - search_wide / 4;
            }
            if ( seach_select_best < search_base_width ) {
                seach_select_best = std::min(search_base_width, std::max(15, search_wide) );
            }

            std::set<GameState> gsSet;
            for ( int pqmax_size = (int)pq_last->size(),
                pq_size = (int)pq_last->size(),
                stop_size = std::max(0, (int)pq_size - search_wide);
                pq_size > stop_size;

                --pq_size, pq_last->dec_size())
            {

                if ( pq_size > 1 ) pq_last->pop_back();

                const MovsState &ms_last = pq_last->back();
                if ( pq_size != pqmax_size && ms_last.first.score > 50000000 ) {
                    break;
                }
                pq->push(ms_last);
                if ( Abort() ) {
                    continue;
                }
                //max_combo = std::max( max_combo, (int)ms_last.pool_last.combo );
                {
                    GameState gs(ms_last.pool_last.hashval, ms_last.pool_last.m_hold, ms_last.att, ms_last.clear, ms_last.combo, ms_last.pool_last.b2b);
                    if ( gsSet.find(gs) == gsSet.end() ) {
                        gsSet.insert(gs);
                    } else {
                        continue;
                    }
                }
                //if ( !ms_last.first.movs.empty() && ms_last.first.movs[0] == Moving::MOV_HOLD ) hold = 1;
                // TODO いまの探索中の盤面の1手目でホールドをしていないときはスキップ
                // 必ずホールドがある状態だけにしたい？それにしては条件が厳しすぎる（最終的にホールドされていれば、firstでホールドする必要はない）
                if ( !ms_last.first.hold ) {
                    continue;
                }

                // 次のTまで何ミノあるか
                int t_dis = 14;
                int d_pos = depth + next_add;
                int cur_num = ms_last.pool_last.m_hold;
                for ( size_t i = 0; d_pos + 1 + i < next.size(); ++i ) {
                    if ( next[d_pos + 1 + i].num == GEMTYPE_T ) {
                        t_dis = i;
                        break;
                    }
                }

                if ( BEG_ADD_Y && ms_last.upcomeAtt < 0 )
                    GenMoving(ms_last.pool_last, movs, getGem( cur_num, 0 ), AI::gem_beg_x, AI::gem_beg_y - ms_last.upcomeAtt, 0 );
                else
                    GenMoving(ms_last.pool_last, movs, getGem( cur_num, 0 ), AI::gem_beg_x, AI::gem_beg_y, 0 );

                if ( movs.empty() ) {
                    MovsState ms = ms_last;
                    ms.first.score += 100000000;
                    pq->push(ms);
                } else {
                    MovQueue<MovsState> p;
                    for (size_t i = 0; i < movs.size() ; ++i) {
                        ++search_nodes;
                        MovsState &ms = p.append();
                        {
                            ms.first = ms_last.first;
                            ms.pool_last = ms_last.pool_last;
                            signed char wallkick_spin = movs[i].wallkick_spin;
                            wallkick_spin = ms.pool_last.WallKickValue(cur_num, movs[i].x, movs[i].y, movs[i].spin, wallkick_spin);
                            ms.pool_last.paste(movs[i].x, movs[i].y, getGem(cur_num, movs[i].spin));
                            int clear = ms.pool_last.clearLines( wallkick_spin );
                            int att = ms.pool_last.getAttack( clear, wallkick_spin );
                            ms.player = player;
                            ms.clear = clear + ms_last.clear;
                            ms.att = att + ms_last.att;
                            ms.upcomeAtt = ms_last.upcomeAtt;
                            if ( clear > 0 ) {
								ms.combo = ms_last.combo + (combo_step_max + 1 - clear) * ai_param.combo / 30;
                                if ( ms_last.upcomeAtt > 0 )
                                    ms.upcomeAtt = std::max(0, ms_last.upcomeAtt - att);
                            } else {
                                ms.combo = 0;
                                if ( ms_last.upcomeAtt > 0 ) {
                                    ms.upcomeAtt = -ms_last.upcomeAtt;
                                    ms.pool_last.minusRow(ms_last.upcomeAtt);
                                }
                            }
                            ms.max_att = std::max((int)ms_last.max_att, ms_last.att + att);
                            ms.max_combo = std::max(ms_last.max_combo, ms.combo); //ms_last.max_combo + getComboAttack( ms.pool_last.combo );
                            ms.first.score2 = ms_last.first.score2;
							double h;
                            ms.first.score = Evaluate(ms.first.score2, h, ai_param,
                                pool,
                                ms.pool_last, cur_num, depth + 1, ms.att, ms.clear, att, clear, wallkick_spin, ms_last.pool_last.combo, t_dis, ms_last.upcomeAtt);

							ms.first.score += score_avoid_softdrop(ai_param.avoid_softdrop, movs[i].softdrop, cur.num, movs[i].wallkick_spin, h);
                        }
                        p.push_back();
                    }

                    for ( int i = 0; i < seach_select_best && ! p.empty(); ++i) {
                        pq->push(p.front());
                        p.pop_back();
                        p.dec_size();
                    }
                }
            }

            // ここは見えないネクストを使える可能性があるため、手なし判断してはいけないのでは？
            // （ここで探索しているのは、現時点で不明なネクストをホールドして、判明しているホールドを使ったケースなので）
            if ( pq->empty() ) {
                return MovingSimple();
            }
        }
        {
            MovsState m, c;
            std::swap(pq_last, pq);
            pq_last->pop(m);
            if ( ! GAMEMODE_4W ) {
                while ( ! pq_last->empty() ) {
                    pq_last->pop(c);
                    if ( m.first.score > c.first.score ) {
                        m = c;
                    }
                }
            }
			last_nodes = search_nodes;
			last_depth = final_depth;
            return m.first;
        }
    }
    // @param param 係数
    // @param sd ソフトドロップが必要な操作か
    // @param cur 現在のミノの種類
    // @param wk wallkickであるか（1より大きい値で呼び出しても、暗黙的にbool0,1に変換される）
    // @param h TODO
	int score_avoid_softdrop(int param, bool sd, int cur, bool wk, double h) {
        // ソフトドロップが必要で「Tスピンの壁蹴り」でないときは、param * 5のスコアを与える
        // そのあと、 / (1 + pow(5, h - 6.5)) で標準化
		return TSD_only? 0 : (int) ((double)((sd && !(cur == AI::GEMTYPE_T && wk))? param * 5 : 0) / (1 + pow(5, h - 6.5)));
	}
    struct AI_THREAD_PARAM {
        TetrisAI_t func;
        Moving* ret_mov;
        int* flag;
        AI_Param ai_param;
        GameField pool;
        int hold;
        Gem cur;
        int x;
        int y;
        std::vector<Gem> next;
        bool canhold;
        int upcomeAtt;
        int maxDeep;
        int *searchDeep;
        int player;
        AI_THREAD_PARAM(TetrisAI_t _func, Moving& _ret_mov, int& _flag, const AI_Param& _ai_param, const GameField& _pool, int _hold, Gem _cur, int _x, int _y, const std::vector<Gem>& _next, bool _canhold, int _upcomeAtt, int _maxDeep, int & _searchDeep, int _player) {
            func = _func;
            ret_mov = &_ret_mov;
            flag = &_flag;
            ai_param = _ai_param;
            pool = _pool;
            hold = _hold;
            cur = _cur;
            x = _x;
            y = _y;
            next = _next;
            canhold = _canhold;
            upcomeAtt = _upcomeAtt;
            maxDeep = _maxDeep;
            searchDeep = &_searchDeep;
            player = _player;
        }
        ~AI_THREAD_PARAM() {
        }
    };
    AI::Gem AI_Thread( void* lpParam ) {
        AI::Gem cur;
        
        AI_THREAD_PARAM* p = (AI_THREAD_PARAM*)lpParam;
        int searchDeep = 0;
        *p->flag = 1;

        // 最も良い置く場所が選択される
        AI::MovingSimple best = AISearch(p->ai_param, p->pool, p->hold, p->cur, p->x, p->y, p->next, p->canhold, p->upcomeAtt, p->maxDeep, searchDeep);

        // 置く場所から、操作を決める
        AI::Moving mov;
        const AI::GameField &gamefield = p->pool;
        std::vector<AI::Gem> &gemNext = p->next;
        mov.movs.push_back(Moving::MOV_DROP);  // 操作にMOV_DROPをいれておく  // あとでちゃんとした操作がみつかったら上書きされる
        if ( best.x != AI::MovingSimple::INVALID_POS ) { // find path
            int hold_num = gamefield.m_hold;
            if ( gamefield.m_hold == 0 && ! gemNext.empty()) {
                // ホールドが空で、ネクストがあるとき、
                // ホールドを使っても大丈夫なように先にネクストから取り出しておく
                hold_num = gemNext[0].num;
            }

            // 操作を探索
            std::vector<AI::Moving> movs;
            if ( best.hold ) {
                // ホールドミノをつかうとき
                cur = AI::getGem(hold_num, 0);  // 現在のミノを入れ替える
                FindPathMoving(gamefield, movs, cur, AI::gem_beg_x, AI::gem_beg_y, true);
            } else {
                cur = p->cur;
                FindPathMoving(gamefield, movs, cur, p->x, p->y, false);
            }

            // movsの中から、置きたい場所と同じ場所のMovingを取り出す
            for ( size_t i = 0; i < movs.size(); ++i ) {
                if ( movs[i].x == best.x && movs[i].y == best.y && movs[i].spin == best.spin ) {
                    if ( (isEnableAllSpin() || cur.num == GEMTYPE_T) ) {
                        if ( movs[i].wallkick_spin == best.wallkick_spin ) {
                            mov = movs[i];
                            break;
                        }
                    } else {
                        mov = movs[i];
                        break;
                    }
                } else if ( cur.num == GEMTYPE_I || cur.num == GEMTYPE_Z || cur.num == GEMTYPE_S ) {
                    // ISZミノは回転方向が反対でもOKなので、その場合も確認する

                    if ( (best.spin + 2 ) % 4 == movs[i].spin ) {
                        if ( best.spin == 0 ) {
                            if ( movs[i].x == best.x && movs[i].y == best.y - 1 ) {
                                mov = movs[i];
                                break;
                            }
                        } else if ( best.spin == 1 ) {
                            if ( movs[i].x == best.x - 1 && movs[i].y == best.y ) {
                                mov = movs[i];
                                break;
                            }
                        } else if ( best.spin == 2 ) {
                            if ( movs[i].x == best.x && movs[i].y == best.y + 1 ) {
                                mov = movs[i];
                                break;
                            }
                        } else if ( best.spin == 3 ) {
                            if ( movs[i].x == best.x + 1 && movs[i].y == best.y ) {
                                mov = movs[i];
                                break;
                            }
                        }
                    }
                }
            }
        }

        // 操作がみつからないときは、強制的にDROPする
        // FindPathMovin()の結果が不正なときになる可能性がありそう
        if ( mov.movs.empty() ) {
            mov.movs.clear();
            mov.movs.push_back( AI::Moving::MOV_NULL );
            mov.movs.push_back( AI::Moving::MOV_DROP );
        }
        *p->ret_mov = mov;

        *p->searchDeep = searchDeep;
        *p->flag = -1;
        delete p;
        //_endthread();
        
        return cur;
    }

    // AIの実行
    // もともと別のThreadで実行されていたが、いまはただの関数呼び出し
    // @param hold poolの中のホールドミノと同じ値。ほとんど使われていなさそう
    AI::Gem RunAI(Moving& ret_mov, int& flag, const AI_Param& ai_param, const GameField& pool, int hold, Gem cur, int x, int y, const std::vector<Gem>& next, bool canhold, int upcomeAtt, int maxDeep, int & searchDeep) {
        flag = 0;
        //_beginthread(AI_Thread, 0, new AI_THREAD_PARAM(NULL, ret_mov, flag, ai_param, pool, hold, cur, x, y, next, canhold, upcomeAtt, maxDeep, searchDeep, level, player) );
        return AI_Thread(new AI_THREAD_PARAM(NULL, ret_mov, flag, ai_param, pool, hold, cur, x, y, next, canhold, upcomeAtt, maxDeep, searchDeep, 0));
    }
}
