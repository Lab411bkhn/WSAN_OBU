#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "console.h"
#include <QTimer>
#include "qextserialenumerator.h"
#include "qextserialport.h"
#include "hled.h"
#include "gps.h"
#include <QThread>
#include <QList>
#include "sensor.h"
#include "define.h"
#include "tranceiver.h"
#include "Dialog/editsensorplacedialog.h"
#include "Dialog/newsensordialog.h"
#include "Dialog/retasksensordialog.h"
#include "Dialog/retskalldialog.h"
#include "Dialog/sensorinformationdialog.h"
#include "Dialog/startuplocationdialog.h"
#include "Dialog/setupserialportdialog.h"
#include "Dialog/aboutdialog.h"
#include "Dialog/serverdialog.h"
#include "receivefromweb.h"
#include <QNetworkAccessManager>
#include "QHash"
#include "QTime"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT
    
public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

protected:
    void changeEvent(QEvent *e);

private:
    QString getIp(int);
    void loadHtmlPage();
    void errorOccured(QString);
    void showMarker(int);
    bool checkJoin(int);
    int getIndexSensor(int);
    int getIndexMarker(int);
    QString findNearestNode();
    void updateListSensor();
    void delay(int secondsToWait);
    int tPredict(QTime t1, QTime t2, double d1, double d2);
    double distance(double lat1, double lng1, double lat2, double lng2);
    double bearing(double lat1, double lng1, double lat2, double lng2);
    double deg2rad(double deg);
    void WriteTextAppend(QString, QString);
    void findLastPoint(QString &lat2, QString &lng2, double lat1, double lng1, double dis, double bear);
    
signals:
    void readySend();

private slots:
    void on_btngetNearestNode_clicked();
    void onOptimizeMove();
    void on_btnPrint_clicked();
    void on_btnUpdate_clicked();
    void on_btnClear_clicked();
    void Send_Broadcast();
    void autoTakePhoto(QString);

    void on_pushButton_clicked();
    void on_pushButton_2_clicked();
    void sendCommand(int, int);
    void onOpenCloseButtonClicked();
    void onGpsData(QString);
    void initMap(bool);
    void initListSensor();
    void onNodeJoin(int, QString);
    void onImageReceived(QString);
    //Map
    void addMarker(QString);
    void removeAllMarkers();
    void ClearMap();
    void ShowAllSensor(bool);
    void ShowJoinedSensor(bool);
    void zoomMap(QString);
    void gotoPlaceByCoordinate(QString);
    //Tranceiver
    void onTranceiverData(QString);
    void onTempAndHum(QString);
    //checkbox
    void onGpsStatus(bool);
    //Action
    void startLinphone();
    void ShowAbout();
    void RetaskOne();
    void RetaskAll();
    void AddNewSensor();
    void EditSensorPlace();
    void ShowSensorInformation();
    void StartupLocation();
    void SetupSerialPort();
    void SendToServer();
    //Push button
    void on_btnExit_clicked();
    void on_btnView_clicked();
    //////////////
    void sendtoWeb();
    void sendImageToWeb();
    void GetDataWeb();

private:
    Ui::MainWindow *ui;
    Console *console;
    GPS *gps;
    QThread *thread_gps, *thread_tranceiver, *thread_receivefromweb;
    HLed *led;
    QString m_organizationName, m_appName;
    bool gpsStarted, tranceiverStarted, gpsDataReceived, sended, detectedFlag, receivedFlag;
    QList<Sensor*> ListSensor;
    QList<bool> ListSensorStt;
    QList<double> ListSensorArg;
    QList<double> ListSensorTemp;
    QList<double> ListSensorDeltaTemp;
    Tranceiver *tranceiver;
    receivefromweb *web;
    QNetworkAccessManager *http1;
    int *joinedMac;
    QString *joinedAddress;
    int indexJoined;
    QHash <QString, QString> hash;
    QHash <QString, QTime> movDetectTime;
    QHash <QString, double> maxTemp;
    double d1, d2;
    double a_test, b_test;
    //QList<int> tempIndex;
    //QList<double> lastTemp;
};

#endif // MAINWINDOW_H
