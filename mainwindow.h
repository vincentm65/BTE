#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLineEdit>
#include <QPushButton>
#include <QTextEdit>
#include <sqlite3.h>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void createDatabase_clicked();
    void removeDuplicates_clicked();
    void queryStock_clicked();
    void authenticate_clicked();
    void fetchStock_clicked();
    void updateAll_clicked();
    void tickerInput();
    void dataOutput();


private:
    Ui::MainWindow *ui;
    sqlite3* DB;

};
#endif // MAINWINDOW_H
