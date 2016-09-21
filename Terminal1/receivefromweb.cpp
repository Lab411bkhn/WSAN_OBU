#include "receivefromweb.h"
#include "unistd.h"
#include "data.h"

receivefromweb::receivefromweb(QObject *parent):
    QObject(parent)
{
}

void receivefromweb::doWork_web()
{
    while(true){
    QEventLoop eventLoop;
    QNetworkAccessManager mgr;
    QObject::connect(&mgr, SIGNAL(finished(QNetworkReply*)), &eventLoop, SLOT(quit()));
    QNetworkRequest req( QUrl( QString("http://lab411s.esy.es/sg/getdata.php") ) );
    req.setHeader(QNetworkRequest::ContentTypeHeader,"application/x-www-form-urlencoded");
    QNetworkReply *reply = mgr.get(req);
    eventLoop.exec(); // blocks stack until "finished()" has been called

    if (reply->error() == QNetworkReply::NoError) {
        //success
        DATA::str = reply->readAll();
        //qDebug() << "Success" <<DATA::str;

        emit Workrequest(DATA::str);
        usleep(5000);
        delete reply;
    }
    else {
        //failure
        //qDebug() << "Failure" << reply->errorString();
        delete reply;
    }
  }
}
