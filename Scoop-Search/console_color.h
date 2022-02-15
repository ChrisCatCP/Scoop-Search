#pragma once

#include <iostream>
#include <windows.h>


namespace console
{
    enum class color
    {
        blue = 0x0001,
        green = 0x0002,
        red = 0x0004,
        intensity = 0x0008,
        white = red | green | blue,
        yellow = red | green,
        pink = red | blue,
        turquoise = blue | green
    };
    //---------------------------------------------------------------------------------        
    class ostream_helper
    {
    private:
        color n_;
        bool inten_;
        std::ostream& (*f_)(std::ostream&, color, bool);
    public:
        ostream_helper(std::ostream& (*f)(std::ostream&, color, bool), color n, bool inten) :f_(f), n_(n), inten_(inten) { }
        friend std::ostream& operator<<(std::ostream& os, ostream_helper helper)
        {
            return helper.f_(os, helper.n_, helper.inten_);
        }
    };

    //---------------------------------------------------------------------------------        
    std::ostream& ColHelper(std::ostream& os, color col, bool inten)
    {
        SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE),
            static_cast<WORD>(inten ? static_cast<int>(col)|static_cast<int>(color::intensity) 
                : static_cast<int>(col)));
        return os;
    }
    //--------------------------------------------------------------------------------        
    inline ostream_helper color(color col, bool intensiv)
    {
        return ostream_helper(&ColHelper, col, intensiv);
    }
}
