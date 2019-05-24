#ifndef SFINDER_CPP_TETRIS_AI_H
#define SFINDER_CPP_TETRIS_AI_H

#include "gamepool.h"

#include <algorithm>
#include <vector>

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
    };

    extern _ai_settings ai_settings[2];

    struct Moving {
        enum {
            MOV_NULL,  // 0
            MOV_L,
            MOV_R,
            MOV_LL,  // 3
            MOV_RR,
            MOV_D,
            MOV_DD,
            MOV_LSPIN,
            MOV_RSPIN,
            MOV_DROP,  // 9
            MOV_HOLD,
            MOV_SPIN2,
            MOV_REFRESH,
        };
        std::vector<int> movs;
        int x, y;
        int score, score2;
        signed char spin;
        signed char wallkick_spin;

        Moving() {
            wallkick_spin = 0;
            movs.reserve(16);
        }

        Moving(const Moving &m) {
            //movs.reserve (16);
            movs = m.movs;
            x = m.x;
            y = m.y;
            spin = m.spin;
            score = m.score;
            score2 = m.score2;
            wallkick_spin = m.wallkick_spin;
        }

        Moving(const Moving &m, int _spin) {
            movs.reserve(16);
            movs = m.movs;
            spin = _spin;
        }

        bool operator<(const Moving &_m) const {
            return score > _m.score;
        }

        bool operator==(const Moving &_m) const {
            if (x != _m.x || y != _m.y) return false;
            if (spin != _m.spin) return false;
            if (wallkick_spin != _m.wallkick_spin) return false;
            return true;
        }
    };

    struct MovingSimple {
        enum {
            MOV_NULL,
            MOV_L,
            MOV_R,
            MOV_LL,
            MOV_RR,
            MOV_D,
            MOV_DD,
            MOV_LSPIN,
            MOV_RSPIN,
            MOV_DROP,
            MOV_HOLD,
            MOV_SPIN2,
            MOV_REFRESH,
        };
        enum {
            INVALID_POS = -64,
        };
        int x, y;
        int lastmove;
        double score, score2;
        signed char spin;
        signed char wallkick_spin;
        bool hold;

        MovingSimple() {
            x = INVALID_POS;
            wallkick_spin = 0;
            lastmove = MovingSimple::MOV_NULL;
        }

        MovingSimple(const Moving &m) {
            x = m.x;
            y = m.y;
            spin = m.spin;
            score = m.score;
            score2 = m.score2;
            wallkick_spin = m.wallkick_spin;
            if (m.movs.empty()) hold = false;
            else hold = (m.movs[0] == MovingSimple::MOV_HOLD);
            if (m.movs.empty()) lastmove = MovingSimple::MOV_NULL;
            else lastmove = m.movs.back();
        }

        MovingSimple(const MovingSimple &m) {
            x = m.x;
            y = m.y;
            spin = m.spin;
            score = m.score;
            score2 = m.score2;
            wallkick_spin = m.wallkick_spin;
            hold = m.hold;
            lastmove = m.lastmove;
        }

        MovingSimple(const MovingSimple &m, int _spin) {
            lastmove = m.lastmove;
            hold = m.hold;
            spin = (signed char) _spin;
        }

        bool operator==(const MovingSimple &_m) const {
            if (x != _m.x || y != _m.y) return false;
            if (spin != _m.spin) return false;
            if (lastmove != _m.lastmove) return false;
            if (hold != _m.hold) return false;
            if (wallkick_spin != _m.wallkick_spin) return false;
            return true;
        }

        bool operator<(const MovingSimple &_m) const {
            return score > _m.score;
        }
    };

    template<class T>
    struct MovList {
        std::vector<T> queue;
        //T queue[1024];
        int beg, end;

        MovList() {
            beg = end = 0;
        }

        MovList(size_t size) {
            beg = end = 0;
            //queue.resize( size );
            queue.reserve(size);
        }

        void clear() {
            beg = end = 0;
            queue.clear();
        }

        size_t size() const {
            return end - beg;
        }

        T &back() {
            return queue[end - 1];
        }

        void push(const T &mov) {
            queue.push_back(mov);
            //queue[end] = mov;
            ++end;
        }

        bool empty() const {
            return beg == end;
        }

        void pop(T &mov) {
            mov = queue[beg++];
        }
    };

    template<class T>
    struct MovQueue {
        std::vector<T> queue;

        //GameField pool;
        MovQueue() {
        }

        MovQueue(size_t size) {
            queue.reserve(size);
        }

        void clear() {
            queue.clear();
        }

        size_t size() const {
            return queue.size();
        }

        T &front() {
            return queue.front();
        }

        T &back() {
            return queue.back();
        }

        T &append() {
            queue.push_back(T());
            return back();
        }

        void push_back() {
            std::push_heap(queue.begin(), queue.end());
        }

        void pop_back() {
            std::pop_heap(queue.begin(), queue.end());
        }

        void dec_size() {
            queue.pop_back();
        }

        void push(const T &mov) {
            queue.push_back(mov);
            std::push_heap(queue.begin(), queue.end());
        }

        bool empty() const {
            return queue.empty();
        }

        void pop(T &mov) {
            std::pop_heap(queue.begin(), queue.end());
            mov = queue.back();
            queue.pop_back();
        }

        void sort() {
            std::sort(queue.begin(), queue.end());
        }
    };

    struct AI_Param {
        double miny_factor; // ��߸߶ȷ�
        double hole; // -����
        double open_hole; // -���Ŷ������ܲ��
        double v_transitions; // -ˮƽת��ϵ��
        double tspin3; // T3������

        double clear_efficient; // ����Ч��ϵ��
        double upcomeAtt; // -Ԥ����������ϵ��
        double h_factor; // -�߶Ȳ�ϵ��
        double hole_dis_factor2; // -������ϵ��
        double hole_dis; // -���ľ����
        //int flat_factor; // ƽֱϵ��

        double hole_dis_factor; // -������ϵ��
        double tspin; // tspinϵ��
        double hold_T; // hold T��Iϵ��
        double hold_I; // hold T��Iϵ��
        double clear_useless_factor; // ��Ч��ϵ��
        //int ready_combo; // ����Ԥ����x

        double dif_factor; //ƫ��ֵ
        double strategy_4w;
    };

    struct MovsState {
        MovingSimple first;
        GameField pool_last;
        int att, clear;
        signed short max_combo, max_att, combo;
        signed short player, upcomeAtt;

        MovsState() { upcomeAtt = max_combo = combo = att = clear = 0; }

        bool operator<(const MovsState &m) const {
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
            if (ai_settings[player].strategy_4w) {
                if ((max_combo > 6 * 32 || m.max_combo > 6 * 32)) {
                    if (max_combo != m.max_combo) {
                        return max_combo < m.max_combo;
                    }
                }
                if ((combo >= 32 * 3 || m.combo >= 32 * 3) && combo != m.combo) {
                    return combo < m.combo;
                }
            } else {
                if (ai_settings[player].combo) {
                    if ((max_combo > 6 * 32 || m.max_combo > 6 * 32)) {
                        if (max_combo != m.max_combo) {
                            return max_combo < m.max_combo;
                        }
                    }
                    if (max_combo > combo && m.max_combo > m.combo && (m.max_combo > 4 * 32 || max_combo > 4 * 32)) {
                        if ((combo <= 2 * 32 && m.combo <= 2 * 32)) {
                            double diff = first.score - m.first.score;
                            if (-1000 < diff && diff < 1000) {
                                if (att != m.att)
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
                if ((combo > 32 * 2 || m.combo > 32 * 2) && combo != m.combo) {
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

    typedef char *(*AIName_t)(int level);

    typedef char *(*TetrisAI_t)(int overfield[], int field[], int field_w, int field_h, int b2b, int combo,
                                char next[], char hold, bool curCanHold, char active, int x, int y, int spin,
                                bool canhold, bool can180spin, int upcomeAtt, int comboTable[], int maxDepth, int level,
                                int player);

    void setComboList(std::vector<int> combolist);

    int getComboAttack(int combo);

    void setSpin180(bool enable);

    bool spin180Enable();

    void setAIsettings(int player, const char *key, int val);

    void GenMoving(const GameField &field, std::vector<MovingSimple> &movs, Gem cur, int x, int y, bool hold);

    void FindPathMoving(const GameField &field, std::vector<Moving> &movs, Gem cur, int x, int y, bool hold);

    MovingSimple
    AISearch(AI_Param ai_param, const GameField &pool, int hold, Gem cur, int x, int y, const std::vector<Gem> &next,
             bool canhold, int upcomeAtt, int maxDeep, int &searchDeep, int level);

    int
    RunAI(Moving &ret_mov, int &flag, const AI_Param &ai_param, const GameField &pool, int hold, Gem cur, int x, int y,
          const std::vector<Gem> &next, bool canhold, int upcomeAtt, int maxDeep, int &searchDeep, int level,
          int player);

    double Evaluate(
            double &clearScore, const AI_Param &ai_param, const GameField &last_pool, const GameField &pool,
            int cur_num,
            int curdepth,
            int total_clear_att, int total_clears, int clear_att, int clears, signed char wallkick_spin,
            int lastCombo, int t_dis, int upcomeAtt
    );
}

#endif //SFINDER_CPP_TETRIS_AI_H
