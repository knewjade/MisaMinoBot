#include "tetris_ai.h"
#include <deque>
#include <map>
#include <set>

#include "tetris_setting.h"
#include <assert.h>

#define GENMOV_W_MASK   15
#define GEN_MOV_NO_PUSH 0

namespace AI {
    _ai_settings ai_settings[2];

    enum {
        MOV_SCORE_DROP = 1,
        MOV_SCORE_LR = 10,
        MOV_SCORE_LR2 = 30,
        MOV_SCORE_LLRR = 20,
        MOV_SCORE_D = 50,
        MOV_SCORE_DD = 100,
        MOV_SCORE_SPIN = 50,
    };

    void setAIsettings(int player, const char *key, int val) {
        if (strcmp(key, "hash") == 0) {
            ai_settings[player].hash = val;
        } else if (strcmp(key, "combo") == 0) {
            ai_settings[player].combo = val;
        } else if (strcmp(key, "4w") == 0) {
            ai_settings[player].strategy_4w = val;
        }
    }

    double
    Evaluate(double &clearScore, const AI_Param &ai_param, const GameField &last_pool, const GameField &pool, int cur_num,
             int curdepth,
             int total_clear_att, int total_clears, int clear_att, int clears, signed char wallkick_spin,
             int lastCombo, int t_dis, int upcomeAtt
    ) {
        const int pool_w = pool.width();
        const int pool_h = pool.height();

        double score = 0;
        // ��߶�
        //int last_min_y[32] = {0};
        int min_y[32] = {0};  // xごとのもっとも小さいy // 最大の高さ
        min_y[31] = -1;
        min_y[pool_w] = min_y[pool_w - 2];
        //last_min_y[pool_w] = last_min_y[pool_w-2];

        int maxy_cnt = 0;  // ブロック最大の高さをもつxの数  // 同じ高さが1つのとき0, 2つのとき1
        int miny_val = 31;  // ブロックがあるもっとも高いy
        int maxy_index = 31;  // もっとも大きいy値をもつx。同じ高さが長く続いている左端のx  // もっとも低いx

        int emptys[32] = {0};  // yごとの空ブロック数
        int maxy_flat_cnt = 0;  // 一番低い高さで続いている最大個数  // 最小1
        int total_hole = 0;

        int beg_y = -5;
        while (pool.row[beg_y] == 0) ++beg_y;  // もっとも高いy  // ブロックがある中で一番小さい値

        //last_min_y[31] = -1;
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
            for (int x = 0; x < pool_w; ++x) {
                for (int y = beg_y, ey = pool_h + 1; y <= ey; ++y) { // Ҫ�е��б�����pool.h����������
                    if (pool.row[y] & (1 << x)) {
                        min_y[x] = y;
                        miny_val = std::min(miny_val, y);
                        if (y > min_y[maxy_index]) {
                            maxy_index = x;
                            maxy_cnt = 0;
                        } else if (y == min_y[maxy_index]) {
                            ++maxy_cnt;
                        }
                        break;
                    }
                }
            }

            // 0の左の壁(1)からはじめて、1->0,0->1の数を数える
            // 最大10
            int transitions = 0;
            for (int y = beg_y; y <= pool_h; ++y) {
                int last = 1; //pool.row[y] & 1;
                if (pool.row[y] > 0) {
                    // ブロックがあるライン
                    for (int x = 0; x < pool_w; ++x) {
                        if (pool.row[y] & (1 << x)) {
                            // ブロックがある
                            if (last == 0) ++transitions;
                            last = 1;
                        } else {
                            // ブロックがない
                            if (last == 1) ++transitions;
                            last = 0;
                            ++emptys[y];
                        }
                    }
                } else {
                    // 空のライン
                    emptys[y] = pool_w;
                }
                transitions += !last;
            }
            score += ai_param.v_transitions * transitions / 10.0;
        }

        if (pool.m_hold == GEMTYPE_I) {
            score -= ai_param.hold_I;
        }

        if (pool.m_hold == GEMTYPE_T) {
            score -= ai_param.hold_T;
        }

        if (maxy_cnt > 0) {
            int ybeg = min_y[maxy_index];
            unsigned rowdata = pool.row[ybeg - 1];  // 一番低い高さの1段上
            int empty = ~rowdata & pool.m_w_mask;
            for (int b = maxy_index; b < pool_w; ++b) {
                if (ybeg != min_y[b]) continue;
                int cnt = 1;
                // Emptyが続いている個数
                for (int b1 = b + 1; empty & (1 << b1); ++b1) ++cnt;
                if (maxy_flat_cnt < cnt) {
                    maxy_flat_cnt = cnt;
                    maxy_index = b;
                }
            }
        }

        // 穴の数
        int x_holes[32] = {0}; // 水平方向  // ホールの数
        int y_holes[32] = {0}; // 垂直方向 // ホールの数
        int x_op_holes[32] = {0}; // 水平方向
        double pool_hole_score;
        int pool_total_cell = 0;  // 穴の合計数  // 穴 = 上方向にブロックが空白の数

        {   // pool
            int first_hole_y[32] = {0}; // 垂直方向に最も近い穴のy
            int x_renholes[32] = {0}; // 縦連続穴数
            double hole_score = 0;
            const GameField &_pool = pool;
            for (int x = 0; x < pool_w; ++x) {
                for (int y = min_y[x]; y <= pool_h; ++y) {
                    if ((_pool.row[y] & (1 << x)) == 0) {
                        pool_total_cell++;
                    }
                }
            }

            for (int x = 0; x < pool_w; ++x) {
                first_hole_y[x] = pool_h + 1;

                int last = 0;
                int next;
                int y = (x > 0) ? std::min(  // 高い方
                        min_y[x] + 1,  // 対象xの1段下
                        std::max(min_y[x - 1] + 6, min_y[x + 1] + 6)  // 左右の行の高さの下6段でより低い方
                ) : min_y[x] + 1;

                for (; y <= pool_h; ++y, last = next) {
                    if ((_pool.row[y] & (1 << x)) == 0) { //_pool.row[y] &&
                        // ブロックがない
                        double factor = (y < 20 ? (1 + (20 - y) / 20.0 * 2) : 1.0);
                        y_holes[x]++;
                        next = 1;
                        if (softdropEnable()) {
                            if (x > 1) {
                                // 左1行、左2行より高い
                                if (min_y[x - 1] > y && min_y[x - 2] > y) {
                                    hole_score += ai_param.open_hole * factor;
                                    if (y >= 0) ++x_op_holes[y];
                                    continue;
                                }
                            }
                            if (x < pool_w - 2) {
                                // 右1行、右2行より高い
                                if (min_y[x + 1] > y && min_y[x + 2] > y) {
                                    hole_score += ai_param.open_hole * factor;
                                    if (y >= 0) ++x_op_holes[y];
                                    continue;
                                }
                            }
                        }

                        if (y >= 0) ++x_holes[y];

                        if (first_hole_y[x] > pool_h) {
                            first_hole_y[x] = y;
                        }

                        double hs = 0;
                        if (last) {
                            // 一つ前にブロックがなかった
                            hs += ai_param.hole / 2.0;
                            if (y >= 0) ++x_renholes[y];
                        } else {
                            // 一つ前にブロックがあった
                            hs += ai_param.hole * 2;
                        }

                        ++total_hole;

                        hole_score += hs * factor;
                    } else {
                        // ブロックがある
                        next = 0;
                    }
                }
            }

            for (int y = 0; y <= pool_h; ++y) {
                if (x_holes[y] > 0) {
                    score += ai_param.hole_dis * (pool_h - y + 1);
                    break;
                }
            }

            if (ai_param.hole_dis_factor) {
                for (int y = 0, cnt = 5, index = -1; y <= pool_h; ++y) {
                    if (x_holes[y] > 0) {
                        if (cnt > 0) --cnt, ++index;
                        else break;
                        for (int x = 0; x <= pool_w; ++x) {
                            if ((_pool.row[y] & (1 << x)) == 0) {
                                // ブロックがない
                                int h = y - min_y[x];
                                if (h > 4 - index) h = 4 + (h - (4 - index)) * cnt / 4;
                                //if ( h > 4 ) h = 4;
                                if (h > 0) {
                                    if ((_pool.row[y - 1] & (1 << x)) != 0) {
                                        score += ai_param.hole_dis_factor * h * cnt / 5.0 / 2.0;
                                    }
                                }
                            }
                        }
                    }
                }
            }

            if (ai_param.hole_dis_factor2) {
                int miny = pool_h;
                for (int y = 0, cnt = 0; y <= pool_h; ++y) {
                    if (x_holes[y] > 0) {
                        if (cnt < 4) ++cnt;
                        else break;
                        for (int x = 0; x <= pool_w; ++x) {
                            if ((_pool.row[y] & (1 << x)) == 0) {
                                int vy = min_y[x] + cnt * 1;
                                if (vy < miny) miny = vy;
                            }
                        }
                    }
                }

                for (int x = 0; x <= pool_w; ++x) {
                    int vy = min_y[x] + 6;
                    if (vy < miny) miny = vy;
                }

                double total_emptys = 0;
                for (int y = miny; y <= pool_h; ++y) {
                    total_emptys += emptys[y] * (y < 10 ? (10 + 10 - y) / 10.0 : 1);
                }

                score += ai_param.hole_dis_factor2 * total_emptys / 4.0;
            }


            pool_hole_score = hole_score;
        }

        score += pool_hole_score;

        // 高度差
        {
            int last = min_y[1];
            for (int x = 0; x <= pool_w; last = min_y[x], ++x) {
                int v = min_y[x] - last;
                if (x == maxy_index) {
                    x += maxy_flat_cnt;
                    continue;
                }
                int absv = abs(v);
                if (x + 1 == maxy_index && v > 0 || x - 2 == maxy_index && v < 0); //
                else score += absv * ai_param.h_factor;
            }
        }

        int center = 10;
        double warning_factor = 1;
        int h_variance_score = 0;
        // 分散？
        {
            int avg = 0;
            {
                int sum = 0;
                int sample_cnt = 0;
                for (int x = 0; x < pool_w; ++x) {
                    avg += min_y[x];
                }

                int h = std::min(std::min(min_y[gem_beg_x], min_y[gem_beg_x + 1]),
                                 std::min(min_y[gem_beg_x + 2], min_y[gem_beg_x + 3]));
                if (h < 8) {
                    score += ai_param.miny_factor * (8 - h) * 2;
                }

                if (avg < pool_w * center) {
                    warning_factor = 0.0 + (double) avg / pool_w / center / 1;
                }

                // 偏差値
                {
                    int dif_sum = 0;
                    for (int x = 0; x < pool_w; ++x) {
                        dif_sum += abs(min_y[x] * pool_w - avg);
                    }
                    score += ai_param.dif_factor * dif_sum / pool_w / pool_w;
                }
            }

            // Attack calculation
            {
                double s = 0;
                int t_att = total_clear_att;
                double t_clear = total_clears; //+ total_clears / 4.0;
                if (pool.b2b) s -= 5; // b2b score

                if (t_clear > 0) {
                    s -= ai_param.clear_efficient * t_att;
                }

                {
                    s += warning_factor * t_clear * ai_param.clear_useless_factor;
                }

                double cs = 0;
                // T eliminates the extra points, which is basically larger than the T1/T2 shape.
                if (cur_num == GEMTYPE_T && wallkick_spin && clears > 0 && ai_param.tspin > 0) {
                    s -= ai_param.hold_T;
                    if (clears >= 3) {
                        if (clear_att >= clears * 2) { // T3
                            cs -= warning_factor * (ai_param.tspin3 * 8.0) + ai_param.hole * 2.0;
                        }
                    } else if (clears >= 2) {
                        if (clear_att >= clears * 2) { // T2
                            cs -= warning_factor * (ai_param.tspin * 5.0 + ai_param.open_hole / 2.0);
                        }
                    } else if (wallkick_spin == 1) { // T1
                        cs -= warning_factor * (ai_param.tspin * 1.0 + ai_param.open_hole / 2.0);
                    } else if (wallkick_spin == 2) { // Tmini
                        cs -= warning_factor * (ai_param.tspin / 2.0);
                    }
                }
                clearScore += cs;
#ifdef XP_RELEASE
                if (1)
                if ( clears > 0 && upcomeAtt >= 4 && ai_param.upcomeAtt > 0 ) {
                    int cur_s = 0;
                    cur_s -= int( ((ai_param.clear_efficient) * ( clear_att ) ) )
                        - int( warning_factor * clears * ai_param.clear_useless_factor);
                    if ( avg - (12 + upcomeAtt) * pool_w > 0 && cur_s + cs < 0 )
                        s -= (cur_s + cs) * ( avg - (12 + upcomeAtt) * pool_w ) * ai_param.upcomeAtt / pool_w / 10 / 20;
                }
#endif
                score += s;
            }
        }

        // 特殊形状判定

        // Computationally attackable (Tetris and T2)
        if (maxy_cnt == 0) {
            int ybeg = 0;
            if (softdropEnable() && maxy_index > 0 && maxy_index < pool_w - 1 && ai_param.tspin > 0) { // T1/T2基本形状分
                ybeg = std::max(min_y[maxy_index - 1], min_y[maxy_index + 1]);
                if (min_y[maxy_index - 1] == min_y[maxy_index + 1]
                    && x_holes[ybeg] == 0 && (!ybeg || x_holes[ybeg - 1] == 0)
                    && x_op_holes[ybeg] == 0 && (!ybeg || x_op_holes[ybeg - 1] == 0)
                        ) { // T preparation
                    int cnt = 0;
                    if (maxy_index > 1 && min_y[maxy_index - 2] >= min_y[maxy_index - 1] - 2) ++cnt;
                    if (maxy_index < pool_w - 2 && min_y[maxy_index + 2] >= min_y[maxy_index + 1] - 2) ++cnt;
                    if (cnt > 0) {
                        score -= warning_factor * ai_param.tspin;
                        if ((~pool.row[ybeg] & pool.m_w_mask) == (1 << maxy_index)) { // T1 foundation
                            score -= warning_factor * ai_param.tspin;
                            if ((~pool.row[ybeg - 1] & pool.m_w_mask) == (7 << (maxy_index - 1))) { // can be T2 perfect pit
                                score -= warning_factor * (ai_param.tspin * cnt);
                            }
                        }
                    }
                } else if (ybeg <= 6 && ybeg - t_dis > 1 || ybeg > 6) {
                    int row_data = pool.row[ybeg - 1];
                    if ((row_data & (1 << (maxy_index - 1))) == 0 &&
                        (row_data & (1 << (maxy_index + 1))) == 0 // The left and right of the pit are empty
                        && x_holes[ybeg] == 0 && (!ybeg || x_holes[ybeg - 1] == 0)// No holes in other locations
                        && x_op_holes[ybeg] == 0 && (!ybeg || x_op_holes[ybeg - 1] <= 1)
                            ) {
                        // T pit shape
                        if ((pool.row[ybeg] & (1 << (maxy_index - 1))) &&
                            (pool.row[ybeg] & (1 << (maxy_index + 1)))) { // The two pieces below the pit exist
                            if (!!(pool.row[ybeg - 2] & (1 << (maxy_index - 1))) +
                                !!(pool.row[ybeg - 2] & (1 << (maxy_index + 1))) == 1) { // The block above the pit exists
                                double s = 0;
                                double factor = ybeg > 6 ? 0.5 : 1 - t_dis / 6.0 * 0.5;
                                if (warning_factor < 1)
                                    factor = ybeg > 6 ? 1.0 / 5 : 1 / (1 + t_dis / 3.0);
                                s += ai_param.open_hole;
                                if ((~pool.row[ybeg] & pool.m_w_mask) == (1 << maxy_index)) { // can be T1
                                    s += ai_param.tspin + ai_param.tspin * 1.0 * factor;
                                    if ((~row_data & pool.m_w_mask) == (7 << (maxy_index - 1))) { // can be T2 perfect pit
                                        s += ai_param.tspin * 3.0 * factor;
                                    }
                                } else {
                                    s += ai_param.tspin * 1.0 + ai_param.tspin * 2.0 * factor / 2.0;
                                }
                                score -= int(warning_factor * s);
                            }
                        }
                    }
                }
            } else {
                if (maxy_index == 0) {
                    ybeg = min_y[maxy_index + 1];
                } else {
                    ybeg = min_y[maxy_index - 1];
                }
            }

            int readatt = 0;
            int last = pool.row[ybeg];
            for (int y = ybeg; y <= pool_h; ++y) {
                if (last != pool.row[y]) break;
                int row_data = ~pool.row[y] & pool.m_w_mask;
                if ((row_data & (row_data - 1)) != 0) break;
                ++readatt;
            }
            if (readatt > 4) readatt = 4;

        }
        // T3 ��״�ж�
        //3001
        //2000
        // 1101
        // 1x01
        // 1101
        //
        // 1003
        // 0002
        //1011
        //10x1
        //1011
        if (softdropEnable() && ai_param.tspin3 > 0) {
            for (int y = 3; y < pool_h; ++y) {
                if (x_holes[y] == 0) continue;
                for (int x = 1; x < pool_w - 1; ++x) {
                    if ((pool.row[y + 1] & (1 << x)) == 0 || (pool.row[y + 1] & (1 << x)) == 0) {
                        continue; // no holes up and down
                    }
                    int row_y[5];
                    for (int i = 0; i < 5; ++i) {
                        row_y[i] = ((pool.row[y - 3 + i] | (3 << pool_w)) << 2) | 3;
                    }
                    if (((row_y[3] >> (x + 1)) & (7)) == 1 /*100*/ ) { // Picture above
                        if (x == pool_w - 2) continue;
                        // All empty places match first
                        if (((row_y[2] >> (x + 1)) & (7)) != 3 /*110*/
                            //|| ( (row_y[4] >> (x + 1)) & ( 15 ) ) != 11 /*1101*/
                            || ((row_y[4] >> (x + 1)) & (13)) != 9 /*1011mask=1001*/
                            || ((row_y[1] >> (x + 1)) & (7)) != 0 /*000*/
                            //|| ( (row_y[0] >> (x + 1)) & ( 3 ) ) != 0 /*00*/
                                ) {
                            continue;
                        }
                        if (min_y[x] != y - 1 || min_y[x - 1] != y - 1) {
                            continue;
                        }
                        if ((row_y[0] & (1 << (x))) == 0 && (row_y[1] & (1 << (x)))) {
                            continue; // High corner
                        }
                        if (min_y[x + 1] > y) { // 洞判定
                            if (x_holes[y - 1] > 0 || x_holes[y + 1] > 0 || x_holes[y] > 1
                                || x_op_holes[y - 1] > 0 || x_op_holes[y + 1] > 0 || x_op_holes[y] > 0) {
                                continue;
                            }
                        } else {
                            if (x_holes[y - 1] > 1 || x_holes[y + 1] > 1 || x_holes[y] > 2
                                || x_op_holes[y - 1] > 0 || x_op_holes[y + 1] > 0 || x_op_holes[y] > 0) {
                                continue;
                            }
                        }
                        if (((row_y[0] >> (x + 3)) & (1)) == 0 && y - min_y[x + 2] > 3) continue;
                        int s = 0;
                        //tp3 * 1
                        s -= int(warning_factor * ai_param.tspin3);// + int( warning_factor * ( ai_param.tspin * 4 + ai_param.open_hole ) );
                        score += s;
                        if (y <= pool_h - 3 && (pool.row[y + 3] & (1 << x)) == 0) {
                            int r = ~pool.row[y + 3] & pool.m_w_mask;
                            if ((r & (r - 1)) == 0) {
                                score -= int(warning_factor * (ai_param.tspin * 4 + ai_param.open_hole));
                            }
                        }
                        //int full = 0;
                        {
                            int e = ~(pool.row[y + 1] | (1 << x)) & pool.m_w_mask;
                            e &= (e - 1);
                            if (e == 0) { // At the bottom, there is only one empty space.
                                //++full;
                            } else {
                                score -= s;
                                continue;
                            }
                        }
                        {
                            int e = ~(pool.row[y] | (1 << (x + 2))) & pool.m_w_mask;
                            e &= (e - 1);
                            if ((e & (e - 1)) == 0) { // There are only two left at the end
                                //++full;
                            } else {
                                if ((pool.row[y] & (1 << (x + 2))) == 0) {
                                    score -= warning_factor * ai_param.tspin3 * 3.0;
                                }
                                score -= s;
                                score -= warning_factor * ai_param.tspin3 / 3.0;
                                continue;
                            }
                        }
                        {
                            int e = ~pool.row[y - 1] & pool.m_w_mask;
                            e &= (e - 1);
                            if (e == 0) { // The bottom three are left empty
                                //++full;
                            } else {
                                score -= s;
                                score -= warning_factor * ai_param.tspin3;
                                continue;
                            }
                        }
                        score -= warning_factor * ai_param.tspin3 * 3.0;
                        if (pool.row[y - 3] & (1 << (x + 1))) {
                            if (t_dis < 7) {
                                score -= warning_factor * (ai_param.tspin3 * 1.0) + ai_param.hole * 2.0;
                                score -= warning_factor * ai_param.tspin3 * 3.0 / (t_dis + 1.0);
                            }
                        }
                    } else if (((row_y[3] >> (x + 1)) & (7)) == 4 /*001*/ ) { // Mirroring situation
                        if (x == 1) continue;
                        //if ( t2_x[x-1] == y ) continue; // �ų�T2��
                        // ���пյĵط���ƥ��
                        if (((row_y[2] >> (x + 1)) & (7)) != 6 /*011*/
                            //|| ( (row_y[4] >> (x)) & ( 15 ) ) != 13 /*1011*/
                            || ((row_y[4] >> (x)) & (11)) != 9 /*1101mask=1001*/
                            || ((row_y[1] >> (x + 1)) & (7)) != 0 /*000*/
                            //|| ( (row_y[0] >> (x + 1)) & ( 3 ) ) != 0 /*00*/
                                ) {
                            continue;
                        }
                        if (min_y[x] != y - 1 || min_y[x + 1] != y - 1) {
                            continue;
                        }
                        if ((row_y[0] & (1 << (x + 4))) == 0 && (row_y[1] & (1 << (x + 4)))) {
                            continue; // �ߴ�ת��
                        }
                        if (min_y[x - 1] > y) { // ���ж�
                            if (x_holes[y - 1] > 0 || x_holes[y + 1] > 0 || x_holes[y] > 1
                                || x_op_holes[y - 1] > 0 || x_op_holes[y + 1] > 0 || x_op_holes[y] > 0) {
                                continue;
                            }
                        } else {
                            if (x_holes[y - 1] > 1 || x_holes[y + 1] > 1 || x_holes[y] > 2
                                || x_op_holes[y - 1] > 0 || x_op_holes[y + 1] > 0 || x_op_holes[y] > 0) {
                                continue;
                            }
                        }
                        if (((row_y[0] >> (x + 1)) & (1)) == 0 && y - min_y[x - 2] > 3) continue;
                        int s = 0;
                        // tp3 * 1
                        // + int( warning_factor * ( ai_param.tspin * 4 + ai_param.open_hole ) );
                        s -= warning_factor * ai_param.tspin3;
                        score += s;
                        if (y <= pool_h - 3 && (pool.row[y + 3] & (1 << x)) == 0) {
                            int r = ~pool.row[y + 3] & pool.m_w_mask;
                            if ((r & (r - 1)) == 0) {
                                score -= warning_factor * (ai_param.tspin * 4.0 + ai_param.open_hole);
                            }
                        }
                        //int full = 0;
                        {
                            int e = ~(pool.row[y + 1] | (1 << x)) & pool.m_w_mask;
                            e &= (e - 1);
                            if (e == 0) { // ���ֻʣһ��
                                //++full;
                            } else {
                                score -= s;
                                continue;
                            }
                        }
                        {
                            int e = ~(pool.row[y] | (1 << x - 2)) & pool.m_w_mask;
                            e &= (e - 1);
                            if ((e & (e - 1)) == 0) { // �׶�ֻʣ����
                                //++full;
                            } else {
                                if ((pool.row[y] & (1 << (x - 2))) == 0) {
                                    score -= warning_factor * ai_param.tspin3 * 3.0;
                                }
                                score -= s;
                                score -= warning_factor * ai_param.tspin3 / 3.0;
                                continue;
                            }
                        }
                        {
                            int e = ~pool.row[y - 1] & pool.m_w_mask;
                            e &= (e - 1);
                            if (e == 0) { // ����ֻʣһ��
                                //++full;
                            } else {
                                score -= s;
                                score -= warning_factor * ai_param.tspin3;
                                continue;
                            }
                        }
                        score -= warning_factor * ai_param.tspin3 * 3.0;
                        if (pool.row[y - 3] & (1 << (x - 1))) {
                            if (t_dis < 7) {
                                score -= warning_factor * (ai_param.tspin3 * 1.0) + ai_param.hole * 2.0;
                                score -= warning_factor * ai_param.tspin3 * 3.0 / (t_dis + 1.0);
                            }
                        }
                    }
                }
            }
        }

        // 4W形状判定
        if (USE4W)
            if (ai_param.strategy_4w > 0 && total_clears < 1) //&& lastCombo < 1 && pool.combo < 1 )
            {
                int maxy_4w = min_y[3];
                maxy_4w = std::max(maxy_4w, min_y[4]);
                maxy_4w = std::max(maxy_4w, min_y[5]);
                maxy_4w = std::max(maxy_4w, min_y[6]);
                int maxy_4w_combo = min_y[0];
                maxy_4w_combo = std::max(maxy_4w_combo, min_y[1]);
                maxy_4w_combo = std::max(maxy_4w_combo, min_y[2]);
                maxy_4w_combo = std::max(maxy_4w_combo, min_y[pool_w - 3]);
                maxy_4w_combo = std::max(maxy_4w_combo, min_y[pool_w - 2]);
                maxy_4w_combo = std::max(maxy_4w_combo, min_y[pool_w - 1]);
                if ((min_y[4] < min_y[3] && min_y[4] <= min_y[5])
                    || (min_y[5] < min_y[6] && min_y[5] <= min_y[4])) {
                    maxy_4w = -10;
                } else
                    for (int x = 0; x < pool_w; ++x) {
                        if (min_y[x] > maxy_4w) {
                            maxy_4w = -10;
                            break;
                        }
                    }
                while (maxy_4w > 0) {
                    if ((pool_h - maxy_4w) * 2 >= maxy_4w - maxy_4w_combo) {
                        maxy_4w = -10;
                        break;
                    }
                    break;
                }
                if (maxy_4w <= pool_h - 4) { // If there are more than 4 attack lines, it will not take
                    maxy_4w = -10;
                }
                //if ( maxy_4w - maxy_4w_combo > 15 ) { // ����г���10Ԥ���оͲ���
                //    maxy_4w = -10;
                //}
                if (maxy_4w - maxy_4w_combo < 9 && pool_hole_score > ai_param.hole * (maxy_4w - maxy_4w_combo) / 2.0 ) {
                    maxy_4w = -10;
                }

                if (maxy_4w > 8) {
                    bool has_hole = false;
                    for (int y = maxy_4w - 1; y >= 0; --y) {
                        if (x_holes[y] || x_op_holes[y]) {
                            has_hole = true;
                            break;
                        }
                    }
                    if (!has_hole && maxy_4w < pool_h) {
                        if (x_holes[maxy_4w] > 1 || x_op_holes[maxy_4w] > 1) {
                            has_hole = true;
                        }
                    }

                    if (!has_hole) {
                        int sum = maxy_4w - min_y[3];
                        sum += maxy_4w - min_y[4];
                        sum += maxy_4w - min_y[5];
                        sum += maxy_4w - min_y[6];
                        int s = 0;
                        if (sum == 3 || sum == 0 || sum == 4) //{ // - (pool_h - maxy_4w) - clears * lastCombo * 2
                        {
                            int hv = (maxy_4w - maxy_4w_combo + 1) * 1 + pool.combo;
                            s += ai_param.strategy_4w * hv + (ai_param.hole * 2.0 + ai_param.tspin * 4.0);
                            if (sum > 0) {
                                s -= ai_param.strategy_4w / 3.0;
                            }
                        }
                        if (s > 0) {
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
        score += clearScore;
        return score;
    }

    struct GameState {
        uint64 hash;
        signed short hold, att, clear, combo, b2b;

        GameState(uint64 _hash, signed short _hold, signed short _att, signed short _clear, signed short _combo,
                  signed short _b2b
        )
                : hash(_hash), hold(_hold), att(_att), combo(_combo), b2b(_b2b) {
        }

        bool operator<(const GameState &gs) const {
            if (hash != gs.hash) return hash < gs.hash;
            if (hold != gs.hold) return hold < gs.hold;
            if (att != gs.att) return att < gs.att;
            if (clear != gs.clear) return clear < gs.clear;
            if (combo != gs.combo) return combo < gs.combo;
            if (b2b != gs.b2b) return b2b < gs.b2b;
            return false;
        }

        bool operator==(const GameState &gs) const {
            if (hash != gs.hash) return false;
            if (hold != gs.hold) return false;
            if (att != gs.att) return false;
            if (clear != gs.clear) return false;
            if (combo != gs.combo) return false;
            if (b2b != gs.b2b) return false;
            return true;
        };
    };

#define BEG_ADD_Y 1

    MovingSimple
    AISearch(AI_Param ai_param, const GameField &pool, int hold, Gem cur, int x, int y, const std::vector<Gem> &next,
             bool canhold, int upcomeAtt, int maxDeep, int &searchDeep, int level, int player) {
        assert(cur.num != 0);
        typedef std::vector<MovingSimple> MovingList;
        MovingList movs;
        MovQueue<MovsState> que(16384);
        MovQueue<MovsState> que2(16384);
        movs.reserve(128);
        int search_nodes = 0;
        const int combo_step_max = 32;
        searchDeep = 0;
        upcomeAtt = std::min(upcomeAtt, pool.height() - gem_beg_y - 1);
        if (pool.combo > 0 && (pool.row[10] || pool.combo > 1)) ai_param.strategy_4w = 0;
        if (ai_param.hole < 0) ai_param.hole = 0;
        ai_param.hole += ai_param.open_hole;
#if AI_WEAK_VERSION || defined(XP_RELEASE)
        int ai_level_map[] = {
            4000,  //LV0 search all
            4000,  //LV1 search all
            4000,
            4000,
            4000,  //lv4
            5000,
            6000,
            8000,  //LV7
            16000,
            32000,
            64000,
        };
        int max_search_nodes = ai_level_map[level];
        //if ( AI_SHOW && GAMEMODE_4W ) max_search_nodes *= 2;
        if ( level <= 0 ) maxDeep = 0;
        else if ( level <= 6 ) maxDeep = std::min(level, 6); // TODO: max deep
        //else maxDeep = level;
#else
        int max_search_nodes = 4000;
#endif
        int next_add = 0;
        if (pool.m_hold == 0) {
            next_add = 1;
            if (next.empty()) {
                return MovingSimple();
            }
        }

        {
            const GameField &_pool = pool;
            int t_dis = 14;  // 次のTまでのミノ数  // ないときはおそらく最悪のケースを想定している
            if (_pool.m_hold != GEMTYPE_T) {
                for (size_t i = 0; i < next.size(); ++i) {
                    if (next[i].num == GEMTYPE_T) {
                        t_dis = i;
                        break;
                    }
                }
            } else {
                t_dis = 0;
            }
            GenMoving(_pool, movs, cur, x, y, 0);
            for (MovingList::iterator it = movs.begin(); it != movs.end(); ++it) {
                ++search_nodes;
                MovsState &ms = que.append();
                ms.pool_last = _pool;
                signed char wallkick_spin = it->wallkick_spin;
                wallkick_spin = ms.pool_last.WallKickValue(cur.num, (*it).x, (*it).y, (*it).spin, wallkick_spin);
                ms.pool_last.paste((*it).x, (*it).y, getGem(cur.num, (*it).spin));
                int clear = ms.pool_last.clearLines(wallkick_spin);
                int att = ms.pool_last.getAttack(clear, wallkick_spin);
                ms.player = player;
                ms.clear = clear;
                ms.att = att;
                if (clear > 0) {
                    ms.combo = _pool.combo * combo_step_max + combo_step_max;// + 1 - clear;
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
                ms.first.score = Evaluate(ms.first.score2, ai_param, pool, ms.pool_last, cur.num, 0, ms.att, ms.clear,
                                          att, clear, wallkick_spin, _pool.combo, t_dis, upcomeAtt);
                if (wallkick_spin == 0 && it->wallkick_spin) ms.first.score += 1;
                que.push_back();
            }
        }
        // ホールドが使える
        if (canhold && !hold &&
            (
                    (pool.m_hold == 0
                    && !next.empty() && !pool.isCollide(gem_beg_x, gem_beg_y, getGem(next[0].num, 0)))
                    || !pool.isCollide(gem_beg_x, gem_beg_y, getGem(pool.m_hold, 0))
            )
                ) {
            int cur_num;
            if (pool.m_hold) {
                cur_num = pool.m_hold;
            } else {
                cur_num = next[0].num;
            }
            if (cur_num != cur.num) {
                GameField _pool = pool;
                _pool.m_hold = cur.num;
                int t_dis = 14;
                if (_pool.m_hold != GEMTYPE_T) {
                    for (size_t i = 0; i + next_add < next.size(); ++i) {
                        if (next[i + next_add].num == GEMTYPE_T) {
                            t_dis = i;
                            break;
                        }
                    }
                } else {
                    t_dis = 0;
                }
                int x = gem_beg_x, y = gem_beg_y;
                Gem cur = getGem(cur_num, 0);
                GenMoving(_pool, movs, cur, x, y, 1);
                for (MovingList::iterator it = movs.begin(); it != movs.end(); ++it) {
                    ++search_nodes;
                    MovsState &ms = que.append();
                    ms.pool_last = _pool;
                    signed char wallkick_spin = it->wallkick_spin;
                    wallkick_spin = ms.pool_last.WallKickValue(cur_num, (*it).x, (*it).y, (*it).spin, wallkick_spin);
                    ms.pool_last.paste((*it).x, (*it).y, getGem(cur_num, (*it).spin));
                    int clear = ms.pool_last.clearLines(wallkick_spin);
                    int att = ms.pool_last.getAttack(clear, wallkick_spin);
                    ms.player = player;
                    ms.clear = clear;
                    ms.att = att;
                    if (clear > 0) {
                        ms.combo = _pool.combo * combo_step_max + combo_step_max;// + 1 - clear;
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
                    ms.first.score = Evaluate(ms.first.score2, ai_param, pool, ms.pool_last, cur.num, 0, ms.att,
                                              ms.clear, att, clear, wallkick_spin, _pool.combo, t_dis, upcomeAtt);
                    if (wallkick_spin == 0 && it->wallkick_spin) ms.first.score += 1;
                    que.push_back();
                }
            }
        }
        if (que.empty()) {
            return MovingSimple();
        }
        int sw_map1[16][8] = {
                {999, 4,   2,   2,   2,   2,   2,   2},
                {999, 4,   4,   2,   2,   2,   2,   2},
                {50,  999, 4,   2,   2,   2,   2,   2},
                {20,  40,  999, 4,   2,   2,   2,   2},
                {15,  30,  20,  999, 2,   2,   2,   2}, // 4
                {13,  25,  15,  12,  999, 2,   2,   2},
                {14,  27,  17,  14,  20,  999, 3,   2},
                //{ 15,  27,  17,  15,  20, 999,   3,   2},
                //{ 20,  30,  20,  20,  20, 100, 999, 999},
                {20,  30,  25,  20,  20,  100, 999, 999}, // 7
                {25,  60,  50,  40,  40,  40,  500, 999},
                //{ 30,  50,  40,  30,  30,  25,  25,  20},
                //{ 30, 150, 130, 130, 110, 100,  100, 80},
                {30,  90,  75,  60,  60,  60,  60,  9999}, // 9
                //{ 50, 720, 720, 480, 480, 480, 480, 480}, // 9 PC
                //{ 30,  90,  80,  60,  60,  60,  60,  60},
                {30,  240, 200, 200, 180, 160, 160, 9999}, // 10
        };
        int sw_map2[16][8] = {
                {999, 999, 999, 999, 999, 999, 999, 999},
                {60,  60,  999, 999, 999, 999, 999, 999},
                {40,  40,  40,  999, 999, 999, 999, 999},
                {30,  60,  60,  60,  999, 999, 999, 999},
                {25,  45,  30,  30,  30,  999, 999, 999}, // 4
                {25,  35,  35,  30,  30,  30,  999, 999},
                {25,  35,  35,  35,  30,  25,  25,  999},
                {25,  45,  40,  30,  30,  30,  30,  30}, // 7
                {25,  90,  80,  60,  50,  50,  50,  50},
                //{ 30, 220, 200, 200, 160, 150, 150, 120},
                {30,  150, 130, 100, 80,  80,  50,  50}, // 9
                //{ 30, 150, 130, 130, 130, 130, 130, 130}, // 9 PC
                {30,  300, 200, 180, 120, 100, 80,  80}, // 10
                //{ 30, 400, 400, 300, 300, 300, 300, 200}, // 10
        };
        int sw_map3[16][8] = {
                {999, 999, 999, 999, 999, 999, 999, 999},
                {60,  60,  999, 999, 999, 999, 999, 999},
                {40,  40,  40,  999, 999, 999, 999, 999},
                {30,  60,  60,  60,  999, 999, 999, 999},
                {25,  45,  30,  30,  30,  999, 999, 999}, // 4
                {25,  35,  35,  30,  30,  30,  999, 999},
                {25,  35,  35,  35,  30,  25,  25,  999},
                {25,  45,  40,  30,  30,  30,  30,  30}, // 7
                {25,  90,  80,  60,  50,  40,  30,  30},
                //{ 30, 220, 200, 200, 160, 150, 150, 120},
                {30,  120, 100, 80,  70,  60,  50,  40}, // 9
                //{ 30, 150, 130, 130, 130, 130, 130, 130}, // 9 PC
                {30,  240, 200, 160, 120, 90,  70,  60}, // 10
        };
        MovQueue<MovsState> *pq_last = &que2, *pq = &que;
        searchDeep = 1;
        for (int depth = 0; search_nodes < max_search_nodes && depth < maxDeep; searchDeep = ++depth) { //d < maxDeep
            std::swap(pq_last, pq);
#if defined(XP_RELEASE)
            int (*sw_map)[8] = sw_map1;
            if ( ai_settings[player].hash ) {
                sw_map = sw_map2;
                //if ( ai_param.strategy_4w > 0 ) {
                //    sw_map = sw_map3;
                //}
            }
            int search_base_width = sw_map[level][0];// - sw_map[level][0] / 6;
            int search_wide = 1000;
            if ( depth > 7 ) search_wide = sw_map[level][7];
            else search_wide = sw_map[level][depth];
#else
            int sw_map[16][8] = {
                    {15, 30, 20, 15, 10, 10, 10, 10},
            };
            int search_wide = 0;
            if (depth > 7) search_wide = sw_map[0][7];
            else search_wide = sw_map[0][depth];
            int search_base_width = sw_map[0][0];;
#endif
            //int seach_select_best = (level <= 3 ? 1000 : (std::min(search_wide, 30) ) );
            int seach_select_best = std::min(search_wide - search_wide / 4, search_base_width);
            if (level <= 3) {
                seach_select_best = search_wide - search_wide / 4;
            }
            if (seach_select_best < search_base_width) {
                seach_select_best = std::min(search_base_width, std::max(15, search_wide));
            }
            pq->clear();
            int max_combo = 3;
            double max_search_score = pq_last->back().first.score;  // 最悪値を取得
            {
                for (int s = pq_last->size(), i = s / 2; i < s; ++i) {
                    max_search_score = std::max(max_search_score, pq_last->queue[i].first.score);
                }
                max_search_score = (max_search_score * 2.0 + pq_last->front().first.score) / 3.0;
            }
            std::set<GameState> gsSet;
            for (int pqmax_size = (int) pq_last->size(),
                         pq_size = (int) pq_last->size(),
                         stop_size = std::max(0, (int) pq_size - search_wide);
                 pq_size > stop_size;

                 --pq_size, pq_last->dec_size()) {

                if (pq_size > 1) pq_last->pop_back();

                const MovsState &ms_last = pq_last->back();
                if (pq_size != pqmax_size && ms_last.first.score > 50000000) { // ���߷ּ�֦
                    break;
                }
                if (search_nodes >= max_search_nodes) {
                    //MovsState ms_last = pq_last->back();
                    pq->push(ms_last);
                    continue;
                }
                max_combo = std::max(max_combo, (int) ms_last.pool_last.combo);

                if (ai_settings[player].hash) {
                    GameState gs(ms_last.pool_last.hashval, ms_last.pool_last.m_hold, ms_last.att, ms_last.clear,
                                 ms_last.combo, ms_last.pool_last.b2b);
                    if (gsSet.find(gs) == gsSet.end()) {
                        gsSet.insert(gs);
                    } else {
                        continue;
                    }
                }
                int hold = 0;
                //if ( !ms_last.first.movs.empty() && ms_last.first.movs[0] == Moving::MOV_HOLD ) hold = 1;
                if (!ms_last.first.hold) hold = 1;
                int t_dis = 14;
                int d_pos = depth;
                if (next_add && ms_last.pool_last.m_hold) d_pos = depth + 1;
                if (d_pos >= next.size()) {
                    pq->push(ms_last);
                    continue;
                }
                int cur_num = next[d_pos].num;
                if (ms_last.pool_last.m_hold != GEMTYPE_T) {
                    for (size_t i = 0; d_pos + 1 + i < next.size(); ++i) {
                        if (next[d_pos + 1 + i].num == GEMTYPE_T) {
                            t_dis = i;
                            break;
                        }
                    }
                } else {
                    t_dis = 0;
                }
                if (BEG_ADD_Y && ms_last.upcomeAtt < 0)
                    GenMoving(ms_last.pool_last, movs, getGem(cur_num, 0), AI::gem_beg_x,
                              AI::gem_beg_y - ms_last.upcomeAtt, 0);
                else
                    GenMoving(ms_last.pool_last, movs, getGem(cur_num, 0), AI::gem_beg_x, AI::gem_beg_y, 0);

                if (movs.empty()) {
                    MovsState ms = ms_last;
                    ms.first.score += 100000000;
                    pq->push(ms);
                    continue; // ���־͹ҵĻ�ʹ��hold�����������
                } else {
                    MovQueue<MovsState> p(movs.size());
                    for (size_t i = 0; i < movs.size(); ++i) {
                        ++search_nodes;
                        MovsState &ms = p.append();
                        {
                            ms.first = ms_last.first;
                            ms.pool_last = ms_last.pool_last;
                            signed char wallkick_spin = movs[i].wallkick_spin;
                            wallkick_spin = ms.pool_last.WallKickValue(cur_num, movs[i].x, movs[i].y, movs[i].spin,
                                                                       wallkick_spin);
                            ms.pool_last.paste(movs[i].x, movs[i].y, getGem(cur_num, movs[i].spin));
                            int clear = ms.pool_last.clearLines(wallkick_spin);
                            int att = ms.pool_last.getAttack(clear, wallkick_spin);
                            ms.player = player;
                            ms.clear = clear + ms_last.clear;
                            ms.att = att + ms_last.att;
                            ms.upcomeAtt = ms_last.upcomeAtt;
                            if (clear > 0) {
                                ms.combo = ms_last.combo + combo_step_max + 1 - clear;
                                if (ms_last.upcomeAtt > 0)
                                    ms.upcomeAtt = std::max(0, ms_last.upcomeAtt - att);
                            } else {
                                ms.combo = 0;
                                if (ms_last.upcomeAtt > 0) {
                                    ms.upcomeAtt = -ms_last.upcomeAtt;
                                    ms.pool_last.minusRow(ms_last.upcomeAtt);
                                }
                            }
                            ms.max_att = std::max((int) ms_last.max_att, ms_last.att + att);
                            ms.max_combo = std::max(ms_last.max_combo,
                                                    ms.combo); //ms_last.max_combo + getComboAttack( ms.pool_last.combo );
                            ms.first.score2 = ms_last.first.score2;
                            ms.first.score = Evaluate(ms.first.score2, ai_param,
                                                      pool,
                                                      ms.pool_last, cur_num, depth + 1, ms.att, ms.clear, att, clear,
                                                      wallkick_spin, ms_last.pool_last.combo, t_dis, ms_last.upcomeAtt);
                            if (wallkick_spin == 0 && movs[i].wallkick_spin) ms.first.score += 1;
                        }
                        p.push_back();
                    }
                    for (int i = 0; i < seach_select_best && !p.empty(); ++i) {
                        pq->push(p.front());
                        p.pop_back();
                        p.dec_size();
                    }
                }
                if (canhold && depth + next_add < next.size()) {
                    MovsState ms_last = pq_last->back();
                    //int cur_num = ms_last.pool_last.m_hold;
                    int cur_num;
                    int d_pos = depth + next_add;
                    if (ms_last.pool_last.m_hold != next[d_pos].num) {
                        if (ms_last.pool_last.m_hold) {
                            cur_num = ms_last.pool_last.m_hold;
                        } else {
                            cur_num = next[d_pos].num;
                        }
                        ms_last.pool_last.m_hold = next[d_pos].num;
                        if (ms_last.pool_last.m_hold != GEMTYPE_T) {
                            t_dis -= next_add;
                            if (t_dis < 0) {
                                for (size_t i = 0; d_pos + 1 + i < next.size(); ++i) {
                                    if (next[i + 1 + d_pos].num == GEMTYPE_T) {
                                        t_dis = i;
                                        break;
                                    }
                                }
                            }
                        } else {
                            t_dis = 0;
                        }
                        if (BEG_ADD_Y && ms_last.upcomeAtt < 0)
                            GenMoving(ms_last.pool_last, movs, getGem(cur_num, 0), AI::gem_beg_x,
                                      AI::gem_beg_y - ms_last.upcomeAtt, 1);
                        else
                            GenMoving(ms_last.pool_last, movs, getGem(cur_num, 0), AI::gem_beg_x, AI::gem_beg_y, 1);

                        if (movs.empty()) {
                            MovsState ms = ms_last;
                            ms.first.score += 100000000;
                            pq->push(ms);
                        } else {
                            MovQueue<MovsState> p(movs.size());
                            for (size_t i = 0; i < movs.size(); ++i) {
                                ++search_nodes;
                                MovsState &ms = p.append();
                                {
                                    ms.first = ms_last.first;
                                    ms.pool_last = ms_last.pool_last;
                                    signed char wallkick_spin = movs[i].wallkick_spin;
                                    wallkick_spin = ms.pool_last.WallKickValue(cur_num, movs[i].x, movs[i].y,
                                                                               movs[i].spin, wallkick_spin);
                                    ms.pool_last.paste(movs[i].x, movs[i].y, getGem(cur_num, movs[i].spin));
                                    int clear = ms.pool_last.clearLines(wallkick_spin);
                                    int att = ms.pool_last.getAttack(clear, wallkick_spin);
                                    ms.player = player;
                                    ms.clear = clear + ms_last.clear;
                                    ms.att = att + ms_last.att;
                                    ms.upcomeAtt = ms_last.upcomeAtt;
                                    if (clear > 0) {
                                        ms.combo = ms_last.combo + combo_step_max + 1 - clear;
                                        if (ms_last.upcomeAtt > 0)
                                            ms.upcomeAtt = std::max(0, ms_last.upcomeAtt - att);
                                    } else {
                                        ms.combo = 0;
                                        if (ms_last.upcomeAtt > 0) {
                                            ms.upcomeAtt = -ms_last.upcomeAtt;
                                            ms.pool_last.minusRow(ms_last.upcomeAtt);
                                        }
                                    }
                                    ms.max_att = std::max((int) ms_last.max_att, ms_last.att + att);
                                    ms.max_combo = std::max(ms_last.max_combo,
                                                            ms.combo); //ms_last.max_combo + getComboAttack( ms.pool_last.combo );
                                    ms.first.score2 = ms_last.first.score2;
                                    ms.first.score = Evaluate(ms.first.score2, ai_param,
                                                              pool,
                                                              ms.pool_last, cur_num, depth + 1, ms.att, ms.clear, att,
                                                              clear, wallkick_spin, ms_last.pool_last.combo, t_dis,
                                                              ms_last.upcomeAtt);
                                    if (wallkick_spin == 0 && movs[i].wallkick_spin) ms.first.score += 1;
                                }
                                p.push_back();
                            }
                            for (int i = 0; i < seach_select_best && !p.empty(); ++i) {
                                pq->push(p.front());
                                p.pop_back();
                                p.dec_size();
                            }
                        }
                    }
                }
            }
            if (pq->empty()) {
                return MovingSimple();
            }
        }
        //if (0)
        if (ai_settings[player].hash && canhold && search_nodes < max_search_nodes) { // extra search
            std::swap(pq_last, pq);
            pq->clear();
            int depth = searchDeep - 1;
#if defined(XP_RELEASE)
            int (*sw_map)[8] = sw_map1;
            if ( ai_settings[player].hash )
                sw_map = sw_map2;
            int search_base_width = sw_map[level][0];// - sw_map[level][0] / 6;
            int search_wide = 1000;
            if ( depth > 7 ) search_wide = sw_map[level][7];
            else search_wide = sw_map[level][depth];
#else
            int search_wide = (depth < 2 ? 20 : 20);
            int search_base_width = 20;
#endif
            //int seach_select_best = (level <= 3 ? 1000 : (std::min(search_wide, 30) ) );
            int seach_select_best = std::min(search_wide - search_wide / 4, search_base_width);
            if (level <= 3) {
                seach_select_best = search_wide - search_wide / 4;
            }
            if (seach_select_best < search_base_width) {
                seach_select_best = std::min(search_base_width, std::max(15, search_wide));
            }

            std::set<GameState> gsSet;
            for (int pqmax_size = (int) pq_last->size(),
                         pq_size = (int) pq_last->size(),
                         stop_size = std::max(0, (int) pq_size - search_wide);
                 pq_size > stop_size;

                 --pq_size, pq_last->dec_size()) {

                if (pq_size > 1) pq_last->pop_back();

                const MovsState &ms_last = pq_last->back();
                if (pq_size != pqmax_size && ms_last.first.score > 50000000) { // ���߷ּ�֦
                    break;
                }
                pq->push(ms_last);
                if (search_nodes >= max_search_nodes) {
                    continue;
                }
                //max_combo = std::max( max_combo, (int)ms_last.pool_last.combo );
                {
                    GameState gs(ms_last.pool_last.hashval, ms_last.pool_last.m_hold, ms_last.att, ms_last.clear,
                                 ms_last.combo, ms_last.pool_last.b2b);
                    if (gsSet.find(gs) == gsSet.end()) {
                        gsSet.insert(gs);
                    } else {
                        continue;
                    }
                }
                //if ( !ms_last.first.movs.empty() && ms_last.first.movs[0] == Moving::MOV_HOLD ) hold = 1;
                if (!ms_last.first.hold) {
                    continue;
                }
                int t_dis = 14;
                int d_pos = depth + next_add;
                int cur_num = ms_last.pool_last.m_hold;
                for (size_t i = 0; d_pos + 1 + i < next.size(); ++i) {
                    if (next[d_pos + 1 + i].num == GEMTYPE_T) {
                        t_dis = i;
                        break;
                    }
                }
                if (BEG_ADD_Y && ms_last.upcomeAtt < 0)
                    GenMoving(ms_last.pool_last, movs, getGem(cur_num, 0), AI::gem_beg_x,
                              AI::gem_beg_y - ms_last.upcomeAtt, 0);
                else
                    GenMoving(ms_last.pool_last, movs, getGem(cur_num, 0), AI::gem_beg_x, AI::gem_beg_y, 0);

                if (movs.empty()) {
                    MovsState ms = ms_last;
                    ms.first.score += 100000000;
                    pq->push(ms);
                } else {
                    MovQueue<MovsState> p;
                    for (size_t i = 0; i < movs.size(); ++i) {
                        ++search_nodes;
                        MovsState &ms = p.append();
                        {
                            ms.first = ms_last.first;
                            ms.pool_last = ms_last.pool_last;
                            signed char wallkick_spin = movs[i].wallkick_spin;
                            wallkick_spin = ms.pool_last.WallKickValue(cur_num, movs[i].x, movs[i].y, movs[i].spin,
                                                                       wallkick_spin);
                            ms.pool_last.paste(movs[i].x, movs[i].y, getGem(cur_num, movs[i].spin));
                            int clear = ms.pool_last.clearLines(wallkick_spin);
                            int att = ms.pool_last.getAttack(clear, wallkick_spin);
                            ms.player = player;
                            ms.clear = clear + ms_last.clear;
                            ms.att = att + ms_last.att;
                            ms.upcomeAtt = ms_last.upcomeAtt;
                            if (clear > 0) {
                                ms.combo = ms_last.combo + combo_step_max + 1 - clear;
                                if (ms_last.upcomeAtt > 0)
                                    ms.upcomeAtt = std::max(0, ms_last.upcomeAtt - att);
                            } else {
                                ms.combo = 0;
                                if (ms_last.upcomeAtt > 0) {
                                    ms.upcomeAtt = -ms_last.upcomeAtt;
                                    ms.pool_last.minusRow(ms_last.upcomeAtt);
                                }
                            }
                            ms.max_att = std::max((int) ms_last.max_att, ms_last.att + att);
                            ms.max_combo = std::max(ms_last.max_combo,
                                                    ms.combo); //ms_last.max_combo + getComboAttack( ms.pool_last.combo );
                            ms.first.score2 = ms_last.first.score2;
                            ms.first.score = Evaluate(ms.first.score2, ai_param,
                                                      pool,
                                                      ms.pool_last, cur_num, depth + 1, ms.att, ms.clear, att, clear,
                                                      wallkick_spin, ms_last.pool_last.combo, t_dis, ms_last.upcomeAtt);
                        }
                        p.push_back();
                    }
                    for (int i = 0; i < seach_select_best && !p.empty(); ++i) {
                        pq->push(p.front());
                        p.pop_back();
                        p.dec_size();
                    }
                }
            }
            if (pq->empty()) {
                return MovingSimple();
            }
        }
        {
            MovsState m, c;
            std::swap(pq_last, pq);
            pq_last->pop(m);
            if (!GAMEMODE_4W) {
                while (!pq_last->empty()) {
                    pq_last->pop(c);
                    if (m.first.score > c.first.score) {
                        m = c;
                    }
                }
            }
            return m.first;
        }
    }

    struct AI_THREAD_PARAM {
        TetrisAI_t func;
        Moving *ret_mov;
        int *flag;
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
        int level;
        int player;

        AI_THREAD_PARAM(TetrisAI_t _func, Moving &_ret_mov, int &_flag, const AI_Param &_ai_param,
                        const GameField &_pool, int _hold, Gem _cur, int _x, int _y, const std::vector<Gem> &_next,
                        bool _canhold, int _upcomeAtt, int _maxDeep, int &_searchDeep, int _level, int _player) {
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
            level = _level;
            player = _player;
        }

        ~AI_THREAD_PARAM() {
        }
    };

    void AI_Thread(void *lpParam) {
        AI_THREAD_PARAM *p = (AI_THREAD_PARAM *) lpParam;
        int searchDeep = 0;
        *p->flag = 1;

        AI::MovingSimple best = AISearch(p->ai_param, p->pool, p->hold, p->cur, p->x, p->y, p->next, p->canhold,
                                         p->upcomeAtt, p->maxDeep, searchDeep, p->level, p->player);

        AI::Moving mov;
        const AI::GameField &gamefield = p->pool;
        std::vector<AI::Gem> &gemNext = p->next;
        mov.movs.push_back(Moving::MOV_DROP);
        if (best.x != AI::MovingSimple::INVALID_POS) { // find path
            int hold_num = gamefield.m_hold;
            if (gamefield.m_hold == 0 && !gemNext.empty()) {
                hold_num = gemNext[0].num;
            }
            std::vector<AI::Moving> movs;
            AI::Gem cur;
            if (best.hold) {
                cur = AI::getGem(hold_num, 0);
                FindPathMoving(gamefield, movs, cur, AI::gem_beg_x, AI::gem_beg_y, true);
            } else {
                cur = p->cur;
                FindPathMoving(gamefield, movs, cur, p->x, p->y, false);
            }
            for (size_t i = 0; i < movs.size(); ++i) {
                if (movs[i].x == best.x && movs[i].y == best.y && movs[i].spin == best.spin) {
                    if ((isEnableAllSpin() || cur.num == GEMTYPE_T)) {
                        if (movs[i].wallkick_spin == best.wallkick_spin) {
                            mov = movs[i];
                            break;
                        }
                    } else {
                        mov = movs[i];
                        break;
                    }
                } else if (cur.num == GEMTYPE_I || cur.num == GEMTYPE_Z || cur.num == GEMTYPE_S) {
                    if ((best.spin + 2) % 4 == movs[i].spin) {
                        if (best.spin == 0) {
                            if (movs[i].x == best.x && movs[i].y == best.y - 1) {
                                mov = movs[i];
                                break;
                            }
                        } else if (best.spin == 1) {
                            if (movs[i].x == best.x - 1 && movs[i].y == best.y) {
                                mov = movs[i];
                                break;
                            }
                        } else if (best.spin == 2) {
                            if (movs[i].x == best.x && movs[i].y == best.y + 1) {
                                mov = movs[i];
                                break;
                            }
                        } else if (best.spin == 3) {
                            if (movs[i].x == best.x + 1 && movs[i].y == best.y) {
                                mov = movs[i];
                                break;
                            }
                        }
                    }
                }
            }
        }
        if (mov.movs.empty()) {
            mov.movs.clear();
            mov.movs.push_back(AI::Moving::MOV_NULL);
            mov.movs.push_back(AI::Moving::MOV_DROP);
        }
        *p->ret_mov = mov;

        *p->searchDeep = searchDeep;
        *p->flag = -1;
        //_endthread();
    }

    int
    RunAI(Moving &ret_mov, int &flag, const AI_Param &ai_param, const GameField &pool, int hold, Gem cur, int x, int y,
          const std::vector<Gem> &next, bool canhold, int upcomeAtt, int maxDeep, int &searchDeep, int level,
          int player) {
        flag = 0;
        AI_THREAD_PARAM *p = new AI_THREAD_PARAM(NULL, ret_mov, flag, ai_param, pool, hold, cur, x, y, next, canhold,
                                                 upcomeAtt,
                                                 maxDeep, searchDeep, level, player);
        AI_Thread(p);
        delete p;

        //_beginthread(AI_Thread, 0, new AI_THREAD_PARAM(NULL, ret_mov, flag, ai_param, pool, hold, cur, x, y, next, canhold, upcomeAtt, maxDeep, searchDeep, level, player) );
        return 0;
    }
}