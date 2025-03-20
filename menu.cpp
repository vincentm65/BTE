#include "menu.h"

Menu::Menu(QWidget *parent) : QListWidget(parent)
{
    addItem("Data Management");
    addItem("Charting");
    addItem("Backtest Engine");

    connect(this, &QListWidget::currentRowChanged, this, &Menu::navigateToPage);
}
