#ifndef MANAGE_DB_H
#define MANAGE_DB_H

#include <sqlite3.h>
#include <string>
#include <vector>
#include <QTextEdit>

void removeDuplicates(sqlite3* DB);
void createDatabase(sqlite3* DB);
void queryStock(sqlite3* DB, std::string ticker);
void queryStockForBT(sqlite3* DB, std::string ticker);
void updatePriceHistory(sqlite3* DB, QTextEdit* outputWidget);

#endif // MANAGE_DB_H
