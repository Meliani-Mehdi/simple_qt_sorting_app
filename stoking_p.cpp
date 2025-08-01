#include "stoking_p.h"
#include "./ui_stoking_p.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include <QMessageBox>
#include <QSqlTableModel>
#include <QSortFilterProxyModel>
#include <QMenu>
#include <QCompleter>
#include <QStandardItemModel>
#include <QKeyEvent>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QInputDialog>
#include <QTextEdit>
#include <QFileDialog>
#include <QPrinter>
#include <QPrintDialog>
#include <QDesktopServices>

void start_db();
void close_db();

stoking_p::stoking_p(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::stoking_p)
{
    ui->setupUi(this);

    start_db();
    setup_search_autocomplete();
    setup_cartTb();
    setupHistoryTable();

    ui->itemListTB->setContextMenuPolicy(Qt::CustomContextMenu);
    ui->cartListTB->setContextMenuPolicy(Qt::CustomContextMenu);
    setup_table();
    setup_form();

    setup_connects();


}
//=====================================================================================================================

void stoking_p::setup_cartTb(){

    QStandardItemModel* model = qobject_cast<QStandardItemModel*>(ui->cartListTB->model());
    if (!model) {
        model = new QStandardItemModel(this);
        ui->cartListTB->setModel(model);
        model->setHorizontalHeaderLabels({"Name", "Type", "Quantity", "Price", "Subtotal", "Expense", "Subexpense"});
        ui->cartListTB->setColumnHidden(5, true);
        ui->cartListTB->setColumnHidden(6, true);
    }

    ui->cartListTB->installEventFilter(this);
    ui->cartListTB->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->cartListTB->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->cartListTB->horizontalHeader()->setStretchLastSection(true);
    ui->cartListTB->verticalHeader()->setVisible(false);
}

void stoking_p::showContextMenuCartList(const QPoint &pos){
    QModelIndex index = ui->cartListTB->indexAt(pos);
    if (!index.isValid()) return;

    QMenu contextMenu(this);
    QAction *editAction = contextMenu.addAction("Set Quantity");
    QAction *deleteAction = contextMenu.addAction("Delete");

    QAction *selectedAction = contextMenu.exec(ui->cartListTB->viewport()->mapToGlobal(pos));

    QAbstractItemModel *model = ui->cartListTB->model();

    if (!selectedAction) return;

    if (selectedAction == deleteAction) {
        auto response = QMessageBox::question(this, "Delete Confirmation", "Are you sure you want to delete this item?");
        if (response == QMessageBox::Yes) {
            model->removeRow(index.row());
        }
    } else if (selectedAction == editAction) {
        int quantityColumn = 2;
        bool ok;
        int currentQty = model->data(model->index(index.row(), quantityColumn)).toInt();

        int newQty = QInputDialog::getInt(this,
                                          "Set Quantity",
                                          "Enter quantity (1 - 1000):",
                                          currentQty,
                                          1,
                                          1000,
                                          1,
                                          &ok);

        if (ok) {
            model->setData(model->index(index.row(), quantityColumn), newQty);
            float price = model->data(model->index(index.row(), 3)).toFloat();
            float expense = model->data(model->index(index.row(), 5)).toFloat();
            double total = newQty * price;
            double totalExpense = newQty * expense;
            QString totalStr = QString::number(total, 'f', 2);
            QString totalExpenseStr = QString::number(totalExpense, 'f', 2);
            model->setData(model->index(index.row(), 4), totalStr);
            model->setData(model->index(index.row(), 6), totalExpenseStr);
        }
    }
    update_transaction_summary();
}


void stoking_p::setup_search_autocomplete() {
    QSqlQuery query("SELECT name FROM products");
    QStringList nameList;

    while (query.next()) {
        nameList << query.value(0).toString();
    }

    QCompleter* completer = new QCompleter(nameList, this);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    completer->setFilterMode(Qt::MatchContains);
    ui->searchShop->setCompleter(completer);
}

bool stoking_p::eventFilter(QObject* obj, QEvent* event) {
    if (obj == ui->cartListTB && event->type() == QEvent::KeyPress) {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
        int row = ui->cartListTB->currentIndex().row();
        if (row < 0) return false;

        QStandardItemModel* model = qobject_cast<QStandardItemModel*>(ui->cartListTB->model());

        int qty = model->item(row, 2)->text().toInt();

        if (keyEvent->key() == Qt::Key_Plus) {
            qty++;
        }
        else if (keyEvent->key() == Qt::Key_Minus && qty > 1) {
            qty--;
        }
        else if (keyEvent->key() == Qt::Key_Delete) {
            auto response = QMessageBox::question(this, "Delete Confirmation", "Are you sure you want to delete this item?");
            if (response == QMessageBox::Yes) {
                model->removeRow(row);
                update_transaction_summary();
                return true;
            }
        }
        else {
            return false;
        }

        float price = model->data(model->index(row, 3)).toFloat();
        float expense = model->data(model->index(row, 5)).toFloat();
        double total = qty * price;
        double totalExpense = qty * expense;
        QString totalStr = QString::number(total, 'f', 2);
        QString totalExpenseStr = QString::number(totalExpense, 'f', 2);
        model->setData(model->index(row, 4), totalStr);
        model->setData(model->index(row, 6), totalExpenseStr);

        update_transaction_summary();

        return true;
    }
    return QMainWindow::eventFilter(obj, event);
}

void stoking_p::update_transaction_summary() {
    QStandardItemModel* model = qobject_cast<QStandardItemModel*>(ui->cartListTB->model());
    if (!model) return;

    float total = 0;
    for (int i = 0; i < model->rowCount(); ++i) {
        total += model->item(i, 4)->text().toFloat();
    }

    ui->summaryLabel->setText(QString("Total: %1").arg(QString::number(total, 'f', 2)));
}



//=====================================================================================================================

bool stoking_p::validateItems(
    QString item_name,
    QString item_type,
    QString item_price,
    QString item_bought,
    int item_count)
{
    if (item_name.trimmed().isEmpty() ||
        item_type.trimmed().isEmpty() ||
        item_price.trimmed().isEmpty() ||
        item_bought.trimmed().isEmpty()) {
        QMessageBox::warning(this, "Input Error", "All fields must be filled.");
        return false;
    }

    static const QRegularExpression nameTypeRe("^[A-Za-z0-9 ]{2,}$");
    static const QRegularExpression priceRe("^\\d+(\\.\\d{1,2})?$");

    if (!nameTypeRe.match(item_name).hasMatch()) {
        QMessageBox::warning(this, "Input Error", "Name must be at least 2 characters and contain only letters, numbers, or spaces.");
        return false;
    }

    if (!nameTypeRe.match(item_type).hasMatch()) {
        QMessageBox::warning(this, "Input Error", "Type must be at least 2 characters and contain only letters, numbers, or spaces.");
        return false;
    }

    if (!priceRe.match(item_price).hasMatch()) {
        QMessageBox::warning(this, "Input Error", "Price must be a real number.");
        return false;
    }

    if (!priceRe.match(item_bought).hasMatch()) {
        QMessageBox::warning(this, "Input Error", "Bought price must be a real number.");
        return false;
    }

    if (item_count > 1000 || item_count < 0) {
        QMessageBox::warning(this, "Input Error", "Quantity can't be below zero or exceed 1000.");
        return false;
    }

    return true;
}


void stoking_p::clear_form(){
    ui->itemName_edit->clear();
    ui->itemType_edit->clear();
    ui->itemPrice_edit->clear();
    ui->itemBuy_edit->clear();
    ui->itemCount_edit->setValue(0);
}

void stoking_p::setupHistoryTable() {
    // Create a model with custom data columns
    QStandardItemModel* model = new QStandardItemModel(ui->historyTable);
    model->setHorizontalHeaderLabels({"Name", "Time", "Total Sold", "Total Expenss", "Net Profit"});

    QSqlQuery query("SELECT name, total, total_expense, date, details FROM transactions ORDER BY date DESC");

    while (query.next()) {
        QString name = query.value(0).toString();
        double totalSold = query.value(1).toDouble();
        double totalExpense = query.value(2).toDouble();
        QString date = query.value(3).toString();
        QString detailsJson = query.value(4).toString();

        double profit = totalSold - totalExpense;

        QList<QStandardItem*> rowItems;
        rowItems << new QStandardItem(name)
                 << new QStandardItem(date)
                 << new QStandardItem(QString::number(totalSold, 'f', 2))
                 << new QStandardItem(QString::number(totalExpense, 'f', 2))
                 << new QStandardItem(QString::number(profit, 'f', 2));

        // Store details JSON as user data on the row for later
        rowItems[0]->setData(detailsJson, Qt::UserRole + 1);

        model->appendRow(rowItems);
    }

    ui->historyTable->setModel(model);
    ui->historyTable->resizeColumnsToContents();
    ui->historyTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->historyTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->historyTable->horizontalHeader()->setStretchLastSection(true);
}

void stoking_p::setup_form(){
    clear_form();
    ui->addTableItem_btn->setText("ADD ITEM");
    ui->IncertMode_lb->setText("ADD MODE");
    ui->IncertMode_lb->setStyleSheet(R"(
    QLabel {
        background-color: #0078d7;
        color: white;
        padding: 4px 10px;
        border-radius: 10px;
        font-weight: bold;
    }
    )");
    disconnect(ui->addTableItem_btn, nullptr, nullptr, nullptr);

    connect(ui->addTableItem_btn, &QPushButton::clicked, this, [this]() {
        ui->addTableItem_btn->setDisabled(true);

        QString item_name = ui->itemName_edit->text();
        QString item_type = ui->itemType_edit->text();
        QString item_price = ui->itemPrice_edit->text();
        QString item_bought = ui->itemBuy_edit->text();
        int item_count = ui->itemCount_edit->value();
        if(validateItems(item_name, item_type, item_price, item_bought, item_count)){
            clear_form();
            insert_item_db(
                item_name,
                item_type,
                item_price.toFloat(),
                item_bought.toFloat(),
                item_count);
        }
        setup_table();

        ui->addTableItem_btn->setDisabled(false);
    });
}

void stoking_p::setup_table() {

    QSqlTableModel *model = new QSqlTableModel(ui->itemListTB);
    model->setTable("products");
    model->select();

    model->setHeaderData(0, Qt::Horizontal, QObject::tr("ID"));
    model->setHeaderData(1, Qt::Horizontal, QObject::tr("Product Name"));
    model->setHeaderData(2, Qt::Horizontal, QObject::tr("Product Type"));
    model->setHeaderData(3, Qt::Horizontal, QObject::tr("Quantity"));
    model->setHeaderData(4, Qt::Horizontal, QObject::tr("Selling Price"));
    model->setHeaderData(5, Qt::Horizontal, QObject::tr("Bought Price"));

    model->setHeaderData(0, Qt::Horizontal, Qt::AlignLeft, Qt::TextAlignmentRole);
    model->setHeaderData(1, Qt::Horizontal, Qt::AlignLeft, Qt::TextAlignmentRole);
    model->setHeaderData(2, Qt::Horizontal, Qt::AlignLeft, Qt::TextAlignmentRole);
    model->setHeaderData(3, Qt::Horizontal, Qt::AlignLeft, Qt::TextAlignmentRole);
    model->setHeaderData(4, Qt::Horizontal, Qt::AlignLeft, Qt::TextAlignmentRole);
    model->setHeaderData(5, Qt::Horizontal, Qt::AlignLeft, Qt::TextAlignmentRole);


    QSortFilterProxyModel *proxyModel = new QSortFilterProxyModel(ui->itemListTB);
    proxyModel->setSourceModel(model);
    proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    proxyModel->setFilterKeyColumn(-1);

    ui->itemListTB->setModel(proxyModel);
    ui->itemListTB->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->itemListTB->resizeColumnsToContents();
    ui->itemListTB->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->itemListTB->horizontalHeader()->setStretchLastSection(true);

    disconnect(ui->searchItem, nullptr, nullptr, nullptr);
    connect(ui->searchItem, &QLineEdit::textChanged, this, [=](const QString &text) {
        proxyModel->setFilterFixedString(text);
    });
}

void stoking_p::showContextMenuItemList(const QPoint &pos) {
    QModelIndex index = ui->itemListTB->indexAt(pos);
    if (!index.isValid()) return;

    QMenu contextMenu(this);
    QAction *editAction = contextMenu.addAction("Edit");
    QAction *deleteAction = contextMenu.addAction("Delete");

    QAction *selectedAction = contextMenu.exec(ui->itemListTB->viewport()->mapToGlobal(pos));

    if (!selectedAction) return;

    QSortFilterProxyModel *proxyModel = static_cast<QSortFilterProxyModel*>(ui->itemListTB->model());
    QSqlTableModel *model = static_cast<QSqlTableModel*>(proxyModel->sourceModel());
    QModelIndex sourceIndex = proxyModel->mapToSource(index);

    if (selectedAction == editAction) {
        auto response = QMessageBox::question(this, "Edit Confirmation", "Are you sure you want to Edit this item?");
        if (response == QMessageBox::No) {
            return;
        }
        clear_form();
        int id = model->data(model->index(sourceIndex.row(), 0)).toInt();
        ui->addTableItem_btn->setText("EDIT ITEM");
        ui->IncertMode_lb->setText("EDIT MODE");
        ui->IncertMode_lb->setStyleSheet(R"(
            QLabel {
                background-color: #ff8800;
                color: white;
                padding: 4px 10px;
                border-radius: 10px;
                font-weight: bold;
            }
            )");

        disconnect(ui->addTableItem_btn, nullptr, nullptr, nullptr);
        connect(ui->addTableItem_btn, &QPushButton::clicked, this, [this, id]() {
            ui->addTableItem_btn->setDisabled(true);

            QString item_name = ui->itemName_edit->text();
            QString item_type = ui->itemType_edit->text();
            QString item_price = ui->itemPrice_edit->text();
            QString item_bought = ui->itemBuy_edit->text();
            int item_count = ui->itemCount_edit->value();
            if(validateItems(item_name, item_type, item_price, item_bought, item_count)){
                setup_form();
                update_item_db(
                    id,
                    item_name,
                    item_type,
                    item_price.toFloat(),
                    item_bought.toFloat(),
                    item_count);
            }
            setup_table();

            ui->addTableItem_btn->setDisabled(false);
        });

        QString name = model->data(model->index(sourceIndex.row(), 1)).toString();
        QString type = model->data(model->index(sourceIndex.row(), 2)).toString();
        int quantity = model->data(model->index(sourceIndex.row(), 3)).toInt();
        float price = model->data(model->index(sourceIndex.row(), 4)).toFloat();
        float bought = model->data(model->index(sourceIndex.row(), 5)).toFloat();

        ui->itemName_edit->setText(name);
        ui->itemType_edit->setText(type);
        ui->itemCount_edit->setValue(quantity);
        ui->itemPrice_edit->setText(QString::number(price));
        ui->itemBuy_edit->setText(QString::number(bought));
    }

    if (selectedAction == deleteAction) {
        auto response = QMessageBox::question(this, "Delete Confirmation", "Are you sure you want to delete this item?");
        if (response == QMessageBox::Yes) {
            model->removeRow(sourceIndex.row());
            model->submitAll();
            setup_table();
        }
    }
}


void stoking_p::insert_item_db(QString name, QString type, float price, float bought, int count){
    QSqlQuery insertQuery;
    insertQuery.prepare("INSERT INTO products (name, item_type, quantity, price, bought) VALUES (?, ?, ?, ?, ?)");
    insertQuery.addBindValue(name);
    insertQuery.addBindValue(type);
    insertQuery.addBindValue(count);
    insertQuery.addBindValue(price);
    insertQuery.addBindValue(bought);

    if (!insertQuery.exec()) {
        qDebug() << "Insert failed:" << insertQuery.lastError();
        QMessageBox::warning(this, "Input Error", "product name must be unique.");
    } else {
        qDebug() << "Insert successful!";
    }
    setup_search_autocomplete();
}

void stoking_p::update_item_db(int id, QString name, QString type, float price, float bought, int count) {
    QSqlQuery query;
    query.prepare("UPDATE products SET name = ?, item_type = ?, quantity = ?, price = ?, bought = ? WHERE id = ?");
    query.addBindValue(name);
    query.addBindValue(type);
    query.addBindValue(count);
    query.addBindValue(price);
    query.addBindValue(bought);
    query.addBindValue(id);

    if (!query.exec()) {
        qDebug() << "Update failed:" << query.lastError();
        QMessageBox::warning(this, "Update Error", "Could not update item. Make sure name is unique.");
    } else {
        qDebug() << "Update successful!";
    }
    setup_search_autocomplete();
}

//=====================================================================================================================

void stoking_p::setup_connects(){
    connect(ui->addNewItem, &QPushButton::clicked, this, [this]() {
        setup_form();
    });

    // a simple window switcher
    enum PageIndex {
        CartPage = 0,
        ItemPage = 1,
        HistoryPage = 2
    };

    connect(ui->ShoppingCart, &QPushButton::clicked, this, [this]() {
        ui->windowHolder->setCurrentIndex(CartPage);
    });

    connect(ui->StoreItems, &QPushButton::clicked, this, [this]() {
        ui->windowHolder->setCurrentIndex(ItemPage);
    });

    connect(ui->ProductHistory, &QPushButton::clicked, this, [this]() {
        ui->windowHolder->setCurrentIndex(HistoryPage);
    });

    // TODAY
    connect(ui->income_today, &QPushButton::clicked, this, [=]() {
        getFinancialSummaryAndShow("DATE(date) = DATE('now')");
    });

    // THIS MONTH
    connect(ui->income_month, &QPushButton::clicked, this, [=]() {
        getFinancialSummaryAndShow("DATE(date) >= DATE('now', '-1 month')");
    });

    // PAST 3 MONTHS
    connect(ui->income_3months, &QPushButton::clicked, this, [=]() {
        getFinancialSummaryAndShow("DATE(date) >= DATE('now', '-3 months')");
    });

    // THIS YEAR
    connect(ui->income_year, &QPushButton::clicked, this, [=]() {
        getFinancialSummaryAndShow("DATE(date) >= DATE('now', '-1 year')");
    });


    // this connect is to add a small context menu that has the functions Edit and Delete to the product table
    connect(ui->itemListTB, &QTableView::customContextMenuRequested, this, &stoking_p::showContextMenuItemList);

    connect(ui->cartListTB, &QTableView::customContextMenuRequested, this, &stoking_p::showContextMenuCartList);

    // this code gets the data based on the clicked row, and shows a table of bought items
    connect(ui->historyTable, &QTableView::clicked, this, [this](const QModelIndex& index) {
        int row = index.row();
        QStandardItemModel* m = qobject_cast<QStandardItemModel*>(ui->historyTable->model());
        QString detailsJson = m->item(row, 0)->data(Qt::UserRole + 1).toString();
        QString transactionName = m->item(row, 1)->text(); // Assuming column 1 has the transaction name

        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(detailsJson.toUtf8(), &error);
        if (error.error != QJsonParseError::NoError || !doc.isArray()) {
            QMessageBox::warning(this, "Error", "Invalid transaction details format.");
            return;
        }

        QJsonArray items = doc.array();

        // Create dialog
        auto* dialog = new QDialog(this);
        dialog->setWindowTitle("Transaction Items");

        auto* layout = new QVBoxLayout(dialog);
        auto* table = new QTableView(dialog);
        auto* detailModel = new QStandardItemModel(items.size(), 7, dialog);
        detailModel->setHorizontalHeaderLabels(
            {"Item", "Quantity", "Sell Price", "Total Sold", "Cost Price", "Total Expense", "Profit"});

        for (int i = 0; i < items.size(); ++i) {
            QJsonObject item = items[i].toObject();
            QString itemName = item["name"].toString();
            int quantity = item["quantity"].toInt();
            double price = item["price"].toDouble();
            double cost = item["cost"].toDouble();
            double totalSold = item["subtotal"].toDouble();
            double totalExpense = item["subexpense"].toDouble();
            double profit = totalSold - totalExpense;

            detailModel->setItem(i, 0, new QStandardItem(itemName));
            detailModel->setItem(i, 1, new QStandardItem(QString::number(quantity)));
            detailModel->setItem(i, 2, new QStandardItem(QString::number(price, 'f', 2)));
            detailModel->setItem(i, 3, new QStandardItem(QString::number(totalSold, 'f', 2)));
            detailModel->setItem(i, 4, new QStandardItem(QString::number(cost, 'f', 2)));
            detailModel->setItem(i, 5, new QStandardItem(QString::number(totalExpense, 'f', 2)));
            detailModel->setItem(i, 6, new QStandardItem(QString::number(profit, 'f', 2)));
        }

        table->setModel(detailModel);
        table->resizeColumnsToContents();
        layout->addWidget(table);

        // Add button to print invoice
        QPushButton* printButton = new QPushButton("Print Invoice", dialog);
        layout->addWidget(printButton);

        // Connect button to print functionality
        connect(printButton, &QPushButton::clicked, this, [this, items, transactionName]() {
            QString invoiceHtml = "<h2>Invoice - " + transactionName + "</h2>";
            invoiceHtml += "<table border='1' cellpadding='4' cellspacing='0' width='100%'>";
            invoiceHtml += "<tr><th>Item</th><th>Quantity</th><th>Price</th><th>Total</th></tr>";

            double grandTotal = 0.0;

            for (const QJsonValue& val : items) {
                QJsonObject item = val.toObject();
                QString name = item["name"].toString();
                int qty = item["quantity"].toInt();
                double price = item["price"].toDouble();
                double total = item["subtotal"].toDouble();
                grandTotal += total;

                invoiceHtml += QString("<tr><td>%1</td><td>%2</td><td>%3</td><td>%4</td></tr>")
                                   .arg(name)
                                   .arg(qty)
                                   .arg(QString::number(price, 'f', 2))
                                   .arg(QString::number(total, 'f', 2));
            }

            invoiceHtml += QString("<tr><td colspan='3'><b>Grand Total</b></td><td><b>%1</b></td></tr>")
                               .arg(QString::number(grandTotal, 'f', 2));
            invoiceHtml += "</table>";

            // Ask user where to save the PDF
            QString filePath = QFileDialog::getSaveFileName(nullptr, "Save Invoice as PDF", "", "PDF Files (*.pdf)");
            if (filePath.isEmpty()) {
                return; // User canceled
            }

            if (!filePath.endsWith(".pdf", Qt::CaseInsensitive)) {
                filePath += ".pdf";
            }

            // Generate PDF using QTextDocument
            QTextDocument doc;
            doc.setHtml(invoiceHtml);

            QPrinter printer(QPrinter::HighResolution);
            printer.setOutputFormat(QPrinter::PdfFormat);
            printer.setOutputFileName(filePath);

            doc.print(&printer);

            QMessageBox::information(nullptr, "Invoice Saved", "Invoice PDF has been saved to:\n" + filePath);

            // Optional: Open the PDF after saving (platform-dependent)
            QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));
        });


        dialog->setLayout(layout);
        dialog->resize(600, 400);
        dialog->exec();
    });


    // clears the cart
    connect(ui->clearCart, &QPushButton::clicked, this, [this](){
        auto response = QMessageBox::question(this, "Clear Confirmation", "Are you sure you want to clear the table?");
        if (response == QMessageBox::Yes) {
            ui->cartListTB->model()->removeRows(0, ui->cartListTB->model()->rowCount());
        }
        update_transaction_summary();
    });

    // adds a new transaction and clears the cart
    connect(ui->add_transaction, &QPushButton::clicked, this, [this]() {
        QStandardItemModel* model = qobject_cast<QStandardItemModel*>(ui->cartListTB->model());
        if (!model || model->rowCount() == 0) {
            QMessageBox::warning(this, "Empty Cart", "Please add items to the cart first.");
            return;
        }

        float totalsold = 0;
        float totalcost = 0;
        QJsonArray items;

        QSqlDatabase db = QSqlDatabase::database();
        QSqlQuery query(db);
        db.transaction();

        for (int i = 0; i < model->rowCount(); ++i) {
            QString name = model->item(i, 0)->text();
            int qtyPurchased = model->item(i, 2)->text().toInt();
            float price = model->item(i, 3)->text().toFloat();
            float subtotal = model->item(i, 4)->text().toFloat();
            float cost = model->item(i, 5)->text().toFloat();
            float subexpense = model->item(i, 6)->text().toFloat();
            totalsold += subtotal;
            totalcost += subexpense;

            QJsonObject itemObj;
            itemObj["name"] = name;
            itemObj["quantity"] = qtyPurchased;
            itemObj["price"] = price;
            itemObj["cost"] = cost;
            itemObj["subtotal"] = subtotal;
            itemObj["subexpense"] = subexpense;
            items.append(itemObj);

            // Step 1: Get current stock
            query.prepare("SELECT quantity FROM products WHERE name = ?");
            query.addBindValue(name);
            if (!query.exec() || !query.next()) {
                QMessageBox::critical(this, "Error", "Failed to fetch product quantity.");
                db.rollback();
                return;
            }

            int currentQty = query.value(0).toInt();
            if (currentQty < qtyPurchased) {
                QMessageBox::critical(this, "Stock Error", QString("%1 has only %2 left.").arg(name).arg(currentQty));
                db.rollback();
                return;
            }

            // Step 2: Update stock
            int newQty = currentQty - qtyPurchased;
            query.prepare("UPDATE products SET quantity = ? WHERE name = ?");
            query.addBindValue(newQty);
            query.addBindValue(name);
            if (!query.exec()) {
                QMessageBox::critical(this, "Error", "Failed to update product stock.");
                db.rollback();
                return;
            }
        }

        // Step 3: Insert into transactions
        QString transName = ui->transactionNameLineEdit->text();
        QJsonDocument doc(items);
        QString details = QString::fromUtf8(doc.toJson(QJsonDocument::Compact));

        query.prepare("INSERT INTO transactions (name, details, total, total_expense) VALUES (?, ?, ?, ?)");
        query.addBindValue(transName);
        query.addBindValue(details);
        query.addBindValue(totalsold);
        query.addBindValue(totalcost);

        if (!query.exec()) {
            QMessageBox::critical(this, "Error", "Failed to save transaction.");
            db.rollback();
            return;
        }

        db.commit();

        QMessageBox::information(this, "Success", "Transaction saved and stock updated!");

        model->removeRows(0, model->rowCount());
        update_transaction_summary();
        ui->transactionNameLineEdit->clear();
        setup_table();
        setupHistoryTable();
    });

    // adds a new item to the cart
    auto triggerAddCartItem = [this]() {
        auto completer = ui->searchShop->completer();
        if (completer && completer->popup()->isVisible()) return;

        QString itemName = ui->searchShop->text();
        if (itemName.isEmpty()) return;


        QSqlQuery query;
        query.prepare("SELECT * FROM products WHERE name = ?");
        query.addBindValue(itemName);

        if (query.exec() && query.next()) {
            QString name = query.value("name").toString();
            QString type = query.value("item_type").toString();
            int quantity = 1;
            float price = query.value("price").toFloat();
            float cost = query.value("bought").toFloat();

            QStandardItemModel* model = qobject_cast<QStandardItemModel*>(ui->cartListTB->model());
            if (!model) {
                model = new QStandardItemModel(this);
                ui->cartListTB->setModel(model);
                model->setHorizontalHeaderLabels({"Name", "Type", "Quantity", "Price", "Subtotal", "Expense", "Subexpense"});
                ui->cartListTB->setColumnHidden(5, true);
                ui->cartListTB->setColumnHidden(6, true);
            }

            QList<QStandardItem*> row;
            row << new QStandardItem(name)
                << new QStandardItem(type)
                << new QStandardItem(QString::number(quantity))
                << new QStandardItem(QString::number(price))
                << new QStandardItem(QString::number(quantity * price))
                << new QStandardItem(QString::number(cost))
                << new QStandardItem(QString::number(quantity * cost));
            model->appendRow(row);

            update_transaction_summary();
            ui->searchShop->clear();
        }
    };

    connect(ui->addCartItem, &QPushButton::clicked, this, triggerAddCartItem);
    connect(ui->searchShop, &QLineEdit::returnPressed, this, triggerAddCartItem);
}

void start_db(){
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName("store.db");

    if (!db.open()) {
        qDebug() << "Error: connection with database failed -" << db.lastError();
    } else {
        qDebug() << "Database: connection ok";

        QSqlQuery query;
        QString createTable = R"(
        CREATE TABLE IF NOT EXISTS products (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT UNIQUE NOT NULL,
            item_type TEXT NOT NULL,
            quantity INTEGER NOT NULL,
            price REAL NOT NULL,
            bought REAL NOT NULL
        )
    )";

        if (!query.exec(createTable)) {
            qDebug() << "Error creating table:" << query.lastError();
        } else {
            qDebug() << "Table created or already exists.";
        }

        QString createTransactions = R"(
        CREATE TABLE IF NOT EXISTS transactions (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT,
            details TEXT, -- JSON or CSV of items
            total REAL,
            total_expense,
            date TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        )
    )";

        if (!query.exec(createTransactions)) {
            qDebug() << "Error creating table:" << query.lastError();
        } else {
            qDebug() << "Table created or already exists.";
        }

    }
}

void stoking_p::showFinancialSummaryWindow(double revenue, double expenses, double netProfit) {
    QDialog* dialog = new QDialog;
    dialog->setWindowTitle("Financial Summary");

    QVBoxLayout* layout = new QVBoxLayout(dialog);

    QLabel* revenueLabel = new QLabel(QString("Total Revenue: %1 DZD").arg(revenue, 0, 'f', 2));
    QLabel* expensesLabel = new QLabel(QString("Total Expenses: %1 DZD").arg(expenses, 0, 'f', 2));
    QLabel* profitLabel = new QLabel(QString("Net Profit: %1 DZD").arg(netProfit, 0, 'f', 2));

    QFont font;
    font.setPointSize(12);
    font.setBold(true);

    revenueLabel->setFont(font);
    expensesLabel->setFont(font);
    profitLabel->setFont(font);

    layout->addWidget(revenueLabel);
    layout->addWidget(expensesLabel);
    layout->addWidget(profitLabel);

    dialog->setLayout(layout);
    dialog->exec();
}

void stoking_p::getFinancialSummaryAndShow(const QString& periodCondition) {
    QSqlQuery query;
    QString queryString = "SELECT details FROM transactions";

    if (!periodCondition.isEmpty()) {
        queryString += " WHERE " + periodCondition;
    }

    if (!query.exec(queryString)) {
        qDebug() << "Database query failed:" << query.lastError();
        return;
    }

    double totalRevenue = 0.0;
    double totalExpenses = 0.0;

    while (query.next()) {
        QString detailsJson = query.value(0).toString();
        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(detailsJson.toUtf8(), &error);

        if (error.error != QJsonParseError::NoError || !doc.isArray()) {
            qDebug() << "Invalid JSON in transaction details.";
            continue;
        }

        QJsonArray items = doc.array();

        for (const QJsonValue& val : items) {
            QJsonObject item = val.toObject();
            double subtotal = item["subtotal"].toDouble();
            double subexpense = item["subexpense"].toDouble();

            totalRevenue += subtotal;
            totalExpenses += subexpense;
        }
    }

    double netProfit = totalRevenue - totalExpenses;

    // Show the popup window
    showFinancialSummaryWindow(totalRevenue, totalExpenses, netProfit);
}


void close_db() {
    QSqlDatabase db = QSqlDatabase::database();
    if (db.isOpen()) {
        db.close();
        QSqlDatabase::removeDatabase(QSqlDatabase::defaultConnection);
        qDebug() << "Database connection closed.";
    }
}


stoking_p::~stoking_p()
{
    close_db();
    delete ui;
}
