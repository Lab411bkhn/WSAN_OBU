#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QDebug>
#include <QMessageBox>
#include <QSettings>
#include <QDebug>
#include <QtWebKit>
#include <QThread>
#include <QFile>
#include "filedata.h"
#include <QFileDialog>
#include "data.h"
#include "math.h"
#include "QDir"
#include "QCoreApplication"
#include "QEventLoop"
#include "QPrintDialog"
#include "QPrinter"
#include "QTextCursor"

class WebPage : public QWebPage
{
    public:  WebPage(QObject * p = 0) : QWebPage(p) {}
    private: QString userAgentForUrl(const QUrl&) const { return "Chrome/1.0"; }
};

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{

    qDebug()<<"okoko";
    ui->setupUi(this);
    joinedMac = new int[20];
    joinedAddress = new QString[20];
    indexJoined = 0;
    http1 = new QNetworkAccessManager(this);
    console = new Console;
    console->setEnabled(true);
    console->setLocalEchoEnabled(true);
    ui->layConsole->addWidget(console);
    led = new HLed();
    led->turnOff();
    ui->layLed->addWidget(led);

    //init Data
    hash["62"] = "05";
    hash["61"] = "08";
    d1 = 20, d2 =20;
    receivedFlag = false;

    //data from web
    web= new receivefromweb();
    thread_receivefromweb=new QThread();
    web->moveToThread(thread_receivefromweb);
    thread_receivefromweb->start();
    connect(thread_receivefromweb,SIGNAL(started()),web,SLOT(doWork_web()));
    connect(web, SIGNAL(Workrequest(QString)), this ,SLOT(GetDataWeb()));
    qDebug()<< "thread start";
    connect(this,SIGNAL(readySend()),SLOT(sendtoWeb()));

    // GPS

    gps = new GPS();
    gpsStarted = false;
    gpsDataReceived = false;
    thread_gps = new QThread();

    gps->moveToThread(thread_gps);
    connect(gps, SIGNAL(workRequested()), thread_gps, SLOT(start()));
    connect(thread_gps, SIGNAL(started()), gps, SLOT(doWork()));
    connect(gps, SIGNAL(receivedData(QString)), SLOT(onGpsData(QString)));
    connect(ui->cbxGps, SIGNAL(toggled(bool)), SLOT(onGpsStatus(bool)));
    connect(ui->cbxAllSensor, SIGNAL(toggled(bool)), SLOT(ShowAllSensor(bool)));
    connect(ui->cbxJoinedSensor, SIGNAL(toggled(bool)), SLOT(ShowJoinedSensor(bool)));

    // Tranceiver
    tranceiver = new Tranceiver();
    tranceiverStarted = false;
    thread_tranceiver = new QThread();
    tranceiver->moveToThread(thread_tranceiver);
    connect(tranceiver, SIGNAL(workRequested()), thread_tranceiver, SLOT(start()));
    connect(thread_tranceiver, SIGNAL(started()), tranceiver, SLOT(doWork()));
    connect(tranceiver, SIGNAL(receivedData(QString)), SLOT(onTranceiverData(QString)));
    connect(tranceiver, SIGNAL(receiveCompletely()), this, SLOT(sendImageToWeb()));
    connect(tranceiver, SIGNAL(nodeJoin(int, QString)), SLOT(onNodeJoin(int, QString)));
    connect(tranceiver, SIGNAL(ImageReceived(QString)), SLOT(onImageReceived(QString)));
    connect(ui->btnOpenClose, SIGNAL(clicked()), SLOT(onOpenCloseButtonClicked()));
    connect(tranceiver, SIGNAL(tempAndHum(QString)), SLOT(onTempAndHum(QString)));
    //connect(tranceiver, SIGNAL(motionDetected(QString)), SLOT(autoTakePhoto(QString)));

    //Tab Map
    ui->webView->setPage( new WebPage(this) );
    loadHtmlPage();
    connect(ui->webView, SIGNAL(loadFinished(bool)), this, SLOT(initMap(bool)));

    //Action
    connect(ui->actionLinphone, SIGNAL(triggered()), SLOT(startLinphone()));
    connect(ui->actionQuit, SIGNAL(triggered()), SLOT(close()));
    connect(ui->actionAbout_2, SIGNAL(triggered()), SLOT(ShowAbout()));
    connect(ui->actionRetask_One, SIGNAL(triggered()), SLOT(RetaskOne()));
    connect(ui->actionRetask_All, SIGNAL(triggered()), SLOT(RetaskAll()));
    connect(ui->actionNew, SIGNAL(triggered()), SLOT(AddNewSensor()));
    connect(ui->actionEdit_Place, SIGNAL(triggered()), SLOT(EditSensorPlace()));
    connect(ui->actionInformation, SIGNAL(triggered()), SLOT(ShowSensorInformation()));
    connect(ui->actionStartup_Location_Map, SIGNAL(triggered()), SLOT(StartupLocation()));
    connect(ui->actionSerial_Port, SIGNAL(triggered()), SLOT(SetupSerialPort()));
    connect(ui->actionServer, SIGNAL(triggered()), SLOT(SendToServer()));

  //  connect(ui->btnBroadcast, SIGNAL(clicked()), SLOT(Send_Broadcast()));
    connect(ui->btnShowVector, SIGNAL(clicked()), this, SLOT(onOptimizeMove()));

    setWindowTitle(tr("Patrolling Software by LAB411 "));
    SetupSerialPort();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::changeEvent(QEvent *e)
{
    QMainWindow::changeEvent(e);
    switch (e->type()) {
    case QEvent::LanguageChange:
        ui->retranslateUi(this);
        break;
    default:
        break;
    }
}

void MainWindow::loadHtmlPage()
{
    QFile htmlFile(":/html/index.html");
    if( ! htmlFile.open(QFile::ReadOnly) )
    {
        errorOccured( "I can't read html file" );
        return;
    }

    QTextStream stream( & htmlFile );
    QString html = stream.readAll();
    ui->webView->setHtml( html );
}

void MainWindow::errorOccured(QString error)
{
    QMessageBox::warning(this, trUtf8("Error"), error);
}

void MainWindow::initMap(bool isok)
{
    if( ! isok ) return;
    QSettings settings(m_organizationName, m_appName);
    QString lat = settings.value("latStartup").toString();
    QString lng = settings.value("lngStartup").toString();
    QString place = lat + ", " + lng;
    if( lat.isEmpty() || lng.isEmpty() ) place = "20.333413, 105.597677";
    QString zm = settings.value("zoomStartup").toString();
    if(zm.isEmpty()) zm = "8";
    QString str = place + ", " + zm;
    ui->webView->page()->mainFrame()->evaluateJavaScript(QString("initialize( %1 )").arg(str));
    initListSensor();
}

void MainWindow::initListSensor()
{
    FileData file(DATA_PATH);
    FileData data(HISTORY_FILE);
    FileData timeData(TIME_DETECT);
    int N = file.length();
    int M = data.length();
    int P = timeData.length();
    for(int i = 0; i < N; i++)
    {
        QTime initTime(0,0,0,0);
        QString line = file.readLine(i+1);
        QStringList lst = line.split(",");
        movDetectTime.insert(lst.value(0), initTime);
        maxTemp.insert(lst.value(0), 0);

        for(int j = P-1; j >=0; j--){
            QString timeLine = timeData.readLine(j+1);
            QStringList timeList = timeLine.split(":");
            if(timeList.value(0) == lst.value(0)){
                QTime time(timeList.value(1).toInt(), timeList.value(2).toInt(), timeList.value(3).toInt());
                movDetectTime[lst.value(0)] = time;
                break;
            }
        }

        QString str = lst.value(0) + "," + lst.value(1) + ", " + lst.value(2) + ",";
        for(int j = M-1; j >=0; j--){
            QString data_line = data.readLine(j+1);
            QStringList data_lst = data_line.split(":");
            if(data_lst.value(0) == lst.value(0)){
                str += data_lst.value(1) + "," + data_lst.value(2);
                ListSensorTemp.push_back(data_lst.value(1).toDouble());
                break;
            }
        }
//        if(lst.value(0) == "09"){
//            //bool ok1,ok2;
//            QString time1 = movDetectTime.value("08").toString("hh:mm:ss");
//            QString time2 = movDetectTime.value("05").toString("hh:mm:ss");
//            QString time3 = movDetectTime.value("09").toString("hh:mm:ss");
//            QStringList lTime1 = time1.split(":");
//            QStringList lTime2 = time2.split(":");
//            QStringList lTime3 = time3.split(":");
//            for(int p=0; p<3; p++){
//                str += ","+lTime1.value(p);
//            }
//            for(int q=0; q<3; q++){
//                str += ","+lTime2.value(q);
//            }
//            for(int r=0; r<3; r++){
//                str += ","+lTime3.value(r);
//            }
//            //str += "," + QString::number(0);
//            ui->webView->page()->mainFrame()->evaluateJavaScript(QString("addTrackingMarker( %1 )").arg(str));
//            //ui->webView->page()->mainFrame()->evaluateJavaScript(QString("setYellowMarker( %1 )").arg(i));
//        }else
            addMarker(str);
        //bool ok;
        //QString imageMac= "http://lab411s.esy.es/sg/camerasensor_" + lst.value(0) + ".jpg";
        //qDebug() << lst.value(0) + ": " + imageDir;
//        QString imageDir = QString(IMAGES_PATH) + "Sensor " + QString(lst.value(0));
//        QDir myDir(imageDir);
//        QStringList imageList = myDir.entryList("*.*");
//        str += QString(imageList.last());
        //qDebug() << str;

        ListSensor.push_back(new Sensor(lst.value(0).toInt()));
        ListSensor.last()->markerIndex = i;
        ListSensor[i]->lat = lst.value(1);
        ListSensor[i]->lng = lst.value(2);
        ListSensor[i]->clusterHead = ListSensor[getIndexMarker(lst.value(6).toInt())];
        double arg = bearing(ListSensor[i]->clusterHead->lat.toDouble(), ListSensor[i]->clusterHead->lng.toDouble(), lst.value(1).toDouble(), lst.value(2).toDouble());
        ListSensorArg.push_back(arg);
        double delta_temp = ListSensorTemp[i] - ListSensorTemp[getIndexMarker(lst.value(6).toInt())];
        ListSensorDeltaTemp.push_back(delta_temp);
        ListSensor[i]->x0 = ListSensorDeltaTemp[i] * sin(ListSensorArg[i]);
        ListSensor[i]->y0 = ListSensorDeltaTemp[i] * cos(ListSensorArg[i]);
        ListSensorStt.push_back(false);

        if(lst.value(0) == lst.value(6))
            ui->webView->page()->mainFrame()->evaluateJavaScript(QString("setYellowMarker( %1 )").arg(i));

        QString vector = lst.value(1) + "," + lst.value(2);
        vector += "," + vector;
        //qDebug() << vector;
        ui->webView->page()->mainFrame()->evaluateJavaScript(QString("addTempVector(%1)").arg(vector));
        ui->webView->page()->mainFrame()->evaluateJavaScript(QString("clearArrow( %1 )").arg(i));


        //qDebug() << "Sensor " << lst.value(0) << ", Argument: " << ListSensorArg[i]*180/M_PI;
        //qDebug() << ListSensor[i]->x0 << " *** " << ListSensor[i]->y0;
        //qDebug() << "Sensor "  << ListSensor[i]->isJoin;
    }
    //QString lat_test, lng_test;
//    findLastPoint(lat_test, lng_test, 53.32055556, 1.7297222, 124800, 1.675894237 );
//    qDebug() << "Test: " << lat_test << " *** " << lng_test ;
    //double dis_test = distance(ListSensor[0]->lat.toDouble(), ListSensor[0]->lng.toDouble(), ListSensor[1]->lat.toDouble(), ListSensor[1]->lng.toDouble());
    //findLastPoint(lat_test, lng_test, ListSensor[0]->lat.toDouble(), ListSensor[0]->lng.toDouble(), dis_test, ListSensorArg[1]);
    //qDebug() << "Test: " << lat_test << " *** " << lng_test ;
   // ClearMap();
}

void MainWindow::updateListSensor()
{
    qDebug() << "Update List Sensor";
    //qDebug() << "Time Appear Predict: " << tPredict(movDetectTime.value("08"), movDetectTime.value("05"), d1, d2);
    ui->webView->page()->mainFrame()->evaluateJavaScript("removeAllMarkers()");
    ui->webView->page()->mainFrame()->evaluateJavaScript("clearAllMarkers()");
    ui->webView->page()->mainFrame()->evaluateJavaScript("removeAllArrows()");
    FileData file(DATA_PATH);
    FileData data(HISTORY_FILE);
    int N = file.length();
    qDebug() << N;
    int M = data.length();
    for(int i = 0; i < N; i++)
    {
        QString line = file.readLine(i+1);
        QStringList lst = line.split(",");
        QString str = lst.value(0) + "," + lst.value(1) + ", " + lst.value(2);

//        double dis = distance(DATA::lat.toDouble(), DATA::lng.toDouble(), QString(lst.value(1)).toDouble(), QString(lst.value(2)).toDouble());
//        qDebug() << "Node " << lst.value(0) << ": " << dis;

        for(int j = M-1; j >=0; j--){
            QString data_line = data.readLine(j+1);
            QStringList data_lst = data_line.split(":");
            if(data_lst.value(0) == lst.value(0)){
                str += "," + data_lst.value(1) + "," + data_lst.value(2);
                ListSensorTemp[i] = data_lst.value(1).toDouble();
                break;
            }
        }
//        if(lst.value(0) == "09"){
//            QString time1 = movDetectTime.value("08").toString("hh:mm:ss");
//            QString time2 = movDetectTime.value("05").toString("hh:mm:ss");
//            QString time3 = movDetectTime.value("09").toString("hh:mm:ss");
//            QStringList lTime1 = time1.split(":");
//            QStringList lTime2 = time2.split(":");
//            QStringList lTime3 = time3.split(":");
//            for(int p=0; p<3; p++){
//                str += ","+lTime1.value(p);
//            }
//            for(int q=0; q<3; q++){
//                str += ","+lTime2.value(q);
//            }
//            for(int r=0; r<3; r++){
//                str += ","+lTime3.value(r);
//            }
//            ui->webView->page()->mainFrame()->evaluateJavaScript(QString("addTrackingMarker( %1 )").arg(str));
//            //ui->webView->page()->mainFrame()->evaluateJavaScript(QString("setYellowMarker( %1 )").arg(i));
//        }else
            addMarker(str);
//        if (ListSensorStt[i] && (i!=8)){
//            ui->webView->page()->mainFrame()->evaluateJavaScript(QString("setGreenMarker(%1)").arg(QString::number(i)));
//        }
            if(lst.value(0) == lst.value(6)){
                if(ListSensorStt[i])
                    ui->webView->page()->mainFrame()->evaluateJavaScript(QString("setPinkMarker( %1 )").arg(i));
                else
                    ui->webView->page()->mainFrame()->evaluateJavaScript(QString("setYellowMarker( %1 )").arg(i));
            }
            else if(ListSensorStt[i])
                ui->webView->page()->mainFrame()->evaluateJavaScript(QString("setGreenMarker( %1 )").arg(i));
        ListSensorDeltaTemp[i] = ListSensorTemp[i] - ListSensorTemp[getIndexMarker(lst.value(6).toInt())];
        ListSensor[i]->x0 = ListSensorDeltaTemp[i] * sin(ListSensorArg[i]);
        ListSensor[i]->y0 = ListSensorDeltaTemp[i] * cos(ListSensorArg[i]);
        //qDebug() << i << ListSensorDeltaTemp[i] << ListSensor[i]->x0 << ListSensor[i]->y0;

        QString lat_test, lng_test;
        QString vector = lst.value(1) + "," + lst.value(2);
        double dis = ListSensorDeltaTemp[i] * 20;
        findLastPoint(lat_test, lng_test, lst.value(1).toDouble(), lst.value(2).toDouble(), dis, ListSensorArg[i]);
        vector += "," + lat_test + "," + lng_test;
        //qDebug() << vector;
        ui->webView->page()->mainFrame()->evaluateJavaScript(QString("addTempVector(%1)").arg(vector));
        if((lst.value(0) == lst.value(6)) || (ListSensor[i]->cur_temp == 0))
        //if(lst.value(0) == lst.value(6))
            ui->webView->page()->mainFrame()->evaluateJavaScript(QString("clearArrow(%1)").arg(i));
    }
   // ClearMap();
}

QString MainWindow::getIp(int mac)
{
    if(mac==100) return "FFFF";
    int N = ListSensor.length();
    for(int i = 0; i < N; i++)
    {
        if(ListSensor[i]->mac == mac) return ListSensor[i]->ip;
    }
    return "";
}

// 506F000$

void MainWindow::sendCommand(int mac, int cmd)
{
    int i = 0;
    QString Ip = getIp(mac);
    QString Cmd = Ip;
    QString Cmd1, Cmd2, Cmd3, func1, func2, func3;
    QSettings settings(m_organizationName, m_appName);
    switch(cmd){
    case 0: // Take temperature and humidity
        if(mac < 10){
            DATA::mac = "0" + QString::number(mac);
        }
        else{
            DATA::mac = QString::number(mac);
        }
        DATA::Ip = Ip;
        Cmd += "000$";
        break;

    case 1: // Take photo
        if(mac < 10){
            DATA::mac = "0" + QString::number(mac);
        }
        else{
            DATA::mac = QString::number(mac);
        }
        DATA::Ip = Ip;
        DATA::img.clear();
        Cmd += "400$";
        break;

    case 2: // Request Status
        Cmd += "410$";
        break;

    case 3: // Retask
        func1 = settings.value("func1Retask").toString();
        func2 = settings.value("func2Retask").toString();
        func3 = settings.value("func3Retask").toString();

        if(!func1.isEmpty())
        {
            i+=1;
            Cmd1 = Ip + "5" + func1 + "0$";
        }
        if(!func2.isEmpty())
        {
            i+=10;
            Cmd2 = Ip + "6" + func2 + "0$";
        }
        if(!func3.isEmpty())
        {
            i+=100;
            Cmd3 = Ip + "7" + func3 + "0$";
        }
        break;

    case 4: // Warning level 1
        Cmd = "0000011$";
        break;

    case 5: // Warning level 2
        Cmd = "0000021$";
        break;

    case 6: // Warning level 3
        Cmd = "0000031$";
        break;

    case 7: // Warning level 4
        Cmd = "0000041$";
        break;

    case 8: // Warning level 5
        Cmd = "0000051$";
        break;

    default: break;
    }

    if(Cmd.length() > 4) {
        tranceiver->writeData(Cmd);
        QString tmp = "Gui ma lenh " + Cmd + "\n";
        console->insertPlainText(tmp);
    }

    switch(i){
    case 1:{
        //qDebug() << "Cmd2: "<< Cmd1 << endl;
        tranceiver->writeData(Cmd1);
        QString tmp = "Gui ma lenh " + Cmd1 + "\n";
        console->insertPlainText(tmp);
        break;
    }

    case 10:{
        //qDebug() << "Cmd2:  " << Cmd2 << endl;
        tranceiver->writeData(Cmd2);
        QString tmp = "Gui ma lenh " + Cmd2 + "\n";
        console->insertPlainText(tmp);
        break;
    }

    case 100:{
        tranceiver->writeData(Cmd3);
        QString tmp = "Gui ma lenh " + Cmd3 + "\n";
        console->insertPlainText(tmp);
        break;
    }

    case 11:{
        tranceiver->writeData(Cmd1);
        sleep(5);
        tranceiver->writeData((Cmd2));
        QString tmp = "Gui ma lenh" + Cmd1 + "\n" + "va ma lenh" + Cmd2 + "\n";
        console->insertPlainText(tmp);
        break;
    }

    case 110:{
        tranceiver->writeData(Cmd2);
        sleep(5);
        tranceiver->writeData((Cmd3));
        QString tmp = "Gui ma lenh" + Cmd2 + "\n" + "va ma lenh" + Cmd3 + "\n";
        console->insertPlainText(tmp);
        break;
    }

    case 101:{
        tranceiver->writeData(Cmd1);
        sleep(5);
        tranceiver->writeData((Cmd3));
        QString tmp = "Gui ma lenh" + Cmd1 + "\n" + "va ma lenh" + Cmd3 + "\n";
        console->insertPlainText(tmp);
        break;
    }

    case 111:{
        tranceiver->writeData(Cmd1);
        sleep(5);
        tranceiver->writeData(Cmd2);
        sleep(5);
        tranceiver->writeData((Cmd3));
        QString tmp = "Gui ma lenh" + Cmd1 + "\n" + "va ma lenh" + Cmd2 + "\n" + "va ma lenh" + Cmd3 + "\n";
        console->insertPlainText(tmp);
        break;
    }

    default: break;
}
}

void MainWindow::on_btnExit_clicked()
{
    this->close();
}

void MainWindow::onOpenCloseButtonClicked()
{
    if(tranceiverStarted)
    {
        if(tranceiver->port->isOpen()){
            tranceiver->port->close();
            led->turnOff();
            ui->btnOpenClose->setText("Open");
            console->insertPlainText("\n-------------- Port Closed -------------\n");
        } else {
            if(tranceiver->port->portName().isEmpty()) tranceiver->port->setPortName(TRANCEIVER_PORT_DEFAULT);
            QSettings settings(m_organizationName, m_appName);
            QString tmp = settings.value("TranceiverBaudrate").toString();
            if(tmp.isEmpty()) tranceiver->port->setBaudRate((BaudRateType)TRANCEIVER_BAUDRATE_DEFAULT);
            tranceiver->port->open(QIODevice::ReadWrite);
            led->turnOn();
            ui->btnOpenClose->setText("Close");
            console->insertPlainText("\n------------- Port Data Opened ------------\n");
        }
    } else {
        tranceiver->requestWork();
        ui->btnOpenClose->setText("Close");
        led->turnOn();
        console->insertPlainText("\n-------------- Port Data Opened -------------\n");
        tranceiverStarted = true;
    }
}

void MainWindow::onGpsData(QString data)
{
    if(gpsDataReceived) {
        ui->lblMyPlace->setText(data);
        ui->webView->page()->mainFrame()->evaluateJavaScript(QString("moveMyMarker(%1)").arg(data));
    } else {
        ui->lblMyPlace->setText(data);
        ui->webView->page()->mainFrame()->evaluateJavaScript(QString("addMyMarker(%1)").arg(data));
        gpsDataReceived = true;
    }
    onOptimizeMove();
}

void MainWindow::onNodeJoin(int mac, QString address)
{
    //console->insertPlainText("Node Join!!!!\n");
    FileData file(DATA_PATH);
    QString tmp = "Node " + QString::number(mac) + " gia nhap voi dia chi mang " + address + "\n";
    console->insertPlainText(tmp);
    //if(!checkJoin(mac)){
        tmp = "Node " + QString::number(mac);
        int index = getIndexMarker(mac);
        qDebug() << index;
        int flag=1;
        for(int i=0; i<(int)ui->boxSensor->size().height();i++){
            if(ui->boxSensor->itemText(i).compare(tmp)==0){
                flag=0; break;
            }

        }
        if (flag)   ui->boxSensor->addItem(tmp, mac);
        //qDebug() << "Index: " << index;
        if(index >= 0){
            ListSensor[index]->isJoin = true;
            ListSensorStt[index]=true;
            ListSensor[index]->ip = address;
//            joinedMac[indexJoined] = index;
//            joinedAddress[indexJoined] = address;
//            qDebug() <<" joinedMac[" + QString::number(indexJoined) + "]: " << joinedMac[indexJoined];
//            qDebug() << "joinedAddress[" + QString::number(indexJoined) + "]: "  + address;
//            indexJoined++;
//            qDebug() << "indexJoined = " << indexJoined << endl;
            QString line = file.readLine(index+1);
            QStringList lst = line.split(",");
            if(lst.value(0) != lst.value(6))
                ui->webView->page()->mainFrame()->evaluateJavaScript(QString("setGreenMarker(%1)").arg(QString::number(index)));
            else
                ui->webView->page()->mainFrame()->evaluateJavaScript(QString("setPinkMarker(%1)").arg(QString::number(index)));
        }
    //}
}

void MainWindow::onImageReceived(QString FileName)
{
    QImage image(FileName);
    ui->lblImage->setPixmap (QPixmap::fromImage (image));
}

bool MainWindow::checkJoin(int mac)
{
    int i = getIndexSensor(mac);
    if(i > -1) return ListSensorStt[i];
    return false;
}

int MainWindow::getIndexSensor(int mac)
{
    int N = ListSensor.length();
    for(int i = 0; i < N; i++)
    {
        if(ListSensor[i]->mac == mac) return i;
    }
    return -1;
}

int MainWindow::getIndexMarker(int mac)
{
    int N = ListSensor.length();
    for(int i = 0; i < N; i++)
    {
        if(ListSensor[i]->mac == mac) return ListSensor[i]->markerIndex;
    }
    return -1;
}

//Map
void MainWindow::addMarker(QString place)
{
    ui->webView->page()->mainFrame()->evaluateJavaScript(QString("addMarker( %1 )").arg(place));
}

void MainWindow::ClearMap()
{
    ui->webView->page()->mainFrame()->evaluateJavaScript("clearAllMarkers()");
}

void MainWindow::removeAllMarkers()
{
    ui->webView->page()->mainFrame()->evaluateJavaScript("removeAllMarkers()");
}

void MainWindow::zoomMap(QString zm)
{
    ui->webView->page()->mainFrame()->evaluateJavaScript(QString("zoom( %1 )").arg(zm));
}

void MainWindow::gotoPlaceByCoordinate(QString place)
{
    QString str =
            QString("var newLoc = new google.maps.LatLng(%1); ").arg(place) +
            QString("map.setCenter(newLoc);");
    ui->webView->page()->mainFrame()->evaluateJavaScript(str);
}

void MainWindow::ShowAllSensor(bool ok)
{
    if(ok)
    {
        if(ui->cbxJoinedSensor->isChecked()) ui->cbxJoinedSensor->setChecked(false);
        ui->webView->page()->mainFrame()->evaluateJavaScript("showAllMarkers()");
    } else {
        ClearMap();
    }
}

void MainWindow::ShowJoinedSensor(bool ok)
{
    if(ok)
    {
        if(ui->cbxAllSensor->isChecked()) ui->cbxAllSensor->setChecked(false);
        ClearMap();
        int N = ListSensor.length();
        for(int i = 0; i < N; i++)
        {
            if(ListSensor[i]->isJoin) showMarker(ListSensor[i]->markerIndex);
        }
    } else {
        ClearMap();
    }
}

void MainWindow::showMarker(int index)
{
    ui->webView->page()->mainFrame()->evaluateJavaScript(QString("showMarker(%1)").arg(QString::number(index)));
}

//Tranceiver
void MainWindow::onTranceiverData(QString data)
{
    console->insertPlainText(data);
    console->moveCursor(QTextCursor::End);
}

void MainWindow::onTempAndHum(QString data)
{
    QString tmp;
    QStringList data_lst = data.split(":");
    console->moveCursor(QTextCursor::End);
//    FileData file(DATA_PATH);
    QString mac = data_lst.value(0);
    int index  = getIndexMarker(mac.toInt());
    ListSensor[index]->cur_temp = data_lst.value(2).toDouble();

    tmp = "\nThong tin nhiet do, do am tu sensor ";
    tmp += data_lst.value(0);
    tmp += ", dia chi Ip ";
    tmp += data_lst.value(1);
    tmp += "\nNhiet do:        ";
    tmp += QString::number(ListSensor[index]->cur_temp);//data_lst.value(2);
    tmp += "\nDo am:           ";
    tmp += data_lst.value(3);
    tmp += "\n";
    console->insertPlainText(tmp + "\n");

    //    QString line = file.readLine(index+1);
//    QStringList lst = line.split(",");

//    qDebug() << mac;
//    movDetectTime[mac] = QTime::currentTime();
//    qDebug() << QTime::currentTime() << movDetectTime.value(mac);
//    removeAllMarkers();
//    ListSensor.clear();
    updateListSensor();


//    QString lat_test, lng_test;
////    QString vector = QString::number(index) + lst.value(1) + "," + lst.value(2);
//    QString vector = lst.value(1) + "," + lst.value(2);
//    double dis = ListSensorDeltaTemp[index] * 20;
//    findLastPoint(lat_test, lng_test, lst.value(1).toDouble(), lst.value(2).toDouble(), dis, ListSensorArg[index]);
//    vector += "," + lat_test + "," + lng_test;
//    //qDebug() << vector;
//    if(lst.value(0) != lst.value(6)){
//        ui->webView->page()->mainFrame()->evaluateJavaScript(QString("clearArrow(%1)").arg(index));
//        ui->webView->page()->mainFrame()->evaluateJavaScript(QString("addTempVector(%1)").arg(vector));
//    }
    receivedFlag = true;
    emit readySend();
}

//Checkbox
void MainWindow::onGpsStatus(bool ok)
{
    if(ok)
    {
        if(gpsStarted) gps->port->open(QIODevice::ReadWrite);
        else
        {
            gps->requestWork();
            gpsStarted = true;
        }
    } else
    {
        if(gps->port->isOpen()) gps->port->close();
    }
}

// Action
void MainWindow::startLinphone()
{
    QProcess *process = new QProcess(this);
    process->start("linphone-3");
}

void MainWindow::ShowAbout()
{
    AboutDialog dialog(this);
    dialog.setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    dialog.setFixedSize(dialog.size());
    dialog.exec();
}

void MainWindow::RetaskOne()
{
    int N = ListSensor.length();
    QString listMac;
    int mac;
    for(int i = 0; i < N; i++)
    {
        if(ListSensor[i]->isJoin) {
            mac = ListSensor[i]->mac;
            listMac += QString::number(mac);
            listMac += ",";
        }
    }
    listMac.remove(N-1, 1);
    RetaskSensorDialog dialog(listMac, this);
    dialog.setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    dialog.setFixedSize(dialog.size());
    if(dialog.exec() == QDialog::Accepted){
        QSettings settings(m_organizationName, m_appName);
        QString mac = settings.value("macRetask").toString();
        if(!mac.isEmpty()) sendCommand(mac.toInt(), RETASK);
    }
}

void MainWindow::RetaskAll()
{
    RetskAllDialog dialog(this);
    dialog.setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    dialog.setFixedSize(dialog.size());
    if(dialog.exec() == QDialog::Accepted){
        int N = ListSensor.length();
        for(int i = 0; i < N; i++)
        {
            if(ListSensor[i]->isJoin)
            {
                sendCommand(ListSensor[i]->mac, RETASK);
            }
        }
    }
}

void MainWindow::AddNewSensor()
{
    NewSensorDialog dialog(this);
    dialog.setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    dialog.setFixedSize(dialog.size());
    if(dialog.exec() == QDialog::Accepted)
    {
        removeAllMarkers();
        ListSensor.clear();
        initListSensor();
    }
}

void MainWindow::EditSensorPlace()
{
    EditSensorPlaceDialog dialog(this);
    dialog.setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    dialog.setFixedSize(dialog.size());
    if(dialog.exec() == QDialog::Accepted)
    {

    }
}

void MainWindow::ShowSensorInformation()
{
    SensorInformationDialog dialog(this);
    dialog.setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    dialog.setFixedSize(dialog.size());
    dialog.exec();
}

void MainWindow::StartupLocation()
{
    StartupLocationDialog dialog(this);
    dialog.setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    dialog.setFixedSize(dialog.size());
    if(dialog.exec() == QDialog::Accepted)
    {
        QSettings settings(m_organizationName, m_appName);
        QString lat = settings.value("latStartup").toString();
        QString lng = settings.value("lngStartup").toString();
        QString zm = settings.value("zoomStartup").toString();
        QString place = lat + ", " + lng;
        if(place.isEmpty()) place = "20.333413, 105.597677";
        if(zm.isEmpty()) zm = "8";
        gotoPlaceByCoordinate(place);
        zoomMap(zm);
    }
}

void MainWindow::SetupSerialPort()
{
    SetupSerialPortDialog dialog(this);
    dialog.setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    dialog.setFixedSize(dialog.size());
    if(dialog.exec() == QDialog::Accepted)
    {
        QSettings settings(m_organizationName, m_appName);
        QString port = settings.value("TranceiverPort").toString();
        if(port.isEmpty()) port = TRANCEIVER_PORT_DEFAULT;
        tranceiver->port->setPortName(port);
        int baudrate = settings.value("TranceiverBaudrate").toInt();
        tranceiver->port->setBaudRate((BaudRateType)baudrate);

        port = settings.value("GpsPort").toString();
        if(port.isEmpty()) port = GPS_PORT_DEFAULT;
        gps->port->setPortName(port);
        baudrate = settings.value("GpsBaudrate").toInt();
        gps->port->setBaudRate((BaudRateType) baudrate);
    }
}

void MainWindow::SendToServer()
{
    ServerDialog dialog(this);
    dialog.setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    dialog.setFixedSize(dialog.size());
    dialog.exec();
}

//Push Button
void MainWindow::on_btnView_clicked()
{
    QFileDialog dialog(this);
    dialog.setNameFilter (tr("Image (*.png *.xpm *.jpg *jpeg)"));
    dialog.setViewMode (QFileDialog::Detail);
    QString fileName= QFileDialog::getOpenFileName (this,tr("Open Image"),IMAGES_PATH
                                                        ,tr("Image File (*.png *.jpg *.bmp *jpeg)"));
    if(!fileName.isEmpty ()){
        QImage image(fileName);
        ui->lblImage->setPixmap (QPixmap::fromImage (image));
    }
}

void MainWindow::on_pushButton_2_clicked()
{
    if(ui->cbxAllSensor->isChecked()) ui->cbxAllSensor->setChecked(false);
    if(ui->cbxJoinedSensor->isChecked()) ui->cbxJoinedSensor->setChecked(false);
    ClearMap();
}

void MainWindow::on_pushButton_clicked()
{
    int mac = ui->boxSensor->itemData(ui->boxSensor->currentIndex()).toInt();
    int cmd = ui->boxCommand->currentIndex();
    if(mac >= 0 && cmd >= 0) {
        if(cmd == 3)
        {
            RetaskSensorDialog dialog(QString::number(mac), this);
            dialog.exec();
        } else sendCommand(mac, cmd);
    }
}

void MainWindow::Send_Broadcast()
{
    QString Cmd="FFFF000$";
    tranceiver->writeData(Cmd);
    console->insertPlainText(Cmd);
}


void MainWindow::GetDataWeb(){
    if(!DATA::str.isEmpty())
    {
        DATA::img == "";
        QString tmp = "Da nhan ma lenh tu web:  " + DATA::str + "\n";
        console->insertPlainText(tmp);
        int mac = DATA::str.mid(0,2).toInt();
        QString ip = getIp(mac);
        QString cmd = ip + DATA::str.mid(6,4);
        tranceiver->writeData(cmd);
        sended = true;
        QString tmp1 = "Gui ma lenh " + cmd + "\n";
        console->insertPlainText(tmp1);
    }
}

void MainWindow::sendtoWeb()
{
    int i=0;
    if((!DATA::lat.isEmpty())&&(!DATA::lng.isEmpty())) i+=1;
    if((!DATA::temp.isEmpty())&&(!DATA::hump.isEmpty())) i+=10;
    QString dataSend;

    switch(i){
    case 1:{
             dataSend = "P" + DATA::mac + DATA::Ip + DATA::lat + DATA::lng;
              break;
        }
    case 10:{
            dataSend = "H" + DATA::mac + DATA::Ip + DATA::temp + DATA::hump;
              break;
        }
    case 11:{
            dataSend = "KP" + DATA::mac + DATA::Ip + DATA::temp + DATA::hump + DATA::lat + DATA::lng;
             break;
        }
    default: break;
    }

    QString url = "http://lab411s.esy.es/sg/rx.php?data=";
    url+=dataSend;
    //qDebug()<<"Data send "<<dataSend;
    QNetworkRequest request = QNetworkRequest(QUrl(url));
    http1->get(request);
}

void MainWindow::on_btnClear_clicked()
{
    console->clear();
}

void MainWindow::sendImageToWeb(){
    //qDebug() << "Sending Image To Web";
    //qDebug() << DATA::mac;
    //qDebug() << DATA::Ip;
    QString dataSend = "RC:" + DATA::Ip + DATA::mac + DATA::img;
    qDebug() << dataSend;
    QString url = "http://lab411s.esy.es/sg/rx.php?data=";
    url+=dataSend;
    QNetworkRequest request = QNetworkRequest(QUrl(url));
    http1->get(request);
    updateListSensor();
    //qDebug() << "adsf";
}

QString MainWindow::findNearestNode(){
    FileData file(DATA_PATH);
    int N = file.length();
    double *dis = new double[N];
    for(int i = 0;i < N; i++){
        QString line = file.readLine(i+1);
        QStringList lst = line.split(",");
        double Nodelat = QString (lst.value(1)).toDouble();
        double Nodelng = QString (lst.value(2)).toDouble();
        dis[i] = sqrt(pow((DATA::lat.toDouble() - Nodelat)*110.574, 2) + pow((DATA::lng.toDouble()*cos(DATA::lat.toDouble()) - Nodelng*cos(Nodelat))*111.320, 2));
        //dis[i]  = dis()
    }
    int index = 0;
    for(int j = 0; j < N; j++){
        if(dis[j] < dis[index])
            index = j;
    }
    QString str = file.readLine(index + 1);
    QString mac = QStringList(str.split(",")).value(0);
    delete dis;
    return mac;
}

void MainWindow::on_btnUpdate_clicked()
{
    updateListSensor();
}

void MainWindow::delay(int secondsToWait){
    QTime dieTime = QTime::currentTime().addSecs(secondsToWait);
    while(QTime::currentTime() < dieTime){
        QCoreApplication::processEvents(QEventLoop::AllEvents,100);
    }
}

int MainWindow::tPredict(QTime t1, QTime t2, double d1, double d2){
    int time = fabs(t1.secsTo(t2));
    double v = d1/time;
    return int(d2/v);
}

void MainWindow::autoTakePhoto(QString mac){
    QString my_mac = hash.value(mac);
    QTime t = QTime::currentTime();
    bool ok;
    //qDebug() << t << "-----" << movDetectTime.value(my_mac);
    //qDebug() << fabs(t.msecsTo(movDetectTime.value(my_mac))) << "---" << mac << " - " << my_mac;
    if(fabs(t.msecsTo(movDetectTime.value(my_mac))) > 5000){
        console->insertPlainText("Gui lenh chup anh tu dong toi sensor " + my_mac + "\n");
        sendCommand(my_mac.toInt(&ok,10), 1);
        movDetectTime[my_mac] = t;
        QString str = my_mac + ":" + t.toString("hh:mm:ss");
        WriteTextAppend(TIME_DETECT, str );
        //qDebug() << movDetectTime.value(my_mac);
    }
    if(mac == "09"){
        int tmp = movDetectTime.value("05").secsTo(t);
        int timePredict = tPredict(movDetectTime.value("08"), movDetectTime.value("05"), d1, d2);
        if(fabs(tmp - timePredict)/timePredict < 0.05){
            console->insertPlainText("Doi tuong tracking xuat hien tai Sensor 09, gui lenh chup anh\n");
            sendCommand(my_mac.toInt(&ok, 10), 1);
            QString str = my_mac + ":" + t.toString("hh:mm:ss");
            WriteTextAppend(TIME_DETECT, str );
            movDetectTime[my_mac] = t;
            detectedFlag = true;
        }
        else{
            console->insertPlainText("Gui lenh chup anh tu dong toi sensor " + my_mac + "\n");
            sendCommand(my_mac.toInt(&ok,10), 1);
            movDetectTime[my_mac] = t;
            QString str = my_mac + ":" + t.toString("hh:mm:ss");
            WriteTextAppend(TIME_DETECT, str );
        }
    }
    //updateListSensor();
}

double MainWindow::distance(double lat1, double lng1, double lat2, double lng2){
    double R = 6371;
    double dLat = deg2rad(fabs(lat2 - lat1));
    double dLng = deg2rad(fabs(lng2 - lng1));
    double a = pow(sin(dLat/2),2) + cos(deg2rad(lat1)) * cos(deg2rad(lat2)) * pow(sin(dLng/2), 2);
    double b = 2 * atan2(sqrt(a), sqrt(1-a));
    return R * b * 1000;
}

double MainWindow::deg2rad(double deg){
   return deg * M_PI/180;
}

void MainWindow::WriteTextAppend(QString FileName, QString Text)
{
    QFile file(FileName);
    if(file.open(QIODevice::Append))
    {
        QTextStream stream(&file);
        stream << Text << endl;
        file.close();
    }
}

//void MainWindow::on_btngetData_clicked()
//{
//    FileData file(DATA_PATH);
//    int N = file.length();
//    double *temp = new double[N];
//    for(int i=0; i<N; i++){
//        QString line = file.readLine(i+1);
//        QStringList lst = line.split(",");
//        double Nodelat = QString (lst.value(1)).toDouble();
//        double Nodelng = QString (lst.value(2)).toDouble();
//        double dis = distance(DATA::lat.toDouble(), DATA::lng.toDouble(), Nodelat, Nodelng);
//        if(dis < 50 && ListSensorStt[i]){
//            sendCommand(QString(lst.value(0)).toInt(), 0);
//            delay(10);
////            while(1){
////                delay(0.5);
////                if(receivedFlag){
////                    FileData tempData(HISTORY_FILE);
////                    QString lineData = tempData.readLine(tempData.length());
////                    QStringList lstData = lineData.split(":");
////                    qDebug() << "Nhiet do " << lstData.value(1);
////                    temp[i] = QString(lstData.value(1)).toDouble();
////                    break;
////                }
////            }
////            for(int j=tempData.length()-1; j>=0; j--){
////                QString lineData = tempData.readLine(j+1);
////                QStringList lstData = lineData.split(":");
////                if(lstData.value(0) == lst.value(0)){
////                    qDebug() << "Nhiet do " << lstData.value(1);
////                    temp[i] = QString(lstData.value(1)).toDouble();
////                    break;
////                }
////            }
//            FileData tempData(HISTORY_FILE);
//            //qDebug() << tempData.length();
//            QString lineData = tempData.readLine(tempData.length());
//            QStringList lstData = lineData.split(":");
//            qDebug() << "Nhiet do " << lstData.value(1);
//            temp[i] = QString(lstData.value(1)).toDouble();
//            maxTemp[lst.value(0)] = temp[i];
//        }
//        else
//            temp[i] = 0;
//        //receivedFlag = false;
//    }
//    int index = 0;
//    for(int k=0; k<N; k++){
////        bool difference = true;
////        for(int j=0; j<=tempIndex.length(); j++){
////            if(j!=0){
////                if(tempIndex[j] == k){
////                    difference = false;
////                    break;
////                }
////            }
////        }
//        //qDebug() << "temp[" <<k<<"] = " << temp[k];
//        //if(difference){
//            if(temp[k] > temp[index])
//                index = k;
//        //}
//    }
//
////    if(temp[index] < lastTemp.last()){
////        for(int m=0; m<N; m++){
////
////        }
////    }
//
//    //tempIndex.push_back(index);
//    //lastTemp.push_back(temp[index]);
//    QString lineResult = file.readLine(index+1);
//    QStringList lstResult = lineResult.split(",");
//    QString str = DATA::lat + "," + DATA::lng + "," + lstResult.value(1) + "," + lstResult.value(2);
//    QString mac = lstResult.value(0);
//    console->insertPlainText("Trong pham vi 50m, Node " + mac + " co nhiet co cao nhat la: " + QString::number(temp[index]) + "\n");
//    ui->webView->page()->mainFrame()->evaluateJavaScript(QString("addDirectionArrow(%1)").arg(str));
//}

void MainWindow::on_btnPrint_clicked()
{
    QPrinter printer;
    QPrintDialog dialog(&printer, this);
    dialog.setWindowTitle(tr("Print Document"));
    QTextCursor c = QTextCursor(console->document());
    console->textCursor() = c;
    if(c.hasSelection())
        dialog.addEnabledOption(QAbstractPrintDialog::PrintSelection);
    if(dialog.exec() != QDialog::Accepted){
        return;
    }
    console->document()->print(&printer);
}

void MainWindow::onOptimizeMove()
{
    FileData sensor(DATA_PATH);
    double p=0, q=0, r, m=0, u, v, arg;
    for(int i=0; i<sensor.length(); i++){
        if(ListSensor[i]->cur_temp != 0){
            p += ListSensor[i]->x0;
            q += ListSensor[i]->y0;
        }
    }
    qDebug() << p << q;
    r = sqrt(p*p + q*q);
    u = p/r;
    v = q/r;
    for(int i=0; i<sensor.length(); i++){
        if(ListSensor[i]->cur_temp)
            m += ListSensor[i]->x0 * u + ListSensor[i]->y0 * v;
    }
    if(m<0){
        u = -u;
        v = -v;
    }
    //qDebug() << u << " *** " << v;
    arg = M_PI/2 - atan2(v,u);
    //qDebug() << atan2(v,u) << arg;
    QString des_lat, des_lng;
    QString vector;
    if(!DATA::lat.isEmpty() && !DATA::lng.isEmpty()){
        vector = DATA::lat + "," + DATA::lng;
        //qDebug() << DATA::lat + "," + DATA::lng;
        findLastPoint(des_lat, des_lng, DATA::lat.toDouble(), DATA::lng.toDouble(), fabs(m)*20, arg);
    }
    else{
        vector = "21.005739,105.842308";
        findLastPoint(des_lat, des_lng, 21.005739,105.842308, fabs(m)*20, arg);
    }
    qDebug() << fabs(m);
    vector += "," + des_lat + "," + des_lng;
    ui->webView->page()->mainFrame()->evaluateJavaScript(QString("removeDirArrows()"));
    ui->webView->page()->mainFrame()->evaluateJavaScript(QString("addDirectionArrow(%1)").arg(vector));
}

double MainWindow::bearing(double lat1, double lng1, double lat2, double lng2){
    double a = cos(deg2rad(lat2)) * sin(deg2rad(lng2 -lng1));
    double b = cos(deg2rad(lat1))*sin(deg2rad(lat2)) - sin(deg2rad(lat1))*cos(deg2rad(lat2))*cos(deg2rad(lng2-lng1));
//    double a = cos(lat2)*sin(lng2-lng1);
//    double b = cos(lat1)*sin(lat2) - sin(lat1)*cos(lat2)*cos(lng2-lng1);
    a_test = a;
    b_test = b;
    return atan2(a,b);
}

void MainWindow::findLastPoint(QString &lat2, QString &lng2, double lat1, double lng1, double dis, double bear){
//    double ad = deg2rad(dis/6371000);
//    lat2 = asin(sin(deg2rad(lat1))*cos(ad) + cos(deg2rad(lat1))*sin(ad)*cos(bear));
//    lng2 = lng1 + atan2(sin(bear)*sin(ad)*cos(deg2rad(lat1)), cos(ad) - sin(deg2rad(lat1))*sin(deg2rad(lat2)));

    lat1 = deg2rad(lat1);
    lng1 = deg2rad(lng1);
    double ad = (dis/6371000);
    double m_lat = asin(sin(lat1)*cos(ad) + cos(lat1)*sin(ad)*cos(bear));
    double m_lng = lng1 + atan2(sin(bear)*sin(ad)*cos(lat1), cos(ad) - sin(lat1)*sin(m_lat));
    m_lat *= 180/M_PI;
    m_lng *= 180/M_PI;
    lat2 = QString::number(m_lat,'f',6);
    lng2 = QString::number(m_lng,'f',6);
}

void MainWindow::on_btngetNearestNode_clicked()
{
    FileData file(DATA_PATH);
    int N = file.length();
    double *dis = new double[N];
    for(int i=0; i<N; i++){
        QString line = file.readLine(i+1);
        QStringList lst = line.split(",");
        double Nodelat = QString (lst.value(1)).toDouble();
        double Nodelng = QString (lst.value(2)).toDouble();
        dis[i] = distance(DATA::lat.toDouble(), DATA::lng.toDouble(), Nodelat, Nodelng);
//            dis[i] = distance(21.005565,105.842328, Nodelat, Nodelng);
    }
    int index = 0;
    for(int j=0; j<N; j++){
        QString line = file.readLine(j+1);
        QStringList lst = line.split(",");
        if(lst.value(4).toInt() && ListSensorStt[j]){
            index = j;
            break;
        }
    }
    for(int k=index; k<N; k++){
        QString line = file.readLine(k+1);
        QStringList lst = line.split(",");
        if(lst.value(4).toInt()){
            if(dis[k] < dis[index])
                index = k;
        }
        else
            continue;
    }
    QString line_result = file.readLine(index+1);
    QStringList lst_result = line_result.split(",");
    if(index==0){
        if(ListSensorStt[0])
            console->insertPlainText("Node gan nhat co dia chi mac " + lst_result.value(0));
        else
            console->insertPlainText("Chua co nut chup anh nao gia nhap mang!\n");
    }
    else
        console->insertPlainText("Node gan nhat co dia chi mac " + lst_result.value(0));
    delete dis;
}
