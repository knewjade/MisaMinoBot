#ifndef BOT_H
#define	BOT_H

#include <iostream>
#include <map>
#include <string>
#include <sstream>

#include "../fumen/color_field.hpp"
#include "../core/types.hpp"
#include "tetrisgame.h"

struct Result {
    AI::GemType holdGem;
    int x;
    int y;
    core::PieceType piece;
    core::RotateType rotate;
    int clears;
    int attack;
    int b2b;
    int combo;
};

class Bot {
public:
    Bot();
    Bot(const Bot& orig);
    virtual ~Bot();

    void startParser();
    void setup();
    void outputAction(Result &result, std::stringstream &out);

    void run();
    double run(std::array<double, 17> params);
    void init();

    struct tetris_ai {
        int style;
        int level;
        tetris_ai() {
            style = 2;
            level = 4;
        }
    };

    struct tetris_rule {
        int spin180;
        int GarbageCancel;
        int GarbageBuffer;
        int GarbageBlocking;
        int combo_table_style;
        tetris_rule() {
            spin180 = 1;
            GarbageCancel = 1;
            GarbageBuffer = 1;
            GarbageBlocking = 1;
            combo_table_style = 2; //1=TOJ, 2=TF
        }
    };

private:
    void updateState(const std::string & p1,const std::string & p2,const std::string & p3);
    void updateQueue(const std::string & s);
    void updateField(const std::string & s);
    void updateField(const fumen::ColorField &colorField);
    void changeSettings(const std::string & p1,const std::string & p2);

    void processMoves(Result &result);

    char m_hold;
    char m_queue[8];
    int *m_field;
    int m_queueLen;
    int m_upcomeAtt;
    std::map<char, int> m_gemMap;

    TetrisGame tetris;
    tetris_rule rule;
    tetris_ai ai;
};

#endif	/* BOT_H */