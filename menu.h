#ifndef MENU_H
#define MENU_H

#include <QWidget>
#include <QListWidget>

class Menu : public QListWidget
{
    Q_OBJECT
public:
    explicit Menu(QWidget *parent = nullptr);

signals:
    void navigateToPage(int index);
};

#endif // MENU_H
