#include "bot.h"
#include <cassert>
#include <deque>
#include <array>
#include <random>
#include <algorithm>

#include "../fumen/parser.hpp"

using namespace std;

Bot::Bot() : m_hold(' ') {
    m_gemMap[' '] = AI::GEMTYPE_NULL;
    m_gemMap['I'] = AI::GEMTYPE_I;
    m_gemMap['T'] = AI::GEMTYPE_T;
    m_gemMap['L'] = AI::GEMTYPE_L;
    m_gemMap['J'] = AI::GEMTYPE_J;
    m_gemMap['Z'] = AI::GEMTYPE_Z;
    m_gemMap['S'] = AI::GEMTYPE_S;
    m_gemMap['O'] = AI::GEMTYPE_O;
}

Bot::Bot(const Bot &orig) {
}

Bot::~Bot() {}

void Bot::startParser() {
    while (true) {
        string command;
        cin >> command;
        if (command == "settings") {
            string part1, part2;
            cin >> part1 >> part2;
            changeSettings(part1, part2);
        } else if (command == "update") {
            string part1, part2, part3;
            cin >> part1 >> part2 >> part3;
            updateState(part1, part2, part3);
#if DEBUG_LEVEL >= 4
            cerr << command <<" "<< part1 <<" "<< part2 <<" "<< part3<<endl;
#endif
        } else if (command == "action2") {
            string part1, part2;
            cin >> part1 >> part2;
            std::stringstream out;
            Result result;
            outputAction(result, out);
        } else if (command.size() == 0) {
            // no more commands, exit.
            break;
        } else {
            cerr << "Unable to parse command: " << command << endl;
        }
    }
}

void Bot::updateField(const std::string &s) {
    vector<int> rows;
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
    tetris.m_pool.reset(10, rows.size());
    for (auto &row : rows) {
        tetris.addRow(row);
    }
}

void Bot::updateField(const fumen::ColorField &colorField) {
    vector<int> rows;
    bool solidGarbage = false;
    int row = 0;
    int col = 0;
    for (int y = 19; 0 <= y; --y) {
        for (int x = 0; x < 10; ++x) {
            bool b = colorField.existsAt(x, y);
            switch (b ? '2' : '0') {
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
    }
    tetris.m_pool.reset(10, rows.size());
    for (auto &row : rows) {
        tetris.addRow(row);
    }
}

void Bot::updateQueue(const std::string &s) {
    int i = 1;
    for (const auto &c : s) {
        if (c == ',')continue;
        tetris.m_next[i++] = AI::getGem(m_gemMap[c], 0);
    }
    tetris.m_next_num = i;
    assert(tetris.newpiece());
}

void Bot::changeSettings(const std::string &p1, const std::string &p2) {
    bool changed = false;
    if (p1 == "level") {
        ai.level = std::stoi(p2);
        if (ai.level > 10)
            ai.level = 10;
        if (ai.level < -2)
            ai.level = -2;
        changed = true;
    } else if (p1 == "style") {
        ai.style = std::stoi(p2);
        if (ai.style == -1)
            ai.style = 2;
        changed = true;
    }

    if (changed) {
        setup();
        cout << "{\"name\":\"" << tetris.m_name << "\"}" << endl;
    }
}

void Bot::updateState(const std::string &p1, const std::string &p2, const std::string &p3) {
    if (p2 == "next_pieces")
        updateQueue(p3);
    else if (p2 == "combo")
        tetris.m_pool.combo = std::stoi(p3);
    else if (p2 == "field")
        updateField(p3);
    else if (p2 == "this_piece_type")
        tetris.m_next[0] = AI::getGem(m_gemMap[p3[0]], 0);
    else if (p2 == "inAtt")
        m_upcomeAtt = std::stoi(p3);
    else if (p2 == "round") {
        if (p3 == "1") tetris.reset(0);
        m_upcomeAtt = 0;
    }
}

void Bot::setup() {
    if (ai.level <= 0) {
        AI::AI_Param param[2] = {
                {

                        //  43,  47,  84,  62,  60,  47,  53,  21,   2,  98,  85,  13,  21,  37,  38,  0
                        //  47,  66,  86,  66, -79,  42,  38,  23,  -3,  95,  74,  13,  27,  36,  37,  0
                        //  45,  61,  99,  49,  63,  40,  42,  31, -27,  88,  80,  28,  28,  41,  33,  0
                        //  58,  61,  90,  82,  19,  27,  44,  17,  -4,  75,  47,  20,  38,  32,  41,  0
                        47, 62, 94, 90, 11, 35, 48, 19, -21, 78, 64, 20, 42, 42, 39, 300
                        //  48,  65,  84,  59, -75,  39,  43,  23, -17,  92,  64,  20,  29,  37,  36,  0

                },
                {

                        //  43,  47,  84,  62,  60,  47,  53,  21,   2,  98,  85,  13,  21,  37,  38,  0
                        //  47,  66,  86,  66, -79,  42,  38,  23,  -3,  95,  74,  13,  27,  36,  37,  0
                        //  45,  61,  99,  49,  63,  40,  42,  31, -27,  88,  80,  28,  28,  41,  33,  0 // a
                        //  58,  61,  90,  82,  19,  27,  44,  17,  -4,  75,  47,  20,  38,  32,  41,  0 // b
                        //  47,  62,  94,  90,  11,  35,  48,  19, -21,  78,  64,  20,  42,  42,  39,  0 // s
                        //  48,  65,  84,  59, -75,  39,  43,  23, -17,  92,  64,  20,  29,  37,  36,  0 // s
                        71, 12, 78, 52, 96, 37, 14, 24, 40,  99, 44, 49, 93, 25, 44, 380
                }
        };
        tetris.m_ai_param = param[0];
    }

    std::string ai_name = "T-spin AI";
    if (ai.style == 1) {
        ai_name = "T-spin+ AI";
    } else if (ai.style == 2) {
        AI::setAIsettings(0, "hash", 0);
        //
    } else if (ai.style == 3) {
        tetris.m_ai_param.tspin = tetris.m_ai_param.tspin3 = -300;
        tetris.m_ai_param.clear_useless_factor *= 0.8;
        ai_name = "Ren AI";
    } else if (ai.style == 4) {
        tetris.hold = false;
        tetris.m_ai_param.clear_useless_factor *= 0.7;
        ai_name = "non-Hold";
        // no 4w
        tetris.m_ai_param.strategy_4w = 0;
        AI::setAIsettings(0, "4w", 0);
    } else if (ai.style == 5) {
        tetris.m_ai_param.tspin = tetris.m_ai_param.tspin3 = -300;
        tetris.m_ai_param.clear_useless_factor *= 0.8;
        tetris.m_ai_param.strategy_4w = 500;
        ai_name = "4W Ren AI";
    } else if (ai.style != -1) { //if ( ai.style == 5 ) {
        AI::AI_Param param[2] = {
                {49, 918, 176, 33, -300, -0, 0, 25, 22, 99, 41, -300, 0, 14, 290, 0}, // defence AI
                {21, 920, 66,  40, -300, -2, 0, 26, 8,  71, 13, -300, 0, 7,  269, 0},
        };
        AI::setAIsettings(0, "combo", 0);
        tetris.m_ai_param = param[0];
        ai_name = "Downstack";
    }

    if (ai.style) {
        char name[200];
        sprintf(name, "%s LV%d", ai_name.c_str(), ai.level);
        tetris.m_name = name;
#if DEBUG_LEVEL >= 1
        cerr << "[debug] Name:" << tetris.m_name << std::endl;
#endif
    }
    if (tetris.pAIName) {
        tetris.m_name = tetris.pAIName(ai.level);
    }
    if (ai.level < 6) {
        tetris.m_ai_param.strategy_4w = 0;
    }
    if (tetris.m_ai_param.strategy_4w > 0) {
        AI::setAIsettings(0, "4w", 1);
    }

    if (rule.combo_table_style == 0) {
        int a[] = {0, 0, 0, 1, 1, 2};
        AI::setComboList(std::vector<int>(a, a + sizeof(a) / sizeof(*a)));
        tetris.m_ai_param.strategy_4w = 0;
        AI::setAIsettings(0, "4w", 0);
    } else if (rule.combo_table_style == 1) {
        int a[] = {0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 4, 5};
        AI::setComboList(std::vector<int>(a, a + sizeof(a) / sizeof(*a)));
    } else if (rule.combo_table_style == 2) {
        int a[] = {0, 0, 0, 1, 1, 1, 2, 2, 3, 3, 4, 4, 4, 5};
        AI::setComboList(std::vector<int>(a, a + sizeof(a) / sizeof(*a)));
    } else {
        int a[] = {0, 0, 0, 1, 1, 2};
        AI::setComboList(std::vector<int>(a, a + sizeof(a) / sizeof(*a)));
    }

    tetris.m_base = AI::point(0, 30);
    tetris.env_change = 1;
    tetris.n_pieces += 1;
    tetris.total_clears += tetris.m_clearLines;
    tetris.m_clearLines = 0;
    tetris.m_attack = 0;
}

void fixPosition(core::PieceType piece, core::RotateType rotate, Result &result) {
    result.piece = piece;
    result.rotate = rotate;

    switch (piece) {
        case core::PieceType::I: {
            switch (rotate) {
                case core::RotateType::Spawn:
                    result.x -= 2;
                    result.y += 2;
                    return;
                case core::RotateType::Right:
                    result.x -= 1;
                    result.y += 2;
                    return;
                case core::RotateType::Reverse:
                    result.x -= 1;
                    result.y += 1;
                    return;
                case core::RotateType::Left:
                    result.x -= 2;
                    result.y += 1;
                    return;
            }
            return;
        }
        case core::PieceType::T:
        case core::PieceType::L:
        case core::PieceType::J:
        case core::PieceType::Z:
        case core::PieceType::S: {
            switch (rotate) {
                case core::RotateType::Spawn:
                case core::RotateType::Right:
                case core::RotateType::Reverse:
                case core::RotateType::Left:
                    result.x -= 1;
                    result.y += 2;
                    return;
            }
            return;
        }
        case core::PieceType::O: {
            switch (rotate) {
                case core::RotateType::Spawn:
                    result.x -= 2;
                    result.y += 2;
                    return;
                case core::RotateType::Right:
                    result.x -= 2;
                    result.y += 3;
                    return;
                case core::RotateType::Reverse:
                    result.x -= 1;
                    result.y += 3;
                    return;
                case core::RotateType::Left:
                    result.x -= 1;
                    result.y += 2;
                    return;
            }
            return;
        }
    }

    assert(false);
}

core::PieceType toPiece(int num) {
    switch (num) {
        case AI::GemType::GEMTYPE_I:
            return core::PieceType::I;
        case AI::GemType::GEMTYPE_T:
            return core::PieceType::T;
        case AI::GemType::GEMTYPE_O:
            return core::PieceType::O;
        case AI::GemType::GEMTYPE_L:
            return core::PieceType::L;
        case AI::GemType::GEMTYPE_J:
            return core::PieceType::J;
        case AI::GemType::GEMTYPE_S:
            return core::PieceType::S;
        case AI::GemType::GEMTYPE_Z:
            return core::PieceType::Z;
    }
    assert(false);
    return static_cast<core::PieceType>(-1);
}

core::RotateType toRotate(int num) {
    switch (num) {
        case 0:
            return core::RotateType::Spawn;
        case 1:
            return core::RotateType::Right;
        case 2:
            return core::RotateType::Reverse;
        case 3:
            return core::RotateType::Left;
    }
    assert(false);
    return static_cast<core::RotateType>(-1);
}

void Bot::processMoves(Result &result) {
    tetris.m_state = AI::Tetris::STATE_MOVING;
    while (tetris.ai_movs_flag == -1 && !tetris.ai_movs.movs.empty()) {
        int mov = tetris.ai_movs.movs[0];
        tetris.ai_movs.movs.erase(tetris.ai_movs.movs.begin());
        if (0);
        else if (mov == AI::Moving::MOV_L) tetris.tryXMove(-1);
        else if (mov == AI::Moving::MOV_R) tetris.tryXMove(1);
        else if (mov == AI::Moving::MOV_D) tetris.tryYMove(1);
        else if (mov == AI::Moving::MOV_LSPIN) tetris.trySpin(1);
        else if (mov == AI::Moving::MOV_RSPIN) tetris.trySpin(3);
        else if (mov == AI::Moving::MOV_LL) { tetris.tryXXMove(-1); } //{ tetris.mov_llrr = AI::Moving::MOV_LL; }
        else if (mov == AI::Moving::MOV_RR) { tetris.tryXXMove(1); } //{ tetris.mov_llrr = AI::Moving::MOV_RR; }
        else if (mov == AI::Moving::MOV_DD) tetris.tryYYMove(1);
        else if (mov == AI::Moving::MOV_DROP) {
            while (tetris.tryYMove(1));

            result.x = 9 - tetris.m_cur_x;
            result.y = 18 - tetris.m_cur_y - 1;
            fixPosition(toPiece(tetris.m_cur.num), toRotate(tetris.m_cur.spin), result);

            if (tetris.m_hold) {
                result.holdGem = static_cast<AI::GemType>(tetris.m_pool.m_hold);
            }

            tetris.drop();
        } else if (mov == AI::Moving::MOV_HOLD) {
            tetris.tryHold();
        } else if (mov == AI::Moving::MOV_SPIN2) {
            if (AI::spin180Enable()) {
                tetris.trySpin180();
            }
        } else if (mov == AI::Moving::MOV_REFRESH) {
            tetris.env_change = 1;
        }
    }
    tetris.clearLines();

    result.clears = tetris.m_clear_info.clears;
    result.attack = tetris.m_clear_info.attack;
    result.b2b = tetris.m_clear_info.b2b;
    result.combo = tetris.m_clear_info.combo;
}

void Bot::outputAction(Result &result, std::stringstream &out) {
    std::vector<AI::Gem> next;
    for (int j = 0; j < 5; ++j) //NEXT size
        next.push_back(tetris.m_next[j]);
    int deep = AI_TRAINING_DEEP;
    bool canhold = tetris.hold;

    AI::RunAI(tetris.ai_movs, tetris.ai_movs_flag,
              tetris.m_ai_param, tetris.m_pool, tetris.m_hold,
              tetris.m_cur,
              tetris.m_cur_x, tetris.m_cur_y, next, canhold, m_upcomeAtt,
              deep, tetris.ai_last_deep, ai.level, 0);

    if (tetris.alive()) {
//        std::cout << "moves: " << tetris.ai_movs.movs.size() << std::endl;
        processMoves(result);
//        out << tetris.m_clearLines << "|" << ((int) tetris.wallkick_spin) << "|";
        tetris.m_state = AI::Tetris::STATE_READY;
    } else {
//        out << "-1|0|";
    }

    int i, bottom = AI_POOL_MAX_H - 5, solid_h = 20 - tetris.m_pool.m_h;
    int max = bottom - solid_h;
    for (i = AI::gem_add_y + 1; i < max; i++) {
        unsigned long mask = 512u; //(2^WIDTH-1)
        for (; mask != 0; mask = mask >> 1) {
            out << (((tetris.m_pool.m_row[i] & mask) == mask) ? 2 : 0);
            if (mask != 1) out << ',';
        }
        if (i != bottom - 1)out << ';';
    }
    for (; i < bottom; i++) {
        out << "3,3,3,3,3,3,3,3,3,3";
        if (i != bottom - 1)out << ';';
    }

}

void Bot::init() {
    changeSettings("style", "1");
    changeSettings("level", "6");

    setup();

    AI::AI_Param param = {
            13.0, 9.0, 17.0, 10.0, 29.0, 25.0, 39.0, 2.0, 12.0, 19.0, 7.0, 24.0, 21.0, 16.0, 14.0, 19.0, 200.0
    };
    tetris.m_ai_param = param;
}

void add7Pieces(
        std::array<AI::GemType, 7> &allTypes,
        std::deque<AI::GemType> &pieces,
        std::mt19937 &mt199371
) {
    std::shuffle(allTypes.begin(), allTypes.end(), mt199371);
    for (const auto &type : allTypes) {
        pieces.push_back(type);
    }
}

void Bot::run() {
    init();

    std::array<AI::GemType, 7> allTypes = {
            AI::GemType::GEMTYPE_I, AI::GemType::GEMTYPE_T, AI::GemType::GEMTYPE_O,
            AI::GemType::GEMTYPE_S, AI::GemType::GEMTYPE_Z, AI::GemType::GEMTYPE_J, AI::GemType::GEMTYPE_L
    };

    std::random_device randomDevice;
    std::mt19937 mt199371(randomDevice());
    std::uniform_int_distribution<> rand010(0, 10);
    std::uniform_int_distribution<> rand08(0, 8);

    auto factory = core::Factory::create();
    auto converter = fumen::ColorConverter::create();

    const int max = 100;
    auto prevAttacks = std::array<int, max>();
    std::fill(prevAttacks.begin(), prevAttacks.end(), 0);

    int lastUpX = 0;

    for (int count = 0; count < 5; ++count) {
        // 開始フィールド
        AI::GemType hold = AI::GemType::GEMTYPE_NULL;

        std::deque<AI::GemType> pieces;
        add7Pieces(allTypes, pieces, mt199371);
        add7Pieces(allTypes, pieces, mt199371);

        auto elements = std::vector<fumen::Element>{};
        auto colorField = fumen::ColorField(24);

        int totalAttack = 0;
        bool b2b = false;
        auto currentAttacks = std::array<int, max>();
        std::fill(currentAttacks.begin(), currentAttacks.end(), 0);

        for (int frame = 0; frame < max; ++frame) {
            if (pieces.size() < 14) {
                add7Pieces(allTypes, pieces, mt199371);
            }

            // round
            tetris.reset(0);

            m_upcomeAtt = prevAttacks[frame];

            // this_piece_type
            AI::GemType head = pieces.front();
            tetris.m_next[0] = AI::getGem(head, 0);
            pieces.pop_front();

            // next_pieces
            std::stringstream p;
            for (const auto &piece : pieces) {
                auto gem = AI::getGem(piece, 0);
                p << gem.getLetter();
            }
            updateQueue(p.str());

            // combo
            tetris.m_pool.m_hold = hold;
            tetris.m_pool.combo = 0;
            tetris.m_pool.b2b = b2b ? 1 : 0;

            // field
            updateField(colorField);

            if (tetris.m_pool.isCollide(3, 1, tetris.m_next[0])) {
                std::cout << "end" << frame << std::endl;
                break;
            }

            // action
            std::stringstream out = std::stringstream();
            Result result{};
            outputAction(result, out);

            if (result.holdGem) {
                if (hold == AI::GemType::GEMTYPE_NULL) {
                    pieces.pop_front();
                }
                hold = result.holdGem;
            }

            totalAttack += result.attack;

            b2b = 0 < result.b2b;

            currentAttacks[frame] = result.attack;

//        std::cout << "[" << i << "] total:" << std::endl;
//        std::cout << "  attack:" << totalAttack << std::endl;
//        std::cout << "  b2b:" << b2b << std::endl;
//        std::cout << "  clears:" << result.clears << std::endl;
//        std::cout << out.str() << std::endl;

            elements.emplace_back(fumen::Element{
                    converter.parseToColorType(result.piece), result.rotate, result.x, result.y,
                    true, colorField, b2b ? "b2b: o" : "b2b: x",
            });

            colorField.put(factory.get(result.piece, result.rotate), result.x, result.y);
            colorField.clearLine();

            auto attacks = std::vector<int>();
            for (int up = 0; up < m_upcomeAtt - result.attack; ++up) {
                if (rand010(mt199371) < (up == 0 ? 9 : 3)) {
                    // 穴がずれる
                    int nextUpX = rand08(mt199371);
                    lastUpX = lastUpX <= nextUpX ? nextUpX : nextUpX + 1;
                }
                attacks.push_back(lastUpX);

            }
            colorField.blockUp(attacks);
        }

        prevAttacks = currentAttacks;

        auto parser = fumen::Parser(factory, converter);
        std::cout << "https://knewjade.github.io/fumen-for-mobile/#?d=v115@" << parser.encode(elements) << std::endl;
    }
}