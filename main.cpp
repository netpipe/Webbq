#include <QApplication>
#include <QMainWindow>
#include <QWebEngineView>
#include <QWebEnginePage>
#include <QWebEngineHistory>
#include <QWebEngineSettings>
#include <QWebEngineProfile>
#include <QWebEngineCookieStore>
#include <QNetworkCookie>
#include <QToolBar>
#include <QLineEdit>
#include <QAction>
#include <QSettings>
#include <QMessageBox>
#include <QFileDialog>
#include <QPageLayout>
#include <QVBoxLayout>
#include <QDialog>
#include <QListWidget>
#include <QTableWidget>
#include <QHeaderView>
#include <QDialogButtonBox>
#include <QUrl>
#include <QVariantList>
#include <qpushbutton.h>
#include <qlabel>

class BrowserWindow : public QMainWindow {
    Q_OBJECT

public:
    BrowserWindow() {
        setWindowTitle("Qt 5.12 Mini Browser (Cookie Manager)");
        resize(1024, 768);

        // 1. Setup WebEngine View
        view = new QWebEngineView(this);
        view->settings()->setAttribute(QWebEngineSettings::JavascriptEnabled, true);
        view->settings()->setAttribute(QWebEngineSettings::LocalStorageEnabled, true);
        setCentralWidget(view);

        // 2. Setup Cookie Store Interception
        QWebEngineCookieStore *cookieStore = QWebEngineProfile::defaultProfile()->cookieStore();
        connect(cookieStore, &QWebEngineCookieStore::cookieAdded, this, [this](const QNetworkCookie &cookie) {
            // Keep a local list of unique cookies for our manager UI
            // (In a real app, you'd rely entirely on the profile's persistent store)
            QString key = cookie.name() + "|" + cookie.domain();
            if (!cookieMap.contains(key)) {
                cookieMap[key] = cookie;
            }
        });

        // Load previously saved cookies on startup
        loadSavedCookies();

        // 3. Setup Toolbar & Actions
        QToolBar *toolbar = addToolBar("Navigation");

        backAction = toolbar->addAction("◀ Back");
        forwardAction = toolbar->addAction("Forward ▶");
        reloadAction = toolbar->addAction("⟳ Reload");
        stopAction = toolbar->addAction("✖ Stop");

        toolbar->addSeparator();

        urlLineEdit = new QLineEdit();
        urlLineEdit->setPlaceholderText("Enter URL and press Enter...");
        toolbar->addWidget(urlLineEdit);

        toolbar->addSeparator();

        cookieManagerAction = toolbar->addAction("🍪 Cookie Manager");
        printAction = toolbar->addAction("🖨 Print to PDF");
        historyAction = toolbar->addAction("📜 History");

        // 4. Connect Signals & Slots
        connect(backAction, &QAction::triggered, view, &QWebEngineView::back);
        connect(forwardAction, &QAction::triggered, view, &QWebEngineView::forward);
        connect(reloadAction, &QAction::triggered, view, &QWebEngineView::reload);
        connect(stopAction, &QAction::triggered, view, &QWebEngineView::stop);

        connect(urlLineEdit, &QLineEdit::returnPressed, this, [this]() {
            QUrl url = QUrl::fromUserInput(urlLineEdit->text());
            view->setUrl(url);
        });

        connect(view, &QWebEngineView::urlChanged, this, [this](const QUrl &url) {
            urlLineEdit->setText(url.toString());
        });

        connect(view, &QWebEngineView::loadStarted, this, [this]() {
            stopAction->setEnabled(true);
            reloadAction->setEnabled(false);
        });

        connect(view, &QWebEngineView::loadFinished, this, [this](bool /*ok*/) {
            stopAction->setEnabled(false);
            reloadAction->setEnabled(true);
            backAction->setEnabled(view->history()->canGoBack());
            forwardAction->setEnabled(view->history()->canGoForward());

            // Record history
            QUrl url = view->url();
            if (url.isValid() && url.scheme().startsWith("http")) {
                QStringList hist = settings.value("history/urls").toStringList();
                hist.prepend(url.toString());
                hist.removeDuplicates();
                if (hist.size() > 100) hist = hist.mid(0, 100);
                settings.setValue("history/urls", hist);
            }
        });

        connect(cookieManagerAction, &QAction::triggered, this, &BrowserWindow::showCookieManager);
        connect(printAction, &QAction::triggered, this, &BrowserWindow::printPage);
        connect(historyAction, &QAction::triggered, this, &BrowserWindow::showHistory);

        // Initial state
        backAction->setEnabled(false);
        forwardAction->setEnabled(false);
        reloadAction->setEnabled(false);

        view->setUrl(QUrl("https://github.com/login")); // Good test site for login cookies
    }

private slots:
    void showCookieManager() {
        QDialog dialog(this);
        dialog.setWindowTitle("Cookie / Session Manager");
        dialog.resize(700, 500);

        QVBoxLayout *layout = new QVBoxLayout(&dialog);

        QLabel *info = new QLabel("Captured Session Tokens & Cookies. "
                                  "Click 'Save to Disk' to persist login states across app restarts.");
        info->setWordWrap(true);
        layout->addWidget(info);

        QTableWidget *table = new QTableWidget();
        table->setColumnCount(4);
        table->setHorizontalHeaderLabels({"Name", "Domain", "Path", "Expires / Session"});
        table->horizontalHeader()->setStretchLastSection(true);
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table->setSelectionBehavior(QAbstractItemView::SelectRows);

        // Populate table
        int row = 0;
        for (auto it = cookieMap.begin(); it != cookieMap.end(); ++it) {
            const QNetworkCookie &cookie = it.value();
            table->insertRow(row);
            table->setItem(row, 0, new QTableWidgetItem(QString(cookie.name())));
            table->setItem(row, 1, new QTableWidgetItem(cookie.domain()));
            table->setItem(row, 2, new QTableWidgetItem(cookie.path()));

            QString expStr = cookie.isSessionCookie() ? "Session" : cookie.expirationDate().toString(Qt::ISODate);
            table->setItem(row, 3, new QTableWidgetItem(expStr));
            row++;
        }

        layout->addWidget(table);

        // Buttons
        QHBoxLayout *btnLayout = new QHBoxLayout();
        QPushButton *saveBtn = new QPushButton("💾 Save to Disk");
        QPushButton *clearBtn = new QPushButton("🗑 Clear All");
        QPushButton *closeBtn = new QPushButton("Close");

        btnLayout->addWidget(saveBtn);
        btnLayout->addWidget(clearBtn);
        btnLayout->addStretch();
        btnLayout->addWidget(closeBtn);
        layout->addLayout(btnLayout);

        connect(saveBtn, &QPushButton::clicked, this, [this, &dialog]() {
            saveCookiesToDisk();
            QMessageBox::information(&dialog, "Saved", "Cookies saved to local settings.");
        });

        connect(clearBtn, &QPushButton::clicked, this, [this, table, &dialog]() {
            cookieMap.clear();
            QWebEngineProfile::defaultProfile()->cookieStore()->deleteAllCookies();
            table->setRowCount(0);
            QMessageBox::information(&dialog, "Cleared", "All cookies cleared.");
        });

        connect(closeBtn, &QPushButton::clicked, &dialog, &QDialog::accept);

        dialog.exec();
    }

    void printPage() {
        QFileDialog dialog(this, "Save PDF");
        dialog.setAcceptMode(QFileDialog::AcceptSave);
        dialog.setDefaultSuffix("pdf");
        dialog.setNameFilter("PDF Files (*.pdf)");

        if (dialog.exec() == QDialog::Accepted) {
            QString filePath = dialog.selectedFiles().first();
            QPageLayout layout(QPageSize(QPageSize::A4), QPageLayout::Portrait, QMarginsF(10, 10, 10, 10));
            view->page()->printToPdf(filePath, layout);
            QMessageBox::information(this, "Print", "Successfully saved PDF to:\n" + filePath);
        }
    }

    void showHistory() {
        QDialog dialog(this);
        dialog.setWindowTitle("Browsing History");
        dialog.resize(500, 400);

        QVBoxLayout *layout = new QVBoxLayout(&dialog);
        QListWidget *list = new QListWidget();

        QStringList hist = settings.value("history/urls").toStringList();
        list->addItems(hist);

        layout->addWidget(list);

        QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Close);
        connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        layout->addWidget(buttonBox);

        dialog.exec();
    }

private:
    void saveCookiesToDisk() {
        QVariantList list;
        for (auto it = cookieMap.begin(); it != cookieMap.end(); ++it) {
            // Serialize cookie to raw byte array, then to Base64 string for QSettings
            QByteArray raw = it.value().toRawForm();
            list.append(QString::fromLatin1(raw.toBase64()));
        }
        settings.setValue("cookies/saved", list);
    }

    void loadSavedCookies() {
        QVariantList list = settings.value("cookies/saved").toList();
        QWebEngineCookieStore *store = QWebEngineProfile::defaultProfile()->cookieStore();

        for (const QVariant &v : list) {
            QByteArray raw = QByteArray::fromBase64(v.toString().toLatin1());
            // parseCookies returns a list, we take the first valid one
            QList<QNetworkCookie> cookies = QNetworkCookie::parseCookies(raw);
            //if (!cookies.isEmpty() && cookies.first().isValid()) {
           //     QNetworkCookie cookie = cookies.first();
            //    store->setCookie(cookie);
            //    cookieMap[cookie.name() + "|" + cookie.domain()] = cookie;
            //}
        }
    }

    QWebEngineView *view;
    QLineEdit *urlLineEdit;
    QAction *backAction, *forwardAction, *reloadAction, *stopAction;
    QAction *cookieManagerAction, *printAction, *historyAction;
    QSettings settings;

    // Local cache of intercepted cookies for the UI
    QMap<QString, QNetworkCookie> cookieMap;
};

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);

    // Required for WebEngine to initialize properly before creating profiles
  //  QtWebEngine::initialize();

    BrowserWindow w;
    w.show();

    return a.exec();
}

#include "main.moc"
