#ifndef STOKING_P_H
#define STOKING_P_H

#include <QMainWindow>

QT_BEGIN_NAMESPACE
namespace Ui {
class stoking_p;
}
QT_END_NAMESPACE

class stoking_p : public QMainWindow
{
    Q_OBJECT

public:
    stoking_p(QWidget *parent = nullptr);
    ~stoking_p();

private:
    Ui::stoking_p *ui;

    void setup_search_autocomplete();
    bool eventFilter(QObject* obj, QEvent* event);
    void update_transaction_summary();
    void clear_cart();
    void setup_cartTb();
    void showContextMenuCartList(const QPoint &pos);


//==============================================================
    bool validateItems(
        QString item_name,
        QString item_type,
        QString item_price,
        QString item_bought,
        int item_count);

    void insert_item_db(
        QString name,
        QString type,
        float price,
        float bought,
        int count);

    void update_item_db(
        int id,
        QString name,
        QString type,
        float price,
        float bought,
        int count);

    void setup_form();
    void setup_table();
    void showContextMenuItemList(const QPoint &pos);

    void clear_form();

//==============================================================
    void setupHistoryTable();
//==============================================================

    void setup_connects();

};
#endif // STOKING_P_H
