#include "gtest/gtest.h"

#include "misa/tetris_gem.h"
#include "misa/tetris_ai.h"
#include "misa/bot.h"

namespace AI {
    using namespace std::literals::string_literals;

    class SampleTest : public ::testing::Test {
    };

    void updateField(AI::GameField &m_pool, const std::string &s) {
        std::vector<int> rows;
        bool solidGarbage = false;
        int row = 0;
        int col = 0;
        for (const auto &c : s) {
            switch (c) {
                case '0':
                case '1':
                    ++col;
                    break;
                case '2':
                    ++col;
                    row |= (1 << (10 - col));
                    break;
                case '3':
                    solidGarbage = true;
                    break;
                default:
                    break;
            }
            if (solidGarbage) {
                break;
            }
            if (col == 10) {
                rows.push_back(row);
                row = 0;
                col = 0;
            }
        }
        m_pool.reset(10, rows.size());
        for (auto &row : rows) {
            m_pool.addRow(row);
        }
    }

    TEST_F(SampleTest, case1) {
        Bot bot;
        bot.init();

        // フィールドの設定
        AI::GameField _pool;
        _pool.combo = 0;
        updateField(_pool,
                    "0,0,0,0,0,0,0,0,0,0;"
                    "0,0,0,0,0,0,0,0,0,0;"
                    "0,0,0,0,0,0,0,0,0,0;"
                    "0,0,0,0,0,0,0,0,0,0;"
                    "0,0,0,0,0,0,0,0,0,0;"
                    "0,0,0,0,0,0,0,0,0,0;"
                    "0,0,0,0,0,0,0,0,0,0;"
                    "0,0,0,0,0,0,0,0,0,0;"
                    "0,0,0,0,0,0,0,0,0,0;"
                    "0,0,0,0,0,0,0,0,0,0;"
                    "0,0,0,0,0,0,0,0,0,0;"
                    "0,0,0,0,0,0,0,0,0,0;"
                    "0,0,0,0,0,0,0,0,0,0;"
                    "0,0,0,0,0,0,0,0,0,0;"
                    "0,0,0,0,0,0,0,0,0,0;"
                    "0,0,0,0,0,0,0,0,0,0;"
                    "0,0,0,0,0,0,0,0,0,0;"
                    "0,0,0,0,0,0,0,0,0,0;"
                    "0,0,0,0,0,0,0,0,0,0;"
                    "0,0,0,2,2,2,2,0,0,0;"
        );

        AI::GameField _pool2 = _pool;

        // 変数の初期化
        int cur_num = AI::GEMTYPE_T;
        const int combo_step_max = 32;
        const int m_upcomeAtt = 4;
        int upcomeAtt = std::min(m_upcomeAtt, _pool.height() - AI::gem_beg_y - 1);

        // MovingSimple
        AI::MovingSimple it = AI::MovingSimple{};
        it.x = 3;
        it.y = 19;
        it.spin = 0;
        it.score = 0;
        it.score2 = 0;
        it.wallkick_spin = 0;
        it.hold = false;
        it.lastmove = AI::MovingSimple::MOV_NULL;

        // MovsState
        AI::MovsState ms = AI::MovsState();
        ms.pool_last = _pool;
        ms.player = 0;

        signed char wallkick_spin = ms.pool_last.WallKickValue(cur_num, it.x, it.y, it.spin, it.wallkick_spin);
        int clear = ms.pool_last.clearLines(wallkick_spin);
        int att = ms.pool_last.getAttack(clear, wallkick_spin);
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
        ms.first = it;
        ms.first.score2 = 0;

        int score2 = 0;  // prev score
//    const AI::AI_Param aiParam = {13, 9, 17, 10, 29, 25, 39, 2, 12, 19, 7, 24, 21, 16, 14, 19, 200};
        const AI::AI_Param aiParam = {13, 26, 17, 10, 29, 25, 39, 2, 12, 19, 7, 24, 21, 16, 14, 19, 200};
        int t_dis = 14;

        ms.pool_last = _pool2;
//    ms.pool_last.paste(it.x, it.y, AI::getGem(cur_num, it.spin));
        AI::Gem cur = AI::getGem(cur_num, 0);

        int score = AI::Evaluate(
                score2, aiParam, _pool, ms.pool_last, cur.num, 0,
                ms.att, ms.clear, att, clear, wallkick_spin, _pool.combo, t_dis, upcomeAtt
        );

        std::cout << "score=" << score << std::endl;  // 75
    }
}
