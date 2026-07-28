#include <QApplication>
#include <QMainWindow>
#include <QToolBar>
#include <QLineEdit>
#include <QAction>
#include <QStatusBar>
#include <QProgressBar>
#include <QFileDialog>
#include <QPrinter>
#include <QPrintDialog>
#include <QUrl>
#include <QDir>

#include <QtWebEngineWidgets/QWebEngineView>
#include <QtWebEngineWidgets/QWebEngineProfile>
#include <QtWebEngineWidgets/QWebEnginePage>
#include <QtWebEngineWidgets/QWebEngineDownloadItem>

class Browser : public QMainWindow
{
public:

    Browser()
    {
        resize(1200,800);

        view = new QWebEngineView(this);
        setCentralWidget(view);

        auto tb = addToolBar("Navigation");

        QAction *back = tb->addAction("◀");
        QAction *forward = tb->addAction("▶");
        QAction *reload = tb->addAction("⟳");
        QAction *stop = tb->addAction("X");
        QAction *home = tb->addAction("Home");
        QAction *print = tb->addAction("Print");

        address = new QLineEdit;
        address->setPlaceholderText("Enter URL or search...");
        address->setMinimumWidth(500);
        tb->addWidget(address);

        progress = new QProgressBar;
        progress->setMaximumWidth(200);
        progress->setVisible(false);

        statusBar()->addPermanentWidget(progress);

        connect(back,&QAction::triggered,
                view,&QWebEngineView::back);

        connect(forward,&QAction::triggered,
                view,&QWebEngineView::forward);

        connect(reload,&QAction::triggered,
                view,&QWebEngineView::reload);

        connect(stop,&QAction::triggered,
                view,&QWebEngineView::stop);

        connect(home,&QAction::triggered,this,[=](){

            view->load(QUrl("https://www.google.com"));

        });

        connect(address,&QLineEdit::returnPressed,this,[=](){

            QString txt=address->text().trimmed();

            QUrl url=QUrl::fromUserInput(txt);

            if(url.scheme().isEmpty())
            {
                url=QUrl(
                        "https://www.google.com/search?q="
                        +QUrl::toPercentEncoding(txt)
                        );
            }

            view->load(url);

        });

        connect(view,&QWebEngineView::urlChanged,
                this,[=](const QUrl &u){

            address->setText(u.toString());

        });

        connect(view,&QWebEngineView::loadProgress,
                this,[=](int p){

            progress->setVisible(true);
            progress->setValue(p);

            if(p==100)
                progress->hide();

        });

        connect(view,&QWebEngineView::loadFinished,
                this,[=](bool){

            setWindowTitle(view->title());

        });

        connect(print,&QAction::triggered,this,[=](){

            QPrinter printer;

            QPrintDialog dlg(&printer,this);

            if(dlg.exec()==QDialog::Accepted)
            {
                view->page()->print(
                            &printer,
                            [](bool){}
                            );
            }

        });

        auto profile=view->page()->profile();

        connect(profile,
                &QWebEngineProfile::downloadRequested,
                this,
                [=](QWebEngineDownloadItem *download){

            QString file=QFileDialog::getSaveFileName(
                        this,
                        "Save File",
                        download->path()
                        );

            if(file.isEmpty())
            {
                download->cancel();
                return;
            }

            download->setPath(file);

            progress->show();
            progress->setValue(0);

            connect(download,
                    &QWebEngineDownloadItem::downloadProgress,
                    this,
                    [=](qint64 received,qint64 total){

                if(total>0)
                {
                    progress->setValue(
                                int(received*100/total));
                }

            });

            connect(download,
                    &QWebEngineDownloadItem::finished,
                    this,
                    [=](){

                progress->hide();
                statusBar()->showMessage(
                            "Download Finished",
                            5000);

            });

            download->accept();

        });

        view->load(QUrl("https://www.google.com"));
    }

private:

    QWebEngineView *view;
    QLineEdit *address;
    QProgressBar *progress;
};

int main(int argc,char *argv[])
{
    QApplication app(argc,argv);

    Browser b;
    b.show();

    return app.exec();
}
