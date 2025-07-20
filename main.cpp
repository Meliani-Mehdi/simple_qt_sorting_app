#include "stoking_p.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    stoking_p w;
    w.show();
    return a.exec();
}
