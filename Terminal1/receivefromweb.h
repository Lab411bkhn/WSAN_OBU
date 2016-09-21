#ifndef RECEIVEFROMWEB_H
#define RECEIVEFROMWEB_H
#include <QObject>
#include <QCoreApplication>
#include <QDebug>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>
#include <QTextCodec>

class receivefromweb : public QObject
{
    Q_OBJECT
public:
    explicit receivefromweb(QObject *parent = 0);
signals:
    void requestWork();
    void Workrequest(QString);
public slots:
    void doWork_web();
};

#endif // RECEIVEFROMWEB_H
