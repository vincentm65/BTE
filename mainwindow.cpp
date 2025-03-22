#include "mainwindow.h"
#include "authenticate.h"
#include "fetch_data.h"
#include "manage_db.h"
#include "./ui_mainwindow.h"
#include "menu.h"


#include <QMessageBox>
#include <QDesktopServices>
#include <QUrl>
#include <QInputDialog>
#include <QDir>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <sstream>
#include <iostream>



MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , DB(nullptr)
    {

    QStackedWidget *stackedWidget = new QStackedWidget;

    QWidget *page1 = new QWidget;
    QWidget *page2 = new QWidget;

    stackedWidget->addWidget(page1);
    stackedWidget->addWidget(page2);


    Menu *menu = new Menu(this);
    connect(menu, &Menu::navigateToPage, stackedWidget, &QStackedWidget::setCurrentIndex);

    QWidget *centralWidget = new QWidget;
    QHBoxLayout *layout = new QHBoxLayout(centralWidget);
    layout->addWidget(menu);
    layout->addWidget(stackedWidget);

    centralWidget->setLayout(layout);
    setCentralWidget(centralWidget);

    ui->setupUi(this);

    // Open SQLite Database
    int exit = sqlite3_open("Universe_OHLCV.db", &DB);
    if (exit) {
        ui->dataOutput->setText("Error opening database: " + QString::fromStdString(sqlite3_errmsg(DB)));
    } else {
        ui->dataOutput->setText("Database opened successfully!");
    }


    connect(ui->createDatabase, &QPushButton::clicked, this, &MainWindow::createDatabase_clicked);
    connect(ui->removeDuplicates, &QPushButton::clicked, this, &MainWindow::removeDuplicates_clicked);
    connect(ui->queryStock, &QPushButton::clicked, this, &MainWindow::queryStock_clicked);
    connect(ui->authenticate, &QPushButton::clicked, this, &MainWindow::authenticate_clicked);
    connect(ui->fetchStock, &QPushButton::clicked, this, &MainWindow::fetchStock_clicked);
    connect(ui->updateAll, &QPushButton::clicked, this, &MainWindow::updateAll_clicked);


}

MainWindow::~MainWindow()
{
    if (DB) {
        sqlite3_close(DB);
    }

    delete ui;
}

void MainWindow::createDatabase_clicked()
{
    createDatabase(DB);
}

void MainWindow::removeDuplicates_clicked()
{
    removeDuplicates(DB);
}

void MainWindow::queryStock_clicked()
{
    QString ticker = ui->tickerInput->text().trimmed();

    if (ticker.isEmpty()) {
        ui->dataOutput->setText("Please enter a ticker symbol.");
        return;
    }

    std::string tickerStr = ticker.toStdString();
    std::ostringstream outputStream;
    std::streambuf* oldCout = std::cout.rdbuf(outputStream.rdbuf());

    queryStock(DB, ticker.toStdString());

    std::cout.rdbuf(oldCout);
    ui->dataOutput->setPlainText(QString::fromStdString(outputStream.str()));  // Display result


}

void MainWindow::authenticate_clicked()
{
    std::cout << "Current working directory: " << QDir::currentPath().toStdString() << std::endl;

    // Build the authentication URL (you can adapt this as needed)
    std::string clientId = "IlG3Rwq8RWkpmoHzPDrcE7QA1a5VxKWH";
    std::string redirectUri = "https://127.0.0.1";
    std::string authUrlStd = "https://api.schwabapi.com/v1/oauth/authorize?client_id=" + clientId +
                             "&redirect_uri=" + redirectUri;
    QString authUrl = QString::fromStdString(authUrlStd);

    // Open the URL in the user's default browser
    QDesktopServices::openUrl(QUrl(authUrl));

    // Prompt the user to paste the redirect URL received after login
    bool ok = false;
    QString returnedUrl = QInputDialog::getText(this,
                                                tr("Authentication"),
                                                tr("Paste the full redirect URL after login:"),
                                                QLineEdit::Normal, QString(), &ok);

    if (ok && !returnedUrl.isEmpty()) {
        // Extract the authorization code from the returned URL
        int codePos = returnedUrl.indexOf("code=");
        if (codePos == -1) {
            QMessageBox::warning(this, tr("Authentication"),
                                 tr("Authorization code not found in URL."));
            return;
        }
        // Assuming the code ends at an '&' or the end of the string:
        int endPos = returnedUrl.indexOf('&', codePos);
        QString authCode = (endPos == -1) ?
                               returnedUrl.mid(codePos + 5) :
                               returnedUrl.mid(codePos + 5, endPos - (codePos + 5));

        // Optionally, decode the auth code if needed, then exchange it for a token
        exchangeAuthCodeForToken(authCode.toStdString());

        QMessageBox::information(this, tr("Authentication"),
                                 tr("Authentication successful. Check console/logs for details."));
    } else {
        QMessageBox::information(this, tr("Authentication"), tr("Authentication cancelled."));
    }
}

void MainWindow::fetchStock_clicked()
{
    QString ticker = ui->tickerInput->text().trimmed();  // Get input and remove spaces
    if (ticker.isEmpty()) {
        ui->dataOutput->setText("Please enter a ticker symbol.");
        return;
    }

    std::string tickerStr = ticker.toStdString();
    std::transform(tickerStr.begin(), tickerStr.end(), tickerStr.begin(), ::toupper);
    std::ostringstream outputStream;
    std::streambuf* oldCout = std::cout.rdbuf(outputStream.rdbuf());

    handleFetchStockData(DB, tickerStr);  // Pass the ticker symbol

    std::cout.rdbuf(oldCout);
    ui->dataOutput->setPlainText(QString::fromStdString(outputStream.str()));  // Display result
}

void MainWindow::tickerInput()
{
    // Example: Retrieve text from input field and print it
    QString ticker = ui->tickerInput->text();
    if (ticker.isEmpty()) {
        ui->dataOutput->setText("Please enter a ticker symbol.");
        return;
    }
    ui->dataOutput->setText("Ticker input: " + ticker);
}

void MainWindow::dataOutput()
{
    // Example: Display some text in the output field
    ui->dataOutput->setPlainText("Data output function called!");
}



void MainWindow::updateAll_clicked()
{
    updatePriceHistory(DB, ui->dataOutput);
}
