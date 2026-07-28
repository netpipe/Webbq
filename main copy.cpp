#include <QApplication>
#include <QMainWindow>
#include <QWebEngineView>
#include <QWebEnginePage>
#include <QWebEngineHistory>
#include <QWebEngineSettings>
#include <QToolBar>
#include <QLineEdit>
#include <QAction>
#include <QMenu>
#include <QSettings>
#include <QMessageBox>
#include <QFileDialog>
#include <QPageLayout>
#include <QVBoxLayout>
#include <QDialog>
#include <QListWidget>
#include <QDialogButtonBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>

class BrowserWindow : public QMainWindow {
    Q_OBJECT

public:
    BrowserWindow() {
        setWindowTitle("Qt 5.12 Mini Browser");
        resize(1024, 768);

        // 1. Setup WebEngine View
        view = new QWebEngineView(this);
        view->settings()->setAttribute(QWebEngineSettings::JavascriptEnabled, true);
        view->settings()->setAttribute(QWebEngineSettings::LocalStorageEnabled, true);
        setCentralWidget(view);

        // 2. Setup Toolbar & Actions
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

        savePassAction = toolbar->addAction("🔑 Save Password");
        printAction = toolbar->addAction("🖨 Print to PDF");
        historyAction = toolbar->addAction("📜 History");

        // 3. Connect Signals & Slots
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
                if (hist.size() > 100) hist = hist.mid(0, 100); // Keep last 100
                settings.setValue("history/urls", hist);
            }
        });

        connect(savePassAction, &QAction::triggered, this, &BrowserWindow::savePassword);
        connect(printAction, &QAction::triggered, this, &BrowserWindow::printPage);
        connect(historyAction, &QAction::triggered, this, &BrowserWindow::showHistory);

        // Initial state
        backAction->setEnabled(false);
        forwardAction->setEnabled(false);
        reloadAction->setEnabled(false);

        view->setUrl(QUrl("https://www.qt.io"));
    }

private slots:
    void savePassword() {
        // Inject JavaScript to find password and username fields
        QString js = R"(
            (function() {
                var pwd = document.querySelector('input[type="password"]');
                if (!pwd) return JSON.stringify({error: "No password field found on this page."});

                var usr = document.querySelector('input[type="email"], input[type="text"], input[name*="user"], input[name*="login"], input[id*="user"], input[id*="login"]');
                return JSON.stringify({
                    user: usr ? usr.value : 'unknown',
                    pass: pwd.value,
                    url: window.location.href
                });
            })()
        )";

        view->page()->runJavaScript(js, [this](const QVariant &result) {
            QJsonDocument doc = QJsonDocument::fromJson(result.toString().toUtf8());
            QJsonObject obj = doc.object();

            if (obj.contains("error")) {
                QMessageBox::warning(this, "Save Password", obj["error"].toString());
                return;
            }

            QString user = obj["user"].toString();
            QString pass = obj["pass"].toString();
            QString url = obj["url"].toString();

            // Save to QSettings
            QStringList savedUrls = settings.value("passwords/urls").toStringList();
            savedUrls.append(url + " | User: " + user);
            settings.setValue("passwords/urls", savedUrls);

            // Note: In a real app, you would encrypt 'pass' before saving.
            // For this demo, we just acknowledge it.
            QMessageBox::information(this, "Save Password",
                QString("Credentials captured for:\n%1\nUser: %2\n\n(Saved to local settings)").arg(url, user));
        });
    }

    void printPage() {
        QFileDialog dialog(this, "Save PDF");
        dialog.setAcceptMode(QFileDialog::AcceptSave);
        dialog.setDefaultSuffix("pdf");
        dialog.setNameFilter("PDF Files (*.pdf)");

        if (dialog.exec() == QDialog::Accepted) {
            QString filePath = dialog.selectedFiles().first();
            // Qt 5.12 uses printToPdf with a file path and layout
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
    QWebEngineView *view;
    QLineEdit *urlLineEdit;
    QAction *backAction, *forwardAction, *reloadAction, *stopAction;
    QAction *savePassAction, *printAction, *historyAction;
    QSettings settings; // Uses default QSettings storage (Registry on Windows, .conf on Linux)
};
#include "main.moc"
int main(int argc, char *argv[]) {
    QApplication a(argc, argv);

    // Optional: Initialize WebEngine (helps with some platform-specific rendering setups)
    // QtWebEngine::initialize();

    BrowserWindow w;
    w.show();

    return a.exec();
}

// Required for single-file Qt compilation when using Q_OBJECT

